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

// Geometrical shadow volume bounds calculation. These functions are called either
// during the preprocessing to determine light objects lists (for static objects
// in combination with lights for which the shadow volume does not change),
// or during stencil shadow volume rendering (for combinations of objects and lights
// for which the geometrical shadow volumes are not static), in which case a
// temporary shadow volume data structure is calculated which is not kept.
//
// The shadow volume bounds are used during stencil shadow volume rendering.
// The are calculated when geometry scissors are enabled; the projection of the
// shadow volume onto the display view plane determines the scissors area.
//
// They are also used before the shadow volume visibility tests (whether a
// shadow volume, or its infinite projection, intersects the frustum or its
// infinite extension).


#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>

#include "win32_compat.h"
#include "sre.h"
#include "sre_internal.h"
#include "sre_bounds.h"

// Calculate an object's shadow volume pyramid for a point source light or spot light, based
// on the oriented bounding box of the object.
// Returns the bounding volume type (SRE_BOUNDING_VOLUME_PYRAMID if a shadow volume
// could be calculated, SRE_BOUNDING_VOLUME_EMPTY if the shadow volume is empty (which
// can happen with a completely flat object that is oriented parallel to the light direction),
// or SRE_BOUNDING_VOLUME_EVERYWHERE if no shadow volume could be calculated).

sreBoundingVolumeType sreObject::CalculateShadowVolumePyramid(const sreLight& light, Point3D *Q,
int& n_convex_hull) const {
        bool P_included[8];
        if (box.PCA[2].SizeIsZero()) {
            // Flat plane object.
            // Have to check for light source lying in the flat plane.
            if (Dot(box.plane[4], light.vector) == 0) {
//                printf("Shadow volume is empty.\n");
                return SRE_BOUNDING_VOLUME_EMPTY;
            }
            for (int i = 0; i < 4; i++)
                P_included[i] = true;
        }
        else {
            // Calculate the dot product of all planes with the light source.
            float dot_L[6];
            for (int i = 0; i < 6; i++)
                dot_L[i] = Dot(box.plane[i], light.vector);
            for (int i = 0; i < 8; i++)
                P_included[i] = false;
            // Traverse all planes.
            bool empty = true;
            for (int i = 0; i < 6; i++)
                if (dot_L[i] < 0) {
                    // The plane is part of the convex hull.
                    P_included[BB_plane_vertex[i][0]] = true;
                    P_included[BB_plane_vertex[i][1]] = true;
                    P_included[BB_plane_vertex[i][2]] = true;
                    P_included[BB_plane_vertex[i][3]] = true;
                    empty = false;
                }
           if (empty) {
               // The light source is within the bounding box.
//               for (int i = 0; i < n; i++)
//                   printf("Dot %d = %f, L[i] = (%f, %f, %f, %f)\n", i, dot_L[i], L[i].x, L[i].y, L[i].z, L[i].w);
               return SRE_BOUNDING_VOLUME_EVERYWHERE;
           }
        }
        Point3D P[8];
        int n_vertices;
        box.ConstructVertices(P, n_vertices);
        // Construct a pyramid with its base far enough to be outside the influence of the light.
        // First define the normal direction from the light source to the center of the bounding box.
        Vector3D V = box.center - light.vector.GetPoint3D();
        Vector3D N = V;
        N.Normalize();
        // Set the apex of the pyramid to the light position.
        Q[0] = light.vector.GetPoint3D();
        bool tried_alternative = false;
again :
        // Define the plane of the base of the pyramid as the plane with the previously calculated
        // normal displaced from the light source by the light volume radius.
        Vector4D K = Vector4D(N, - Dot(N, light.vector.GetPoint3D() + N * light.sphere.radius));
        n_convex_hull = 1;
        for (int i = 0; i < n_vertices; i++) {
            if (P_included[i]) {
                // For each bounding box vertex that is part of the convex hull, project it
                // on the base of the pyramid.
                Vector3D W = P[i] - light.vector.GetPoint3D();
                float dot = Dot(K, W);
                if (dot <= 0) {
                    // The object is close to the light source and one of its bounding box corners
                    // that is part of the convex hull is at an angle of 90 degrees or more to the
                    // normal direction. Chose a different normal direction.
                    if (tried_alternative) {
                        // We already tried the alternative normal direction. Give up.
//                        printf("Alternative normal direction failed.\n");
                        return SRE_BOUNDING_VOLUME_EVERYWHERE;
                    }
                    // Try the normal direction of the smallest dimension of the object.
                    Vector3D N_previous = N;
                    if (box.PCA[2].SizeIsZero())
                        N = box.T_normal;
                    else
                        N = box.PCA[2].vector * box.PCA[2].scale_factor;
                    if (Dot(N, N_previous) < 0)
                        N = -N;
                    tried_alternative = true;
                    goto again;
                }
                float t = - Dot(K, light.vector.GetPoint3D()) / dot;
                Q[n_convex_hull] = light.vector.GetPoint3D() + t * W;
                n_convex_hull++;
            }
        }
//        if (tried_alternative)
//            printf("Alternative normal direction for pyramid base succesfully processed.\n");
//        printf("n_convex_hull = %d\n", n_convex_hull);
//        for (int i = 0; i < n_convex_hull; i++)
//            printf("(%f, %f, %f) ", Q[i].x, Q[i].y, Q[i].z);
//        printf("\n");
        return SRE_BOUNDING_VOLUME_PYRAMID;
}

// Calculate an object's shadow volume pyramid cone for a point source light or spot light, based
// on the oriented bounding box of the object.
// Returns the bounding volume type (SRE_BOUNDING_VOLUME_PYRAMID_CONE if a shadow volume
// could be calculated, SRE_BOUNDING_VOLUME_EMPTY if the shadow volume is empty (which
// can happen with a completely flat object that is oriented parallel to the light direction),
// or SRE_BOUNDING_VOLUME_EVERYWHERE if no shadow volume could be calculated).

sreBoundingVolumeType sreObject::CalculatePointSourceOrSpotShadowVolume(
const sreLight& light, Point3D *Q, int& n_convex_hull, Vector3D& axis, float& radius,
float& cos_half_angular_size) const {
        bool P_included[8];
        if (box.PCA[2].SizeIsZero()) {
            // Flat plane object.
            // Have to check for light source lying in the flat plane.
            if (Dot(box.plane[4], light.vector) == 0) {
//                printf("Shadow volume is empty.\n");
                return SRE_BOUNDING_VOLUME_EMPTY;
            }
            for (int i = 0; i < 4; i++)
                P_included[i] = true;
        }
        else {
            // Calculate the dot product of all planes with the light source.
            float dot_L[6];
            for (int i = 0; i < 6; i++)
                dot_L[i] = Dot(box.plane[i], light.vector);
            for (int i = 0; i < 8; i++)
                P_included[i] = false;
            // Traverse all planes.
            bool empty = true;
            for (int i = 0; i < 6; i++)
                if (dot_L[i] < 0) {
                    // The plane is part of the convex hull.
                    P_included[BB_plane_vertex[i][0]] = true;
                    P_included[BB_plane_vertex[i][1]] = true;
                    P_included[BB_plane_vertex[i][2]] = true;
                    P_included[BB_plane_vertex[i][3]] = true;
                    empty = false;
                }
           if (empty) {
               // The light source is within the bounding box.
//               for (int i = 0; i < n; i++)
//                   printf("Dot %d = %f, L[i] = (%f, %f, %f, %f)\n", i, dot_L[i], L[i].x, L[i].y, L[i].z, L[i].w);
               return SRE_BOUNDING_VOLUME_EVERYWHERE;
           }
        }
        Point3D P[8];
        int n_vertices;
        box.ConstructVertices(P, n_vertices);
        // Construct a pyramid cone with the axis equal to the direction from the light source to the
        // center of the bounding box, and the axis length equal to the light volume radius.
        Vector3D V = box.center - light.vector.GetPoint3D();
        Vector3D N = V;
        N.Normalize();
        axis = N;
        // The range of the light is the radius. Note that the bounding sphere of the spotlight
        // cannot be used since it is centered somewhere in the middle of the spotlight volume.
        radius = light.attenuation.x;
        Vector3D unnormalized_axis = axis * radius;
        // Set the apex of the pyramid to the light position.
        Q[0] = light.vector.GetPoint3D();
        n_convex_hull = 1;
        float min_cos_angle = 1.0f;
        for (int i = 0; i < n_vertices; i++) {
            if (P_included[i]) {
                // For each bounding box vertex that is part of the convex hull, calculate the edge from the
                // apex and make its length equal to the light sphere radius.
                Vector3D E = P[i] - light.vector.GetPoint3D();
                Vector3D E_normalized = E;
                E_normalized.Normalize();
                float cos_angle = Dot(axis, E_normalized);
                min_cos_angle = minf(min_cos_angle, cos_angle);
                if (min_cos_angle <= 0)
                    break;
                Q[n_convex_hull] = light.vector.GetPoint3D() + E_normalized * radius;
                n_convex_hull++;
            }
        }
        if (min_cos_angle > 0.0f) {
            // The maximum angle between the axis and any edge is smaller than 90 degrees,
            // which means the pyramid cone can be used.
            cos_half_angular_size = min_cos_angle;
            return SRE_BOUNDING_VOLUME_PYRAMID_CONE;
        }
        // When the maximum angle is 90 degrees or greater, we have to find better shadow volume bounds.
        // such as a spherical sector.
        cos_half_angular_size = min_cos_angle;
        return SRE_BOUNDING_VOLUME_SPHERICAL_SECTOR;
}

// Create an object's geometrical shadow volume for a directional light. A half-cylinder
// (cylinder that is open-ended on one end) is created, based on the object's bounding sphere.
// Always returns the bounding volume type SRE_BOUNDING_VOLUME_HALF_CYLINDER.

sreBoundingVolumeType sreObject::CalculateShadowVolumeHalfCylinderForDirectionalLight(
const sreLight& light, Point3D &E, float& cylinder_radius, Vector3D& cylinder_axis) const {
    // Calculate the endpoint. It is situated on the bounding sphere of the object in
    // the direction of where the light is (which is the inverse of the direction of the
    // light).
    E = sphere.center + sphere.radius * light.vector.GetVector3D();
    // The axis points in the direction of the light.
    cylinder_axis = - light.vector.GetVector3D();
    // Simply use the bounding sphere radius for the cylinder's radius.
    cylinder_radius = sphere.radius;
//    return SRE_BOUNDING_VOLUME_EVERYWHERE;
    return SRE_BOUNDING_VOLUME_HALF_CYLINDER;
}

// Create an object's geometrical shadow volume for a beam light. A cylinder is created,
// based on the object's bounding sphere. Normally returns the bounding volume type
// SRE_BOUNDING_VOLUME_CYLINDER; returns SRE_BOUNDING_VOLUME_EMPTY when the object is
// outside the light volume.

sreBoundingVolumeType sreObject::CalculateShadowVolumeCylinderForBeamLight(
const sreLight& light, Point3D& center, float& length, Vector3D& cylinder_axis,
float& cylinder_radius) const {
    // Define the plane through the beam light position, with normal in the beam light direction.
    // The axis from beam light's cylinder bounding volume is used, which should be the same as the
    // vector defined in light.spotlight.
    Vector4D K = Vector4D(light.cylinder.axis, - Dot(light.cylinder.axis, light.vector.GetPoint3D()));
    // Calculate the distance from the plane of the object's bounding sphere center.
    float dist = Dot(K, sphere.center);
    // Check whether the object's bounding sphere is in fact inside the light volume cylinder.
    if (dist <= - sphere.radius || dist >= light.cylinder.length + sphere.radius)
        return SRE_BOUNDING_VOLUME_EMPTY;
    // The first endpoint of the shadow volume cylinder will be equal to sphere.center moved one
    // sphere.radius in the direction of the light position, clipped to be within the light volume.
    float dist_endpoint1 = maxf(dist - sphere.radius, 0);
    // The second endpoint will be equal to sphere.center moved in the light direction to the
    // light cylinder's end of range. In terms of distance from the light position, this is simply
    // the length of the light's cylinder.
    float dist_endpoint2 = light.cylinder.length;
    // Set the shadow volume cylinder data.
    center = light.vector.GetPoint3D() + (dist_endpoint1 + dist_endpoint2) * 0.5f * light.cylinder.axis;
    length = dist_endpoint2 - dist_endpoint1;
    cylinder_axis = light.cylinder.axis;
    cylinder_radius = sphere.radius;
    // Note: The beam light volume has the special property of being delimited by the cylinder radius.
    // This could be taken advantage of by creating a smaller geometrical shadow volume if the object
    // is partly outside the beam. The fact that the shadow volume drawn by the GPU is based on the full
    // silhouette should not affect correctness. The smaller geometrical shadow volume would help
    // limit any geometry scissors while increasing the likelyhood of the shadow volume being
    // outside the frustum in an intersection test. However, it is not easy to express this
    // shadow volume in terms a cylinder (it comes down to circle-circle intersection tests).
    return SRE_BOUNDING_VOLUME_CYLINDER;
}

// Calculate the scissors (with region and depth bounds) of the geometrical shadow volume
// of the object. Returns true if the scissors region is not empty, false otherwise.
// The scissors region is not clipped to screen dimensions; it may be larger.
//
// Because geometry scissors are not applied to directional lights, the shadow volume is often
// of the type SRE_BOUNDING_VOLUME_PYRAMID_CONE type which is used for point and spot lights.
// However, SRE_BOUNDING_SPHERICAL_SECTOR may be used in certain cases, and for beam lights,
// a potential cylinder-shaped shadow volume would have to converted to a box.

bool sreObject::CalculateShadowVolumeScissors(const sreLight& light, const sreFrustum& frustum,
const sreShadowVolume& sv, sreScissors& shadow_volume_scissors) const {
    if (sv.type == SRE_BOUNDING_VOLUME_PYRAMID_CONE) {
        shadow_volume_scissors.SetEmptyRegion();
        Point3DPadded P[12];
	for (int i = 0; i < sv.pyramid_cone->nu_vertices; i++)
            P[i] = sv.pyramid_cone->vertex[i];
        sreScissorsRegionType t = shadow_volume_scissors.UpdateWithWorldSpaceBoundingPyramid(
            &P[0], sv.pyramid_cone->nu_vertices, frustum);
        if (t == SRE_SCISSORS_REGION_DEFINED)
            return true;
        else if (t == SRE_SCISSORS_REGION_EMPTY)
            return false;
        // When undefined, set the full region and depth bounds.
        shadow_volume_scissors.SetFullRegionAndDepthBounds();
        return true;
    }
    // Scissors calculation for other shadow volume types, like spherical sectors or cylinders,
    // has not yet been implemented.
    shadow_volume_scissors.SetFullRegionAndDepthBounds();
    return true;
}

// Temporary shadow volume structures used when a geometrical shadow volume has
// to be calculated, but not stored, on the fly.

static sreShadowVolume sre_internal_sv_pyramid, sre_internal_sv_half_cylinder,
    sre_internal_sv_cylinder, sre_internal_sv_pyramid_cone, sre_internal_sv_spherical_sector;

void sreInitializeInternalShadowVolume() {
    sre_internal_sv_pyramid.type = SRE_BOUNDING_VOLUME_PYRAMID;
    sre_internal_sv_pyramid.pyramid = new sreBoundingVolumePyramid;
    // Maximum of 12 vertices.
    sre_internal_sv_pyramid.pyramid->AllocateStorage(12);
    sre_internal_sv_pyramid_cone.type = SRE_BOUNDING_VOLUME_PYRAMID_CONE;
    sre_internal_sv_pyramid_cone.pyramid_cone = new sreBoundingVolumePyramidCone;
    // Maximum of 12 vertices.
    sre_internal_sv_pyramid_cone.pyramid_cone->AllocateStorage(12);
    sre_internal_sv_spherical_sector.type = SRE_BOUNDING_VOLUME_SPHERICAL_SECTOR;
    sre_internal_sv_spherical_sector.spherical_sector = new sreBoundingVolumeSphericalSector;
    sre_internal_sv_half_cylinder.type = SRE_BOUNDING_VOLUME_HALF_CYLINDER;
    sre_internal_sv_half_cylinder.half_cylinder = new sreBoundingVolumeHalfCylinder;
    sre_internal_sv_cylinder.type = SRE_BOUNDING_VOLUME_CYLINDER;
    sre_internal_sv_cylinder.cylinder = new sreBoundingVolumeCylinder;
}

// Calculates or looks up the shadow volume for an object with respect to the
// given light and assigns it to sv. Sets a SRE_BOUNDING_VOLUME_PYRAMID_CONE,
// SRE_BOUNDING_VOLUME_HALF_CYLINDER or SRE_BOUNDING_VOLUME_CYLINDER if a shadow
// volume could be calculated, SRE_BOUNDING_VOLUME_EMPTY if it is empty, and
// SRE_BOUNDING_VOLUME_EVERYWHERE if no shadow volume could be calculated.
//
// The shadow volume is for temporary use and may only be valid until the next
// shadow volume is calculate using this function. It should not be freed.

void sreObject::CalculateTemporaryShadowVolume(const sreLight& light, sreShadowVolume **sv) const {
    // If the light does not produce changing shadow volumes out of itself,
    // and the object does not move, look up the precalculated shadow volume
    // in the object's static shadow volume list.
    if (!(light.type & SRE_LIGHT_DYNAMIC_SHADOW_VOLUME) && !(flags & SRE_OBJECT_DYNAMIC_POSITION)) {
        // Note: For directional lights this could work for moving objects as well by
        // translating the shadow volume. Since the object's bounding sphere is used to create
        // a directional light shadow volume, any rotation of the object would not affect its shape.
        *sv = LookupShadowVolume(sre_internal_current_light_index);
        if (*sv != NULL)
            return;
    }
    if (light.type & SRE_LIGHT_DIRECTIONAL) {
        *sv = &sre_internal_sv_half_cylinder;
        // Calculate the object's shadow volume for the directional light.
        // This is a very quick and simple calculation using the bounding sphere of the object.
        sreBoundingVolumeType t = CalculateShadowVolumeHalfCylinderForDirectionalLight(light,
             (*sv)->half_cylinder->endpoint, (*sv)->half_cylinder->radius, (*sv)->half_cylinder->axis);
        (*sv)->type = t;
        (*sv)->is_complete = true;
        return;
    }
    if (!(light.type & (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_SPOT))) {
        // Beam light. A cylinder-shaped bounding volume is assigned.
        *sv = &sre_internal_sv_cylinder;
        // This is a fairly simple calculation using the bounding sphere of the object.
        sreBoundingVolumeType t = CalculateShadowVolumeCylinderForBeamLight(light,
             (*sv)->cylinder->center, (*sv)->cylinder->length, (*sv)->cylinder->axis,
             (*sv)->cylinder->radius);
        (*sv)->type = t;
        (*sv)->is_complete = true;
        // Note: No neeed to calculate axis coefficients, since we never test intersection
        // of an AABB against a shadow volume (generally only a test of the shadow volume
        // against the convex hull of the frustum is performed).
        return;
    }
    // Point source or spot light.
#if 0
    *sv = &sre_internal_sv_pyramid;
    // Calculating a point light shadow volume requires a little work.
    sreBoundingVolumeType t = CalculateShadowVolumePyramid(light, (*sv)->pyramid->vertex,
        (*sv)->pyramid->nu_vertices);
    (*sv)->type = t;
    if (t == SRE_BOUNDING_VOLUME_PYRAMID)
        // Note: If an actual pyramid was calculated, it is not "completed" yet (plane
        // information), because it requires some relatively slow math operations.
        // CompleteParameters() is called when the plane data is required (for example
        // some intersection tests).
        (*sv)->is_complete = false;
    else
        (*sv)->is_complete = true;
#else
    *sv = &sre_internal_sv_pyramid_cone;
    // Calculating a point light shadow volume requires a little work.
    sreBoundingVolumeType t = CalculatePointSourceOrSpotShadowVolume(light, (*sv)->pyramid->vertex,
        (*sv)->pyramid->nu_vertices, (*sv)->pyramid_cone->axis, (*sv)->pyramid_cone->radius,
        (*sv)->pyramid_cone->cos_half_angular_size);
    (*sv)->type = t; // Normally SRE_BOUNDING_VOLUME_PYRAMID_CONE.
    (*sv)->is_complete = true;
    if (t == SRE_BOUNDING_VOLUME_SPHERICAL_SECTOR) {
        sreShadowVolume *sv2;
        sv2 = &sre_internal_sv_spherical_sector;
        sv2->spherical_sector->radius = (*sv)->pyramid_cone->radius;
        sv2->spherical_sector->axis = (*sv)->pyramid_cone->axis;
        sv2->spherical_sector->cos_half_angular_size = (*sv)->pyramid_cone->cos_half_angular_size;
        // The following is very slow.
        sv2->spherical_sector->sin_half_angular_size = sinf(acosf((*sv)->pyramid_cone->cos_half_angular_size));
        sv2->spherical_sector->center = light.vector.GetPoint3D();
        *sv = sv2;
    }
#endif
    return;
}

