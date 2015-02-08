/*

Copyright (c) 2014 Harm Hanemaaijer <fgenfb@yahoo.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef __GNUC__
#include <fenv.h>
#endif
#ifdef OPENGL_ES2
#include <GLES2/gl2.h>
#endif
#ifdef OPENGL
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#include "sre.h"
#include "sre_internal.h"
#include "sre_bounds.h"

static void SetFrustum(sreScene *scene, sreFrustum *frustum, sreView *view) {
    // Update view lookat parameters based on current view mode.
    Point3D object_position;
    if (view->GetViewMode() == SRE_VIEW_MODE_FOLLOW_OBJECT)
        object_position = scene->object[view->GetFollowedObject()]->position;
    view->UpdateParameters(object_position);
    Point3D viewpoint = view->GetViewPoint();
    Vector3D lookat = view->GetLookatPosition();
    Vector3D upvector = view->GetUpVector();
    sreLookAt(viewpoint.x, viewpoint.y, viewpoint.z, lookat.x, lookat.y, lookat.z,
        upvector.x, upvector.y, upvector.z);

    // Set the viewpoint for shader set-up.
    sre_internal_viewpoint = viewpoint;

    // Depending on the setting of SRE_NU_FRUSTUM_PLANES, the far clipping plane
    // may not actually be used.
    frustum->SetParameters(60.0f * view->GetZoom(),
        (float)sre_internal_window_width / sre_internal_window_height,
        sre_internal_near_plane_distance, sre_internal_far_plane_distance);
    frustum->Calculate();
}

// Main render function. Renders the scene based on the specified view. The text overlay
// function is also called. Finally, the configured OpenGL SwapBuffers function is used
// to make the new framebuffer visible.

void sreScene::Render(sreView *view) {
    sre_internal_scene = this;

    if (sre_internal_invalidate_geometry_scissors_cache) {
        InvalidateGeometryScissorsCache();
        sre_internal_invalidate_geometry_scissors_cache = false;
    }

    // Only change the projection matrix if it has changed since the last frame
    // (true when the zoom factor changes).
    if (view->ProjectionHasChangedSinceLastFrame(sre_internal_current_frame))
        sreApplyNewZoom(view);

    if (sre_internal_frustum == NULL)
        sre_internal_frustum = new sreFrustum;
    sreFrustum *frustum = sre_internal_frustum;
    // Recalculate the frustum if the camera view has changed.
    // Also recalculate when the reselect_shaders flag is set (for example when
    // switching to shadow mapping, in which case the frustum shadow map region may
    // be undefined).
    if (CameraHasChangedSinceLastFrame(view, sre_internal_current_frame) ||
    sre_internal_reselect_shaders) {
        // This will set frustum->most_recent_frame_changed to the current frame.
        SetFrustum(this, frustum, view);
    }

    // Restore GL settings for rendering.
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDepthRangef(0, 1.0f);

    CHECK_GL_ERROR("Error before frame.\n");
    if (sre_internal_HDR_enabled) {
        glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_HDR_multisample_framebuffer);
        glClearColor(0, 0, 0, 1.0);
    }
    else
        glClearColor(0, 0, 0, 1.0);
    CHECK_GL_ERROR("Error after glClearColor()\n");
#ifdef OPENGL_ES2
    glClearDepthf(1.0);
#else
    glClearDepth(1.0);
#endif
    glClearStencil(0x00);
    glDisable(GL_STENCIL_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (sre_internal_multi_pass_rendering) {
        // In case of multi-pass rendering, we can already initialize the shaders
        // for the new frame (no light parameters are initialized).
        CHECK_GL_ERROR("Error before GL3InitializeShadersBeforeFrame.\n");
        GL3InitializeShadersBeforeFrame();
        CHECK_GL_ERROR("Error after GL3InitializeShadersBeforeFrame.\n");
    }

    // Perform the visible object determination.
    DetermineVisibleEntities(*frustum);

    if (!sre_internal_multi_pass_rendering) {
        // Single pass rendering (with a final pass for possibly transparent emission only
        //objects).
        // The single pass shaders are limited to one light. Calculate the most prominent
        // light based on the list of determined visible lights.
        CalculateVisibleActiveLights(view, 1);
        // Set the current light.
        if (nu_active_lights > 0) {
            sre_internal_current_light_index = active_light[0];
            sre_internal_current_light = light[active_light[0]];
        }
        else
            sre_internal_current_light = NULL;
        // Initialization of shaders was delayed because the single active light
        // parameters are only known after visible entity determination.
        GL3InitializeShadersBeforeFrame();

        // Render objects.
        RenderVisibleObjectsSinglePass(*frustum);
        RenderFinalPassObjectsSinglePass(*frustum);
    }
    else if (sre_internal_shadows == SRE_SHADOWS_NONE) {
        // Multi-pass lighting without shadows.
        // Perform the ambient pass.
        sre_internal_current_light_index = - 1;
        GL3InitializeShadersBeforeLight();
        // Render the ambient pass which also initializes the depth buffer.
        RenderVisibleObjectsAmbientPass(*frustum);
        // Perform the lighting passes.
        // Enable additive blending.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        // The depth buffer has already been initialized by the ambient pass;
        // we need the depth test, but don't need to update the depth buffer.
        glDepthMask(GL_FALSE);
        // On some OpenGL-ES2 GPU's, GL_EQUAL is slower than the default GL_LEQUAL
        // so only use GL_EQUAL with OpenGL.
#ifndef OPENGL_ES2
        glDepthFunc(GL_EQUAL); 
#else
        // Provide a small offset so that depth buffer precision is less of a problem
        // for additive lighting passes with a non-deterministic tile-based rendering
        // architecture.
        glDepthRangef(0, 0.99999f);
#endif
        RenderLightingPassesNoShadow(frustum, view);
        // Perform the final pass.
        glDisable(GL_BLEND);
#ifndef OPENGL_ES2
        // For OpenGL, restore the GL_LEQUAL depth test for the final pass.
        glDepthFunc(GL_LEQUAL);
#else
        glDepthRangef(0, 1.0f);
#endif
        glDepthMask(GL_TRUE);
        // Note: some objects in the final pass might need blending, but this
        // be enabled/disabled on a per-object basis.
        RenderFinalPassObjectsMultiPass(*frustum);
    }
    else if (sre_internal_shadows == SRE_SHADOWS_SHADOW_VOLUMES) {
        // Multi-pass stencil shadow volumes.
        sreResetShadowCacheStats();
        // Perform the ambient pass.
        sre_internal_current_light_index = - 1;
        GL3InitializeShadersBeforeLight();
#ifdef OPENGL_ES2
        // With a tile-based rendering architecture like ARM Mali-400, depth values are
	// non-deterministic between different lighting passes. The depth range mapping
        // can be adjusted to remedy this. With stencil shadows, care must be taken that
        // depth fail stencil shadows render correctly. The following glDepthRangef are
        // used for different stencil shadows stages:
        // - Ambient pass: Initialize depth buffer with depth range (0, 0.99999f).
        // - Stencil shadow volume passes:
        //  - Depth-pass shadow volumes: Use depth range (0, 0.99998f)
        //  - Depth-fail shadow volumes: Use depth range (0, 1.0f)
        // - Lighting passes: Use depth range (0, 0.99998f) to avoid artifacts when
        //   depth values for the same fragment differ slightly between the lighting
        //   pass and the ambient pass.
        // - Final pass: Use depth range (0, 0.99999f).
        glDepthRangef(0, 0.99999f);
#endif
        // Render the ambient pass which also initializes the depth buffer.
        RenderVisibleObjectsAmbientPass(*frustum);
        // Perform the lighting passes.
        // Enable additive blending.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        // Enable the stencil test (must be enabled for both stencil buffer creation
        // and the actual lighting pass with stencil shadows.
        glEnable(GL_STENCIL_TEST);
        // The depth buffer has already been initialized by the ambient pass;
        // we need the depth test, but don't need to update the depth buffer.
        glDepthMask(GL_FALSE);
        // On some OpenGL-ES2 GPU's, GL_EQUAL is slower than the default GL_LEQUAL
        // so only use GL_EQUAL with OpenGL.
#if !defined(OPENGL_ES2)
        glDepthFunc(GL_EQUAL);
#else
	// For OpenGL ES 2.0 with stencil shadows, glDepthRangef is used by
        // RenderLightingPasses as appropriate.
#endif
        RenderLightingPasses(frustum, view);
        // Perform the final pass.
        glDisable(GL_STENCIL_TEST);
        // Disable blending
        glDisable(GL_BLEND);
#if !defined(OPENGL_ES2)
        // For OpenGL, restore the GL_LEQUAL depth test for the final pass.
        glDepthFunc(GL_LEQUAL);
#else
        glDepthRangef(0, 0.99999f);
#endif
        glDepthMask(GL_TRUE);
        RenderFinalPassObjectsMultiPass(*frustum);
    }
    else if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING) {
        // Multi-pass lighting with shadow mapping.
        // Perform the ambient pass.
        sre_internal_current_light_index = - 1;
        CHECK_GL_ERROR("Error before ambient pass (shadow mapping).\n");
        GL3InitializeShadersBeforeLight();
        // Render the ambient pass which also initializes the depth buffer.
        RenderVisibleObjectsAmbientPass(*frustum);
        CHECK_GL_ERROR("Error after ambient pass (shadow mapping).\n");
        // Perform the lighting passes.
        // Enable additive blending.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        // The depth buffer has already been initialized by the ambient pass;
        // we need the depth test, but don't need to update the depth buffer.
        glDepthMask(GL_FALSE);
        // On some OpenGL-ES2 GPU's, GL_EQUAL is slower than the default GL_LEQUAL
        // so only use GL_EQUAL with OpenGL.
#ifndef OPENGL_ES2
        glDepthFunc(GL_EQUAL); 
#endif
	// For OpenGL ES 2.0 with stencil shadows, glDepthRangef is used by
        // RenderLightingPasses as appropriate.
        RenderLightingPasses(frustum, view);
        CHECK_GL_ERROR("Error after lighting pass (shadow mapping).\n");
        // Perform the final pass.
        // Disable blending
        glDisable(GL_BLEND);
#ifndef OPENGL_ES2
        // For OpenGL, restore the GL_LEQUAL depth test for the final pass.
        glDepthFunc(GL_LEQUAL);
#endif
        glDepthMask(GL_TRUE);
        CHECK_GL_ERROR("Error before final pass (shadow mapping).\n");
        RenderFinalPassObjectsMultiPass(*frustum);
    }
    CHECK_GL_ERROR("Error after rendering.\n");

    // Post-processing for HDR rendering.
#ifndef NO_HDR
    if (sre_internal_HDR_enabled) {
        // Resolve the multi-sampled float framebuffer into a regular float framebuffer.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, sre_internal_HDR_multisample_framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_HDR_framebuffer);
        glBlitFramebuffer(0, 0, sre_internal_window_width, sre_internal_window_height,
            0, 0, sre_internal_window_width, sre_internal_window_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        // Tone mapping pass one: calculate average and maximum log luminance values into a 256x256 texture.
        glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_HDR_log_luminance_framebuffer);
        glViewport(0, 0, 256, 256);
        GL3InitializeHDRLogLuminanceShader();
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, sre_internal_HDR_full_screen_vertex_buffer);
        glVertexAttribPointer(
           0,                  // attribute 0 (positions)
           2,                  // size
           GL_FLOAT,           // type
           GL_FALSE,           // normalized?
           0,                  // stride
           (void *)0	   // array buffer offset in bytes
        );
        glDrawArrays(GL_TRIANGLES, 0, 6);
//        glDisableVertexAttribArray(0);
        CHECK_GL_ERROR("Error after tone mapping pass one.\n");
        // Repeatedly calculate the average of 4x4 blocks to arrive at a single pixel.
        GL3InitializeHDRAverageLuminanceShader();
        CHECK_GL_ERROR("Error after average luminance shader initialization.\n");
        int w = 64;
        int h = 64;
        for (int i = 0; i < 4; i++) {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_HDR_average_luminance_framebuffer[i]);
            glViewport(0, 0, w, h);
            if (i == 0)
                GL3InitializeHDRAverageLuminanceShaderWithLogLuminanceTexture();
            else
                GL3InitializeHDRAverageLuminanceShaderWithAverageLuminanceTexture(i - 1);
            CHECK_GL_ERROR("Error after average luminance shader texture initialization.\n");
            glDrawArrays(GL_TRIANGLES, 0, 6);
            w /= 4;
            h /= 4;
        }
//        glDisableVertexAttribArray(0);
        CHECK_GL_ERROR("Error after tone mapping pass two.\n");
        // Tone mapping pass three: luminance history storage.
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_HDR_luminance_history_storage_framebuffer);
        int slot = sre_internal_current_frame & 15;
	glViewport(slot, 0, 1, 1);
        GL3InitializeHDRLuminanceHistoryStorageShader();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // Tone mapping pass four: luminance history comparison.
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_HDR_luminance_history_comparison_framebuffer);
	glViewport(0, 0, 1, 1);
        GL3InitializeHDRLuminanceHistoryComparisonShader(sre_internal_current_frame & 15);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // Tone mapping pass five: tone mapping.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(0, 0, sre_internal_window_width, sre_internal_window_height);
        GL3InitializeHDRToneMapShader();
        CHECK_GL_ERROR("Error after tone mapping shader initialization.\n");
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(0);
    }
#endif

    // Note: When HDR rendering is enabled, shadow volume shadows disappear (error?).

    // Visualize shadow maps in an overlay if requested.
#ifndef NO_SHADOW_MAP
    if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING &&
    sre_internal_visualized_shadow_map != -1)
        // Visualize shadow map/cube map for a specific light if possible.
        sreVisualizeShadowMap(sre_internal_visualized_shadow_map, frustum);
#endif

    // Draw overlayed text. Any interfering settings such as depth buffer
    // and back-face culling are disabled.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_FALSE);
    // Enable additive blending by default, but blending setting can be changed
    // (for example using sreSetImageBlendingMode()).
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    sreDrawTextOverlayFunc();
 
    // Display.
    sre_internal_swap_buffers_func();

    // In case the frame has been drawn with mandatory new shader selection for each object,
    // clear the flag so that shaders are remembered.
    sre_internal_reselect_shaders = false;
    // Any aspect ratio change will have been applied to loaded shaders.
    sre_internal_aspect_changed = false;

    sre_internal_current_frame++;
}

// Impact of static frustum optimization.
// sre-demo --no-shadows --benchmark demo2
//
// Without static frustum optimization:    178.9 fps
// With static frustum optimization:       210.6 fps
//

// Quick estimation of the projected screen size of an object (or bounding volume).
// The size returned is a length in screen space units (which range from -1.0 to 1.0),
// not an area.

static float ProjectedSize(const Point3D& V, float bounding_radius) {
    float w = Dot(sre_internal_view_projection_matrix.GetRow(3), V);
    // If the projected distance in the view direction (the w coordinate)
    // of V is very small, dividing by w will yield a big number, which is OK,
    // because the object should not be skipped (it is already known to
    // intersect the view frustum). To avoid division by zero, just return
    // 2.0 (the full normalized screen size) when w is very small.
    if (w <= 0.0001f)
        return 2.0f;
    return fabsf(bounding_radius * 2.0f / w);
}

#if 0

// Projected size based on distance from the viewpoint, which is not affected by
// distance to the view plane. This is a often a better indicator of the prominence
// of an object.

static float NormalizedProjectedSize(const Point3D& P, float bounding_radius) {
    float d = Magnitude(P - sre_internal_viewpoint);
    if (d <= 0.0001f)
        return 2.0f;
    return bounding_radius * 2.0f / d;
}

#endif

// More accurate determination of projected size on the screen.

static float AccurateProjectedSize(const sreFrustum& f, const Point3D& P, float bounding_radius) {
    float z = Dot(sre_internal_view_matrix.GetRow(2), P);
    // z is the distance from the viewpoint in the view direction.
    float r = bounding_radius;
    Point3D Q;
    if (z < f.nearD) {
        // If the center of the object is in front of the near plane, then adjust
        // the bounding radius to the size at the near plane.
        r = bounding_radius * (bounding_radius + z - f.nearD);
        Q.x = P.x;
        Q.y = P.y;
        Q.z = f.nearD;
    }
    else if (z < f.nearD + bounding_radius) {
        // When the center of the object is beyond the near plane but the bounding radius
	// intersects it, calculate the z' for which the projected size is greatest.
        // Maximize r / w
        // <-> Maximize r / z' for f.nearD <= z - bounding_radius <= z' <= z + bounding_radius
        // where r = bounding_radius * sin(frustum_slope * (z' - z) / bounding_radius * PI / 2)
        // and frustum_slope = f.e.
        // <-> Maximize sin(frustum_slope * (z' - z) / bounding_radius * PI / 2) / z'
        // Let c = (frustum_slope * 0.5 * PI / bounding_radius), f = sin(c * (z' - z)) and
        // g = 1 / z' = z' ^ (-1)
        // Derivative is f' * g + f * g' =
        // cos(c * (z' - z)) * c / z' + sin(c * (z' - z)) * (-1) * z' ^ (-2)
        // = cos(c * (z' - z)) * c / z' - sin(c * (z' - z)) / z'^2 = 0
        // <-> cos(c * (z' - z)) * c / z' - cos(c * (z' - z) - 0.5 * PI) / z'^2 = 0 
        // <-> cos(c * (z' - z)) * c / z' = cos(c * (z' - z) - 0.5 * PI) / z'^2
        // <-> cos(c * (z' - z)) * c = cos(c * (z' - z) - 0.5 * PI) / z'
        // <-> cos(c * (z' - z)) * c = cos(c * (z' - z)) * cos(0.5 * PI) +
        //     sin(c * (z' - z)) * sin(0.5 * PI)
        // <-> cos(c * (z' - z)) * c = sin (c * (z' - z) <->
        // <-> c * (z' - z)) = 0.25 * PI or c * (z' - z) = - 0.75 * PI
        // <-> z' = (0.25 * PI + c * z) / c = 0.25 * PI / c + z
        //     or z' = z - 0.75 * PI / c
        // <-> z' = 0.5 / (frustum_slope / bounding_radius) + z
        //     or z' = z - 1.5 / (frustum_slope / bounding_radius) + z
        // <-> z' = 0.5 * bounding_radius / f.e + z.
        // z1 is valid (z1 < z + bounding_radius) if frustum.e > 0.5
        float z1 = 0.5f * bounding_radius / f.e + z;
        // z2 is valid (z2 > z - bounding_radius) if frustum.e > 1.5 (unusually wide view)
        float z2 = z - 1.5f * bounding_radius / f.e;
        Q.x = P.x;
        Q.y = P.y;
        Q.z = z1;
        r = bounding_radius * sinf(f.e * (z1 - z) / bounding_radius * M_PI / 2);
    }
    else
	Q = P;
    float w = Dot(sre_internal_view_projection_matrix.GetRow(3), Q);
    // w is the division factor to obtain the projected size of a hypothetical
    // flat object in the same plane as the view plane. Since 2.0 * bounding_radius
    // is a max bound on the real size, 2.0 bounding_radius / w will bound the
    // projected x or y size in screen space, which has coordinates ranging from
    // [-1.0, 1.0].
    return fabsf(bounding_radius * 2.0f / w);
}


static int octree_culled_count_frustum = 0;
static int octree_culled_count_projected = 0;
static int octree_objects_inside = 0;

// Determine whether the object is visible (intersects the view frustum), and if so
// add it to a few possible arrays:
//
// - The object is added to visible_object[] if it should be drawn in a lighting pass.
//   In this case, the most_recent_frame_visible field in the object is set to
//   the current frame. This information is latee is used when drawing objects for
//   local lights from the list of static light-receiving objects for the light, to
//   quickly determine whether the object is visible (it may be used in other places
//   such as shadows).
// - The object is added to final_pass_object[] if it should be drawn in the final
//   pass.

void sreScene::DetermineObjectIsVisible(sreObject& so, const sreFrustum& frustum,
BoundsCheckResult bounds_check_result) {
    // Bounds checks on view frustum.
    if (bounds_check_result != SRE_COMPLETELY_INSIDE) {
#if SRE_NU_FRUSTUM_PLANES == 6
        // Infinite distance object should not be clipped by the far plane.
        if (so.flags & SRE_OBJECT_INFINITE_DISTANCE) {
            if (!Intersects(so, frustum.frustum_without_far_plane_world))
                return;
        }
        else
#endif
        if (!Intersects(so, frustum.frustum_world))
            return;
    }
    else
        octree_objects_inside++;

    // Check the projected object size, and store it. Objects below the threshold
    // size will not be rendered. The projected size is stored in the object
    // structure for potential use by geometry scissors calculations.
    // "Infinite distance" objects generally have very large coordinate values,
    // so that their projected size is still material.
    so.projected_size = ProjectedSize(so.sphere.center, so.sphere.radius);
    if (so.projected_size < SRE_OBJECT_SIZE_CUTOFF)
        return;

    // If the object should be drawn in lighting passes, mark the object as visible.
    if (!(so.flags & (SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_LIGHT_HALO | SRE_OBJECT_PARTICLE_SYSTEM))) {
        so.most_recent_frame_visible = sre_internal_current_frame;
        if (nu_visible_objects == max_visible_objects) {
            // Dynamically increase the visible objects array size when needed.
            int *new_visible_object = new int[max_visible_objects * 2];
            memcpy(new_visible_object, visible_object, sizeof(int) * max_visible_objects);
            delete [] visible_object;
            visible_object = new_visible_object;
            max_visible_objects *= 2;
        }
        visible_object[nu_visible_objects] = so.id;
        nu_visible_objects++;
        return;
    }

    // If the object should be drawn in the final pass, queue the object for later sorting and rendering.
    // Note: It is not necessary to set so.most_recent_frame_visible for final pass objects.
    if (nu_final_pass_objects == max_final_pass_objects) {
        // Dynamically increase the final pass objects array size when needed.
        int *new_final_pass_object = new int[max_final_pass_objects * 2];
        memcpy(new_final_pass_object, final_pass_object, sizeof(int) *
            max_final_pass_objects);
        delete [] final_pass_object;
        final_pass_object = new_final_pass_object;
        max_final_pass_objects *= 2;
    }
    final_pass_object[nu_final_pass_objects] = so.id;
    nu_final_pass_objects++;
    return;
}


// Determine visibility of an array of entities defined in a single node of a "fast" or
// "fast strict" octree. nu_entities entities starting at fast_oct array index array_index
// are processed.

void sreScene::DetermineFastOctreeNodeVisibleEntities(const sreFastOctree& fast_oct,
const sreFrustum& frustum, BoundsCheckResult bounds_check_result, int array_index, int nu_entities) {
    for (int i = 0; i < nu_entities; i++) {
        sreSceneEntityType type;
        int index;
        fast_oct.GetEntity(array_index + i, type, index);
        if (type == SRE_ENTITY_OBJECT) {
            sreObject *so = object[index];
            if (!(so->flags & SRE_OBJECT_HIDDEN))
                DetermineObjectIsVisible(*so, frustum, bounds_check_result);
        }
        else if (type == SRE_ENTITY_LIGHT) {
            sreLight *l = light[index];
//            printf("Checking visibility of light %d\n", l->id);
            if (!(l->type & SRE_LIGHT_DIRECTIONAL)) {
                if (l->type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE) {
                    // If there are worst-case bounds, use them, because when the frustum doesn't
                    // change for a while in subsequent frames, the variable light volume might
                    // come into view.
                    if (!Intersects(l->worst_case_sphere, frustum.frustum_world))
                        // If outside, skip the light.
                        continue;
                }
                else if (!Intersects(*l, frustum.frustum_world))
                    // If outside, skip the light.
                    continue;
            }
            // Check whether the project size of the light volume is too small.
            if (!(l->type & SRE_LIGHT_DIRECTIONAL)) {
                l->projected_size = AccurateProjectedSize(frustum, l->vector.GetPoint3D(),
                    l->sphere.radius);
                if (l->projected_size < SRE_LIGHT_VOLUME_SIZE_CUTOFF)
                    continue;
            }
//            printf("Light %d is visible, type = %d.\n", l->id, l->type);
            visible_light_array.Add(l->id);
        }
    }
}

// Recursive determination of entities (objects and light volumes) that intersect the view
// frustum using a "fast" octree.
//
// visible_light_array is updated when a visible light is encountered (meaning a light
// that can affect objects in within the view frustum).
// The visible_object[] array (the object's most_recent_frame_visible field) is
// updated for visible objects that need to be drawn in lighting passes, and final_pass_object[]
// is updated for visible final-pass objects 
//
// The "fast" octree uses one combined integer array to store information. It has the
// following structure:
// 
// - A node index number.
// - The number of non-empty octants (sub-nodes).
// - The number of entities in the node itself (not including entities in deeper sub-nodes).
// - The array of entities, each encoded as a single integer (light or object flag in
//   bit 31, the other bits are the index).
// - The starting index into the array of the information for each non-empty sub-node.
//
// The root node information is at array index 0.
//
// The AABB bounding volume of each node has no restrictions; is it defined by the node_index
// and stored in the node_bounds[] array. Seperate AABB bounds are defined for every non-empty
// node. Allowing non-regular variation of the subnode sizes (as compared to a traditional
// octree implementation) allows a lower total number of nodes and lower octree depth.

void sreScene::DetermineVisibleEntitiesInFastOctree(const sreFastOctree& fast_oct, int array_index,
const sreFrustum& frustum, BoundsCheckResult bounds_check_result) {
    int node_index = fast_oct.array[array_index];
    if (array_index != 0 && bounds_check_result != SRE_COMPLETELY_INSIDE) {
        // If it's not the root node, check the bounds of this node against the view frustum.
        bounds_check_result = QueryIntersection(fast_oct.node_bounds[node_index], frustum.frustum_world);
        if (bounds_check_result == SRE_COMPLETELY_OUTSIDE) {
            // If they do not intersect, discard this part of the octree.
            octree_culled_count_frustum++;
            return;
        }
#if SRE_NU_FRUSTUM_PLANES == 5
        // In the case there is no far frustum plane, if the projected size of the octree is too
        // small, skip it. Have to check that octree does not contain the viewpoint.
        if (!Intersects(sre_internal_viewpoint, fast_oct.node_bounds[node_index].AABB)) {
            float size = AccurateProjectedSize(fast_oct.node_bounds[node_index].sphere.center,
                fast_oct.node_bounds[node_index].sphere.radius);
//            printf("size = %f\n", size);
            if (size < SRE_OCTREE_SIZE_CUTOFF) {
                octree_culled_count_projected++;
                return;
            }
        }
#endif
    }
    int nu_octants = fast_oct.GetNumberOfOctants(array_index + 1);
    int nu_entities = fast_oct.array[array_index + 2];
//    printf("Number of entities in node: %d.\n", nu_entities);
    array_index += 3;
    DetermineFastOctreeNodeVisibleEntities(fast_oct, frustum, bounds_check_result,
        array_index, nu_entities);
    array_index += nu_entities;
    // Check every non-empty subnode.
    for (int i = 0; i < nu_octants; i++) {
        DetermineVisibleEntitiesInFastOctree(fast_oct, fast_oct.array[array_index + i], frustum,
            bounds_check_result);
    }
}

// Process all entities in the octree except the entities in the root node.

void sreScene::DetermineVisibleEntitiesInFastOctreeNonRootNode(const sreFastOctree& fast_oct, int array_index,
const sreFrustum& frustum, BoundsCheckResult bounds_check_result) {
    int nu_octants = fast_oct.GetNumberOfOctants(array_index + 1);
    int nu_entities = fast_oct.array[array_index + 2];
    // Just skip the entities in the root node.
    array_index += nu_entities + 3;
    // Check every non-empty subnode.
    for (int i = 0; i < nu_octants; i++) {
        DetermineVisibleEntitiesInFastOctree(fast_oct, fast_oct.array[array_index + i], frustum,
            bounds_check_result);
    }
}

// This function is similar to regular fast octree traversal, but only determines visibility
// for the list of object in the root node of a "fast" octree. This is used for entities in
// the "infinite distance" octree, which only has a root node, like directional lights and
// far-away objects like sky textures, sky objects, and horizons. These are never affected by
// the far plane of the frustum.
//
// When static frustum optimization is enabled, this function is also used to seperate the
// root node objects of the main octree at the end of the visible objects array.

void sreScene::DetermineVisibleEntitiesInFastOctreeRootNode(const sreFastOctree& fast_oct, int array_index,
const sreFrustum& frustum, BoundsCheckResult bounds_check_result) {
    int nu_entities = fast_oct.array[array_index + 2];
    DetermineFastOctreeNodeVisibleEntities(fast_oct, frustum, bounds_check_result,
        array_index + 3, nu_entities);
}

// A "fast strict" octree is an optimized but more limited kind of "fast" octree that has
// less memory requirements. The AABB bounds of the subnodes are calculated on the fly.
// The octree subnodes are always a regular subdivision (half the dimension, eight possible
// octants) of the parent octree, like in traditional octrees.
//
// - The number of non-empty octants (sub-nodes) is defined by bits 0-7 of the first integer.
// - Bits 8 up to 31 of the integer encode the non-empty octant indices (value 0-7). Bits
//   8-10 contain the first octant, bits 11-13 define the second octant, etc. Up to eight
//   octants may be present.
// - The second integer is the number of entities in the node itself (not including entities
//   in deeper sub-nodes).
// - The array of entities, each encoded as a single integer (light or object flag in
//   bit 31, the other bits are the index).
// - Subsequent integers (up to 8) represent the starting index into the data array of
//   the data for each non-empty sub-node, in the previously defined order.   

// Relative position of subnode centers in terms of the current node dimensions,
// offset from AABB.dim_min of the current node.

static Vector3D subnode_center_vector[8] = {
    Vector3D(0.25f, 0.25f, 0.25f),
    Vector3D(0.75f, 0.25f, 0.25f),
    Vector3D(0.25f, 0.75f, 0.25f),
    Vector3D(0.75f, 0.75f, 0.25f),
    Vector3D(0.25f, 0.25f, 0.75f),
    Vector3D(0.75f, 0.25f, 0.75f),
    Vector3D(0.25f, 0.75f, 0.75f),
    Vector3D(0.75f, 0.75f, 0.75f)
};

// Process all entities in the fast strict octree except the entities in the root node.

void sreScene::DetermineVisibleEntitiesInFastStrictOptimizedOctreeNonRootNode(const sreFastOctree& fast_oct,
const sreOctreeNodeBounds& node_bounds, int array_index, const sreFrustum& frustum,
BoundsCheckResult bounds_check_result) {
    // The optimized fast strict octree has no node index.
    unsigned int octant_data = (unsigned int)fast_oct.array[array_index];
    int nu_octants = octant_data & 0xFF;
    // Immediately return when they are no octants.
    if (nu_octants == 0)
        return;
    // Shift the octant data so that the bits representing the first non-empty octant index
    // is at bit 0.
    octant_data >>= 8;
    int nu_entities = fast_oct.array[array_index + 1];
    // Just skip the entities, and point the array index to the list of subnode array index
    // pointers.
    array_index += nu_entities + 2;
    // Recursively process every non-empty subnode.
#if 1
    float dim = node_bounds.AABB.dim_max.x - node_bounds.AABB.dim_min.x;
    Vector3D subnode_half_dim = Vector3D(0.25f, 0.25f, 0.25f) * dim;
    for (int i = 0; i < nu_octants; i++) {
        // Bits 0-2 of octant_data contain the octant index.
        int octant = octant_data & 7;
        // Shift octant data to the next index.
        octant_data >>= 3;
        // Dynamically calculate the octants bounds.
        sreOctreeNodeBounds subnode_bounds;
        subnode_bounds.sphere.center = node_bounds.AABB.dim_min + subnode_center_vector[octant] * dim;
        subnode_bounds.AABB.dim_min = subnode_bounds.sphere.center - subnode_half_dim;
        subnode_bounds.AABB.dim_max = subnode_bounds.sphere.center + subnode_half_dim;
        subnode_bounds.sphere.radius = node_bounds.sphere.radius * 0.5f;
        DetermineVisibleEntitiesInFastStrictOptimizedOctree(fast_oct, subnode_bounds,
            fast_oct.array[array_index + i], frustum, bounds_check_result);
    }
#else
    sreOctreeNodeBounds *subnode_bounds = (sreOctreeNodeBounds *)alloca(nu_octants * sizeof(sreOctreeNodeBounds));
    float dim = node_bounds.AABB.dim_max.x - node_bounds.AABB.dim_min.x;
    int i = 0;
    // Dynamically calculate the octree sub-node bounds for the used octants.
    switch (first_octant & 7) {
    // Fall-through cases.
    case 0 :
            subnode_bounds[i].sphere.center = node_bounds.AABB.dim_min + Vector3D(dim * 0.25, dim * 0.25, dim * 0.25);
            subnode_bounds[i].AABB.dim_min = node_bounds.AABB.dim_min;
            subnode_bounds[i].AABB.dim_max = node_bounds.sphere.center;
            i++;
            if (i == nu_octants)
                break;
    case 1 :
        if (octant_flags & 2) {
            subnode_bounds[i].sphere.center = node_bounds.AABB.dim_min + Vector3D(dim * 0.75, dim * 0.25, dim * 0.25);
            subnode_bounds[i].AABB.dim_min = node_bounds.AABB.dim_min + Vector3D(dim * 0.5, 0, 0);
            subnode_bounds[i].AABB.dim_max = node_bounds.sphere.center + Vector3D(dim * 0.5, 0, 0);
            i++;
            if (i == nu_octants)
                break;
        }
    case 2 :
        if (octant_flags & 4) {
            subnode_bounds[i].sphere.center = node_bounds.AABB.dim_min + Vector3D(dim * 0.25, dim * 0.75, dim * 0.25);
            subnode_bounds[i].AABB.dim_min = node_bounds.AABB.dim_min + Vector3D(0, dim * 0.5, 0);
            subnode_bounds[i].AABB.dim_max = node_bounds.sphere.center + Vector3D(0, dim * 0.5, 0);
            i++;
            if (i == nu_octants)
                break;
        }
    case 3 :
        if (octant_flags & 8) {
            subnode_bounds[i].sphere.center = node_bounds.AABB.dim_min + Vector3D(dim * 0.75, dim * 0.75, dim * 0.25);
            subnode_bounds[i].AABB.dim_min = node_bounds.AABB.dim_min + Vector3D(dim * 0.5, dim * 0.5, 0);
            subnode_bounds[i].AABB.dim_max = node_bounds.sphere.center + Vector3D(dim * 0.5, dim * 0.5, 0);
            i++;
            if (i == nu_octants)
                break;
        }
    case 4 :
        if (octant_flags & 16) {
            subnode_bounds[i].sphere.center = node_bounds.AABB.dim_min + Vector3D(dim * 0.25, dim * 0.25, dim * 0.75);
            subnode_bounds[i].AABB.dim_min = node_bounds.AABB.dim_min + Vector3D(0, 0, dim * 0.5);
            subnode_bounds[i].AABB.dim_max = node_bounds.sphere.center + Vector3D(0, 0, dim * 0.5);
            i++;
            if (i == nu_octants)
                break;
        }
    case 5 :
        if (octant_flags & 32) {
            subnode_bounds[i].sphere.center = node_bounds.AABB.dim_min + Vector3D(dim * 0.75, dim * 0.25, dim * 0.75);
            subnode_bounds[i].AABB.dim_min = node_bounds.AABB.dim_min + Vector3D(dim * 0.5, 0, dim * 0.5);
            subnode_bounds[i].AABB.dim_max = node_bounds.sphere.center + Vector3D(dim * 0.5, 0, dim * 0.5);
            i++;
            if (i == nu_octants)
                break;
        }
    case 6 :
        if (octant_flags & 64) {
            subnode_bounds[i].sphere.center = node_bounds.AABB.dim_min + Vector3D(dim * 0.25, dim * 0.75, dim * 0.75);
            subnode_bounds[i].AABB.dim_min = node_bounds.AABB.dim_min + Vector3D(0, dim * 0.5, dim * 0.5);
            subnode_bounds[i].AABB.dim_max = node_bounds.sphere.center + Vector3D(0, dim * 0.5, dim * 0.5);
            i++;
            if (i == nu_octants)
                break;
        }
    case 7 :
        if (octant_flags & 128) {
            subnode_bounds[i].sphere.center = node_bounds.AABB.dim_min + Vector3D(dim * 0.75, dim * 0.75, dim * 0.75);
            subnode_bounds[i].AABB.dim_min = node_bounds.sphere.center;
            subnode_bounds[i].AABB.dim_max = node_bounds.AABB.dim_max;
            i++;
            if (i == nu_octants)
                break;
        }
    }
    for (int i = 0; i < nu_octants; i++) {
        subnode_bounds[i].sphere.radius = node_bounds.sphere.radius * 0.5f;
        DetermineVisibleEntitiesInFastStrictOptimizedOctree(fast_oct, subnode_bounds[i],
            fast_oct.array[array_index + i], frustum, bounds_check_result);
    }
#endif
}

void sreScene::DetermineVisibleEntitiesInFastStrictOptimizedOctree(const sreFastOctree& fast_oct,
const sreOctreeNodeBounds& node_bounds, int array_index, const sreFrustum& frustum,
BoundsCheckResult bounds_check_result) {
    if (array_index != 0 && bounds_check_result != SRE_COMPLETELY_INSIDE) {
        // If it's not the root node, check the bounds of this node against the view frustum.
        bounds_check_result = QueryIntersection(node_bounds, frustum.frustum_world);
        if (bounds_check_result == SRE_COMPLETELY_OUTSIDE) {
            // If they do not intersect, discard this part of the octree.
            octree_culled_count_frustum++;
            return;
        }
#if SRE_NU_FRUSTUM_PLANES == 5
        // In the case there is no far frustum plane, if the projected size of the octree is too small,
        // skip it.
        // Have to check that octree does not contain the viewpoint.
        if (!Intersects(sre_internal_viewpoint, node_bounds.AABB)) {
            float size = AccurateProjectedSize(node_bounds.sphere.center,
                node_bounds.sphere.radius);
            if (size < SRE_OCTREE_SIZE_CUTOFF) {
                octree_culled_count_projected++;
                return;
            }
        }
#endif
    }
    // The optimized fast strict octree has no node index.
    int nu_entities = fast_oct.array[array_index + 1];
    // Determine visibility for the entities stored in this node.
    DetermineFastOctreeNodeVisibleEntities(fast_oct, frustum, bounds_check_result,
        array_index + 2, nu_entities);

    // To avoid duplicating the sub-node bounds calculation code, just call the non-root-node
    // function to process the subnodes.
    DetermineVisibleEntitiesInFastStrictOptimizedOctreeNonRootNode(fast_oct, node_bounds,
        array_index, frustum, bounds_check_result);
}

// Only process root-node entities in a fast strict octree.

void sreScene::DetermineVisibleEntitiesInFastStrictOptimizedOctreeRootNode(const sreFastOctree& fast_oct,
int array_index, const sreFrustum& frustum, BoundsCheckResult bounds_check_result) {
    // The optimized fast strict octree has no node index.
    int nu_entities = fast_oct.array[array_index + 1];
    DetermineFastOctreeNodeVisibleEntities(fast_oct, frustum, bounds_check_result,
        array_index + 2, nu_entities);
}

// Keep track of the number of static visible objects and lights so that
// they can be reused for visible entity determination in the next frame
// if the view frustum doesn't change.
static int nu_static_visible_objects;
static int nu_static_final_pass_objects;
static int nu_static_visible_lights;

void sreScene::DetermineVisibleEntities(const sreFrustum& frustum) {
    octree_culled_count_frustum = 0;
    octree_culled_count_projected = 0;
    octree_objects_inside = 0;

    // An optimization is possible when the view frustum has not changed
    // (frustum.most_recent_frame_changed < current_frame). The visible/final pass object
    // and visible light arrays from the previous frame will still be present and can be
    // reused. Only the static objects and lights can be reused; the visibility of dynamic
    // object and lights has to redetermined using the dynamic entities octrees.
    if (frustum.most_recent_frame_changed < sre_internal_current_frame) {
        // Re-use visible objects up to nu_static_visible_objects and nu_static_final_pass_objects,
        // visible lights up to nu_static_visible_lights;
        nu_visible_objects = nu_static_visible_objects;
        nu_final_pass_objects = nu_static_final_pass_objects;
        visible_light_array.Truncate(nu_static_visible_lights);
        if (sre_internal_octree_type == SRE_OCTREE_STRICT_OPTIMIZED || sre_internal_octree_type ==
        SRE_QUADTREE_XY_STRICT_OPTIMIZED) {
            // Only need to recheck the dynamic entities.
            DetermineVisibleEntitiesInFastStrictOptimizedOctreeRootNode(
                fast_octree_dynamic, 0, frustum, SRE_COMPLETELY_INSIDE);
            DetermineVisibleEntitiesInFastStrictOptimizedOctreeRootNode(
                fast_octree_dynamic_infinite_distance, 0, frustum, SRE_COMPLETELY_INSIDE);
        }
        else {
            // Only need to recheck the dynamic entities.
            DetermineVisibleEntitiesInFastOctreeRootNode(fast_octree_dynamic, 0, frustum,
                SRE_COMPLETELY_INSIDE);
            DetermineVisibleEntitiesInFastOctreeRootNode(fast_octree_dynamic_infinite_distance,
                0, frustum, SRE_COMPLETELY_INSIDE);
        }
        return;
    }

    // Full visible entity determination (static and dynamic objects).
    nu_visible_objects = 0;
    nu_final_pass_objects = 0;
    visible_light_array.Truncate(0);

    if (sre_internal_octree_type == SRE_OCTREE_STRICT_OPTIMIZED || sre_internal_octree_type ==
    SRE_QUADTREE_XY_STRICT_OPTIMIZED) {
        // When an optimized strict octree or quadtree is used, calculate node bounding information
        // on the fly instead of looking it up in memory.
        // Traverse the static entities octrees.
        DetermineVisibleEntitiesInFastStrictOptimizedOctree(fast_octree_static,
            fast_octree_static.node_bounds[0], 0, frustum, SRE_BOUNDS_UNDEFINED);
        DetermineVisibleEntitiesInFastStrictOptimizedOctreeRootNode(
            fast_octree_static_infinite_distance, 0, frustum, SRE_BOUNDS_UNDEFINED);
        nu_static_visible_objects = nu_visible_objects;
        nu_static_final_pass_objects = nu_final_pass_objects;
        nu_static_visible_lights = visible_light_array.Size();
        // Handle all dynamic entities. They will be stored at the end of the visible entity arrays.
        DetermineVisibleEntitiesInFastStrictOptimizedOctreeRootNode(fast_octree_dynamic,
            0, frustum, SRE_COMPLETELY_INSIDE);
        DetermineVisibleEntitiesInFastStrictOptimizedOctreeRootNode(
            fast_octree_dynamic_infinite_distance, 0, frustum, SRE_COMPLETELY_INSIDE);
    }
    else {
        // Traverse the static entities octrees.
        DetermineVisibleEntitiesInFastOctree(fast_octree_static, 0, frustum, SRE_BOUNDS_UNDEFINED);
        DetermineVisibleEntitiesInFastOctree(fast_octree_static_infinite_distance,
            0, frustum, SRE_BOUNDS_UNDEFINED);
        nu_static_visible_objects = nu_visible_objects;
        nu_static_final_pass_objects = nu_final_pass_objects;
        nu_static_visible_lights = visible_light_array.Size();
        // Handle all dynamic entities. They will be stored at the end of the visible entity arrays.
        DetermineVisibleEntitiesInFastOctreeRootNode(fast_octree_dynamic, 0, frustum,
            SRE_COMPLETELY_INSIDE);
        DetermineVisibleEntitiesInFastOctreeRootNode(
            fast_octree_dynamic_infinite_distance, 0, frustum, SRE_COMPLETELY_INSIDE);
    }
//    printf("Number of visible objects: %d, lights: %d\n", nu_visible_objects,
//          visible_light_array.Size());
//    printf("octrees culled: frustum: %d projected size: %d\n", octree_culled_count_frustum,
//        octree_culled_count_projected);
//    printf("objects inside octrees completely inside frustum: %d\n", octree_objects_inside);
//    printf("%d light volumes intersect frustum\n", visible_light_array.Size());
}

// Adjust the GPU scissors region, based on the supplied scissors coordinates
// (which are in the floating point range [-1, 1]). Depth bound are not set.

static void SetGLScissors(const sreScissors& scissors) {
#ifndef __GNUC__
    int left = floorf((scissors.left + 1.0f) * 0.5f * sre_internal_window_width);
    int right = ceilf((scissors.right + 1.0f) * 0.5f * sre_internal_window_width);
    int bottom = floorf((scissors.bottom + 1.0f) * 0.5f * sre_internal_window_height);
    int top = ceilf((scissors.top + 1.0f) * 0.5f * sre_internal_window_height);
#else
    fesetround(FE_DOWNWARD);
    int left = lrint((scissors.left + 1.0f) * 0.5f * sre_internal_window_width);
    int bottom = lrint((scissors.bottom + 1.0f) * 0.5f * sre_internal_window_height);
    fesetround(FE_UPWARD);
    int right = lrint((scissors.right + 1.0f) * 0.5f * sre_internal_window_width);
    int top = lrint((scissors.top + 1.0f) * 0.5f * sre_internal_window_height);
    fesetround(FE_TONEAREST);
#endif
    glScissor(left, bottom, right - left, top - bottom);
}

static void DisableScissors() {
    glDisable(GL_SCISSOR_TEST);
#ifndef NO_DEPTH_BOUNDS
    if (GLEW_EXT_depth_bounds_test) {
// Disabling depth bounds test while rendering is not possible.
//        glDisable(GL_DEPTH_BOUNDS_EXT);
        glDepthBoundsEXT(0, 1.0f);
    }
#endif
}

// Set complete scissor region for a light, including depth bounds when enabled.

static void SetScissorsBeforeLight(const sreScissors& scissors) {
    CHECK_GL_ERROR("Error before scissors set up before light\n");
    // If geometry scissors are enabled, make sure the scissor test is enabled.
    if (!(sre_internal_scissors & SRE_SCISSORS_GEOMETRY_MASK) && scissors.left == - 1.0f &&
    scissors.right == 1.0f && scissors.bottom == - 1.0f && scissors.top == 1.0f)
        glDisable(GL_SCISSOR_TEST);
    else { 
        glEnable(GL_SCISSOR_TEST);
        SetGLScissors(scissors);
    }
    CHECK_GL_ERROR("Error after scissors set up.\n");

#ifndef NO_DEPTH_BOUNDS
    if (GLEW_EXT_depth_bounds_test) {
        glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
        glDepthBoundsEXT(scissors.near, scissors.far);
    }
#endif
#ifdef DEBUG_OPENGL
    {
    GLenum errorTmp = glGetError();
    if (errorTmp != GL_NO_ERROR) {
        printf("Error after scissors and depth bounds set up before light, scissors = (%f, %f), (%f, %f), "
            "near/far = (%f, %f).\n",
            scissors.left, scissors.right, scissors.bottom, scissors.top, scissors.near, scissors.far);
        while (glGetError() != GL_NO_ERROR);
    }
    }
#endif
}

// Rendering objects. The visible object lists (regular and final passs) that were
// compiled earlier are used.
//
// When multi-pass rendering is enabled, objects are rendered for one light at
// a time. For static lights the static object list associated with the light may
// be used.

static int object_count;  // Keep track of the visible object count.

// Single-pass rendering.

static void RenderVisibleObjectSinglePass(sreObject& so) {
    object_count++;

    // Draw the object.
    sreDrawObjectSinglePass(&so);
}

void sreScene::RenderVisibleObjectsSinglePass(const sreFrustum& frustum) const {
    object_count = 0;
    for (int i = 0; i < nu_visible_objects; i++)
        RenderVisibleObjectSinglePass(*object[visible_object[i]]);
}

// The final pass of single-pass rendering. At the moment, reserved for the following objects:
//
// - Objects with the SRE_OBJECT_EMISSION_ONLY flag set. They are not influenced by lights.
//   This includes objects with the SRE_OBJECT_LIGHT_HALO or SRE_OBJECT_BILLBOARD
//   flags set.
//
// Some object might be transparent (generally, all transparent object will be
// rendered in the final pass).
//
// These functions are also used for the final pass in multi-pass rendering.

static void RenderFinalPassObject(sreObject &so) {
    if (so.flags & SRE_OBJECT_PARTICLE_SYSTEM)
        GL3SetParticleSystem(&so);
    else
    if (so.flags & SRE_OBJECT_LIGHT_HALO)
        GL3SetBillboard(&so);
    // Draw the object.
    sreDrawObjectFinalPass(&so);
}

// Compare function for sorting final pass objects.

static int DistanceCompare(const void *e1, const void *e2) {
    sreObject *so1 = sre_internal_scene->object[*(int *)e1];
    sreObject *so2 = sre_internal_scene->object[*(int *)e2];
    // If the SRE_OBJECT_INFINITE_DISTANCE_FLAG is set, meaning that the object
    // cannot occlude any other final pass object, or if the
    // SRE_OBJECT_NOT_OCCLUDING flag is set, meaning that the object
    // cannot occlude any other final pass object (expect infinite distance
    // objects) from any allowed viewpoint, put it early in the sorting order.
    if ((so1->flags | so2->flags) &
    (SRE_OBJECT_INFINITE_DISTANCE | SRE_OBJECT_NOT_OCCLUDING)) {
        if (so1->flags & SRE_OBJECT_INFINITE_DISTANCE)
            if (so2->flags & SRE_OBJECT_INFINITE_DISTANCE)
                goto order_by_id;
            else
                return - 1;
        else if (so2->flags & SRE_OBJECT_INFINITE_DISTANCE)
            return 1;
        // At least one object has SRE_OBJECT_NOT_OCCLUDING set.
        if (so1->flags & SRE_OBJECT_NOT_OCCLUDING)
            if (so2->flags & SRE_OBJECT_NOT_OCCLUDING)
                goto order_by_id;
            else
                return - 1;
        else
            // SRE_OBJECT_NOT_OCCLUDING must be set for so2.
            return 1;  
order_by_id :
            // If both objects are at infinite distance, impose an order by id.
            if (so1->id < so2->id)
                 return - 1;
            else
                 return 1;
    }
    // For other objects, sort of distance from the viewpoint, using either the
    // bounding sphere center or the bounding box center.
    Point3D center1, center2;
    if (so1->model->bounds_flags & SRE_BOUNDS_PREFER_SPHERE)
        center1 = so1->sphere.center;
    else
        center1 = so1->box.center;
    if (so2->model->bounds_flags & SRE_BOUNDS_PREFER_SPHERE)
        center2 = so2->sphere.center;
    else
        center2 = so2->box.center;
    float sqrdist1 = SquaredMag(center1 - sre_internal_viewpoint);
    float sqrdist2 = SquaredMag(center2 - sre_internal_viewpoint);
    if (sqrdist1 > sqrdist1)
        return - 1;
    if (sqrdist1 < sqrdist2)
        return 1;
    return 0;
}

void sreScene::RenderFinalPassObjectsSinglePass(const sreFrustum& frustum) const {
    // Sort the objects in order of decreasing distance.
    // Sorting is actually only required for transparent objects, but is performed
    // for every object.
    // Preserve the seperation between static and dynamic final-pass objects
    // in the final pass array.
    // For correct rendering when both static and dynamic transparent objects
    // are present, the sorted static and dynamic arrays must be merged, while preserving
    // the static-only object array for the next frame in case the frustum doesn't change.
    qsort(final_pass_object, nu_static_final_pass_objects, sizeof(int), DistanceCompare);
    qsort(&final_pass_object[nu_static_final_pass_objects],
        nu_final_pass_objects - nu_static_final_pass_objects, sizeof(int), DistanceCompare);
    int i_static = 0;
    int i_dynamic = nu_static_final_pass_objects;
    for (;;) {
         int i;
         if (i_static < nu_static_final_pass_objects)
             // We have static objects left.
             if (i_dynamic < nu_final_pass_objects)
                 // There are also dynamic objects left; check which one
                 // is further away.
                 if (DistanceCompare(&final_pass_object[i_static],
                 &final_pass_object[i_dynamic]) <= 0) {
                     i = i_static;
                     i_static++;
                 }
                 else {
                     i = i_dynamic;
                     i_dynamic++;
                  }
             else {
                  // Only static objects left.
                  i = i_static;
                  i_static++;
              }
         else if (i_dynamic < nu_final_pass_objects) {
             // Only dynamic objects left
             i = i_dynamic;
             i_dynamic++;
         }
         else
             // All objects have been rendered.
             break;
         RenderFinalPassObject(*object[final_pass_object[i]]);
    }
}

// Multi-pass rendering.

// Ambient pass of multi-pass rendering.

static void RenderVisibleObjectAmbientPass(sreObject& so) {
    object_count++;

    // Draw the object.
    sreDrawObjectAmbientPass(&so);
}

void sreScene::RenderVisibleObjectsAmbientPass(const sreFrustum& frustum) const {
    object_count = 0;
    for (int i = 0; i < nu_visible_objects; i++)
        RenderVisibleObjectAmbientPass(*object[visible_object[i]]);
}

// Lighting pass of multi-pass rendering.

// Render an object that is completely inside the light volume. In case of a directional
// light, this is true of all objects.

static void RenderVisibleObjectLightingPassCompletelyInside(sreObject& so) {
    object_count++;

     // Draw the object.
    sreDrawObjectMultiPassLightingPass(&so, sre_internal_current_light->shadow_map_required);
}

static int light_volume_intersection_test_count;

// Render object in lighting pass that has been predetermined to be visible, but not completely
// inside the light volume of a non-directional light such as a point source light.
// No geometry (per-object) scissors are applied. Only a check of the object's bounding volume
// with the light volume is performed. If no check is required,
// RenderVisibleObjectLightingPassCompletelyInside() should be used.

static void RenderVisibleObjectLightingPass(sreObject& so,
const sreLight& light, const sreFrustum &frustum) {
    // Do an intersection test against the light volume.
    light_volume_intersection_test_count++;
    if (!Intersects(so, light))
       return;

    object_count++;

    // Draw the object.
    sreDrawObjectMultiPassLightingPass(&so, light.shadow_map_required);
}

enum {
   SRE_GEOMETRY_SCISSORS_STATIC_VISIBLE_OBJECT = 1,
   SRE_GEOMETRY_SCISSORS_USE_PREVIOUS = 2
};

// Render object in lighting pass that has been predetermined to be visible, but not completely
// inside the light volume of a non-directional light such as a point source light. Geometry
// (per object) scissors are active, and will be used when deemed advantageous.

// Flag indicating whether custom scissors smaller than the light scissor region are active.
static bool custom_scissors_set;
#ifndef NO_DEPTH_BOUNDS
// Flag indicating whether custom depth bounds smaller than the light depth bounds are active.
static bool custom_depth_bounds_set;
#endif

// When SRE_GEOMETRY_SCISSORS_STATIC_VISIBLE_OBJECT is set in flags, it guarantees that the
// object is a static object that at least partially inside the light volume. No intersection
// test is necessary, and cached geometry scissors from previous frames can be reused if the
// SRE_GEOMETRY_SCISSORS_USE_PREVIOUS flag is also set (true if the frustum hasn't changed).

// This subfunction applies the supplied scissors and draws the region. A special value
// < - 1.5 (for example - 2.0) for object_scissors.left indicates that the object is
// completely inside the light volume and no object-specific scissors need to be set
// (however, scissors may still need to be restored to the normal light scissors if
// they are still set for a previous object).

static void RenderVisibleObjectLightingPassWithSpecifiedScissors(sreObject& so,
const sreLight& light, const sreFrustum &frustum, const sreScissors& object_scissors) {
    // Since the geometry scissors may still be set for a previously drawn object,
    // carefully check whether new scissors/depth bounds are required.
    bool viewport_adjusted = false;
#ifndef NO_DEPTH_BOUNDS
    bool depth_bounds_adjusted = false;
#endif

    // Set the working scissors to the light scissors.
    // (Alternatively, if no scissors were calculated, scissors could be disabled).
    sreScissors scissors = frustum.scissors;

    // If usable object (geometry) scissors were calculated, intersect with them.
    // Also set flags indicating whether the light scissors or light depth bounds
    // were adjusted.
    if (object_scissors.left >= - 1.5f) {
        if (object_scissors.left > scissors.left) {
            scissors.left = object_scissors.left;
            viewport_adjusted = true;
        }
        if (object_scissors.right < scissors.right) {
            scissors.right = object_scissors.right;
            viewport_adjusted = true;
        }
        if (object_scissors.bottom > scissors.bottom) {
            scissors.bottom = object_scissors.bottom;
            viewport_adjusted = true;
        }
        if (object_scissors.top < scissors.top) {
            scissors.top = object_scissors.top;
            viewport_adjusted = true;
        }
        // Check for an empty region (when present, skip the object entirely).
        if (scissors.left >= scissors.right || scissors.bottom >= scissors.top ||
        scissors.near >= scissors.far)
            return;
#ifdef DEBUG_SCISSORS
        if (viewport_adjusted) {
            printf("Light scissors (%f, %f), (%f, %f) ", frustum.scissors.left, frustum.scissors.right,
                frustum.scissors.bottom, frustum.scissors.top);
            printf(" adjusted to (%f, %f), (%f, %f) for object %d\n", scissors.left, scissors.right,
                scissors.bottom, scissors.top, so.id);
        }
#endif
#ifndef NO_DEPTH_BOUNDS
        // Also update the depth bounds (which are part of the calculated scissors parameters).
        if (object_scissors.near > scissors.near) {
            scissors.near = object_scissors.near;
            depth_bounds_adjusted = true;
        }
        if (object_scissors.far < scissors.far) {
            scissors.far = object_scissors.far;
            depth_bounds_adjusted = true;
        }
#ifdef DEBUG_SCISSORS
        if (depth_bounds_adjusted)
            printf("Depth bounds adjusted to (%lf, %lf) for object %d\n", scissors.near, scissors.far, so.id);
#endif
#endif
    }

    // If the required scissors are smaller than the light scissors, or
    // if normal light scissors are required but custom scissors smaller than
    // the light scissors region are still active, update the scissors region.
    if (viewport_adjusted || custom_scissors_set) {
        SetGLScissors(scissors);

#ifdef DEBUG_SCISSORS
        printf("Light scissors (%f, %f), (%f, %f) ", frustum.scissors.left, frustum.scissors.right,
        frustum.scissors.bottom, frustum.scissors.top);
        printf(" adjusted to (%f, %f), (%f, %f) for object %d\n", scissors.left, scissors.right,
        scissors.bottom, scissors.top, so.id);
#endif
#ifdef DEBUG_OPENGL
        {
        GLenum errorTmp = glGetError();
        if (errorTmp != GL_NO_ERROR) {
            printf("Error after scissors set up, scissors = (%f, %f), (%f, %f).\n",
                scissors.left, scissors.right, scissors.bottom, scissors.top);
            printf("so.scissors = (%f, %f), (%f, %f).\n",
                object_scissors.left, object_scissors.right, object_scissors.bottom, object_scissors.top);
            printf("frustum.scissors = (%f, %f), (%f, %f).\n",
                frustum.scissors.left, frustum.scissors.right, frustum.scissors.bottom, frustum.scissors.top);
            while (glGetError() != GL_NO_ERROR);
       }
       }
#endif
        // Update the flag indicating whether the scissors region that was just set is
        // equal to the light scissors region.
        custom_scissors_set = viewport_adjusted;
    }

    // If the required depth bounds are smaller than the light depth bounds,
    // or if normal light depth bounds are required by custom depth bounds that
    // are smaller are still set, update the depth bounds.
#ifndef NO_DEPTH_BOUNDS
    if (GLEW_EXT_depth_bounds_test && (depth_bounds_adjusted || custom_depth_bounds_set)) {
        glDepthBoundsEXT(scissors.near, scissors.far);
        custom_depth_bounds_set = depth_bounds_adjusted;
#ifdef DEBUG_OPENGL
        {
            GLenum errorTmp = glGetError();
            if (errorTmp != GL_NO_ERROR) {
                printf("Error after depth bounds set up, near/far = (%f, %f).\n",
                    scissors.near, scissors.far);
                while (glGetError() != GL_NO_ERROR);
            }
        }
#endif
    }
#endif

    object_count++;

    // Draw the object.
    sreDrawObjectMultiPassLightingPass(&so, light.shadow_map_required);
}

// Render a lighting pass visible object, using geometry scissors if possible,
// without caching/storing the used scissors (useful for dynamic objects).

static void RenderVisibleObjectLightingPassGeometryScissors(sreObject& so,
const sreLight& light, const sreFrustum &frustum) {
    sreScissors object_scissors;

    // Decide whether to use geometry scissors using a heuristic.
    bool use_geometry_scissors = false;
    // Use the projected size calculated for the object during visible object
    // determination. It is an upper bound for the object's screen size that
    // was mainly derived from the object's bounding sphere radius and z-distance.
    if (so.projected_size >= SRE_GEOMETRY_SCISSORS_OBJECT_SIZE_THRESHOLD) {
        // Try to calculate the maximum screen area. This helps for thin objects.
        float ratio = so.model->PCA[1].size / so.model->PCA[0].size;
        if (so.projected_size * so.projected_size * ratio >=
        SRE_GEOMETRY_SCISSORS_OBJECT_AREA_THRESHOLD)
            use_geometry_scissors = true;
    }

    if (!use_geometry_scissors) {
        // If geometry scissors are not deemed advantageous, check the object's
        // bounding volumes against the light volume, and do not
        // draw the object it is outside the light volume.
        light_volume_intersection_test_count++;
        if (!Intersects(so, light))
            return;
         // Set special value in scissors cache indicating the object
        // completely inside the light volume (or at least no usable scissors
        // could be calculated).
        object_scissors.left = - 2.0f;
    }
    else {
        // When geometry scissors are deemed to be advantageous, the geometry
        // scissors region will be calculated. The check of whether the object
        // intersects with the light volume is still performed, but integrated
        // into the geometry scissors calculation.
        light_volume_intersection_test_count++;
        BoundsCheckResult r = so.CalculateGeometryScissors(light, frustum, object_scissors);
        // If the object is outside the light volume, do not draw the object.
        if (r == SRE_COMPLETELY_OUTSIDE)
            return;
        if (r == SRE_COMPLETELY_INSIDE)
            // Set special value indicating no usable scissors (or completely inside the
            // light volume).
           object_scissors.left = - 2.0f;
    }

    RenderVisibleObjectLightingPassWithSpecifiedScissors(so, light, frustum, object_scissors);
}

// Render a lighting pass visible object, using geometry scissors if possible,
// caching/storing the used scissors information for subsequent frames. Useful for
// static objects; when the frustum does not change information can be reused in
// subsequent frames. Scissors information is only stored, already stored scissors
// are not used. This function is normally called only for static objects that
// are partially within the light volume of a static light.

static void RenderVisibleObjectLightingPassCacheGeometryScissors(sreObject& so,
const sreLight& light, const sreFrustum &frustum) {
    // Decide whether to use geometry scissors using a heuristic.
    bool use_geometry_scissors = false;
    // Use the projected size calculated for the object during visible object
    // determination. It is an upper bound for the object's screen size that
    // was mainly derived from the object's bounding sphere radius and z-distance.
    if (so.projected_size >= SRE_GEOMETRY_SCISSORS_OBJECT_SIZE_THRESHOLD) {
        // Try to calculate the maximum screen area. This helps for thin objects.
        float ratio = so.model->PCA[1].size / so.model->PCA[0].size;
        if (so.projected_size * so.projected_size * ratio >=
        SRE_GEOMETRY_SCISSORS_OBJECT_AREA_THRESHOLD)
            use_geometry_scissors = true;
    }

    if (!use_geometry_scissors) {
        // If geometry scissors are not deemed advantageous, we can assume
        // the object is within the light volume, because this function is called
        // only for static objects that are partially within the light volume of
        // a static light.
        // Set special value in scissors cache indicating the object
        // completely inside the light volume (or at least no usable scissors
        // could be calculated).
        so.geometry_scissors_cache[so.static_light_order].left = - 2.0f;
    }
    else {
        // When geometry scissors are deemed to be advantageous, the geometry
        // scissors region will be calculated. The check of whether the object
        // intersects with the light volume is still performed, but integrated
        // into the geometry scissors calculation.
        light_volume_intersection_test_count++;
        // Calculated scissors are stored in the object structure
        // (so.geometry_scissors_cache[so.static_light_order]).
        // This is useful for the combination of static light, static object and unchanged
        // frustum.
        // Note that the any resultings scissors are not clamped to the screen boundaries.
        BoundsCheckResult r = so.CalculateGeometryScissors(light, frustum,
            so.geometry_scissors_cache[so.static_light_order]);
        // If the object is outside the light volume, do not draw the object.
        // Although this would contradict the fact that the object is
        // partially inside the object's light volume as determined in the precalculated
        // list, the geometry scissors calculations is somewhat different and might be
        // more precise.
        if (r == SRE_COMPLETELY_OUTSIDE) {
            // Set special value in scissors cache indicating that the object is
            // outside the light volume.
            so.geometry_scissors_cache[so.static_light_order].left = 2.0f;
            return;
        }
        else if (r == SRE_COMPLETELY_INSIDE)
            // Set special value indicating no usable scissors (or completely inside the
            // light volume).
            so.geometry_scissors_cache[so.static_light_order].left = - 2.0f;
        else
            // Scissors were calculated. Clamp just the left boundary to the screen because
            // values outside the boundary are used as special values.
            so.geometry_scissors_cache[so.static_light_order].left = maxf(
                so.geometry_scissors_cache[so.static_light_order].left , - 1.0f);
    }

    RenderVisibleObjectLightingPassWithSpecifiedScissors(so, light, frustum,
        so.geometry_scissors_cache[so.static_light_order]);
}

// Render a lighting pass visible object, re-using the geometry scissors from the
// previous frame stored in so.geometry_scissors_cache[so.static_light_order].
// This function is normally called only for static objects that
// are partially within the light volume of a static light.

static void RenderVisibleObjectLightingPassReuseGeometryScissors(sreObject& so,
const sreLight& light, const sreFrustum &frustum) {
    // When the last frustum change was before the current frame, as indicated
    // by the flag, any previously calculated geometry scissors information for
    // a static object/static light combination must still be valid.
    // Special left scissors boundary value > 1.5 indicates the object is outside
    // the light volume. In this case, we can exit early.
    if (so.geometry_scissors_cache[so.static_light_order].left > 1.5f)
        return;
    // Special value of left scissors bounday of < -1.5 indicates object is
    // completely inside the light volume (or at least no usable scissors were
    // calculated).

#ifdef DEBUG_SCISSORS
    printf("Geometry scissors reused in frame %d for object %d\n",
        sre_internal_current_frame, so.id);
#endif

    RenderVisibleObjectLightingPassWithSpecifiedScissors(so, light, frustum,
        so.geometry_scissors_cache[so.static_light_order]);
}

static int object_count_all_lights;
static int intersection_tests_all_lights;

// Render predetermined visible objects for lighting passes. This is straightforward for
// directional lights, but several optimization can be performed for other types of light
// that have a limited sphere of influence.
// This does directly affect any field in the sreScene class so is declared const.

void sreScene::RenderVisibleObjectsLightingPass(const sreFrustum& frustum, const sreLight& light) const {
    object_count = 0;
    light_volume_intersection_test_count = 0;
    if (light.type & SRE_LIGHT_DIRECTIONAL) {
        // For directional lights, every object is completely inside the light volume.
        // Scissors will have been disabled by the calling function.
        for (int i = 0; i < nu_visible_objects; i++)
            RenderVisibleObjectLightingPassCompletelyInside(*object[visible_object[i]]);
    }
    else
    if ((light.type & SRE_LIGHT_STATIC_OBJECTS_LIST) && sre_internal_light_object_lists_enabled) {
        // Static light.
#if 0
        printf("Light volume objects for light %d: ", light.id);
        for (int i = 0; i < light.nu_light_volume_objects; i++) {
            printf("%d ", light.light_volume_object[i]);
        }
        printf("\n");
#endif
        if (sre_internal_geometry_scissors_active) {
            // Geometry scissors are active.
            custom_scissors_set = false;
#ifndef NO_DEPTH_BOUNDS
            custom_depth_bounds_set = false;
#endif
            // Render the dynamic objects in the visible objects list. The dynamic
            // objects are at the end of the array. Since their visibility was
            // determined in the current frame, they all need to rendered.
            for (int i = nu_static_visible_objects; i < nu_visible_objects; i++)
                 RenderVisibleObjectLightingPassGeometryScissors(
                    *object[visible_object[i]], light, frustum);
            // Render the precalculated list of static objects within the light volume from
            // the light's data structure, only rendering visible objects.
            // First the render objects that are partially inside the light volume; the
            // geometry scissors are likely to be applied on a per-object basis.
            if (light.type & SRE_LIGHT_DYNAMIC_LIGHT_VOLUME) {
                // For lights with a dynamic (variable) light volume that have a worst case bounds
                // so for which an approximate static objects lists is defined, do not store any
                // geometry scissors.
                for (int i = 0; i < light.nu_light_volume_objects_partially_inside; i++) {
                    sreObject *so = object[light.light_volume_object[i]];
                    if (so->most_recent_frame_visible < frustum.most_recent_frame_changed)
                        // Object is not visible; skip it.
                        continue;
                    RenderVisibleObjectLightingPassGeometryScissors(*so, light, frustum);
                }
            }
            else if (sre_internal_current_frame > frustum.most_recent_frame_changed + 1 &&
            (sre_internal_rendering_flags & SRE_RENDERING_FLAG_GEOMETRY_SCISSORS_CACHE_ENABLED)) {
                // If the frustum has not changed, we can reuse previously calculated scissors.
                // Note that we wait one extra frame after the frustum stops changing before
                // starting to store cached geometry scissors; in the common case of a
                // continuously changing frustum, this saves overhead.
//                sreMessage(SRE_MESSAGE_INFO, "Frame %d: reusing geometry scissors",
//                    sre_internal_current_frame);
                for (int i = 0; i < light.nu_light_volume_objects_partially_inside; i++) {
                    sreObject *so = object[light.light_volume_object[i]];
                    // Many objects in the light's light volume object list might not be visible.
                    // Comparing the frame time-stamps for the object's visibility
                    // and the last frustum change should ensure that the object is
                    // currently visible (since static object visibility was determined
                    // at the time of the last frustum change).
                    if (so->most_recent_frame_visible < frustum.most_recent_frame_changed)
                        // Object is not visible; skip it.
                        continue;
#ifdef DEBUG_SCISSORS
                    sreMessage(SRE_MESSAGE_INFO,
                        "Object %d scissors cache timestamp = %d, frame = %d\n", so->id,
                        so->geometry_scissors_cache_timestamp, sre_internal_current_frame);
#endif
                    if (so->geometry_scissors_cache_timestamp < sre_internal_current_frame) {
                        // First static light rendered for the object; reset the light order
                        // for the geometry scissors cache.
                        so->static_light_order = 0;
                        so->geometry_scissors_cache_timestamp = sre_internal_current_frame;
                    }
                    // Because lights can sometimes be skipped entirely, or geometry scissors may
                    // be temporarily deactivated for a light as it moves away, make sure that
                    // static_light_order corresponds to the current light by checking the light
                    // id in the geometry scissors cache entry. It should always be present.
                    int static_light_order = so->static_light_order;
                    while (so->geometry_scissors_cache[static_light_order].light_id !=
                    sre_internal_current_light_index) {
#ifdef DEBUG_SCISSORS
                        sreMessage(SRE_MESSAGE_INFO,
                            "Scissors cache order = %d, cache entry light id = %d, current light = %d",
                            static_light_order,
                            so->geometry_scissors_cache[static_light_order].light_id,
                            sre_internal_current_light_index);
                        fflush(stdout);
#endif
                        static_light_order++;
                    }
                    so->static_light_order = static_light_order;
#ifdef DEBUG_SCISSORS
                    sreMessage(SRE_MESSAGE_INFO,
                        "Drawing object for light %d, geometry scissors cache order = %d, "
                        "cache entry light id = %d, left = %f",
                        sre_internal_current_light_index, static_light_order,
                        so->geometry_scissors_cache[static_light_order].light_id,
                        so->geometry_scissors_cache[static_light_order].left);
                    sreMessage(SRE_MESSAGE_INFO,"Scissors = (%f, %f), (%f, %f), depth (%f, %f).",
                        so->geometry_scissors_cache[static_light_order].left,
                        so->geometry_scissors_cache[static_light_order].right,
                        so->geometry_scissors_cache[static_light_order].bottom,
                        so->geometry_scissors_cache[static_light_order].top,
                        so->geometry_scissors_cache[static_light_order].near,
                        so->geometry_scissors_cache[static_light_order].far);
                    }
#endif

                    // Special scissors left boundary value of 3.0 indicates that geometry scissors
                    // have not yet been calculated (the light was previously skipped at the time of
                    // the last frustum change or geometry scissors were inactive for the light).
                    if (so->geometry_scissors_cache[static_light_order].left > 2.5f)
                        RenderVisibleObjectLightingPassCacheGeometryScissors(*so, light, frustum);
                    else
                        RenderVisibleObjectLightingPassReuseGeometryScissors(*so, light, frustum);
                    // Update the light order for the geometry scissors cache.
                    so->static_light_order = static_light_order + 1;
                }
            }
            else {
                // If the frustum has just changed, store the calculated scissors for potential
                // subsequent use. However, when the frustum is changing every frame, do not
                // store scissors until the frustums stops changing.
                if (frustum.IsChangingEveryFrame(sre_internal_current_frame))
                    for (int i = 0; i < light.nu_light_volume_objects_partially_inside; i++) {
                        sreObject *so = object[light.light_volume_object[i]];
                        // Comparing the frame time-stamps for the object's visibility
                        // and the last frustum change should ensure that the object is
                        // currently visible (since static object visibility was determined
                        // at the time of the last frustum change).
                        if (so->most_recent_frame_visible < frustum.most_recent_frame_changed)
                            // Object is not visible, skip it.
                            continue;
                        RenderVisibleObjectLightingPassGeometryScissors(*so, light, frustum);
                    }
                else {
//                sreMessage(SRE_MESSAGE_INFO, "Frame %d: storing (caching) geometry scissors",
//                    sre_internal_current_frame);
                for (int i = 0; i < light.nu_light_volume_objects_partially_inside; i++) {
                    sreObject *so = object[light.light_volume_object[i]];
                    // Comparing the frame time-stamps for the object's visibility
                    // and the last frustum change should ensure that the object is
                    // currently visible (since static object visibility was determined
                    // at the time of the last frustum change).
                    if (so->most_recent_frame_visible < frustum.most_recent_frame_changed)
                        // Object is not visible, skip it.
                        continue;

                    if (!(sre_internal_rendering_flags &
                    SRE_RENDERING_FLAG_GEOMETRY_SCISSORS_CACHE_ENABLED)) {
                        RenderVisibleObjectLightingPassGeometryScissors(*so, light, frustum);
                        continue;
                    }

                    // Prepare an entry in the object's geometry scissors cache.
                    if (so->geometry_scissors_cache_timestamp < sre_internal_current_frame) {
                        // First static light rendered for the object; reset the light order
                        // for the geometry scissors cache.
                        so->static_light_order = 0;
                        so->geometry_scissors_cache_timestamp = sre_internal_current_frame;
                    }
                    so->geometry_scissors_cache[so->static_light_order].light_id =
                        sre_internal_current_light_index;
#ifdef DEBUG_SCISSORS
                    sreMessage(SRE_MESSAGE_INFO,
                        "Writing scissors cache entry for light %d, object %d, order %d",
                        sre_internal_current_light_index, so->id, so->static_light_order);
#endif
                    RenderVisibleObjectLightingPassCacheGeometryScissors(*so, light, frustum);
#ifdef DEBUG_SCISSORS
                    sreMessage(SRE_MESSAGE_INFO,"Scissors = (%f, %f), (%f, %f), depth (%f, %f).\n",
                        so->geometry_scissors_cache[so->static_light_order].left,
                        so->geometry_scissors_cache[so->static_light_order].right,
                        so->geometry_scissors_cache[so->static_light_order].bottom,
                        so->geometry_scissors_cache[so->static_light_order].top,
                        so->geometry_scissors_cache[so->static_light_order].near,
                        so->geometry_scissors_cache[so->static_light_order].far);
#endif
                    // Update the light order for the geometry scissors cache.
                    so->static_light_order++;
                }
                }
            }
            // Finally draw objects that are completely inside the light volume.
            if (light.nu_light_volume_objects_partially_inside < light.nu_light_volume_objects) {
                // With active geometry scissors, the scissors may still be set for a
                // a previous object. Since the objects are all completely inside the
                // light volume, we can disable scissors completely (it doesn't help
                // to use the light-specific scissors).
                DisableScissors();
                for (int i = light.nu_light_volume_objects_partially_inside;
                i < light.nu_light_volume_objects; i++) {
                    sreObject *so = object[light.light_volume_object[i]];
                    // Only render visible objects.
                    if (so->most_recent_frame_visible >= frustum.most_recent_frame_changed)
                        RenderVisibleObjectLightingPassCompletelyInside(*so);
                }
            }
        }
        else {
            // No geometry scissors active.
            // Render the dynamic objects in the visible objects list. The dynamic
            // objects are at the end of the array.
            for (int i = nu_static_visible_objects; i < nu_visible_objects; i++)
                RenderVisibleObjectLightingPass(
                    *object[visible_object[i]], light, frustum);
            // Render the precalculated list of static objects within the light volume.
            // Restoring the complete light scissor region if scissors are enabled
            // should not be necessary. The light-specific scissors should still be active
            // if enabled.
            // First the objects that are partially inside the light volume.
            for (int i = 0; i < light.nu_light_volume_objects_partially_inside; i++) {
                sreObject *so = object[light.light_volume_object[i]];
                // Comparing the frame time-stamps for the object's visibility
                // and the last frustum change should ensure that the object is
                // currently visible (since static object visibility was determined
                // at the time of the last frustum change).
                if (so->most_recent_frame_visible >= frustum.most_recent_frame_changed)
                    // Just try apply lighting to the whole object (even though part of it
                    // is outside the light volume and won't be affected). Any light-specific
                    // scissors will be taken advantage of.
                    RenderVisibleObjectLightingPassCompletelyInside(*so);
            }
            // When geometry scissors are enabled, the geometry scissors cache data still need to
            // be initialized during frames when the cache date is being initialized for other
            // lights.
            if (sre_internal_scissors & SRE_SCISSORS_GEOMETRY_MASK)
                UpdateGeometryScissorsCacheData(frustum, light);
            // Finally draw the objects that are completely inside the light volume.
            if (light.nu_light_volume_objects_partially_inside < light.nu_light_volume_objects) {
                // Since the objects are all completely inside the light volume, we can disable
                // scissors completely (it doesn't help to use the light-specific scissors).
                DisableScissors();
                for (int i = light.nu_light_volume_objects_partially_inside; i < light.nu_light_volume_objects; i++) {
                    sreObject *so = object[light.light_volume_object[i]];
                    if (so->most_recent_frame_visible >= frustum.most_recent_frame_changed)
                        RenderVisibleObjectLightingPassCompletelyInside(*so);
                }
            }
        }
//        printf("%d objects rendered for light %d, %d light volume intersection tests.\n", object_count, light.id,
//            light_volume_intersection_test_count);
//        printf("%d dynamic objects in the root node, %d objects partially inside, %d objects completely inside.\n",
//            nu_dynamic_root_node_objects, light.nu_light_volume_objects_partially_inside, light.nu_light_volume_objects -
//                light.nu_light_volume_objects_partially_inside);
    }
    else {
        // Dynamic light. There are no static object lists with objects that are affected by the light.
        // However, we know the light is visible, so we have to check every visible object against
        // the light volume. It may be possible to optimize this using information gathered in the
        // visible object determination. The disabled code further below tried to do that, but is
        // no longer functional. A possible optimization is to take advantage of the fact that there
        // are likely to be octrees that are completely inside the light volume (which would reduce
        // light volume checks); these are present as a stretch of consecutive objects in the
        // visible object list, but there may be several of these stretches.
        custom_scissors_set = false;
#ifndef NO_DEPTH_BOUNDS
        custom_depth_bounds_set = false;
#endif
        // Render all visible objects, checking each with the light volume.
        if (sre_internal_geometry_scissors_active)
            for (int i = 0; i < nu_visible_objects; i++)
                RenderVisibleObjectLightingPassGeometryScissors(
                    *object[visible_object[i]], light, frustum);
        else
            for (int i = 0; i < nu_visible_objects; i++)
                RenderVisibleObjectLightingPass(*object[visible_object[i]], light, frustum);
//        sreMessage(SRE_MESSAGE_LOG, "%d visible objects rendered for dynamic light %d.",
//            nu_visible_objects, light.id);
#if 0
        int object_count_total = object_count;
        // Render the objects that start at the root subnode the light is in all the way down in the path where
        // the light is in.
//        printf("Starting object for light %d is %d.\n", sre_internal_current_light_index, light.starting_object);
//        printf("Ending object for light %d is %d.\n", sre_internal_current_light_index, light.ending_object);
//        printf("Starting object completely inside for light %d is %d.\n", sre_internal_current_light_index,
//            light.starting_object_completely_inside);
//        printf("Ending object completely inside for light %d is %d.\n", sre_internal_current_light_index,
//            light.ending_object_completely_inside);
        object_count = 0;
        // Draw the first batch of object that are not completely inside the light volume.
        int ending_object;
        bool no_objects_completely_inside;
        if (light.starting_object_completely_inside == - 1 || light.starting_object_completely_inside >
        light.ending_object_completely_inside) {
            ending_object = light.ending_object;
            no_objects_completely_inside = true;
        }
        else {
            ending_object = light.starting_object_completely_inside - 1;
            no_objects_completely_inside = false;
        }
        for (int i = light.starting_object; i <= ending_object ; i++)
            RenderVisibleObjectLightingPass(*object[visible_object[i]], light, frustum, false);
        object_count_total += object_count;
        int object_count_completely_inside = 0;
        if (!no_objects_completely_inside) { 
            object_count = 0;
            int ending_object_completely_inside;
            if (light.ending_object_completely_inside == - 1)
                ending_object_completely_inside = light.ending_object;
            else {
                // Draw the last batch of objects that are not completely inside the light volume.
                ending_object_completely_inside = light.ending_object_completely_inside;
                for (int i = ending_object_completely_inside + 1; i <= light.ending_object; i++)
                    RenderVisibleObjectLightingPass(*object[visible_object[i]], light, frustum, false);
                object_count_total += object_count;
            }
            if (sre_internal_geometry_scissors_active)
                DisableScissors();
            object_count = 0;
            // Draw the objects that are completely inside the light volume.
            for (int i = light.starting_object_completely_inside; i <= ending_object_completely_inside; i++)
                RenderVisibleObjectLightingPassCompletelyInside(*object[visible_object[i]]);
//                 RenderVisibleObjectLightingPass(*object[visible_object[i]], light, frustum);
            object_count_completely_inside = object_count;
            object_count_total += object_count;
        }
        printf("%d objects rendered for light %d, of which %d for which the octree is completely inside the light "
            "volume. %d light volume intersection tests.\n", object_count_total, light.id,
            object_count_completely_inside, light_volume_intersection_test_count);
#endif
    }
    object_count_all_lights += object_count;
    intersection_tests_all_lights += light_volume_intersection_test_count;
}

// Render predetermined visible objects for the final pass with multi-pass
// rendering enabled.

void sreScene::RenderFinalPassObjectsMultiPass(const sreFrustum& frustum) const {
     // The same function that is used for the final pass with single pass rendering
     // can be used, since objects can be drawn with sreDrawObjectFinalPass()
     // in both cases.
     RenderFinalPassObjectsSinglePass(frustum);
}

// Process a skipped light; when the frustum has recently changed, and the same frame that
// geometry scissors data is cached for other lights, need to create the geometry
// scissors cache entries for the list of object that are partially inside the light volume
// that are also visible (intersect the frustum). Generally this only happens for lights
// that have a variable light volume which has only just come into view (some time after
// the last frustum change).

void sreScene::UpdateGeometryScissorsCacheData(const sreFrustum& frustum,
const sreLight& light) const {
    if (sre_internal_current_frame > frustum.most_recent_frame_changed + 1)
        return;
    if (frustum.IsChangingEveryFrame(sre_internal_current_frame))
        return;
    if (!(light.type & SRE_LIGHT_STATIC_OBJECTS_LIST))
        return;
#ifdef DEBUG_SCISSORS
    sreMessage(SRE_MESSAGE_INFO, "Initializing object scissors cache data for light %d.", light.id);
#endif
    for (int i = 0; i < light.nu_light_volume_objects_partially_inside; i++) {
        sreObject *so = object[light.light_volume_object[i]];
        if (so->most_recent_frame_visible < frustum.most_recent_frame_changed)
            // Object is not visible; skip it.
            continue;
        if (so->geometry_scissors_cache_timestamp < sre_internal_current_frame) {
            // First static light considered for the object; reset the light order
            // for the geometry scissors cache.
            so->static_light_order = 0;
            so->geometry_scissors_cache_timestamp = sre_internal_current_frame;
        }
        so->geometry_scissors_cache[so->static_light_order].light_id =
            sre_internal_current_light_index;
        // Set special scissors left value indicating that geometry scissors have not
        // yet been calculated.
        so->geometry_scissors_cache[so->static_light_order].left = 3.0f;
        // Update the light order for the geometry scissors cache.
        so->static_light_order++;
    }
}

// Render lighting passes for stencil shadowing or shadow buffering.
// Assumes the depth test, blending etc. is appropriately configured.
// When using shadow volumes, the stencil test should be enabled before calling
// this function.
//
// Additive blending should be enabled.
// Depth buffer updates aren't required and should be off (glDepthMask(GL_FALSE)).
// The depth test should be configured as GL_EQUAL or GL_LEQUAL.

void sreScene::RenderLightingPasses(sreFrustum *frustum, sreView *view) {
    object_count_all_lights = 0;
    intersection_tests_all_lights = 0;

    // Set the max_active_lights field in the sreScene structure.
    if (sre_internal_max_active_lights == SRE_MAX_ACTIVE_LIGHTS_UNLIMITED ||
    visible_light_array.Size() <= sre_internal_max_active_lights)
        // All visible lights will be rendered.
        nu_active_lights = visible_light_array.Size();
    else {
        // The following function sets nu_active_lights.
        CalculateVisibleActiveLights(view, sre_internal_max_active_lights);
    }

    for (int i = 0; i < nu_active_lights; i++) {
        // Set the light to be rendered.
        if (nu_active_lights == visible_light_array.Size())
            sre_internal_current_light_index = visible_light_array.Get(i);
        else
            sre_internal_current_light_index = active_light[i];
        sre_internal_current_light = light[sre_internal_current_light_index];
// printf("Light %d, type = %d\n", sre_internal_current_light_index, sre_internal_current_light->type);
        // Clear flag indicating whether geometry scissors are actually enabled for the current light.
        sre_internal_geometry_scissors_active = false;

        // Visible objects for each light have been predetermined before this function was called.
        // However, for variable lights with a defined worst-case light volume, when the frustum
        // hasn't changed so visibility has not been predetermined, it is very possible that the light
        // volume is now outside the frustum, in which case we can skip the light.
        if ((sre_internal_current_light->type & SRE_LIGHT_DYNAMIC_LIGHT_VOLUME) &&
        sre_internal_current_frame > frustum->most_recent_frame_changed) {
            if (!Intersects(*sre_internal_current_light, frustum->frustum_world)) {
                // Skip the light.
                UpdateGeometryScissorsCacheData(*frustum, *sre_internal_current_light);
                continue;
            }
        }

        // Always clear this flag because DrawObjectMultiPassLightingPass checks it.
        sre_internal_current_light->shadow_map_required = false;

#ifndef NO_SHADOW_MAP
        if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING) {
            // Shadow mapping. Render shadow maps without scissors.
            DisableScissors();
            // Disable blending.
            glDisable(GL_BLEND);
            // Enable writes to the depth buffer.
            glDepthMask(GL_TRUE);
#ifdef OPENGL_ES2
	    glDepthRangef(0, 1.0f);
#endif
            bool r = GL3RenderShadowMapWithOctree(this, *sre_internal_current_light, *frustum);
#ifdef OPENGL_ES2
            glDepthRangef(0, 0.99999f);
#endif
            glDepthMask(GL_FALSE);
            if (!r) {
                // There are no shadow casters and no shadow (or light) receivers for this light.
                sreMessage(SRE_MESSAGE_LOG, "There are no shadow casters and no shadow (or light) "
                    "receivers for light %d.", sre_internal_current_light->id);
                UpdateGeometryScissorsCacheData(*frustum, *sre_internal_current_light);
                continue;
            }
            // Note: when there are no shadow receivers (but there are light receivers),
            // the light's shadow_map_required field will be set to false.
            if (!sre_internal_current_light->shadow_map_required)
                sreMessage(SRE_MESSAGE_LOG, "No shadow map required for light %d.",
                    sre_internal_current_light->id);
            else
                sreMessage(SRE_MESSAGE_LOG, "Shadow map required for light %d.",
                    sre_internal_current_light->id);
        }
#endif

        if (!(sre_internal_scissors & SRE_SCISSORS_LIGHT_MASK))
            goto skip_scissors;
        if (sre_internal_current_light->type & SRE_LIGHT_DIRECTIONAL) {
            DisableScissors();
            goto skip_scissors;
        }
        // Calculate the scissors region on the viewport where the point light source has influence.
        frustum->CalculateLightScissors(sre_internal_current_light);
        // If the scissors region is empty, skip the light.
        if (frustum->scissors.left == frustum->scissors.right || frustum->scissors.bottom ==
        frustum->scissors.top || frustum->scissors.near == frustum->scissors.far) {
            UpdateGeometryScissorsCacheData(*frustum, *sre_internal_current_light);
            continue;
        }

        // Set the on-screen scissors region for the light and when geometry scissors are
        // enabled determine whether the region is large enough that additional per-object
        // geometry scissors (which further reduce the scissors area) might be useful.
        SetScissorsBeforeLight(frustum->scissors);
        if (sre_internal_scissors & SRE_SCISSORS_GEOMETRY_MASK) {
            float scissors_area = (frustum->scissors.right - frustum->scissors.left) *
                (frustum->scissors.top - frustum->scissors.bottom);
            if (scissors_area >= SRE_GEOMETRY_SCISSORS_LIGHT_AREA_THRESHOLD) {
                sre_internal_geometry_scissors_active = true;
//              printf("Geometry scissors active for light %d.\n", sre_internal_current_light_index);
            }
        }

skip_scissors :
        if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING) {
            if (sre_internal_current_light->shadow_map_required)
                sreBindShadowMapTexture(sre_internal_current_light);
            goto do_lighting_pass;
        }

        // Render shadow volumes into stencil buffer. This function may disable the stencil test
        // if there are no shadows for the light. Therefore, if there were no shadows for the
        // previous light, the stencil test might still be disabled, so enable it now.
        glEnable(GL_STENCIL_TEST);
        // Disable blending.
        glDisable(GL_BLEND);
#ifdef OPENGL_ES2
        // Provide a small offset so that depth buffer precision is less of a problem
        // for additive lighting passes with a non-deterministic tile-based rendering
        // architecture. For depth-fail shadow volumes, the depth range will be temporarily
        // reset to (0, 1.0f).
        glDepthRangef(0, 0.99998f);
#endif
        sreRenderShadowVolumes(this, sre_internal_current_light, *frustum);
        glDepthRangef(0, 0.99998f);

        if ((sre_internal_scissors & SRE_SCISSORS_LIGHT_MASK) && !(sre_internal_current_light->type &
        SRE_LIGHT_DIRECTIONAL))
            SetScissorsBeforeLight(frustum->scissors); // Restore scissors for complete light volume.

do_lighting_pass :

        // The shadow map and shadow volume generation usually sets the depth test to GL_LESS,
        // so change it back.
#ifdef OPENGL_ES2
        glDepthFunc(GL_LEQUAL);
#else
        glDepthFunc(GL_EQUAL);
#endif

        // Enable additive blending, because it was disabled for shadow map/stencil buffer
        // generation.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        // Render the objects affected by this light.
        GL3InitializeShadersBeforeLight();
        RenderVisibleObjectsLightingPass(*frustum, *sre_internal_current_light);
    }
    DisableScissors();

    if (sre_internal_debug_message_level >= 3)
        printf("All lights: %d objects rendered, %d intersection tests.\n", object_count_all_lights,
            intersection_tests_all_lights);
//    sreReportShadowCacheStats();

#ifndef OPENGL_ES2
    if (sre_internal_debug_message_level >= 3) {
        if (GLEW_NVX_gpu_memory_info && sre_internal_current_frame % 50 == 0) {
            GLint available_memory[1];
            glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &available_memory[0]);
            GLint total_memory[1];
            glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total_memory[0]);
            GLint eviction_count[1];
            glGetIntegerv(GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX, &eviction_count[0]);
            printf("Available GPU memory: %d kb out of %d kb, %d evictions.\n", available_memory[0], total_memory[0],
                eviction_count[0]);
        }
    }
#endif
}

// Render lighting passes without shadows. Assumes the depth test, blending etc. is appropriately
// configured. We don't need to change them in this function.
//
// Additive blending should be enabled.
// Depth buffer updates aren't required and should be off (glDepthMask(GL_FALSE)).
// The depth test should be configured as GL_EQUAL or GL_LEQUAL.

void sreScene::RenderLightingPassesNoShadow(sreFrustum *frustum, sreView *view) {
    CHECK_GL_ERROR("Error before RenderLightingPassesNoShadow\n");
    object_count_all_lights = 0;
    intersection_tests_all_lights = 0;

    // Set the max_active_lights field in the sreScene structure.
    if (visible_light_array.Size() <= sre_internal_max_active_lights)
        // All visible lights will be rendered.
        nu_active_lights = visible_light_array.Size();
    else {
        // The following function sets nu_active_lights.
        CalculateVisibleActiveLights(view, sre_internal_max_active_lights);
    }

    for (int i = 0; i < nu_active_lights; i++) {
        // Set the light to be rendered.
        if (nu_active_lights == visible_light_array.Size())
            sre_internal_current_light_index = visible_light_array.Get(i);
        else
            sre_internal_current_light_index = active_light[i];
        sre_internal_current_light = light[sre_internal_current_light_index];
        sre_internal_geometry_scissors_active = false;

        // Visible objects for each light have been predetermined before this function was called.
        // However, for variable lights with a defined worst-case light volume, when the frustum
        // hasn't changed so visiblity has not been redetermined, it is very possible that the light
        // volume is now outside the frustum. In that case we can skip the light, but if we do so
        // when geometry scissors caching is enabled we must also update the geometry scissors cache
        // timestamp for every object in the predetermined list of visible static objects that are
        // partially inside the light volume, so that the "static_light_order" is preserved from
        // frame to frame.
        if ((sre_internal_current_light->type & SRE_LIGHT_DYNAMIC_LIGHT_VOLUME) &&
        sre_internal_current_frame > frustum->most_recent_frame_changed) {
            if (!Intersects(*sre_internal_current_light, frustum->frustum_world)) {
                // Skip the light.
                UpdateGeometryScissorsCacheData(*frustum, *sre_internal_current_light);
                continue;
            }
        } 

        // Always clear this flag because DrawObjectMultiPassLightingPass checks it.
        sre_internal_current_light->shadow_map_required = false;

        if (!(sre_internal_scissors & SRE_SCISSORS_LIGHT_MASK))
            goto skip_scissors;
        if (sre_internal_current_light->type & SRE_LIGHT_DIRECTIONAL) {
            DisableScissors();
            goto skip_scissors;
        }
        // Calculate the scissors region on the viewport where the point light source has influence.
        frustum->CalculateLightScissors(sre_internal_current_light);
        // If the scissors region is empty, skip the light.
        if (frustum->scissors.left == frustum->scissors.right || frustum->scissors.bottom ==
        frustum->scissors.top || frustum->scissors.near == frustum->scissors.far) {
            UpdateGeometryScissorsCacheData(*frustum, *sre_internal_current_light);
            continue;
        }

        // Set the on-screen scissors region for the light and when geometry scissors are
        // enabled determine whether the region is large enough that additional per-object
        // geometry scissors (which further reduce the scissors area) might be useful.
        SetScissorsBeforeLight(frustum->scissors);
        if (sre_internal_scissors & SRE_SCISSORS_GEOMETRY_MASK) {
            float scissors_area = (frustum->scissors.right - frustum->scissors.left) *
                (frustum->scissors.top - frustum->scissors.bottom);
            if (scissors_area >= SRE_GEOMETRY_SCISSORS_LIGHT_AREA_THRESHOLD) {
                sre_internal_geometry_scissors_active = true;
//              printf("Geometry scissors active for light %d.\n", sre_internal_current_light_index);
            }
        }

skip_scissors :

        CHECK_GL_ERROR("Error before lighting pass RenderVisibleObjects\n");
        GL3InitializeShadersBeforeLight();
        RenderVisibleObjectsLightingPass(*frustum, *sre_internal_current_light);
        CHECK_GL_ERROR("Error after lighting pass RenderVisibleObjects\n");
    }

    DisableScissors();
    if (sre_internal_debug_message_level >= 3)
        printf("All lights: %d objects rendered, %d intersection tests.\n", object_count_all_lights,
            intersection_tests_all_lights);
    CHECK_GL_ERROR("Error after RenderLightingPassesNoShadow\n");
}

