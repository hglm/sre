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
#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>

#include "sre.h"
#include "sre_internal.h"
#include "sre_bounds.h"
#include "win32_compat.h"

// This file contains initialization functions for specific bounding volume types,
// followed by the implementation of the generic sreBoundingVolume class (which can
// hold a variety of different bounding volumes), followed by a large set of optimized
// intersection tests of one type of bounding volume against another, or high level
// entities like lights (sreLight), scene objects (sreObject), and octree nodes
// (OctreeNodeBounds).

// Initialization functions for various specific bounding volume types.

void sreBoundingVolumeHull::AllocateStorage(int n_vertices) {
   nu_vertices = n_vertices;
   if (n_vertices > 0)
       vertex = new Point3D[n_vertices];
}

sreBoundingVolumeConvexHull::sreBoundingVolumeConvexHull(int n_planes) {
    AllocateStorage(n_planes);
}

void sreBoundingVolumeConvexHull::AllocateStorage(int n_planes) {
   nu_planes = n_planes;
   if (n_planes > 0)
       plane = new Vector4D[n_planes];
}

sreBoundingVolumeConvexHullWithVertices::sreBoundingVolumeConvexHullWithVertices(int n_vertices,
int n_planes) {
    AllocateStorage(n_vertices, n_planes);
}

void sreBoundingVolumeConvexHullWithVertices::AllocateStorage(int n_vertices, int n_planes) {
   hull.nu_vertices = n_vertices;
   if (n_vertices > 0)
       hull.vertex = new Point3D[n_vertices];
   nu_planes = n_planes;
   if (n_planes > 0)
       plane = new Vector4D[n_planes];
}

void sreBoundingVolumeConvexHullFull::AllocateStorage(int n_vertices, int n_planes) {
   hull.nu_vertices = n_vertices;
   if (n_vertices > 0)
       hull.vertex = new Point3D[n_vertices];
   nu_planes = n_planes;
   if (n_planes > 0) {
       plane = new Vector4D[n_planes];
       plane_radius = new float[n_planes];
   }
}

// Calculate the planes of a box bounding volume.

void sreBoundingVolumeBox::CalculatePlanes() {
    Vector3D normal;
    normal = PCA[0].GetNormal();
    // Construct a plane vector where the normal points inward towards the center of the bounding box.
    // R_positive.
    plane[0] = Vector4D(- normal, - Dot(- normal, center + 0.5f * PCA[0].vector));
    // R_negative.
    plane[1] = Vector4D(normal, - Dot(normal, center - 0.5f * PCA[0].vector));
    // S_positive;
    normal = PCA[1].GetNormal();
    plane[2] = Vector4D(- normal, - Dot(- normal, center + 0.5f * PCA[1].vector));
    // S_negative.
    plane[3] = Vector4D(normal, - Dot(normal, center - 0.5f * PCA[1].vector));
    // T_positive.
    if (PCA[2].SizeIsZero())
        // Have to detect flat planes with a third dimension size of zero.
        normal = T_normal;
    else
        normal = PCA[2].GetNormal();
    plane[4] = Vector4D(- normal, - Dot(- normal, center + 0.5f * PCA[2].vector));
    // T_negative.
    plane[5] = Vector4D(normal, - Dot(normal, center - 0.5 * PCA[2].vector));
}

// Calculate "axis coefficients" of a cylinder (used during intersection tests).

void sreBoundingVolumeCylinder::CalculateAxisCoefficients() {
    axis_coefficients.x = sqrtf(1.0f - sqrf(axis.x));
    axis_coefficients.y = sqrtf(1.0f - sqrf(axis.y));
    axis_coefficients.z = sqrtf(1.0f - sqrf(axis.z));
}

// sreBoundingVolume (generic bounding volume) class.

void sreBoundingVolume::SetEmpty() {
    type = SRE_BOUNDING_VOLUME_EMPTY;
    is_complete = true;
}

void sreBoundingVolume::SetEverywhere() {
    type = SRE_BOUNDING_VOLUME_EVERYWHERE;
    is_complete = true;
}

// Set a bounding volume to the pyramid with n points with positions
// in P[]. P[0] is the base. Space for the pyramid data structures
// is dynamically allocated.

void sreBoundingVolume::SetPyramid(const Point3D *P, int n) {
    type = SRE_BOUNDING_VOLUME_PYRAMID;
    pyramid = new sreBoundingVolumePyramid;
    // Allocate space for n vertices.
    pyramid->AllocateStorage(n);
    // Only need to explicitly set the vertices, the rest (base normal) should be calculated
    // by CompleteParameters().
    for (int i = 0; i < n; i++)
        pyramid->vertex[i] = P[i];
    is_complete = false;
    CompleteParameters();
}

// Set a bounding volume to the pyramid cone with n points with positions
// in P[] and the given unnormalized axis. P[0] is the apex. Space for the pyramid data structures
// is dynamically allocated.

void sreBoundingVolume::SetPyramidCone(const Point3D *P, int n, const Vector3D& axis, float radius,
float cos_half_angular_size) {
    type = SRE_BOUNDING_VOLUME_PYRAMID_CONE;
    pyramid_cone = new sreBoundingVolumePyramidCone;
    // Allocate space for n vertices.
    pyramid_cone->AllocateStorage(n);
    for (int i = 0; i < n; i++)
        pyramid_cone->vertex[i] = P[i];
    pyramid_cone->axis = axis;
    pyramid_cone->radius = radius;
    pyramid_cone->cos_half_angular_size = cos_half_angular_size;
    is_complete = true;
}

void sreBoundingVolume::SetSphericalSector(const Vector3D& axis, float radius, float cos_half_angular_size) {
    type = SRE_BOUNDING_VOLUME_SPHERICAL_SECTOR;
    spherical_sector = new sreBoundingVolumeSphericalSector;
    spherical_sector->axis = axis;
    spherical_sector->radius = radius;
    spherical_sector->cos_half_angular_size = cos_half_angular_size; 
    spherical_sector->sin_half_angular_size = sinf(acosf(cos_half_angular_size));
    is_complete = true;
}
void sreBoundingVolume::SetHalfCylinder(const Point3D& E, float radius, const Vector3D& axis) {
    type = SRE_BOUNDING_VOLUME_HALF_CYLINDER;
    half_cylinder = new sreBoundingVolumeHalfCylinder;
    half_cylinder->endpoint = E;
    half_cylinder->radius = radius;
    half_cylinder->axis = axis;
    is_complete = true;
}

void sreBoundingVolume::SetCylinder(const Point3D& center, float length, const Vector3D& axis,
float radius) {
    type = SRE_BOUNDING_VOLUME_CYLINDER;
    cylinder = new sreBoundingVolumeCylinder;
    cylinder->center = center;
    cylinder->length = length;
    cylinder->axis = axis;
    cylinder->radius = radius;
    // The axis_coefficients parameter is not used for shadow volumes so doesn't
    // need to be calculated; it is explicitly calculated for spot/beam light
    // cylinder volumes.
    is_complete = true;
}

// Complete the secondary parameters of the generic bounding volume. These can be calculated
// from the primary parameters. For sreBoundingVolumeConvexHullFull and
// sreBoundingVolumeConvexHullConfigurable, it is assumed that plane
// data pointers (plane and plane_radius) already contain an array of sufficient size.

void sreBoundingVolume::CompleteParameters() {
    if (is_complete)
        return;
    if (type == SRE_BOUNDING_VOLUME_PYRAMID) {
//        pyramid->nu_planes = pyramid->nu_vertices;
        if (pyramid->nu_vertices == 0) {
            type = SRE_BOUNDING_VOLUME_EMPTY;
            is_complete = true;
            return;
        }
#if 0
        // For a pyramid, we can't just take the mean value of the vertex positions,
        // because the center would be shifted into the direction of the base.
        // We have to give the same weight to the apex as all base vertices combined.
        pyramid->center = pyramid->vertex[0] * (pyramid->nu_vertices - 1);
        for (int i = 0; i < pyramid->nu_vertices - 1; i++)
            pyramid->center += pyramid->vertex[i + 1];
        pyramid->center /= (pyramid->nu_vertices - 1) * 2;
        pyramid->nu_planes = pyramid->nu_vertices;
        // Add the side planes. The normals should point inwards.
        for (int i = 0; i < pyramid->nu_vertices - 1; i++) {
            pyramid->plane[i] = dstPlaneFromPoints(pyramid->vertex[0], pyramid->vertex[i + 1],
                pyramid->vertex[((i + 1) % (pyramid->nu_vertices - 1)) + 1]);
            pyramid->plane[i].OrientPlaneTowardsPoint(pyramid->center);
        }
#endif
        // Add the base plane. We can any use three of the base vertices, the result should be
        // the same, although the accuracy would suffer if the angle between the first three
        // side planes is small. When a pyramid is derived from a projected bounding
        // box, the number of base vertices will usually be four, six or seven. We can try to
        // avoid this issue by spreading out the used vertices.
        int v0 = 1;  // Always use the first base vertex.
        int v1, v2;
        if (pyramid->nu_vertices == 5) {
            // Four base vertices.
            v1 = 2;
            v2 = 4;
        }
        else if (pyramid->nu_vertices == 7) {
            // Six base vertices.
            v1 = 3;
            v2 = 6;
        }
        else if (pyramid->nu_vertices == 8) {
            // Seven base vertices.
            v1 = 4;
            v2 = 7;
        }
        else {
            v1 = 2;
            v2 = 3;
        }
        Vector4D base_plane = dstPlaneFromPoints(pyramid->vertex[v0],
            pyramid->vertex[v1], pyramid->vertex[v2]);
        // The normal should point inwards (which is towards the apex).
        base_plane.OrientPlaneTowardsPoint(pyramid->vertex[0]);
        pyramid->base_normal = base_plane.GetVector3D();
    }
    else if (type == SRE_BOUNDING_VOLUME_CONVEX_HULL) {
        // This is not yet used. A sreBoundingVolumeConvexHullConfigurable type is assumed.
        // Determine the center using the mean position of the vertices.
        convex_hull_configurable->center = Point3D(0, 0, 0);
        for (int i = 0; i < convex_hull_configurable->hull.nu_vertices; i++)
            convex_hull_configurable->center += convex_hull_configurable->hull.vertex[i];
        convex_hull_configurable->center /= convex_hull_configurable->hull.nu_vertices;
        // For each plane, create the plane vector using the information in the
        // plane definitions array.
        int j = 0;
        for (int i = 0; i < convex_hull_configurable->nu_planes; i++) {
            int *vertex_indices = &convex_hull_configurable->plane_definitions[j + 1];
            // Just use the first three vertices defined for the plane. For planes defined with
            // three or four vertices, this should be OK.
            convex_hull_configurable->plane[i] = dstPlaneFromPoints(
                convex_hull_configurable->hull.vertex[vertex_indices[0]],
                convex_hull_configurable->hull.vertex[vertex_indices[1]],
                convex_hull_configurable->hull.vertex[vertex_indices[2]]);
            convex_hull_configurable->plane[i].OrientPlaneTowardsPoint(
                convex_hull_configurable->center);
            // To the next plane in the plane definitions array.
            j += 1 + convex_hull_configurable->plane_definitions[j];
        }
    }
    else
        return;
    // For convex hull derived types (including configurable convex hulls),
    // calculate the radius of each plane with respect to the center, and store minimum
    // and maximum values found.
    convex_hull_full->min_radius = POSITIVE_INFINITY_FLOAT;
    convex_hull_full->max_radius = 0;
    for (int i = 0; i < convex_hull_full->nu_planes; i++) {
        convex_hull_full->plane_radius[i] = fabsf(Dot(convex_hull_full->plane[i], convex_hull_full->center));
        convex_hull_full->min_radius = minf(convex_hull_full->min_radius, convex_hull_full->plane_radius[i]);
        convex_hull_full->max_radius = maxf(convex_hull_full->max_radius, convex_hull_full->plane_radius[i]);
    }
    is_complete = true;
    return;
}

void sreBoundingVolume::Clear() {
    // Free dynamically allocated structures.
    if (type == SRE_BOUNDING_VOLUME_UNDEFINED)
        return;
    if (type == SRE_BOUNDING_VOLUME_SPHERE)
        delete sphere;
    else if (type == SRE_BOUNDING_VOLUME_ELLIPSOID)
        delete ellipsoid;
    else if (type == SRE_BOUNDING_VOLUME_BOX)
        delete box;
    else if (type == SRE_BOUNDING_VOLUME_CYLINDER)
        delete cylinder;
    else if (type == SRE_BOUNDING_VOLUME_CONVEX_HULL) {
        delete [] convex_hull_configurable->plane_definitions;
        delete [] convex_hull_configurable->plane_radius;
        delete [] convex_hull_configurable->plane;
        delete convex_hull_configurable;
    }
    else if (type == SRE_BOUNDING_VOLUME_PYRAMID) {
//        delete [] pyramid->plane;
        delete [] pyramid->vertex;
        delete pyramid;
    }
    else if (type == SRE_BOUNDING_VOLUME_PYRAMID_CONE)
        delete pyramid_cone;
    else if (type == SRE_BOUNDING_VOLUME_SPHERICAL_SECTOR)
        delete spherical_sector;
    else if (type == SRE_BOUNDING_VOLUME_HALF_CYLINDER)
        delete half_cylinder;
    else if (type == SRE_BOUNDING_VOLUME_CAPSULE)
        delete capsule;
    type = SRE_BOUNDING_VOLUME_UNDEFINED;
}

// Destructor for sreBoundingVolume.

sreBoundingVolume::~sreBoundingVolume() {
    Clear();
}

// Calculating a bounding volume of another bounding volume.

// Calculate the bounding sphere of a spherical sector (itself a bounding volume based on a
// sphere, but the spherical bounds can be much smaller than the spherical parameters of the
// sector).

void CalculateBoundingSphere(const sreBoundingVolumeSphericalSector& spherical_sector,
sreBoundingVolumeSphere& sphere) {
    // The radius of the bounding sphere will be less than the sector,
    // which means it will "curve around" the spherical cap of the sector,
    // so the side of the spherical cap will be the furthest.
    Vector2D furthest = spherical_sector.radius * Vector2D(spherical_sector.cos_half_angular_size,
        spherical_sector.sin_half_angular_size);
    float squared_dist = SquaredMag(furthest);
    // The light position provides the furthest distance at the other end. The
    // the bounding sphere center will be on the axis, and the distance to the
    // cap endpoint and the spherical sector center (light position) should be
    // the same. So
    //     sector.radius - sphere.center.x =
    //          sqrtf(sqrf(furthest.y) + sqrf(furthest.x - sphere_center.x))
    //     sqrf(sector.radius) - 2 * sphere.center.x * sector.radius + sqrf(sphere.center.x) =
    //          sqrf(furthest.y) + sqrf(furthest.x) - 2 * sphere_center.x * furthest.x +
    //          sqrf(sphere_center.x)
    //     2 * sphere_center.x * (furthest.x - sector.radius) = sqrf(furthest.y) +
    //         sqrf(furthest.x) - sqrf(sector.radius)
    //     sphere_center.x = 0.5f * (sqrf(furthest.y) + sqrf(furthest.x) - sqrf(sector.radius))
    //         / (furthest.x - sector.radius)
    //
//    sphere.radius = 0.5f * (squared_dist - sqrf(spherical_sector.radius)) / (furthest.x - spherical_sector.radius);
    sphere.radius = 0.5f * furthest.x;
    sphere.center = spherical_sector.center + spherical_sector.axis * sphere.radius;
}

// Calculate the AABB of a spherical sector.

void CalculateAABB(const sreBoundingVolumeSphericalSector& spherical_sector, sreBoundingVolumeAABB& AABB) {
        // The circular edge of the spherical cap, the entire top of the spherical cap, and the
        // spherical sector center/origin (the light position) have to be included when
        //calculating the AABB.
        // Add the center.
        AABB.dim_min = AABB.dim_max = spherical_sector.center;
        // Extend the AABB to include the top of the spherical cap. Since the spherical cap is part
        // of the sphere that the sector is based on, we calculate the angle (dot product) between
        // the axis and the AABB dimension vectors and using that to project the axis onto or into
        // the direction of the dimension vector, limiting the angle to the half angular size of
        // the sector (in which case the projection ends at the circular ring of the cap). 
        UpdateAABB(AABB, spherical_sector.center + spherical_sector.radius * ProjectOntoWithLimit(
            spherical_sector.axis, Vector3D(-1.0f, 0, 0), spherical_sector.cos_half_angular_size));
        UpdateAABB(AABB, spherical_sector.center + spherical_sector.radius * ProjectOntoWithLimit(
            spherical_sector.axis, Vector3D(1.0f, 0, 0), spherical_sector.cos_half_angular_size));
        UpdateAABB(AABB, spherical_sector.center + spherical_sector.radius * ProjectOntoWithLimit(
            spherical_sector.axis, Vector3D(0, -1.0f, 0), spherical_sector.cos_half_angular_size));
        UpdateAABB(AABB, spherical_sector.center + spherical_sector.radius * ProjectOntoWithLimit(
            spherical_sector.axis, Vector3D(0, 1.0f, 0), spherical_sector.cos_half_angular_size));
        UpdateAABB(AABB, spherical_sector.center + spherical_sector.radius * ProjectOntoWithLimit(
            spherical_sector.axis, Vector3D(0, 0, -1.0f), spherical_sector.cos_half_angular_size));
        UpdateAABB(AABB, spherical_sector.center + spherical_sector.radius * ProjectOntoWithLimit(
            spherical_sector.axis, Vector3D(0, 0, 1.0f), spherical_sector.cos_half_angular_size));
#if 0
        // This piece of code is unnecessary, the circular edge should already have been included
        // above when required.
        // Calculate the center and radius of the circular edge of the spherical cap.
        Point3D circle_center = spherical_sector.center + spherical_sector.axis *
            spherical_sector.radius * (1.0f - spherical_sector.cos_half_angular_size);
        float circle_radius = spherical_sector.radius * spherical_sector.sin_half_angular_size;
        // Update the AABB with the circle's minimum and maximum bounds in each dimension.
        UpdateAABB(AABB, circle_center + circle_radius * Vector3D(- 1.0f, 0, 0) *
            (1.0f - Dot(spherical_sector.axis, Vector3D(- 1.0f, 0, 0))));
        UpdateAABB(AABB, circle_center + circle_radius * Vector3D(1.0f, 0, 0) *
            (1.0f - Dot(spherical_sector.axis, Vector3D(1.0f, 0, 0))));
        UpdateAABB(AABB, circle_center + circle_radius * Vector3D(0, - 1.0f, 0) *
            (1.0f - Dot(spherical_sector.axis, Vector3D(0, - 1.0f, 0))));
        UpdateAABB(AABB, circle_center + circle_radius * Vector3D(0, 1.0f, 0) *
            (1.0f - Dot(spherical_sector.axis, Vector3D(0, 1.0f, 0))));
        UpdateAABB(AABB, circle_center + circle_radius * Vector3D(0, 0, - 1.0f) *
            (1.0f - Dot(spherical_sector.axis, Vector3D(0, 0, - 1.0f))));
        UpdateAABB(AABB, circle_center + circle_radius * Vector3D(0, 0, 1.0f) *
            (1.0f - Dot(spherical_sector.axis, Vector3D(0, 0, 1.0f))));
#endif
}

void CalculateAABB(const sreBoundingVolumeCylinder& cylinder, sreBoundingVolumeAABB& AABB) {
        AABB.dim_min = AABB.dim_max = cylinder.center;
        for (float factor = - 0.5f; factor < 1.0f; factor += 1.0f) {
            UpdateAABB(AABB, cylinder.center + factor * cylinder.length * cylinder.axis + cylinder.radius *
                Vector3D(-1.0f, 0, 0) * (1.0f - Dot(cylinder.axis, Vector3D(-1.0f, 0, 0))));
            UpdateAABB(AABB, cylinder.center + factor * cylinder.length * cylinder.axis + cylinder.radius *
                Vector3D(1.0f, 0, 0) * (1.0f - Dot(cylinder.axis, Vector3D(1.0f, 0, 0))));
            UpdateAABB(AABB, cylinder.center + factor * cylinder.length * cylinder.axis + cylinder.radius *
                Vector3D(0, -1.0f, 0) * (1.0f - Dot(cylinder.axis, Vector3D(0, -1.0f, 0))));
            UpdateAABB(AABB, cylinder.center + factor * cylinder.length * cylinder.axis + cylinder.radius *
                Vector3D(0, 1.0f, 0) * (1.0f - Dot(cylinder.axis, Vector3D(0, 1.0f, 0))));
            UpdateAABB(AABB, cylinder.center + factor * cylinder.length * cylinder.axis + cylinder.radius *
                Vector3D(0, 0, -1.0f) * (1.0f - Dot(cylinder.axis, Vector3D(0, 0, -1.0f))));
            UpdateAABB(AABB, cylinder.center + factor * cylinder.length * cylinder.axis + cylinder.radius *
                Vector3D(0, 0, 1.0f) * (1.0f - Dot(cylinder.axis, Vector3D(0, 0, 1.0f))));
       }
#if 0
        // Construct two spheres at the endpoints of the cylinder, and use the union of their AABBs.
        // Should be optimized to exclude half of the spheres at the endpoint.
        sreBoundingVolumeSphere endpoint_sphere;
        endpoint_sphere.center = cylinder.center - cylinder.length * cylinder.axis * 0.5;
        endpoint_sphere.radius = cylinder.radius;
        AABB.dim_min = endpoint_sphere.center - Vector3D(endpoint_sphere.radius, endpoint_sphere.radius,
            endpoint_sphere.radius);
        AABB.dim_max = endpoint_sphere.center + Vector3D(endpoint_sphere.radius, endpoint_sphere.radius,
            endpoint_sphere.radius);
        endpoint_sphere.center = cylinder.center - cylinder.length * cylinder.axis * 0.5;
        endpoint_sphere.radius = cylinder.radius;
        sreBoundingVolumeAABB AABB_endpoint2;
        AABB_endpoint2.dim_min = endpoint_sphere.center - Vector3D(endpoint_sphere.radius,
            endpoint_sphere.radius, endpoint_sphere.radius);
        AABB_endpoint2.dim_max = endpoint_sphere.center + Vector3D(endpoint_sphere.radius,
            endpoint_sphere.radius, endpoint_sphere.radius);
        UpdateAABB(AABB, AABB_endpoint2);
#endif
}

void CalculateBoundingSphere(const sreBoundingVolumeCylinder& cylinder, sreBoundingVolumeSphere& sphere) {
    // The center is the same as the cylinder, and the radius is the distance to the
    // edge of the cylinder cap.
    sphere.center = cylinder.center;
    sphere.radius = sqrtf(sqrf(0.5f * cylinder.length) + sqrf(cylinder.radius));
}

void CalculateBoundingCylinder(const sreBoundingVolumeSphericalSector& spherical_sector,
sreBoundingVolumeCylinder& cylinder) {
    // The cylinder length will be equal to the sector's radius (from top of the spherical cap to
    // the light position.
    cylinder.length = spherical_sector.radius;
    // We just need the distance to the axis of the sperical cap rim, which is the sine of
    // half of the angular size.
    cylinder.radius = spherical_sector.radius * spherical_sector.sin_half_angular_size;
    cylinder.center = spherical_sector.center + 0.5f * cylinder.length * spherical_sector.axis;
    cylinder.axis = spherical_sector.axis;
}

// Bounding box functions and data used for intersection hull calculation when using geometry
// scissors, and when constructing pyramid shadow volumes.

// The order of the bounding box vertices assigned can have implications
// later, for example when the angle between pyramid sides resulting from
// a projected bounding box may impact the accuracy of plane vector
// calculations.

void sreBoundingVolumeBox::ConstructVertices(Point3D *P, int& n) const {
    P[0] = GetCorner(0.5f, 0.5f, 0.5f);
    P[1] = GetCorner(- 0.5f, 0.5f, 0.5f);
    P[2] = GetCorner(- 0.5f, - 0.5f, 0.5f);
    P[3] = GetCorner(0.5f, - 0.5f, 0.5f);
    if (PCA[2].SizeIsZero()) {
        n = 4;
        return;
    }
    P[4] = GetCorner(0.5f, 0.5f, - 0.5f);
    P[5] = GetCorner(- 0.5f, 0.5f, - 0.5f);
    P[6] = GetCorner(- 0.5f, - 0.5f, - 0.5f);
    P[7] = GetCorner(0.5f, - 0.5f, - 0.5f);
    n = 8;
}

const int BB_plane_vertex[6][4] = {
    { 0, 3, 4, 7 },  // R positive
    { 1, 2, 5, 6 },  // R negative
    { 0, 1, 4, 5 },  // S positive
    { 3, 2, 7, 6 },  // S negative
    { 0, 1, 3, 2 },  // T positive
    { 4, 5, 7, 6 }   // T negative
}; 

const int flat_BB_plane_nu_vertices[6] = {
    2, 2, 2, 2, 0, 0 
};

const int BB_edge_vertex[12][2] = {
    { 1, 2 }, // Top left edge.
    { 0, 3 }, // Top right edge.
    { 1, 0 }, // Top far edge.
    { 2, 3 }, // Top near edge.
    { 5, 6 }, // Bottom left edge.
    { 4, 7 }, // Bottom right edge.
    { 5, 4 }, // Bottom far edge.
    { 6, 7 }, // Bottom near edge.
    { 6, 2 }, // Left side near edge.
    { 5, 1 }, // Left side far edge.
    { 7, 3 }, // Right side near edge.
    { 4, 0 }  // Right side far edge.
};

const int BB_edge_plane[12][2] = {
    { 1, 4 },
    { 0, 4 },
    { 2, 4 },
    { 3, 4 },
    { 1, 5 },
    { 0, 5 },
    { 2, 5 },
    { 3, 5 },
    { 1, 3 },
    { 1, 2 },
    { 0, 3 },
    { 0, 2 }
};
