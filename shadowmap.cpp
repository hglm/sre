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
#include <limits.h>
#include <float.h>
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
#include "shader.h"
#include "win32_compat.h"

// Shadow map implementation. Directional lights, point lights, spot light and beam lights
// are supported. Point lights use a cube map with a different map for each of the up to six
// faces, the other light types use a single shadow map.
//
// For all types of light, AABBs are determined that bound all objects that can cast shadow
// into the frustum (the shadow casters), and all visible objects that can receive shadows
// (the shadow receivers). The frustum shadow caster volume (defined as the extension of the
// frustum from which an object can potentially cast shadows into the frustum) is used to
// select potential shadow casters, and for local lights a check against the light volume
// is performed for both shadow casters and shadow receivers.
//
// The shadow map transformation matrix is calculated so that all 3D positions within the
// shadow caster AABB 
//
// The shadow map is then generated from 

#ifndef NO_SHADOW_MAP

// Draw object into shadow map. Transparent textures are supported.

static void RenderShadowMapObject(sreObject *so, const sreLight& light) {
    // Apply the global object flags mask. Note render_flags will (unnecessarily)
    // be set again in the lighting pass, but the overhead is of course minimal.
    so->render_flags = so->flags & sre_internal_object_flags_mask;
    // Note: the shadow casters/receivers determination is not affected by
    // the global object flags mask, but it would not be very useful anyway.

// printf("RenderShadowMapObject: object id %d, light type 0x%08X\n", so->id, light.type);

    // Initialize the shadow map shader. The shader program is enabled (regular or transparent
    // version), the MVP matrix uniform is set, and when a transparent texture is used it is
    // bound to GL_TEXTURE0 and the UV transformation matrix is set.
    // the UV transformation.
    if (light.type & SRE_LIGHT_POINT_SOURCE)
        GL3InitializeCubeShadowMapShader(*so);
    else if (light.type & SRE_LIGHT_SPOT)
        GL3InitializeProjectionShadowMapShader(*so);
    else
        GL3InitializeShadowMapShader(*so);
    glEnableVertexAttribArray(0);
    sreLODModel *m = sreCalculateLODModel(*so);
    glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION]);
    // XXX Should take interleaved attributes into account.
    glVertexAttribPointer(
        0,                  // attribute 0 (positions)
        4,                  // size
        GL_FLOAT,           // type
        GL_FALSE,           // normalized?
        0,                  // stride
        (void *)0	    // array buffer offset in bytes
        );
    // Support for multi-mesh transparent textures is missing, should be easy to add.
    if (so->render_flags & SRE_OBJECT_TRANSPARENT_TEXTURE) {
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_TEXCOORDS]);
        glVertexAttribPointer(
            1,                                // attribute
            2,                                // size
            GL_FLOAT,                         // type
            GL_FALSE,                         // normalized?
            0,                                // stride
            (void *)0                         // array buffer offset
        );
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->GL_element_buffer);
    for (int i = 0; i < m->nu_meshes; i++) {
        int vertex_offset = 0;
        int nu_vertices = m->nu_triangles * 3;
        if (m->nu_meshes > 1) {
            vertex_offset = m->mesh[i].starting_vertex;
            nu_vertices = m->mesh[i].nu_vertices;
            if (nu_vertices == 0) // Skip empty meshes
                continue;
        }
        if (m->GL_indexsize == 2)
            glDrawElements(GL_TRIANGLES, nu_vertices, GL_UNSIGNED_SHORT, (void *)(vertex_offset * 2));
        else
            glDrawElements(GL_TRIANGLES, nu_vertices, GL_UNSIGNED_INT, (void *)(vertex_offset * 4));
    }
    glDisableVertexAttribArray(0);
    if (so->render_flags & SRE_OBJECT_TRANSPARENT_TEXTURE)
        glDisableVertexAttribArray(1);
}

static void RenderShadowMapFromCasterArray(sreScene *scene, const sreLight& light) {
    for (int i = 0; i < scene->nu_shadow_caster_objects; i++)
        RenderShadowMapObject(scene->object[scene->shadow_caster_object[i]], light);
}

// The calculated shadow AABBs are stored in a global variable.
static sreBoundingVolumeAABB AABB_shadow_receiver;
static sreBoundingVolumeAABB AABB_shadow_caster;

// During shadow AABB determination, SIMD registers/code may be used.
#ifdef USE_SIMD
#define SHADOW_AABB_TYPE sreBoundingVolumeAABB_SIMD
#else
#define SHADOW_AABB_TYPE sreBoundingVolumeAABB
#endif

class sreShadowAABBGenerationInfo {
public :
    SHADOW_AABB_TYPE casters;
    SHADOW_AABB_TYPE receivers;

    void Initialize() {
#ifdef USE_SIMD
        casters.m_dim_min = simd128_set1_float(POSITIVE_INFINITY_FLOAT);
        casters.m_dim_max = simd128_set1_float(NEGATIVE_INFINITY_FLOAT);
        receivers.m_dim_min = casters.m_dim_min;
        receivers.m_dim_max = casters.m_dim_max;
#else
        casters.dim_min = receivers.dim_min = Vector3D(POSITIVE_INFINITY_FLOAT, 
            POSITIVE_INFINITY_FLOAT, POSITIVE_INFINITY_FLOAT);
        casters.dim_max = receivers.dim_max = Vector3D(NEGATIVE_INFINITY_FLOAT,
            NEGATIVE_INFINITY_FLOAT, NEGATIVE_INFINITY_FLOAT);
#endif
    }
    void GetCasters(sreBoundingVolumeAABB& AABB) {
#ifdef USE_SIMD
        casters.Get(AABB);
#else
        AABB = casters;
#endif
    }
    void GetReceivers(sreBoundingVolumeAABB& AABB) {
#ifdef USE_SIMD
        receivers.Get(AABB);
#else
        AABB = receivers;
#endif
    }
};

// Update an AABB with the union of the AABB and the bounding volume of an object.

static void UpdateAABBWithObject(SHADOW_AABB_TYPE &AABB, sreObject *so) {
     if (!(so->flags & SRE_OBJECT_DYNAMIC_POSITION)) {
         // Static object, use the precalculated precise AABB.
         // Use inline function from sre_bounds.h that updates an AAAB with the union with
         // another AABB.
         UpdateAABB(AABB, so->AABB); 
         return;
     }
     // Dynamic object.
     if (so->model->bounds_flags & SRE_BOUNDS_PREFER_SPHERE) {
         sreBoundingVolumeAABB sphere_AABB;
         sphere_AABB.dim_min =  so->sphere.center - Vector3D(1.0f, 1.0f, 1.0f) * so->sphere.radius;
         sphere_AABB.dim_max =  so->sphere.center + Vector3D(1.0f, 1.0f, 1.0f) * so->sphere.radius;
         UpdateAABB(AABB, sphere_AABB);
         return;
    }
    // SRE_BOUNDS_PREFER_BOX or SRE_BOUNDS_PREFER_BOX_LINE_SEGMENT
    // Note: The AABB may not be a good fit if the bounding box is not oriented
    // towards the coordinate system axi. But there is no alternative unless the
    // bounding volume to be updated is not an AABB but something like a convex hull.
    sreBoundingVolumeAABB box_AABB;
    Vector3D max_extents;
    max_extents.Set(
        max3f(so->box.PCA[0].vector.x, so->box.PCA[1].vector.x, so->box.PCA[2].vector.x),
        max3f(so->box.PCA[0].vector.y, so->box.PCA[1].vector.y, so->box.PCA[2].vector.y),
        max3f(so->box.PCA[0].vector.z, so->box.PCA[1].vector.z, so->box.PCA[2].vector.z)
        );
    box_AABB.dim_min = so->box.center - 0.5f * max_extents;
    box_AABB.dim_max = so->box.center + 0.5f * max_extents;
    UpdateAABB(AABB, box_AABB);
}

static void CheckShadowCasterCapacity(sreScene *scene) {
    if (scene->nu_shadow_caster_objects == scene->max_shadow_caster_objects) {
        // Dynamically increase the shadow caster object array when needed.
        int *new_shadow_caster_object = new int[scene->max_shadow_caster_objects * 2];
        memcpy(new_shadow_caster_object, scene->shadow_caster_object,
            sizeof(int) * scene->max_shadow_caster_objects);
        delete [] scene->shadow_caster_object;
        scene->shadow_caster_object = new_shadow_caster_object;
        scene->max_shadow_caster_objects *= 2;
    }
}

// Find the AABB for a directional light. Bounds checks are performed starting from the
// root node. The special bound check result value SRE_BOUNDS_DO_NOT_CHECK disables
// all octree bounds checks (useful for root node-only octrees).

static void FindAABBDirectionalLight(sreShadowAABBGenerationInfo& AABB_generation_info,
const sreFastOctree& fast_oct, int array_index,
sreScene *scene, const sreFrustum& frustum, BoundsCheckResult octree_bounds_check_result) {
    // For directional lights, the shadow caster volume defined for the frustum is equal
    // to the shadow receiver volume.
    // Check whether the octree is completely outside or completely inside that volume,
    // if the octree is not already completely inside.
    // When intersection tests are not allowed, the macro expression will always return
    // false.
    if (SRE_BOUNDS_NOT_EQUAL_AND_TEST_ALLOWED(octree_bounds_check_result,
    SRE_COMPLETELY_INSIDE)) {
        int node_index = fast_oct.array[array_index];
        octree_bounds_check_result = QueryIntersection(fast_oct.node_bounds[node_index],
            frustum.shadow_caster_volume);
        if (octree_bounds_check_result == SRE_COMPLETELY_OUTSIDE)
            return;
    }
    // Check all objects in this node.
    int nu_octants = fast_oct.array[array_index + 1];
    int nu_entities = fast_oct.array[array_index + 2];
    array_index += 3;
    for (int i = 0; i < nu_entities; i++) {
        sreSceneEntityType type;
        int index;
        fast_oct.GetEntity(array_index + i, type, index);
        if (type != SRE_ENTITY_OBJECT)
            continue;
        sreObject *so = scene->object[index];
        if (so->exists) {
            // Note: for a root node-only octree where no bounds are defined,
            // the intersection test of the object with the shadow caster volume
            // is always performed.
            if (octree_bounds_check_result != SRE_COMPLETELY_INSIDE)
                if (!Intersects(*so, frustum.shadow_caster_volume))
                    continue;
            if (so->flags & SRE_OBJECT_CAST_SHADOWS) {
                UpdateAABBWithObject(AABB_generation_info.casters, so);
                CheckShadowCasterCapacity(scene);
                scene->shadow_caster_object[scene->nu_shadow_caster_objects]= so->id;
                scene->nu_shadow_caster_objects++;
            }
            // For all objects that receive light, update the shadow receiver AABB.
            if (!(so->flags & SRE_OBJECT_EMISSION_ONLY)) 
                UpdateAABBWithObject(AABB_generation_info.receivers, so);
        }
    }
    // Check every non-empty subnode.
    array_index += nu_entities;
    for (int i = 0; i < nu_octants; i++)
        FindAABBDirectionalLight(AABB_generation_info,
            fast_oct, fast_oct.array[array_index + i], scene,
            frustum, octree_bounds_check_result);
}

// Find the AABB for all potential shadow casters within the range of a local light.
// Also keep track of the shadow receivers AABB.

static void FindAABBLocalLight(sreShadowAABBGenerationInfo& AABB_generation_info,
const sreFastOctree& fast_oct, int array_index, sreScene *scene,
const sreFrustum& frustum, const sreLight& light, BoundsCheckResult octree_bounds_check_result) {
    int node_index = fast_oct.array[array_index];
    if (SRE_BOUNDS_NOT_EQUAL_AND_TEST_ALLOWED(octree_bounds_check_result, SRE_COMPLETELY_INSIDE)) {
        // If checks are allowed and the octree is not already completely inside the light volume,
        // check the interestion of the octree with the light volume.
        octree_bounds_check_result = QueryIntersection(fast_oct.node_bounds[node_index], light);
        if (octree_bounds_check_result == SRE_COMPLETELY_OUTSIDE)
            return;
    }
    int nu_octants = fast_oct.array[array_index + 1];
    int nu_entities = fast_oct.array[array_index + 2];
    array_index += 3;
    for (int i = 0; i < nu_entities; i++) {
        sreSceneEntityType type;
        int index;
        fast_oct.GetEntity(array_index + i, type, index);
        if (type != SRE_ENTITY_OBJECT)
            continue;
        sreObject *so = scene->object[index];
        // Skip objects attached to the current light and infinite distance objects.
        if (so->exists && so->attached_light != light.id) {
            // Both shadow casters and shadow receivers must intersect the light volume.
            // Note: for a root-node only octree, the intersection test is always performed.
            if (octree_bounds_check_result == SRE_COMPLETELY_INSIDE || Intersects(*so, light)) {
                if (so->flags & SRE_OBJECT_CAST_SHADOWS) {
                    // For objects that cast shadows, update the caster AABB if the object falls within
                    // the shadow caster volume.
                    if (Intersects(*so, frustum.shadow_caster_volume)) {
                        UpdateAABBWithObject(AABB_generation_info.casters, so);
                        CheckShadowCasterCapacity(scene);
                        scene->shadow_caster_object[scene->nu_shadow_caster_objects]= so->id;
                        scene->nu_shadow_caster_objects++;
                    }
                }
                // For all objects that receive light, update the shadow receiver AABB.
                if (!(so->flags & SRE_OBJECT_EMISSION_ONLY))
                    UpdateAABBWithObject(AABB_generation_info.receivers, so);
            }
        }
    }
    // Check every non-empty subnode.
    array_index += nu_entities;
    for (int i = 0; i < nu_octants; i++)
        FindAABBLocalLight(AABB_generation_info,
            fast_oct, fast_oct.array[array_index + i], scene, frustum, light,
            octree_bounds_check_result);
}


// dim_min multiplication vectors, depending on signs of the spotlight direction.
// dim_max multiplication vector is derived by taking "reverse" (1.0 - x, 1.0 - y, 1.0 - z).

static const Vector3D signs_table[8] = {
    Vector3D(1.0f, 1.0f, 1.0f),      // -x, -y, -z
    Vector3D(0.0f, 1.0f, 1.0f),      // +x, -y, -z 
    Vector3D(1.0f, 0.0f, 1.0f),      // -x, +y, -z
    Vector3D(0.0f, 0.0f, 1.0f),      // +x, +y, -z 
    Vector3D(1.0f, 1.0f, 0.0f),      // -x, -y, +z
    Vector3D(0.0f, 1.0f, 0.0f),      // +x, -y, +z 
    Vector3D(1.0f, 0.0f, 0.0f),      // -x, +y, +z
    Vector3D(0.0f, 0.0f, 0.0f)       // +x, +y, +z
};

void RenderSpotOrBeamLightShadowMap(sreScene *scene, const sreLight& light, const sreFrustum &frustum) {
    sreBoundingVolumeAABB relative_AABB;
    relative_AABB.dim_min = AABB_shadow_caster.dim_min - light.vector.GetVector3D();
    relative_AABB.dim_max = AABB_shadow_caster.dim_max - light.vector.GetVector3D();
    int signs = (light.spotlight.x > 0) + (light.spotlight.y > 0) * 2 +
        (light.spotlight.z > 0) * 4;
    // Pick the AABB vertex that is furthest away in the direction of the light.
    // Thanks to the properties of the AABB, it depends only on the spotlight direction
    // signs.
    Point3D V =
        // Add dim_min components depending on the sign of the spotlight direction.
        (relative_AABB.dim_min & signs_table[signs]) +
        // Add dim_max components. We can simply take the "reverse" of the dim_min factor.
        (relative_AABB.dim_max & (Vector3D(1.0f, 1.0f, 1.0f) - signs_table[signs]));
    // Now calculate the distance to this vertex.
    float zmax = Dot(light.spotlight.GetVector3D(), V);

    if (zmax <= 0) {
        // This shouldn't happen due to earlier checks on the shadow caster and
        // receiver volumes.
        printf("RenderSpotOrBeamLightShadowMap: Unexpect zmax <= 0 for light %d (zmax = %f).\n",
            light.id, zmax);
//        light.shadow_map_required = false;
        return;
    }
    zmax = 1.001f * zmax;  // Avoid precision problems at the end of the z range.

    // Create a local coordinate system.
    Vector3D up;
    if (fabsf(light.spotlight.x) < 0.01f && fabsf(light.spotlight.z) < 0.01f) {
        if (light.spotlight.y > 0)
            up = Vector3D(0, 0, - 1.0f);
        else
            up = Vector3D(0, 0, 1.0f);
    }
    else
        up = Vector3D(0, 1.0f, 0);
    // Calculate tangent planes.
    Vector3D x_dir = Cross(up, light.spotlight.GetVector3D());
    x_dir.Normalize();
    Vector3D y_dir = Cross(light.spotlight.GetVector3D(), x_dir);
    y_dir.Normalize();

//    printf("x_dir = (%f, %f, %f) y_dir = (%f, %f, %f) zmax = %f\n",
//    x_dir.x, x_dir.y, x_dir.z, y_dir.x, y_dir.y, y_dir.z, zmax);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_small_shadow_map_framebuffer);
    // For performance reasons, and because spotlight don't need the full size of the shadow map
    // that is required for directional lights, use a seperate smaller shadow buffer.
    glViewport(0, 0, SRE_SMALL_SHADOW_BUFFER_SIZE, SRE_SMALL_SHADOW_BUFFER_SIZE);
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
#if !defined(NO_DEPTH_CLAMP)
    if (sre_internal_use_depth_clamping)
        glDisable(GL_DEPTH_CLAMP);
#endif
    glClear(GL_DEPTH_BUFFER_BIT);
    if (light.type & SRE_LIGHT_BEAM) {
        Vector3D dim_min, dim_max;
        // For a beam light the x and y extents are defined by the cylinder radius.
        dim_min.Set(- light.cylinder.radius, - light.cylinder.radius, 0);
        dim_max.Set(light.cylinder.radius, light.cylinder.radius, zmax);
        GL3CalculateShadowMapMatrix(light.vector.GetPoint3D(), light.spotlight.GetVector3D(),
            x_dir, y_dir, dim_min, dim_max);
    }
    else
        // Spotlight. Use a projection shadow map matrix.
        GL3CalculateProjectionShadowMapMatrix(light.vector.GetPoint3D(),
            light.spotlight.GetVector3D(), x_dir, y_dir, zmax);

    RenderShadowMapFromCasterArray(scene, light);

    if (sre_internal_HDR_enabled)
        glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_HDR_multisample_framebuffer);
    else
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sre_internal_window_width, sre_internal_window_height);
    glEnable(GL_CULL_FACE);
#if !defined(NO_DEPTH_CLAMP)
    // Restore the normal setting.
    if (sre_internal_use_depth_clamping)
        glEnable(GL_DEPTH_CLAMP);
#endif
    return;
}


static Vector3D cube_map_zdir[6] = {
    Vector3D(1.0f, 0, 0), Vector3D(- 1.0f, 0, 0), Vector3D(0, 1.0f, 0),
    Vector3D(0, - 1.0f, 0), Vector3D(0, 0, 1.0f), Vector3D(0, 0, - 1.0f)
    };

static Vector3D cube_map_up_vector[6] = {
    Vector3D(0, - 1.0f, 0), Vector3D(0, - 1.0f, 0), Vector3D(0, 0, 1.0f),
    Vector3D(0, 0, - 1.0f), Vector3D(0, - 1.0f, 0), Vector3D(0, - 1.0f, 0)
    };

void RenderPointLightShadowMap(sreScene *scene, const sreLight& light, const sreFrustum &frustum) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_cube_shadow_map_framebuffer);
        glViewport(0, 0, SRE_CUBE_SHADOW_BUFFER_SIZE, SRE_CUBE_SHADOW_BUFFER_SIZE);
        glDisable(GL_CULL_FACE);
        glDepthFunc(GL_LESS);
#if !defined(NO_DEPTH_CLAMP)
        if (sre_internal_use_depth_clamping)
            glDisable(GL_DEPTH_CLAMP);
#endif
        CHECK_GL_ERROR("Error before shadow map shaders initialization before light.\n");
        GL3InitializeShadowMapShadersBeforeLight();
        CHECK_GL_ERROR("Error after shadow map shaders initialization before light.\n");

        for (int i = 0; i < 6; i++) {
            float zmax, xmin, xmax, ymin, ymax;
            float zmax_casters, zmin_casters;
            // Calculate the extents within the segment.
            // For xmin/xmax and ymin/ymax the axi may not be correct but it's not important because
            // we are only interested in the maximum distance.
            switch (i) {
            case 0 :
                zmax = AABB_shadow_receiver.dim_max.x - light.vector.x;
                zmax_casters = AABB_shadow_caster.dim_max.x - light.vector.x;
                zmin_casters = AABB_shadow_caster.dim_min.x - light.vector.x;
                xmax = AABB_shadow_receiver.dim_max.y - light.vector.y;
                xmin = AABB_shadow_receiver.dim_min.y - light.vector.y;
                ymax = AABB_shadow_receiver.dim_max.z - light.vector.z;
                ymin = AABB_shadow_receiver.dim_min.z - light.vector.z;
                break;
            case 1 :
                zmax = light.vector.x - AABB_shadow_receiver.dim_min.x;
                zmax_casters = light.vector.x - AABB_shadow_caster.dim_min.x;
                zmin_casters = light.vector.x - AABB_shadow_caster.dim_max.x;
                xmax = AABB_shadow_receiver.dim_max.y - light.vector.y;
                xmin = AABB_shadow_receiver.dim_min.y - light.vector.y;
                ymax = AABB_shadow_receiver.dim_max.z - light.vector.z;
                ymin = AABB_shadow_receiver.dim_min.z - light.vector.z;
                break;
            case 2 :
                zmax = AABB_shadow_receiver.dim_max.y - light.vector.y;
                zmax_casters = AABB_shadow_caster.dim_max.y - light.vector.y;
                zmin_casters = AABB_shadow_caster.dim_min.y - light.vector.y;
                xmax = AABB_shadow_receiver.dim_max.x - light.vector.x;
                xmin = AABB_shadow_receiver.dim_min.x - light.vector.x;
                ymax = AABB_shadow_receiver.dim_max.z - light.vector.z;
                ymin = AABB_shadow_receiver.dim_min.z - light.vector.z;
                break;
            case 3 :
                zmax = light.vector.y - AABB_shadow_receiver.dim_min.y;
                zmax_casters = light.vector.y - AABB_shadow_caster.dim_min.y;
                zmin_casters = light.vector.y - AABB_shadow_caster.dim_max.y;
                xmax = AABB_shadow_receiver.dim_max.x - light.vector.x;
                xmin = AABB_shadow_receiver.dim_min.x - light.vector.x;
                ymax = AABB_shadow_receiver.dim_max.z - light.vector.z;
                ymin = AABB_shadow_receiver.dim_min.z - light.vector.z;
                break;
            case 4 :
                zmax = AABB_shadow_receiver.dim_max.z - light.vector.z;
                zmax_casters = AABB_shadow_caster.dim_max.z - light.vector.z;
                zmin_casters = AABB_shadow_caster.dim_min.z - light.vector.z;
                xmax = AABB_shadow_receiver.dim_max.x - light.vector.x;
                xmin = AABB_shadow_receiver.dim_min.x - light.vector.x;
                ymax = AABB_shadow_receiver.dim_max.y - light.vector.y;
                ymin = AABB_shadow_receiver.dim_min.y - light.vector.y;
                break;
            case 5 :
                zmax = light.vector.z - AABB_shadow_receiver.dim_min.z;
                zmax_casters = light.vector.z - AABB_shadow_caster.dim_min.z;
                zmin_casters = light.vector.z - AABB_shadow_caster.dim_max.z;
                xmax = AABB_shadow_receiver.dim_max.x - light.vector.x;
                xmin = AABB_shadow_receiver.dim_min.x - light.vector.x;
                ymax = AABB_shadow_receiver.dim_max.y - light.vector.y;
                ymin = AABB_shadow_receiver.dim_min.y - light.vector.y;
                break;
            }
            // We now have a shadow receiver AABB in a coordinate system relative to the cube map
            // defined by (xmin, ymin, zmin) to (xmax, ymax, zmax), which is oriented with
            // the z-coordinate directed into the direction of the cube segment away from
            // the light source, which is at (0, 0, 0). We are only interested in objects
            // that overlap with the z range [0, zmax].
            //
            // 
            bool skip = false;
            if (zmax <= 0 || zmax_casters <= 0 || zmin_casters > zmax)
                // If the segment has no casters or receivers, skip it.
                // Also skip if the casters bounds are completely beyond the receivers bounds in z.
                skip = true;
            // In normal circumstances, continue with the next segment if we can skip the current
            // one.
            // If optimization is disabled (shader_mask == 0x01), we do clear the cube map segment.
            if (skip && sre_internal_shader_mask != 0x01) {
                shadow_cube_segment_distance_scaling[i] = - 1.0f;
                continue;
            }
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                sre_internal_depth_cube_map_texture, 0, i);
            glClear(GL_DEPTH_BUFFER_BIT);
            if (skip) {
                shadow_cube_segment_distance_scaling[i] = - 1.0f;
                continue;
            }
            // Determine the maximum distance within the cube segment
            float distmax_squared = SquaredMag(Vector3D(xmin, ymin, zmax));
            float dist_squared = SquaredMag(Vector3D(xmax, ymin, zmax));
            distmax_squared = maxf(distmax_squared, dist_squared);
            dist_squared = SquaredMag(Vector3D(xmin, ymax, zmax));
            distmax_squared = maxf(distmax_squared, dist_squared);
            dist_squared = SquaredMag(Vector3D(xmax, ymax, zmax));
            distmax_squared = maxf(distmax_squared, dist_squared);
            distmax_squared *= 1.001f; // Avoid precision problems at the end of the range.
            // Scale to [0, 1.0].
            shadow_cube_segment_distance_scaling[i] = 1.0f / sqrtf(distmax_squared);
            GL3CalculateCubeShadowMapMatrix(light.vector.GetVector3D(), cube_map_zdir[i],
                cube_map_up_vector[i], zmax);
            CHECK_GL_ERROR("Error after glFramebufferTextureLayer\n");
            GL3InitializeShadowMapShadersWithSegmentDistanceScaling(shadow_cube_segment_distance_scaling[i]);
            RenderShadowMapFromCasterArray(scene, light);
//            RenderShadowMapForOctree(scene->octree, scene, light, frustum, SRE_BOUNDS_UNDEFINED);
        }
    if (sre_internal_HDR_enabled)
        glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_HDR_multisample_framebuffer);
    else
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CHECK_GL_ERROR("Error after glBindFramebuffer(0)\n");
    glViewport(0, 0, sre_internal_window_width, sre_internal_window_height);
    glEnable(GL_CULL_FACE);
#if !defined(NO_DEPTH_CLAMP)
    // Restore the normal setting.
    if (sre_internal_use_depth_clamping)
        glEnable(GL_DEPTH_CLAMP);
#endif
    return;
}

bool GL3RenderShadowMapWithOctree(sreScene *scene, sreLight& light, sreFrustum &frustum) {
    // Calculate shadow caster volume.
    frustum.CalculateShadowCasterVolume(light.vector, 6);
    scene->nu_shadow_caster_objects = 0;
    // This flag will be set to false when a shadow map is actually not needed for the light.
    light.shadow_map_required = true;

    if (!(light.type & SRE_LIGHT_DIRECTIONAL)) {
        // Point light, spot light or beam light.
        // Find the AABB for all potential shadow casters and receivers within the range of the light.
        sreShadowAABBGenerationInfo AABB_generation_info;
        AABB_generation_info.Initialize();
        // Note: infinite distance objects do not cast shadows, their octrees can be skipped.
        FindAABBLocalLight(AABB_generation_info,
            scene->fast_octree_static, 0, scene, frustum, light, SRE_BOUNDS_UNDEFINED);
        // Since the dynamic object octree has no bounds, disable octree bounds checking.
        FindAABBLocalLight(AABB_generation_info,
            scene->fast_octree_dynamic, 0, scene, frustum, light, SRE_BOUNDS_DO_NOT_CHECK);
        AABB_generation_info.GetCasters(AABB_shadow_caster);
        AABB_generation_info.GetReceivers(AABB_shadow_receiver);
        if (AABB_shadow_caster.dim_min.x == POSITIVE_INFINITY_FLOAT ||
        AABB_shadow_receiver.dim_min.x == POSITIVE_INFINITY_FLOAT ||
        !Intersects(AABB_shadow_caster, AABB_shadow_receiver)) {
            // No objects cast or no objects receive shadows for this light,
            // or the shadow caster and shadow receiver volumes do not intersect.
            light.shadow_map_required = false;
            if (AABB_shadow_receiver.dim_min.x == POSITIVE_INFINITY_FLOAT) {
                // If no objects receive shadows (or light), we can skip the light entirely.
//                printf("Skipping light entirely.\n");
                return false;
            }
            // No objects cast shadow, but there are light receivers.
            if (sre_internal_debug_message_level >= 3)
                printf("Note: no shadow casters for point/spot/beam light %d, "
                    "can use non-shadow map shaders.\n", light.id);
            // With non shadow-map shaders being properly selected for this case,
            // there should be no need to initialize the shadow map matrix/segments
            // to empty.
            return true;
        }
        // Adjust the shadow receiver volume by the light volume (approximated by a bounding box).
        // Note: For spot and beam lights, this should use the bounding box of the spherical sector
        // or cylinder.
        sreBoundingVolumeAABB light_AABB;
        light_AABB.dim_min = light.vector.GetVector3D() + Vector3D(- light.sphere.radius,
            - light.sphere.radius, - light.sphere.radius);
        light_AABB.dim_max = light.vector.GetVector3D() + Vector3D(light.sphere.radius,
            light.sphere.radius, light.sphere.radius);
        UpdateAABBWithIntersection(AABB_shadow_receiver, light_AABB);

        if (light.type & (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM)) {
            RenderSpotOrBeamLightShadowMap(scene, light, frustum);
            return true;
        }
        // Point light.
        RenderPointLightShadowMap(scene, light, frustum);
        return true;
    }

    // Directional light.
    // Calculate AABB of the objects in the shadow casters and shadow receivers volumes.
    sreShadowAABBGenerationInfo AABB_generation_info;
    AABB_generation_info.Initialize();
    // Note: infinite distance objects do not cast shadows, their octrees can be skipped.
    FindAABBDirectionalLight(AABB_generation_info,
        scene->fast_octree_static, 0, scene, frustum, SRE_BOUNDS_UNDEFINED);
    FindAABBDirectionalLight(AABB_generation_info,
        scene->fast_octree_dynamic, 0, scene, frustum, SRE_BOUNDS_DO_NOT_CHECK);
    AABB_generation_info.GetCasters(AABB_shadow_caster);
    AABB_generation_info.GetReceivers(AABB_shadow_receiver);
    if (AABB_shadow_caster.dim_min.x == POSITIVE_INFINITY_FLOAT ||
    AABB_shadow_receiver.dim_min.x == POSITIVE_INFINITY_FLOAT ||
    !Intersects(AABB_shadow_caster, AABB_shadow_receiver)) {
        // No objects cast or no objects receive shadows for this light.
        light.shadow_map_required = false;
        if (AABB_shadow_receiver.dim_min.x == POSITIVE_INFINITY_FLOAT) {
            // If no objects receive shadows (or light), we can skip the light entirely.
            return false;
        }
        // No objects cast shadow, but there are light receivers.
        return true;
    }

    // At this point, there will be a shadow map, so we can initialize the shadow map
    // in already.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_shadow_map_framebuffer);
    glViewport(0, 0, SRE_SHADOW_BUFFER_SIZE, SRE_SHADOW_BUFFER_SIZE);
    CHECK_GL_ERROR("Error after glBindFramebuffer\n");
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
#if !defined(NO_DEPTH_CLAMP)
    if (sre_internal_use_depth_clamping)
        glDisable(GL_DEPTH_CLAMP);
#endif
    glClear(GL_DEPTH_BUFFER_BIT);

    // Clip the shadow receivers AABB against the AABB of the view frustum (including far plane).
    sreBoundingVolumeAABB frustum_AABB;
    frustum_AABB.dim_min.Set(POSITIVE_INFINITY_FLOAT, POSITIVE_INFINITY_FLOAT, POSITIVE_INFINITY_FLOAT);
    frustum_AABB.dim_max.Set(NEGATIVE_INFINITY_FLOAT, NEGATIVE_INFINITY_FLOAT, NEGATIVE_INFINITY_FLOAT);
    for (int i = 0; i < 8; i++)
        // Extend AABB so that includes the frustum vertex.
        UpdateAABB(frustum_AABB, frustum.frustum_world.hull.vertex[i]);
    UpdateAABBWithIntersection(AABB_shadow_receiver, frustum_AABB);
    sreBoundingVolumeAABB AABB = AABB_shadow_receiver;
    UpdateAABB(AABB, AABB_shadow_caster);
    // Check whether casters or receivers can be clipped where the light enters and exits the area.
    if (Dot(Vector3D(- 1.0f, 0, 0), light.vector.GetVector3D()) > 0) {
        // The light is shining in x-positive direction.
        if (AABB_shadow_caster.dim_min.x > AABB_shadow_receiver.dim_min.x)
            // The caster volume is beyond the receivers where the light enters the area.
            AABB.dim_min.x = AABB_shadow_caster.dim_min.x;
        if (AABB_shadow_caster.dim_max.x > AABB_shadow_receiver.dim_max.x)
            // Where the light exits the area, the caster volume is beyond the receivers.
            AABB.dim_max.x = AABB_shadow_receiver.dim_max.x;
    }
    else {
        // The light is shining in x-negative direction.
        if (AABB_shadow_caster.dim_max.x < AABB_shadow_receiver.dim_max.x)
            // The caster volume is beyond the receivers where the light enters the area.
            AABB.dim_max.x = AABB_shadow_caster.dim_max.x;
        if (AABB_shadow_caster.dim_min.x < AABB_shadow_receiver.dim_min.x)
            // Where the light exits the area, the caster volume is beyond the receivers.
            AABB.dim_min.x = AABB_shadow_receiver.dim_min.x;
    }
    if (Dot(Vector3D(0, - 1.0f, 0), light.vector.GetVector3D()) > 0) {
        // The light is shining in y-positive direction.
        if (AABB_shadow_caster.dim_min.y > AABB_shadow_receiver.dim_min.y)
            // The caster volume is beyond the receivers where the light enters the area.
            AABB.dim_min.y = AABB_shadow_caster.dim_min.y;
        if (AABB_shadow_caster.dim_max.y > AABB_shadow_receiver.dim_max.y)
            // Where the light exits the area, the caster volume is beyond the receivers.
            AABB.dim_max.y = AABB_shadow_receiver.dim_max.y;
    }
    else {
        // The light is shining in y-negative direction.
        if (AABB_shadow_caster.dim_max.y < AABB_shadow_receiver.dim_max.y)
            // The caster volume is beyond the receivers where the light enters the area.
            AABB.dim_max.y = AABB_shadow_caster.dim_max.y;
        if (AABB_shadow_caster.dim_min.y < AABB_shadow_receiver.dim_min.y)
            // Where the light exits the area, the caster volume is beyond the receivers.
            AABB.dim_min.y = AABB_shadow_receiver.dim_min.y;
    }
    if (Dot(Vector3D(0, 0, - 1.0f), light.vector.GetVector3D()) > 0) {
        // The light is shining in z-positive direction.
        if (AABB_shadow_caster.dim_min.z > AABB_shadow_receiver.dim_min.z)
            // The caster volume is beyond the receivers where the light enters the area.
            AABB.dim_min.z = AABB_shadow_caster.dim_min.z;
        if (AABB_shadow_caster.dim_max.z > AABB_shadow_receiver.dim_max.z)
            // Where the light exits the area, the caster volume is beyond the receivers.
            AABB.dim_max.z = AABB_shadow_receiver.dim_max.z;
    }
    else {
        // The light is shining in z-negative direction.
        if (AABB_shadow_caster.dim_max.z < AABB_shadow_receiver.dim_max.z)
            // The caster volume is beyond the receivers where the light enters the area.
            AABB.dim_max.y = AABB_shadow_caster.dim_max.y;
        if (AABB_shadow_caster.dim_min.z < AABB_shadow_receiver.dim_min.z)
            // Where the light exits the area, the caster volume is beyond the receivers.
            AABB.dim_min.z = AABB_shadow_receiver.dim_min.z;
    }
    // Calculate the intersection lines of caster volume's shadow with the AABB and update the AABB.
    if (Dot(Vector3D(- 1.0f, 0, 0), light.vector.GetVector3D()) > 0) {
        // Calculate the intersection lines of the caster volume's shadow with the AABB in the y and z dimensions.
        float x1 = AABB_shadow_caster.dim_max.x + (AABB.dim_max.z - AABB.dim_min.z) * fabsf(light.vector.z);
        float x2 = AABB_shadow_caster.dim_max.x + (AABB.dim_max.y - AABB.dim_min.y) * fabsf(light.vector.y);
        // Adjust the AABB with the furthest extent where shadows can fall.
        AABB.dim_max.x = minf(AABB.dim_max.x, maxf(x1, x2)); 
    }
    else {
        // Calculate the intersection lines of the caster volume's shadow with the AABB in the y and z dimensions.
        float x1 = AABB_shadow_caster.dim_min.x - (AABB.dim_max.z - AABB.dim_min.z) * fabsf(light.vector.z);
        float x2 = AABB_shadow_caster.dim_min.x - (AABB.dim_max.y - AABB.dim_min.y) * fabsf(light.vector.y);
        // Adjust the AABB with the furthest extent where shadows can fall.
        AABB.dim_min.x = maxf(AABB.dim_min.x, minf(x1, x2)); 
    }
    if (Dot(Vector3D(0, - 1.0f, 0), light.vector.GetVector3D()) > 0) {
        // Calculate the intersection lines of the caster volume's shadow with the AABB in the x and z dimensions.
        float y1 = AABB_shadow_caster.dim_max.y + (AABB.dim_max.z - AABB.dim_min.z) * fabsf(light.vector.z);
        float y2 = AABB_shadow_caster.dim_max.y + (AABB.dim_max.y - AABB.dim_min.y) * fabsf(light.vector.y);
        // Adjust the AABB with the furthest extent where shadows can fall.
        AABB.dim_max.y = minf(AABB.dim_max.y, maxf(y1, y2)); 
    }
    else {
        // Calculate the intersection lines of the caster volume's shadow with the AABB in the y and z dimensions.
        float y1 = AABB_shadow_caster.dim_min.y - (AABB.dim_max.z - AABB.dim_min.z) * fabsf(light.vector.z);
        float y2 = AABB_shadow_caster.dim_min.y - (AABB.dim_max.y - AABB.dim_min.y) * fabsf(light.vector.y);
        // Adjust the AABB with the furthest extent where shadows can fall.
        AABB.dim_min.y = maxf(AABB.dim_min.y, minf(y1, y2)); 
    }
    if (Dot(Vector3D(0, 0, - 1.0f), light.vector.GetVector3D()) > 0) {
        // Calculate the intersection lines of the caster volume's shadow with the AABB in the x and z dimensions.
        float z1 = AABB_shadow_caster.dim_max.z + (AABB.dim_max.z - AABB.dim_min.z) * fabsf(light.vector.z);
        float z2 = AABB_shadow_caster.dim_max.z + (AABB.dim_max.y - AABB.dim_min.y) * fabsf(light.vector.y);
        // Adjust the AABB with the furthest extent where shadows can fall.
        AABB.dim_max.z = minf(AABB.dim_max.z, maxf(z1, z2)); 
    }
    else {
        // Calculate the intersection lines of the caster volume's shadow with the AABB in the y and z dimensions.
        float z1 = AABB_shadow_caster.dim_min.z - (AABB.dim_max.z - AABB.dim_min.z) * fabsf(light.vector.z);
        float z2 = AABB_shadow_caster.dim_min.z - (AABB.dim_max.y - AABB.dim_min.y) * fabsf(light.vector.y);
        // Adjust the AABB with the furthest extent where shadows can fall.
        AABB.dim_min.z = maxf(AABB.dim_min.z, minf(z1, z2)); 
    }
    // Clip the AABB against the shadow mapping region so that the AABB is not too large.
    UpdateAABBWithIntersection(AABB, frustum.shadow_map_region_AABB);
#if 0
    AABB.dim_min.x = maxf(AABB.dim_min.x, frustum.shadow_map_region_AABB.dim_min.x);
    AABB.dim_max.x = minf(AABB.dim_max.x, frustum.shadow_map_region_AABB.dim_max.x);
    AABB.dim_min.y = maxf(AABB.dim_min.y, frustum.shadow_map_region_AABB.dim_min.y);
    AABB.dim_max.y = minf(AABB.dim_max.y, frustum.shadow_map_region_AABB.dim_max.y);
    AABB.dim_min.z = maxf(AABB.dim_min.z, frustum.shadow_map_region_AABB.dim_min.z);
    AABB.dim_max.z = minf(AABB.dim_max.z, frustum.shadow_map_region_AABB.dim_max.z);
// printf("AABB = (%f, %f, %f) - (%f, %f, %f)\n", AABB.dim_min.x, AABB.dim_min.y, AABB.dim_min.z,
//    AABB.dim_max.x, AABB.dim_max.y, AABB.dim_max.z);
#endif
#if 0
    // Calculate the vertex for which the plane defined by the light direction is furthest from the center
    // in the direction of the light.
    int signs = (light.vector.x > 0) + (light.vector.y > 0) * 2 +
        (light.vector.z > 0) * 4;
    // Pick the AABB vertex that is furthest away. Thanks to the properties of the AABB,
    // it depends only on the light direction direction signs.
    Point3D V =
        // Add dim_min components depending on the sign of the light direction.
        (AABB.dim_min & signs_table[signs]) +
        // Add dim_max components. We can simply take the "reverse" of the dim_min factor.
        (AABB.dim_max & (Vector3D(1.0f, 1.0f, 1.0f) - signs_table[signs]));
    // Now calculate the distance to this vertex.
    float max_dist = - Dot(light.vector.GetVector3D(), V);
#else
    Vector3D dim_min, dim_max;
    // Calculate the vertex for which the plane defined by the light direction is furthest from the center
    // in the direction of the light.
    Point3D AABB_vertex[8];
    AABB_vertex[0] = Point3D(AABB.dim_min.x, AABB.dim_min.y, AABB.dim_min.z);
    AABB_vertex[1] = Point3D(AABB.dim_max.x, AABB.dim_min.y, AABB.dim_min.z);
    AABB_vertex[2] = Point3D(AABB.dim_min.x, AABB.dim_max.y, AABB.dim_min.z);
    AABB_vertex[3] = Point3D(AABB.dim_max.x, AABB.dim_max.y, AABB.dim_min.z);
    AABB_vertex[4] = Point3D(AABB.dim_min.x, AABB.dim_min.y, AABB.dim_max.z);
    AABB_vertex[5] = Point3D(AABB.dim_max.x, AABB.dim_min.y, AABB.dim_max.z);
    AABB_vertex[6] = Point3D(AABB.dim_min.x, AABB.dim_max.y, AABB.dim_max.z);
    AABB_vertex[7] = Point3D(AABB.dim_max.x, AABB.dim_max.y, AABB.dim_max.z);
    Point3D center = Point3D(0, 0, 0) + 0.5 * (AABB.dim_min + AABB.dim_max);
    float max_dist = 0;
    Vector4D K_temp, K;
    for (int i = 0; i < 8; i++) {
        K_temp = Vector4D(light.vector.GetVector3D(), - Dot(AABB_vertex[i], light.vector.GetVector3D()));
        float dist = Dot(K_temp, center);
        if (dist > max_dist) {
            max_dist = dist;
            K = K_temp;
        }
    }
    if (max_dist == 0) {
        printf("Warning: Max distance from camera for directional light shadow map is invalid.\n");
    }
// printf("max distance = %f, K = (%f, %f, %f, %f)\n", max_dist, K.x, K.y, K.z, K.w);
#endif
    Point3D camera_position;

    // Calculate the intersection of that plane with the line going through the center, which will be the
    // camera position.
    float t = - Dot(K, center) / Dot(K, light.vector);
// printf("t = %f\n", t);
    camera_position = center - t * light.vector.GetVector3D();
// printf("Camera position = (%f, %f, %f)\n", camera_position.x, camera_position.y, camera_position.z);
    // Calculate the absolute value of the cosine of the angle of the normal of the plane with the AABB normals.
    float dot1 = fabsf(Dot(K.GetVector3D(), Vector3D(1.0f, 0, 0)));
    float dot2 = fabsf(Dot(K.GetVector3D(), Vector3D(0, 1.0f, 0)));
    float dot3 = fabsf(Dot(K.GetVector3D(), Vector3D(0, 0, 1.0f)));
    Vector3D up;
    if (dot1 < dot2)
        if (dot3 < dot1)
            up = Vector3D(0, 0, 1.0f);
        else
            up = Vector3D(1.0f, 0, 0);
    else
        if (dot3 < dot2)
            up = Vector3D(0, 0, 1.0f);
        else
            up = Vector3D(0, 1.0f, 0);
    // Make sure up and the K are oriented in the same direction.
    if (Dot(K.GetVector3D(), up) < 0)
        up = - up;
    // Calculate tangent planes.
    Vector3D x_dir = Cross(light.vector.GetVector3D(), up);
    Vector3D y_dir = Cross(x_dir, light.vector.GetVector3D());
    // Calculate the vertex that is furthest from the camera position in both directions along x_dir and y_dir,
    // and the vertex that is furthest from the camera opposition in the opposite light vector direction.
    dim_max.x = dim_max.y = dim_max.z = NEGATIVE_INFINITY_FLOAT;
    dim_min.x = dim_min.y = POSITIVE_INFINITY_FLOAT;
    for (int i = 0; i < 8; i++) {
        Vector4D L = Vector4D(x_dir, - Dot(AABB_vertex[i], x_dir));
        float dist = Dot(L, camera_position);
        if (dist > dim_max.x)
            dim_max.x = dist;
        if (dist < dim_min.x)
            dim_min.x = dist;
        L = Vector4D(y_dir, - Dot(AABB_vertex[i], y_dir));
        dist = Dot(L, camera_position);
        if (dist > dim_max.y)
            dim_max.y = dist;
        if (dist < dim_min.y)
            dim_min.y = dist;
        L = Vector4D(K.GetVector3D(), - Dot(AABB_vertex[i], K.GetVector3D()));
        dist = Dot(L, camera_position);
        if (dist > dim_max.z)
            dim_max.z = dist;
    }
    dim_min.z = 0;

    GL3CalculateShadowMapMatrix(camera_position, - light.vector.GetVector3D(),
        x_dir, y_dir, dim_min, dim_max);
    RenderShadowMapFromCasterArray(scene, light);
//    RenderShadowMapForOctree(scene->octree, scene, light, frustum, SRE_BOUNDS_UNDEFINED);

    // Switch back to default framebuffer.
    if (sre_internal_HDR_enabled)
        glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_HDR_multisample_framebuffer);
    else
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sre_internal_window_width, sre_internal_window_height);
    glEnable(GL_CULL_FACE);
#if !defined(NO_DEPTH_CLAMP)
    // Restore the normal setting.
    if (sre_internal_use_depth_clamping)
        glEnable(GL_DEPTH_CLAMP);
#endif
    return true;
}


// The shadow map visualization functions are called after all rendering has finished.
// They have to recalculate the shadow map because it is often overwritten by later
// lights in the rendering order.

static const Vector2D font_size1 = Vector2D(0.02, 0.03);

static const Vector2D font_size2 = Vector2D(0.015, 0.02);

void sreScene::sreVisualizeShadowMap(int light_index, sreFrustum *frustum) {
    if (light_index >= nu_lights)
        return;
    // At least one shadow map shader uniform setting function uses sre_internal_current_light.
    sre_internal_current_light = light[light_index];
    // Before a regular lighting pass, the appropriate shadow map texture is bound,
    // so we have to do it explicitly.
    sreBindShadowMapTexture(sre_internal_current_light);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    bool r = GL3RenderShadowMapWithOctree(this, *light[light_index], *frustum);
    glDepthMask(GL_FALSE);
    // The shadow map generation binds the multi-sample framebuffer when finished, switch
    // back to the final framebuffer.
    if (sre_internal_HDR_enabled)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Disable the depth test.
    glDisable(GL_DEPTH_TEST);

    sreSetImageBlendingMode(SRE_IMAGE_BLEND_ADDITIVE);
    char s[64];
    sprintf(s, "Light %d shadow map visualization", light_index);
    sreSetFont(NULL);
    // Set font size and default colors (white).
    sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE | SRE_IMAGE_SET_COLORS, NULL, &font_size1);
    sreDrawText(s, 0.01, 0.97);

    if (!r) {
        sreDrawTextCentered("Light has no light receivers",
            0.2f, 0.485f, 0.6f);
        return;
    }

    if (!light[light_index]->shadow_map_required) {
        sreDrawTextCentered("Light has no shadow casters or no shadow receivers",
            0.2f, 0.485f, 0.6f);
        return;
    }

    if (light[light_index]->type & SRE_LIGHT_POINT_SOURCE)
        sreVisualizeCubeMap(light_index);
    else if (light[light_index]->type & SRE_LIGHT_DIRECTIONAL)
        sreVisualizeDirectionalLightShadowMap(light_index);
    else if (light[light_index]->type & (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM))
        sreVisualizeBeamOrSpotLightShadowMap(light_index);
}

// Invert depth value, and use yellow.

static Vector4D shadow_map_visualization_colors[2] = {
    Vector4D(- 1.0f, - 1.0f, 0, 0),
    Vector4D(1.0f, 1.0f, 0, 1.0f)
};

void sreVisualizeDirectionalLightShadowMap(int light_index) {
    // Set the source to the shadow map texture.
    sreSetImageSource(SRE_IMAGE_SET_TEXTURE | SRE_IMAGE_SET_ONE_COMPONENT_SOURCE,
        sre_internal_depth_texture, 0);
    // Set the colors and the default texture coordinate transform (NULL).
    sreSetImageParameters(SRE_IMAGE_SET_COLORS | SRE_IMAGE_SET_TRANSFORM,
        shadow_map_visualization_colors, NULL);
    float aspect = 16.0f / 9.0f;
    sreDrawImage(0, 0, 1.0f / aspect, 1.0f);
}

static Vector4D spotlight_shadow_map_visualization_colors[2] = {
    Vector4D(- 1.0f, - 1.0f, 0, 0),
    Vector4D(1.0f, 1.0f, 0, 1.0f)
};

void sreVisualizeBeamOrSpotLightShadowMap(int light_index) {
    // Set the source to the shadow map texture.
    sreSetImageSource(SRE_IMAGE_SET_TEXTURE | SRE_IMAGE_SET_ONE_COMPONENT_SOURCE,
        sre_internal_small_depth_texture, 0);
    // Set the colors and the default texture coordinate transform (NULL).
    sreSetImageParameters(SRE_IMAGE_SET_COLORS | SRE_IMAGE_SET_TRANSFORM,
        spotlight_shadow_map_visualization_colors, NULL);
    float aspect = 16.0f / 9.0f;
    sreDrawImage(0, 0, 1.0f / aspect, 1.0f);
}

// The texture array has the order +X -X +Y -Y +Z -Z.
// Table to convert to more convenient order to visualize.

static int order[6] = {
    1, 3, 5, 0, 2, 4
};

static const char *cube_map_name[6] = {
    "-X", "-Y", "-Z", "+X", "+Y", "+Z" };


// The cube maps are oriented fairly arbitrarily. We can provide
// a texture coordinate transform to the sreDrawImage function
// to change the orientation into one that makes sense for a
// z = 0 ground world.
//
// -X:  x and y have to be swapped.
// +X : x and y have to be swapped.
// -Y has to be mirrored in x.
// +Y has to be mirrored in y.
// -Z has to be mirrored in x.
// +Z ?

static Matrix3D cube_uv_transform[6] = {
    // -X: x and y have to be swapped and both have to be
    // mirrored.
    Matrix3D(0.0, -1.0f, 1.0f,
             -1.0f, 0.0, 1.0f,
             0.0, 0.0, 1.0f),
    // -Y has to be mirrored in x.
    Matrix3D(-1.0f, 0.0, 1.0f,
             0.0, 1.0f, 0.0,
             0.0, 0.0, 1.0f),
    // -Z has to be mirrored in x.
    Matrix3D(-1.0f, 0.0, 1.0f,
             0.0, 1.0f, 0.0,
             0.0, 0.0, 1.0f),
    // +X : x and y have to be swapped.
    Matrix3D(0.0, 1.0f, 0.0,
             1.0f, 0.0, 0.0,
             0.0, 0.0, 1.0f),
    // +Y has to be mirrored in y.
    Matrix3D(1.0f, 0.0, 0.0,
             0.0, -1.0f, 1.0f,
             0.0, 0.0, 1.0f),
   // +Z?
    Matrix3D(1.0f, 0.0, 0.0,
             0.0, 1.0f, 0.0,
             0.0, 0.0, 1.0f)
};

static Vector4D cube_visualization_colors[2] = {
    Vector4D(- 1.0f, - 1.0f, 0, 0),
    Vector4D(1.0f, 1.0f, 0, 1.0f)
};

void sreVisualizeCubeMap(int light_index) {
    char s[16];
    // Draw the cube map in overlay (3 x 2 images).
    float w_step = 1.0f / 3.0f;
    float w = w_step * 0.95;
    float h = w_step * (16.0f / 9.0f);
    float h_step = h * 1.04;
    if (h_step + h > 0.96) {
        // Scale.
        float factor = 0.96 / (h_step + h);
        w *= factor;
        w_step *= factor;
        h *= factor;
        h_step *= factor;
    }
    // Set the source to the cube depth texture array.
    sreSetImageSource(SRE_IMAGE_SET_TEXTURE_ARRAY | SRE_IMAGE_SET_ONE_COMPONENT_SOURCE,
        sre_internal_depth_cube_map_texture, 0);
    // Set the colors and the default texture coordinate transform (NULL).
    sreSetImageParameters(SRE_IMAGE_SET_COLORS | SRE_IMAGE_SET_TRANSFORM,
        cube_visualization_colors, NULL);
    for (int i = 0; i < 3; i++) {
        if (shadow_cube_segment_distance_scaling[order[i]] < 0)
            continue;
        // Update only the array index.
        sreSetImageSource(SRE_IMAGE_SET_TEXTURE_ARRAY_INDEX, 0, order[i]);
        // Set the uv transformation so that the cube-map is oriented conviently.
        sreSetImageParameters(SRE_IMAGE_SET_TRANSFORM, NULL, &cube_uv_transform[i]);
        sreDrawImage(i * w_step, 0, w, h);
    }
    for (int i = 0; i < 3; i++) {
        if (shadow_cube_segment_distance_scaling[order[i + 3]] < 0)
            continue;
        sreSetImageSource(SRE_IMAGE_SET_TEXTURE_ARRAY_INDEX, 0, order[i + 3]);
        sreSetImageParameters(SRE_IMAGE_SET_TRANSFORM, NULL, &cube_uv_transform[i + 3]);
        sreDrawImage(i * w_step, h * 1.04, w, h);
    }

    // Draw labels.
    sreSetTextParameters(SRE_IMAGE_SET_COLORS, NULL, NULL); // Set default text colors.
    float centered_x_offset = (w - 0.02 * 6) * 0.5;
    for (int i = 0; i < 3; i++) {
        sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size1);
        sreDrawText(cube_map_name[i], i * w_step + w * 0.40, h - 0.06);
        if (shadow_cube_segment_distance_scaling[order[i]] < 0)
            sreDrawText("(Empty)", i * w_step + centered_x_offset, h * 0.5 - 0.015);
        else {
            sprintf(s, "(Range %.1f)", 1.0f / shadow_cube_segment_distance_scaling[order[i]]);
            sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size2);
            sreDrawTextCentered(s, i * w_step, h - 0.025, w);
        }
    }
    for (int i = 0; i < 3; i++) {
        sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size1);
        sreDrawText(cube_map_name[i + 3], i * w_step + w * 0.40, h * 1.04 + h - 0.06);
        if (shadow_cube_segment_distance_scaling[order[i + 3]] < 0)
            sreDrawText("(Empty)", i * w_step + centered_x_offset, h * 1.04 + h * 0.5 - 0.015);
        else {
            sprintf(s, "(Range %.2f)", 1.0f / shadow_cube_segment_distance_scaling[order[i + 3]]);
            sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size2);
            sreDrawTextCentered(s, i * w_step, h * 1.04 + h - 0.025, w);
        }
    }
}

#endif // !defined(NO_SHADOW_MAP)
