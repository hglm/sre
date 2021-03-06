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
// is performed for both shadow casters and shadow receivers. A list of shadow casters for
// the light is compiled. For point lights, the cube map segments into which each shadow caster
// falls is also stored.
//
// The shadow map transformation matrix is calculated so that all 3D positions within the
// shadow caster AABB fall within the shadow map.
//
// The shadow map is then generated using the predetermined list of shadow casters. 

#ifndef NO_SHADOW_MAP

// Draw object into shadow map. Transparent textures are supported.

// static int object_count;

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
    sreLODModel *m = sreCalculateLODModel(*so);
    // Non-closed model handling is activated from non-closed models that are not "almost closed"
    // (virtually perfect for shadow mapping purposes). Additionally, when the open side of a
    // non-closed model is permanently hidden from light as well as from view, special handling is
    // not required.
    bool non_closed_model_handling =
        ((m->flags & (SRE_LOD_MODEL_NOT_CLOSED | SRE_LOD_MODEL_ALMOST_CLOSED))
        == SRE_LOD_MODEL_NOT_CLOSED) &&
        !((so->flags & (SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_LIGHT | SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_VIEW))
        == (SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_LIGHT | SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_VIEW));
    if (non_closed_model_handling)
        // Disable front-face culling for non-closed models.
        glDisable(GL_CULL_FACE);

    CHECK_GL_ERROR("RenderShadowMapObject: Error before shadow map shader initialization.\n");
    bool non_closed_add_bias = false;
    if (light.type & SRE_LIGHT_POINT_SOURCE)
        GL3InitializeCubeShadowMapShader(*so);
    else if (light.type & SRE_LIGHT_SPOT)
        GL3InitializeSpotlightShadowMapShader(*so);
    else {
        // Directional or beam light.
        // When the model is not closed, and the open side is not hidden from light, use special
        // handling so that a bias value is already added to the depth value for light-facing
	// triangles.
        if (non_closed_model_handling) {
            non_closed_add_bias = true;
            GL3InitializeShadowMapShaderNonClosedObject(*so);
        }
        else
            GL3InitializeShadowMapShader(*so);
    }
    CHECK_GL_ERROR("RenderShadowMapObject: Error after shadow map shader initialization.\n");

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION]);
    // XXX Should take interleaved attributes into account.
    glVertexAttribPointer(
        0,                  // attribute 0 (position)
        4,                  // size
        GL_FLOAT,           // type
        GL_FALSE,           // normalized?
        0,                  // stride
        (void *)0	    // array buffer offset in bytes
        );
    if (non_closed_add_bias) {
        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_NORMAL]);
        // XXX Should take interleaved attributes into account.
        glVertexAttribPointer(
            2,                  // attribute 2 (normal)
            3,                  // size
            GL_FLOAT,           // type
            GL_FALSE,           // normalized?
            0,                  // stride
            (void *)0	        // array buffer offset in bytes
            );
    }
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
    if (non_closed_add_bias)
        glDisableVertexAttribArray(2);
    if (so->render_flags & SRE_OBJECT_TRANSPARENT_TEXTURE)
        glDisableVertexAttribArray(1);
    if (non_closed_model_handling)
        glEnable(GL_CULL_FACE);
    CHECK_GL_ERROR("Error after RenderShadowMapObject.\n");
//    object_count++;
}

static void RenderShadowMapFromCasterArray(sreScene *scene, const sreLight& light) {
//    sreMessage(SRE_MESSAGE_INFO, "Rendering %d objects into shadow map for light %d.",
//        scene->shadow_caster_array.Size(), light.id);
    for (int i = 0; i < scene->shadow_caster_array.Size(); i++)
        RenderShadowMapObject(scene->object[
            scene->shadow_caster_array.Get(i)], light);
}

static void RenderCubeShadowMapFromCasterArray(sreScene *scene, const sreLight& light, int segment) {
    // For point lights, the cube segments that the object falls into are stored in bits
    // 26 to 31 of each entry in the shadow caster object array.
//    object_count = 0;
    unsigned int segment_bit = (unsigned int)1 << (26 + segment);
    for (int i = 0; i < scene->shadow_caster_array.Size(); i++)
        if (scene->shadow_caster_array.Get(i) & segment_bit)
            RenderShadowMapObject(scene->object[
                scene->shadow_caster_array.Get(i) & 0x03FFFFFF], light);
//    sreMessage(SRE_MESSAGE_INFO, "Light %d shadow cube map generation: segment mask = 0x%08X, "
//          "%d objects rendered.", light.id, segment_bit, object_count);
}

// The calculated shadow AABBs are stored in a global variable.
static sreBoundingVolumeAABB AABB_shadow_receiver;
static sreBoundingVolumeAABB AABB_shadow_caster;
// For point lights, calculate the minimum depth of any object over all shadow cube map segments.
// This is similar to the distance between the light and the closest bounding volume of an object.
static float min_segment_depth;

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
        casters.m_dim_min = simd128_set_same_float(FLT_MAX);
        casters.m_dim_max = simd128_set_same_float(- FLT_MAX);
        receivers.m_dim_min = casters.m_dim_min;
        receivers.m_dim_max = casters.m_dim_max;
#else
        casters.dim_min = receivers.dim_min = Vector3D(FLT_MAX, 
            FLT_MAX, FLT_MAX);
        casters.dim_max = receivers.dim_max = Vector3D(- FLT_MAX,
            - FLT_MAX, - FLT_MAX);
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

#define INV_SQRT_2 (1.0f / sqrtf(2.0f))

static const Vector3D cube_map_plane_normal[6][4] = {
    // Positive x segment.
    {
    Vector3D(INV_SQRT_2, 0.0f, - INV_SQRT_2), // Top
    Vector3D(INV_SQRT_2, 0.0f, INV_SQRT_2),   // Bottom
    Vector3D(INV_SQRT_2, - INV_SQRT_2, 0.0f), // Far
    Vector3D(INV_SQRT_2, INV_SQRT_2, 0.0f)    // Near
    },
    // Negative x segment.
    {
    Vector3D(- INV_SQRT_2, 0.0f, - INV_SQRT_2), // Top
    Vector3D(- INV_SQRT_2, 0.0f, INV_SQRT_2),   // Bottom
    Vector3D(- INV_SQRT_2, - INV_SQRT_2, 0.0f), // Far
    Vector3D(- INV_SQRT_2, INV_SQRT_2, 0.0f)    // Near
    },
    // Positive y segment.
    {
    Vector3D(0.0f, INV_SQRT_2, - INV_SQRT_2), // Top
    Vector3D(0.0f, INV_SQRT_2, INV_SQRT_2),   // Bottom
    Vector3D(- INV_SQRT_2, INV_SQRT_2, 0.0f), // Right
    Vector3D(INV_SQRT_2, INV_SQRT_2, 0.0f)    // Left
    },
    // Negative y segment.
    {
    Vector3D(0.0f, - INV_SQRT_2, - INV_SQRT_2), // Top
    Vector3D(0.0f, - INV_SQRT_2, INV_SQRT_2),   // Bottom
    Vector3D(- INV_SQRT_2, - INV_SQRT_2, 0.0f), // Right
    Vector3D(INV_SQRT_2, - INV_SQRT_2, 0.0f)    // Left
    },
    // Positive z segment.
    {
    Vector3D(0.0f, - INV_SQRT_2, INV_SQRT_2), // Far
    Vector3D(0.0f, INV_SQRT_2, INV_SQRT_2),   // Near
    Vector3D(- INV_SQRT_2, 0.0f, INV_SQRT_2), // Right
    Vector3D(INV_SQRT_2, 0.0f, INV_SQRT_2)    // Left
    },
    // Negative z segment.
    {
    Vector3D(0.0f, - INV_SQRT_2, - INV_SQRT_2), // Far
    Vector3D(0.0f, INV_SQRT_2, - INV_SQRT_2),   // Near
    Vector3D(- INV_SQRT_2, 0.0f, - INV_SQRT_2), // Right
    Vector3D(INV_SQRT_2, 0.0f, - INV_SQRT_2)    // Left
    }
};

static sreBoundingVolumeConvexHull cube_segment_ch[6];
static Vector4D cube_segment_ch_plane[6][4];

static void CalculateCubeSegmentHulls(const sreLight& light) {
    sreBoundingVolumeConvexHull ch;
    unsigned int mask = 0;
    for (int i = 0; i < 6; i++) {
        cube_segment_ch[i].nu_planes = 4;
        cube_segment_ch[i].plane = &cube_segment_ch_plane[i][0];
        // Take the plane orientation from the table and make it go through
        // the light position.
        for (int j = 0; j < 4; j++)
            cube_segment_ch[i].plane[j] = Vector4D(cube_map_plane_normal[i][j],
                - Dot(cube_map_plane_normal[i][j], light.sphere.center));
    }
}

static unsigned int GetSegmentMask(const sreLight& light, const sreObject& so) {
    // Check whether object is inside each convex hull representing the six
    // cube segments of the light volume.
    // This could be optimized because each plane test is identical to one of the plane
    // tests for another segment. SIMD could be used.
    unsigned int mask = 0;
    for (int i = 5; i >= 0; i--) {
        mask <<= 1;
        if (Intersects(so, cube_segment_ch[i]))
            mask |= 1;
    }
    return mask;
}

static void CalculateObjectAABB(const sreObject& so, sreBoundingVolumeAABB& object_AABB) {
     if (!(so.flags & SRE_OBJECT_DYNAMIC_POSITION))
         // Static object, use the precalculated precise AABB.
         object_AABB = so.AABB;
     else
         // Dynamic object.
         if (so.model->bounds_flags & SRE_BOUNDS_PREFER_SPHERE) {
             object_AABB.dim_min =  so.sphere.center - Vector3D(1.0f, 1.0f, 1.0f) * so.sphere.radius;
             object_AABB.dim_max =  so.sphere.center + Vector3D(1.0f, 1.0f, 1.0f) * so.sphere.radius;
         }
         else {
             // SRE_BOUNDS_PREFER_BOX or SRE_BOUNDS_PREFER_BOX_LINE_SEGMENT
             Vector3D max_extents;
             max_extents.Set(
                 max3f(so.box.PCA[0].vector.x, so.box.PCA[1].vector.x, so.box.PCA[2].vector.x),
                 max3f(so.box.PCA[0].vector.y, so.box.PCA[1].vector.y, so.box.PCA[2].vector.y),
                 max3f(so.box.PCA[0].vector.z, so.box.PCA[1].vector.z, so.box.PCA[2].vector.z)
                 );
             object_AABB.dim_min = so.box.center - 0.5f * max_extents;
             object_AABB.dim_max = so.box.center + 0.5f * max_extents;
         }
}

static void UpdateMinSegmentDepth(const sreLight& light, const sreObject& so) {
    if (min_segment_depth == 0.0f)
        return;
    sreBoundingVolumeAABB object_AABB;
    CalculateObjectAABB(so, object_AABB);
    // Check whether the bounding volume intersects the light position; if so, there is no
    // bound on the minimum segment depth.
    if (Intersects(light.sphere.center, object_AABB)) {
        min_segment_depth = 0.0f;
        return;
    }
    // Calculate the distance from the light to the AABB.
    float max_dist = 0.0f;
    // X-positive segment.
    if (object_AABB.dim_min.x >= light.sphere.center.x)
        max_dist = object_AABB.dim_min.x - light.sphere.center.x;
    // X-negative segment.
    if (object_AABB.dim_max.x <= light.sphere.center.x)
        max_dist = maxf(max_dist, light.sphere.center.x - object_AABB.dim_max.x);
    // Y-positive segment.
    if (object_AABB.dim_min.y >= light.sphere.center.y)
        max_dist = maxf(max_dist, object_AABB.dim_min.y - light.sphere.center.y);
    // Y-negative segment.
    if (object_AABB.dim_max.y <= light.sphere.center.y)
        max_dist = maxf(max_dist, light.sphere.center.y - object_AABB.dim_max.y);
    // Z-positive segment.
    if (object_AABB.dim_min.z >= light.sphere.center.z)
        max_dist = maxf(max_dist, object_AABB.dim_min.z - light.sphere.center.z);
    // Z-negative segment.
    if (object_AABB.dim_max.z <= light.sphere.center.z)
        max_dist = maxf(max_dist, light.sphere.center.z - object_AABB.dim_max.z);
    min_segment_depth = minf(min_segment_depth, max_dist);
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
                scene->shadow_caster_array.Add(so->id);
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

// Keep track whether each segment of a point light cube map remains empty.
static unsigned int segment_non_empty_mask;

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
//    sreMessage(SRE_MESSAGE_INFO, "FindAABBLocalLight: Checking %d objects in octree node.",
//        nu_entities);
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
                        if (light.type & SRE_LIGHT_POINT_SOURCE) {
                            unsigned int segment_mask = GetSegmentMask(light, *so);
                            scene->shadow_caster_array.Add(so->id | (segment_mask << 26));
                            segment_non_empty_mask |= segment_mask;
                            // Update the minimum segment depth.
                            UpdateMinSegmentDepth(light, *so);
                        }
                        else
                            scene->shadow_caster_array.Add(so->id);
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


void RenderSpotOrBeamLightShadowMap(sreScene *scene, const sreLight& light, const sreFrustum &frustum) {
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

    // Pick a shadow map size appropriate for the projected size of the light volume.
    float threshold;
#ifdef OPENGL_ES2
    threshold = SRE_MAX_SHADOW_MAP_SIZE_THRESHOLD_GLES2;
#else
    threshold = SRE_MAX_SHADOW_MAP_SIZE_THRESHOLD_OPENGL;
#endif
    // Adjust the threshold according to the window resolution (the base tresholds are
    // calibrated for approximate FullHD resolution).
    threshold *= 1920.0f / sre_internal_window_width;
    // When the viewpoint is inside the light volume, use a lower threshold (higher resolution).
    bool inside = false;
    if (light.type & SRE_LIGHT_BEAM) {
        if (Intersects(sre_internal_viewpoint, light.cylinder))
            inside = true;
    }
    else
        if (Intersects(sre_internal_viewpoint, light.spherical_sector))
            inside = true;
    if (inside)
        threshold *= 0.75f;
    for (int level = 0;; level++) {
        if (light.projected_size >= threshold ||
        level + 1 == sre_internal_nu_shadow_map_size_levels) {
           sre_internal_current_shadow_map_index = level;
           break;
        }
        threshold *= 0.5f;
    }
    glViewport(0, 0,
        sre_internal_max_shadow_map_size >> sre_internal_current_shadow_map_index,
        sre_internal_max_shadow_map_size >> sre_internal_current_shadow_map_index);

#ifdef OPENGL_ES2
    glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_shadow_map_framebuffer[
        sre_internal_current_shadow_map_index]);
#else
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_shadow_map_framebuffer[
        sre_internal_current_shadow_map_index]);
#endif
    // Using front-face culling (only drawing back faces) reduces shadow map artifacts.
    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
#if !defined(NO_DEPTH_CLAMP)
    if (sre_internal_use_depth_clamping)
        glDisable(GL_DEPTH_CLAMP);
#endif
    glClear(GL_DEPTH_BUFFER_BIT);
    if (light.type & SRE_LIGHT_BEAM) {
    // Use an orthogonal transformation matrix for beam lights.
        Vector3D dim_min, dim_max;
        // For both beam lights and spot lights the x and y extents of the shadow map area
        // are defined by the cylinder radius.
        dim_min.Set(- light.cylinder.radius, - light.cylinder.radius, 0);
        dim_max.Set(light.cylinder.radius, light.cylinder.radius, light.cylinder.length);
        GL3CalculateShadowMapMatrix(light.vector.GetPoint3D(), light.spotlight.GetVector3D(),
            x_dir, y_dir, dim_min, dim_max);
        sre_internal_current_shadow_map_dimensions = Vector3D(light.cylinder.radius,
            light.cylinder.radius, light.cylinder.length);
        GL3InitializeShadowMapShadersBeforeLight(Vector4D(sre_internal_current_shadow_map_dimensions,
            sre_internal_max_shadow_map_size >> sre_internal_current_shadow_map_index));
    }
    else {
        // Spotlight. Use a projection shadow map matrix.
        GL3CalculateProjectionShadowMapMatrix(light.vector.GetPoint3D(),
            light.spotlight.GetVector3D(), x_dir, y_dir,
            SRE_MIN_SPOT_LIGHT_SHADOW_MAP_NEAR_PLANE_DISTANCE, light.spherical_sector.radius);
        // Only the fourth component of the shadow map dimensions (the size in pixels of the
        // shadow map) is used (and only by the lighting pass shaders, not the shadow map shaders).
        sre_internal_current_shadow_map_dimensions = Vector3D(light.cylinder.radius,
            light.cylinder.radius, light.spherical_sector.radius);
        GL3InitializeSpotlightShadowMapShadersBeforeLight();
    }

    RenderShadowMapFromCasterArray(scene, light);

    if (sre_internal_HDR_enabled)
        glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_HDR_multisample_framebuffer);
    else
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sre_internal_window_width, sre_internal_window_height);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
#if !defined(NO_DEPTH_CLAMP)
    // Restore the normal setting.
    if (sre_internal_use_depth_clamping)
        glEnable(GL_DEPTH_CLAMP);
#endif
    return;
}

static Vector3D cube_map_zdir[6] = {
    Vector3D(1.0f, 0, 0), Vector3D(- 1.0f, 0, 0),
    Vector3D(0, 1.0f, 0), Vector3D(0, - 1.0f, 0),
    Vector3D(0, 0, 1.0f), Vector3D(0, 0, - 1.0f)
    };

#if 1
static Vector3D cube_map_s_vector[6] = {
    Vector3D(0, 0.0f, - 1.0f), Vector3D(0, 0.0f, 1.0f),
    Vector3D(1.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
    Vector3D(1.0f, 0.0f, 0.0f), Vector3D(- 1.0f, 0.0f, 0.0f)
    };


static Vector3D cube_map_t_vector[6] = {
    Vector3D(0, - 1.0f, 0.0f), Vector3D(0, - 1.0f, 0.0f),
    Vector3D(0, 0, 1.0f), Vector3D(0, 0, - 1.0f),
    Vector3D(0, - 1.0f, 0), Vector3D(0, - 1.0f, 0)
    };
#else
static Vector3D cube_map_s_vector[6] = {
    Vector3D(0, 0.0f, - 1.0f), Vector3D(0, 0.0f, 1.0f),
    Vector3D(1.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
    Vector3D(1.0f, 0.0f, 0.0f), Vector3D(- 1.0f, 0.0f, 0.0f)
    };

static Vector3D cube_map_t_vector[6] = {
    Vector3D(0, - 1.0f, 0.0f), Vector3D(0, - 1.0f, 0.0f),
    Vector3D(0, 0, 1.0f), Vector3D(0, 0, - 1.0f),
    Vector3D(0, - 1.0f, 0), Vector3D(0, - 1.0f, 0)
    };
#endif

static void BindAndClearCubeMap(int i) {
#ifdef OPENGL_ES2
    glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_cube_shadow_map_framebuffer[
            sre_internal_current_cube_shadow_map_index][i]);
#else
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_cube_shadow_map_framebuffer[
         sre_internal_current_cube_shadow_map_index][i]);
#endif
    if (!sre_internal_depth_cube_map_texture_is_clear[
    sre_internal_current_cube_shadow_map_index][i])
        glClear(GL_DEPTH_BUFFER_BIT);
    sre_internal_depth_cube_map_texture_is_clear[sre_internal_current_cube_shadow_map_index][i] = true;
}

void RenderPointLightShadowMap(sreScene *scene, const sreLight& light, const sreFrustum &frustum) {
//    sreMessage(SRE_MESSAGE_LOG, "Rendering cube shadow map for point light %d.", light.id);
        // Using front-face culling (only drawing back faces) reduces shadow map artifacts.
        glCullFace(GL_FRONT);
        glEnable(GL_CULL_FACE);
        glDepthFunc(GL_LESS);
#if !defined(NO_DEPTH_CLAMP)
        if (sre_internal_use_depth_clamping)
            glDisable(GL_DEPTH_CLAMP);
#endif
        CHECK_GL_ERROR("Error before shadow map shaders initialization before light.\n");
        GL3InitializeCubeShadowMapShadersBeforeLight();
        CHECK_GL_ERROR("Error after shadow map shaders initialization before light.\n");

        // Pick the a cube shadow map size appropriate for the projected size of the light volume.
        float threshold;
#ifdef OPENGL_ES2
        threshold = SRE_MAX_CUBE_SHADOW_MAP_SIZE_THRESHOLD_GLES2;
#else
        threshold = SRE_MAX_CUBE_SHADOW_MAP_SIZE_THRESHOLD_OPENGL;
#endif
        // Adjust the threshold according to the window resolution (the base tresholds are
        // calibrated for approximate FullHD resolution).
        threshold *= 1920.0f / sre_internal_window_width;
        // When the viewpoint is inside the light volume, use a lower threshold (higher resolution).
        if (Intersects(sre_internal_viewpoint, light.sphere))
            threshold *= 0.5f;
        for (int level = 0;; level++) {
            if (light.projected_size >= threshold ||
            level + 1 == sre_internal_nu_cube_shadow_map_size_levels) {
                sre_internal_current_cube_shadow_map_index = level;
                break;
            }
            threshold *= 0.5f;
        }
        glViewport(0, 0,
            sre_internal_max_cube_shadow_map_size >> sre_internal_current_cube_shadow_map_index,
            sre_internal_max_cube_shadow_map_size >> sre_internal_current_cube_shadow_map_index);

	float zmax[6], zmax_casters[6], zmin_casters[6];
        // Calculate the extents within all segments
	// zmin_all_faces is the minimum z for casters over all segments. This can be used
        // as the near plane distance in the projection matrix for the cube faces. When a
        // segment is empty, AABB_shadow_caster.dim_max.x/y/z will be equal to - FLT_MAX.
        zmax[0] = AABB_shadow_receiver.dim_max.x - light.vector.x;
        zmax_casters[0] = AABB_shadow_caster.dim_max.x - light.vector.x;
        zmin_casters[0] = maxf(AABB_shadow_caster.dim_min.x - light.vector.x, 0.0f);
        zmax[1] = light.vector.x - AABB_shadow_receiver.dim_min.x;
        zmax_casters[1] = light.vector.x - AABB_shadow_caster.dim_min.x;
        zmin_casters[1] = maxf(light.vector.x - AABB_shadow_caster.dim_max.x, 0.0f);
        zmax[2] = AABB_shadow_receiver.dim_max.y - light.vector.y;
        zmax_casters[2] = AABB_shadow_caster.dim_max.y - light.vector.y;
        zmin_casters[2] = maxf(AABB_shadow_caster.dim_min.y - light.vector.y, 0.0f);
        zmax[3] = light.vector.y - AABB_shadow_receiver.dim_min.y;
        zmax_casters[3] = light.vector.y - AABB_shadow_caster.dim_min.y;
        zmin_casters[3] = maxf(light.vector.y - AABB_shadow_caster.dim_max.y, 0.0f);
        zmax[4] = AABB_shadow_receiver.dim_max.z - light.vector.z;
        zmax_casters[4] = AABB_shadow_caster.dim_max.z - light.vector.z;
        zmin_casters[4] = maxf(AABB_shadow_caster.dim_min.z - light.vector.z, 0.0f);
        zmax[5] = light.vector.z - AABB_shadow_receiver.dim_min.z;
        zmax_casters[5] = light.vector.z - AABB_shadow_caster.dim_min.z;
        zmin_casters[5] = maxf(light.vector.z - AABB_shadow_caster.dim_max.z, 0.0f);

//        sreMessage(SRE_MESSAGE_INFO, "min_segment_depth = %f", min_segment_depth);
        min_segment_depth = maxf(min_segment_depth, SRE_MIN_SHADOW_CUBE_MAP_NEAR_PLANE_DISTANCE);

        sreUpdateShadowMapNearPlaneDistance(min_segment_depth);
#ifdef CUBE_MAP_STORES_DISTANCE
        // In the new shadow map method, the distance scaling is the same for all segments
        // (light volume radius corresponds to [0, 1]).
        sreUpdateShadowMapSegmentDistanceScaling((1.0f / light.sphere.radius) * 0.999f);
        GL3InitializeCubeShadowMapShadersWithSegmentDistanceScaling();
#endif
        GL3InitializeCubeShadowMapShadersBeforeLight();

        int cube_map_mask = 0;
        for (int i = 0; i < 6; i++) {
            // Bind the framebuffer with correct cube face and clear the cube map segment.
            BindAndClearCubeMap(i);
            CHECK_GL_ERROR("Error after BindAndClearCubeMap\n");

            // Immediately skip the segment if no shadow casting objects where found to be inside it.
            if (!(segment_non_empty_mask & (1 << i))) {
                continue;
            }

            // We now have a shadow receiver AABB in a coordinate system relative to the cube map
            // with z values ranging from zmin_caster[i] to zmax[i], which is oriented with
            // the z-coordinate directed into the direction of the cube segment away from
            // the light source, which is at (0, 0, 0). We are only interested in objects
            // that overlap with the z range [0, zmax[i]].
            bool skip = false;
            if (zmax[i] <= 0 || zmax_casters[i] <= 0 || zmin_casters[i] > zmax[i])
                // If the segment has no casters or receivers, skip it.
                // Also skip if the casters bounds are completely beyond the receivers bounds in z.
                skip = true;
            if (skip)
                continue;
            cube_map_mask |= (1 << i);
            GL3CalculateCubeShadowMapMatrix(light.vector.GetVector3D(), cube_map_zdir[i],
                cube_map_s_vector[i], cube_map_t_vector[i], min_segment_depth, light.attenuation.x);
            CHECK_GL_ERROR("Error after GL3InitializeShadowMapShadersBeforeLight\n");
            RenderCubeShadowMapFromCasterArray(scene, light, i);
            CHECK_GL_ERROR("Error after RenderCubeShadowMapFromCasterArray\n");
            sre_internal_depth_cube_map_texture_is_clear[sre_internal_current_cube_shadow_map_index][i] =
                false;
        }

    // Set the current cube map texture.
#if 1
    sre_internal_current_depth_cube_map_texture = sre_internal_depth_cube_map_texture[
        sre_internal_current_cube_shadow_map_index];
#else
    // When all faces are used, just use the pre-initialized cubemap with all six faces.
    // Otherwise, only use the non-empty cube-map faces and use a small, light-weight
    // empty texture for the empty faces.
    if (cube_map_mask == 0x3F)
         sre_internal_current_depth_cube_map_texture = sre_internal_depth_cube_map_texture[
             sre_internal_current_cube_shadow_map_index];
    else {
        for (int i = 0; i < 6; i++)
            if (cube_map_mask & (1 << i))
                glTexImage2D(cube_map_target[i], 0, GL_DEPTH_COMPONENT16, size, size, 0,
                    GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
            // To do.
    }
#endif

    // Restore framebuffer, viewport and other GL state parameters.
    if (sre_internal_HDR_enabled)
        glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_HDR_multisample_framebuffer);
    else
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CHECK_GL_ERROR("Error after glBindFramebuffer(0)\n");
    glViewport(0, 0, sre_internal_window_width, sre_internal_window_height);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
#if !defined(NO_DEPTH_CLAMP)
    // Restore the normal setting.
    if (sre_internal_use_depth_clamping)
        glEnable(GL_DEPTH_CLAMP);
#endif
    return;
}

bool GL3RenderShadowMapWithOctree(sreScene *scene, sreLight& light, sreFrustum &frustum) {
    // Check whether the the required shadow map technique is available.
    if (((light.type & SRE_LIGHT_POINT_SOURCE) &&
    !(sre_internal_rendering_flags & SRE_RENDERING_FLAG_CUBE_SHADOW_MAP_SUPPORT)) ||
    (!(light.type & SRE_LIGHT_POINT_SOURCE) &&
    !(sre_internal_rendering_flags & SRE_RENDERING_FLAG_SHADOW_MAP_SUPPORT))) {
        light.shadow_map_required = false;
        return true;
    }

    // Calculate shadow caster volume.
    frustum.CalculateShadowCasterVolume(light.vector, 6);
    scene->shadow_caster_array.Truncate(0);
    // This flag will be set to false when a shadow map is actually not needed for the light.
    light.shadow_map_required = true;

    if (!(light.type & SRE_LIGHT_DIRECTIONAL)) {
        // Point light, spot light or beam light.
        // Find the AABB for all potential shadow casters and receivers within the range of the light.
        sreShadowAABBGenerationInfo AABB_generation_info;
        AABB_generation_info.Initialize();
        CalculateCubeSegmentHulls(light);
        min_segment_depth = FLT_MAX;
        segment_non_empty_mask = 0;
        // Note: infinite distance objects do not cast shadows, their octrees can be skipped.
        FindAABBLocalLight(AABB_generation_info,
            scene->fast_octree_static, 0, scene, frustum, light, SRE_BOUNDS_UNDEFINED);
        // Since the dynamic object octree has no bounds, disable octree bounds checking.
        FindAABBLocalLight(AABB_generation_info,
            scene->fast_octree_dynamic, 0, scene, frustum, light, SRE_BOUNDS_DO_NOT_CHECK);
        AABB_generation_info.GetCasters(AABB_shadow_caster);
        AABB_generation_info.GetReceivers(AABB_shadow_receiver);
        if (AABB_shadow_caster.dim_min.x == FLT_MAX ||
        AABB_shadow_receiver.dim_min.x == FLT_MAX ||
        !Intersects(AABB_shadow_receiver, frustum.frustum_world)) {
            // No objects cast or no objects receive shadows for this light,
            // or the shadow receiver volume does not intersect the frustum.
            light.shadow_map_required = false;
            if (AABB_shadow_receiver.dim_min.x == FLT_MAX) {
                // If no objects receive shadows and light, we can skip the light entirely.
                sreMessage(SRE_MESSAGE_LOG, "No shadow or light receivers for light %d.", light.id);
                return false;
            }
            // No objects cast shadow, but there are light receivers.
            sreMessage(SRE_MESSAGE_LOG, "Note: no shadow casters for point/spot/beam light %d, "
                "can use non-shadow map shaders.", light.id);
            // With non shadow-map shaders being properly selected for this case,
            // there should be no need to initialize the shadow map matrix/segments
            // to empty.
            return true;
        }
//        sreMessage(SRE_MESSAGE_LOG, "Rendering shadow map for light %d.", light.id);

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
    if (AABB_shadow_caster.dim_min.x == FLT_MAX ||
    AABB_shadow_receiver.dim_min.x == FLT_MAX ||
    !Intersects(AABB_shadow_receiver, frustum.frustum_world)) {
        // No objects cast or no objects receive shadows for this light.
        light.shadow_map_required = false;
        if (AABB_shadow_receiver.dim_min.x == FLT_MAX) {
            // If no objects receive shadows and light, we can skip the light entirely.
            return false;
        }
        // No objects cast shadow, but there are light receivers.
        return true;
    }

    // At this point, there will be a shadow map, so we can initialize the shadow map
    // in already. Always use the largest shadow map (level 0) for directional lights.
    sre_internal_current_shadow_map_index = 0;
    glViewport(0, 0,
        sre_internal_max_shadow_map_size >> sre_internal_current_shadow_map_index,
        sre_internal_max_shadow_map_size >> sre_internal_current_shadow_map_index);
#ifdef OPENGL_ES2
    glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_shadow_map_framebuffer[
        sre_internal_current_shadow_map_index]);
    // DEBUG
//    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
//        sre_internal_depth_texture[sre_internal_current_shadow_map_index], 0);
#else
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_shadow_map_framebuffer[
        sre_internal_current_shadow_map_index]);
#endif
    glViewport(0, 0, sre_internal_max_shadow_map_size, sre_internal_max_shadow_map_size);
    CHECK_GL_ERROR("Error after glBindFramebuffer\n");
    // Using front-face culling (only drawing back faces) reduces shadow map artifacts.
    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
#if !defined(NO_DEPTH_CLAMP)
    if (sre_internal_use_depth_clamping)
        glDisable(GL_DEPTH_CLAMP);
#endif
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Clip the shadow receivers AABB against the AABB of the view frustum (including far plane).
    sreBoundingVolumeAABB frustum_AABB;
    frustum_AABB.dim_min.Set(FLT_MAX, FLT_MAX, FLT_MAX);
    frustum_AABB.dim_max.Set(- FLT_MAX, - FLT_MAX, - FLT_MAX);
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
printf("AABB = (%f, %f, %f) - (%f, %f, %f)\n", AABB.dim_min.x, AABB.dim_min.y, AABB.dim_min.z,
    AABB.dim_max.x, AABB.dim_max.y, AABB.dim_max.z);
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
    Point3D center = Point3D(0.0f, 0.0f, 0.0f) + 0.5f * (AABB.dim_min + AABB.dim_max);
    float max_dist = 0.0f;
    Vector4D K_temp, K;
    for (int i = 0; i < 8; i++) {
        K_temp = Vector4D(light.vector.GetVector3D(), - Dot(AABB_vertex[i], light.vector.GetVector3D()));
        float dist = Dot(K_temp, center);
        if (dist > max_dist) {
            max_dist = dist;
            K = K_temp;
        }
    }
    if (max_dist == 0.0f || isnan(max_dist)) {
        sreMessage(SRE_MESSAGE_WARNING,
            "Warning: Max distance from camera for directional light shadow map is invalid.");
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
    dim_max.x = dim_max.y = dim_max.z = - FLT_MAX;
    dim_min.x = dim_min.y = FLT_MAX;
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
    float dim_xy = maxf(dim_max.x - dim_min.x, dim_max.y - dim_min.y);
    sre_internal_current_shadow_map_dimensions = Vector3D(dim_xy, dim_xy, dim_max.z);
    CHECK_GL_ERROR("Error before GL3InitializeShadowMapShadersBeforeLight.\n");
    GL3InitializeShadowMapShadersBeforeLight(Vector4D(
        sre_internal_current_shadow_map_dimensions,
        sre_internal_max_shadow_map_size >> sre_internal_current_shadow_map_index));
    CHECK_GL_ERROR("Error after GL3InitializeShadowMapShadersBeforeLight.\n");
    RenderShadowMapFromCasterArray(scene, light);

    // Switch back to default framebuffer.
    if (sre_internal_HDR_enabled)
        glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_HDR_multisample_framebuffer);
    else
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, sre_internal_window_width, sre_internal_window_height);
    glCullFace(GL_BACK);
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
    sre_internal_current_light_index = light_index;
    sre_internal_current_light = light[light_index];
    sre_internal_current_light->shadow_map_required = false;

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    bool r = GL3RenderShadowMapWithOctree(this, *light[light_index], *frustum);
    glDepthMask(GL_FALSE);
    // The shadow map generation binds the multi-sample framebuffer when finished, switch
    // back to the final framebuffer.
#ifndef NO_HDR
    if (sre_internal_HDR_enabled)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
    // Disable the depth test.
    glDisable(GL_DEPTH_TEST);

    sreSetImageBlendingMode(SRE_IMAGE_BLEND_ADDITIVE);
    int size;
    if (light[light_index]->type & SRE_LIGHT_POINT_SOURCE)
        size = sre_internal_max_cube_shadow_map_size >> sre_internal_current_cube_shadow_map_index;
    else if (light[light_index]->type & SRE_LIGHT_DIRECTIONAL)
        size = sre_internal_max_shadow_map_size >> sre_internal_current_shadow_map_index;
    else if (light[light_index]->type & (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM))
        size = sre_internal_max_shadow_map_size >> sre_internal_current_shadow_map_index;
    char s[64];
    if (!r || !light[light_index]->shadow_map_required)
        sprintf(s, "Light %d shadow map visualization", light_index);
    else {
        sprintf(s, "Light %d shadow map visualization (%dx%d)", light_index, size, size);
        if (light[light_index]->type & SRE_LIGHT_POINT_SOURCE)
            sprintf(&s[strlen(s)], " x6");
    }
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
        sre_internal_depth_texture[sre_internal_current_shadow_map_index], 0);
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
        sre_internal_depth_texture[sre_internal_current_shadow_map_index], 0);
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

    // Set the source to a scratch texture. Rather than having the image drawing functions
    // explicitly support cube textures, just copy the cube texture faces into a scratch texture.
#ifndef OPENGL_ES2
    // Not supported with OpenGL ES2.
#if 1
    GLuint scratch_texture;
    glGenTextures(1, &scratch_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scratch_texture);
#ifdef OPENGL_ES2
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        sre_internal_max_cube_shadow_map_size >> sre_internal_current_cube_shadow_map_index,
        sre_internal_max_cube_shadow_map_size  >> sre_internal_current_cube_shadow_map_index,
        0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16,
        sre_internal_max_cube_shadow_map_size >> sre_internal_current_cube_shadow_map_index,
        sre_internal_max_cube_shadow_map_size >> sre_internal_current_cube_shadow_map_index,
        0, GL_DEPTH_COMPONENT, GL_HALF_FLOAT, 0);
    CHECK_GL_ERROR("Error after glTexImage2D.\n");
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    CHECK_GL_ERROR("Error after glTexParameteri.\n");
#endif
    sreSetImageSource(SRE_IMAGE_SET_TEXTURE | SRE_IMAGE_SET_ONE_COMPONENT_SOURCE,
        scratch_texture, 0);
    // Set the colors and the default texture coordinate transform (NULL).
    sreSetImageParameters(SRE_IMAGE_SET_COLORS | SRE_IMAGE_SET_TRANSFORM,
        cube_visualization_colors, NULL);
    for (int i = 0; i < 3; i++) {
        // Update the scratch texture.
        glBindFramebuffer(GL_READ_FRAMEBUFFER,
            sre_internal_cube_shadow_map_framebuffer[sre_internal_current_cube_shadow_map_index]
                [order[i]]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, scratch_texture);
        CHECK_GL_ERROR("Error before glCopyTexImage2D.\n");
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 0, 0,
            sre_internal_max_cube_shadow_map_size >> sre_internal_current_cube_shadow_map_index,
            sre_internal_max_cube_shadow_map_size >> sre_internal_current_cube_shadow_map_index, 0);
        CHECK_GL_ERROR("Error after glCopyTexImage2D.\n");
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        CHECK_GL_ERROR("Error after glBindFramebuffer(0).\n");
        // Set the uv transformation so that the cube-map is oriented conveniently.
        sreSetImageParameters(SRE_IMAGE_SET_TRANSFORM, NULL, &cube_uv_transform[i]);
        sreDrawImage(i * w_step, 0, w, h);
    }
    for (int i = 0; i < 3; i++) {
        // Update the scratch texture.
        glBindFramebuffer(GL_READ_FRAMEBUFFER,
            sre_internal_cube_shadow_map_framebuffer[sre_internal_current_cube_shadow_map_index]
                [order[i + 3]]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, scratch_texture);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 0, 0,
            sre_internal_max_cube_shadow_map_size >> sre_internal_current_cube_shadow_map_index,
            sre_internal_max_cube_shadow_map_size >> sre_internal_current_cube_shadow_map_index, 0);
        CHECK_GL_ERROR("Error after glCopyTexImage2D.\n");
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        // Set the uv transformation so that the cube-map is oriented conveniently.
        sreSetImageParameters(SRE_IMAGE_SET_TRANSFORM, NULL, &cube_uv_transform[i + 3]);
        sreDrawImage(i * w_step, h * 1.04, w, h);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &scratch_texture);
#endif

    // Draw labels.
    sreSetTextParameters(SRE_IMAGE_SET_COLORS, NULL, NULL); // Set default text colors.
    float centered_x_offset = (w - 0.02 * 6) * 0.5;
    for (int i = 0; i < 3; i++) {
        sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size1);
        sreDrawText(cube_map_name[i], i * w_step + w * 0.40, h - 0.06);
#if 0
        if (shadow_cube_segment_distance_scaling < 0)
            sreDrawText("(Empty)", i * w_step + centered_x_offset, h * 0.5 - 0.015);
        else {
            sprintf(s, "(Range %.1f)", 1.0f / shadow_cube_segment_distance_scaling);
            sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size2);
            sreDrawTextCentered(s, i * w_step, h - 0.025, w);
        }
#endif
    }
    for (int i = 0; i < 3; i++) {
        sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size1);
        sreDrawText(cube_map_name[i + 3], i * w_step + w * 0.40, h * 1.04 + h - 0.06);
#if 0
        if (shadow_cube_segment_distance_scaling[order[i + 3]] < 0)
            sreDrawText("(Empty)", i * w_step + centered_x_offset, h * 1.04 + h * 0.5 - 0.015);
        else {
            sprintf(s, "(Range %.2f)", 1.0f / shadow_cube_segment_distance_scaling[order[i + 3]]);
            sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size2);
            sreDrawTextCentered(s, i * w_step, h * 1.04 + h - 0.025, w);
        }
#endif
    }
}

#endif // !defined(NO_SHADOW_MAP)
