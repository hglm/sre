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

// Bounding volume intersection tests.

#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>

#include "sre.h"
#include "sre_internal.h"
#include "sre_bounds.h"
#include "win32_compat.h"

// Intersection tests against a hull (a collection of vertex positions).

// Intersection of a (convex or not) hull with vertex information with a convex hull.

bool Intersects(const sreBoundingVolumeHull& h, const sreBoundingVolumeConvexHull& ch) {
    // For all planes of the convex hull.
    for (int i = 0; i < ch.nu_planes; i++) {
        // Check whether all vertices of the hull are outside the plane.
        int j = 0;
        bool inside = false;
        while (j < h.nu_vertices) {
            if (Dot(h.vertex[j], ch.plane[i]) > 0) {
                // When we find a vertex that is inside the plane, we can go on
                // to the next plane.
                inside = true;
                break;
            }
            j++;
        }
        // All vertices are outside the plane, so the hull is completely outside the
        // convex hull.
        if (!inside)
            return false;
    }
    // The hull is not completely outside any plane, so it must intersect the convex hull.
    return true;
}

// Intersection tests against a convex hull (a collection of planes that is convex).

// Intersection of a sphere and a convex hull. This test may miss some cases of non-intersection.

bool Intersects(const sreBoundingVolumeSphere& sphere, const sreBoundingVolumeConvexHull& ch) {
    for (int i = 0; i < ch.nu_planes; i++) {
        if (Dot(ch.plane[i], sphere.center) <= - sphere.radius)
            return false;
    }
    return true;
}

// Test with more information for a sphere and a convex hull. This test may return
// SRE_PARTIALLY_INSIDE in some cases when the sphere is actually completely outside.

static BoundsCheckResult QueryIntersection(const sreBoundingVolumeSphere& sphere,
const sreBoundingVolumeConvexHull& ch) {
    int count = 0;
    for (int i = 0; i < ch.nu_planes; i++) {
        float dot = Dot(ch.plane[i], sphere.center);
        if (dot <= - sphere.radius)
            return SRE_COMPLETELY_OUTSIDE;
        if (dot >= sphere.radius)
            count++;
    }
    if (count == ch.nu_planes)
        return SRE_COMPLETELY_INSIDE;
    return SRE_PARTIALLY_INSIDE;
}

// Intersection test of an ellipsoid and a convex hull.

static bool Intersects(const sreBoundingVolumeEllipsoid& ellipsoid,
const sreBoundingVolumeConvexHull& ch) {
    for (int i = 0; i < ch.nu_planes; i++) {
        float r_eff_squared = sqrf(Dot(ellipsoid.PCA[0].vector, ch.plane[i].GetVector3D())) +
            sqrf(Dot(ellipsoid.PCA[1].vector, ch.plane[i].GetVector3D())) +
            sqrf(Dot(ellipsoid.PCA[2].vector, ch.plane[i].GetVector3D()));
        float dist = Dot(ch.plane[i], ellipsoid.center);
        if (dist <= 0 && sqrf(dist) >= r_eff_squared)
            return false;
    }
    return true;
}

static bool Intersects(const sreBoundingVolumeLineSegment& segment,
const sreBoundingVolumeConvexHull &ch) {
    // Line segment bounds check.
    Point3D Q1 = segment.E1;
    Point3D Q2 = segment.E2;
    for (int i = 0; i < ch.nu_planes; i++) {
        float dot1 = Dot(ch.plane[i], Q1);
        float dot2 = Dot(ch.plane[i], Q2);
        if (dot1 <= 0 && dot2 <= 0)
            return false;
        if (i == ch.nu_planes - 1)
            return true;
        if (dot1 >= 0 && dot2 >= 0)
            continue;
        // We now have that one of the dot products is less than - r_eff and the other is
        // greater than - r_eff. Calculate the point Q3 such that Dot(K, Q3)  = - r_eff
        // and replace the exterior endpoint with it.
        Vector3D R = Q2 - Q1;
        float t = - dot1 / Dot(ch.plane[i], R);
        Vector3D Q3 = Q1 + t * R;
        if (dot1 <= 0)
            Q1 = Q3;
        else
            Q2 = Q3;
    }
    return true;
}

// Intersection test of a box against a convex hull. Uses either a line segment test (more
// accurate for a box extended in one direction) or a standard box test, depending on box.flags.
// Like most plane distance based tests, it may miss some cases of non-intersection.

static bool Intersects(const sreBoundingVolumeBox& box, const sreBoundingVolumeConvexHull &ch) {
    // Bounding box check.
    if (box.flags & SRE_BOUNDS_PREFER_BOX_LINE_SEGMENT) {
        // Line segment box bounds check.
        Point3D Q1 = box.center + box.PCA[0].vector * 0.5f;
        Point3D Q2 = box.center - box.PCA[0].vector * 0.5f;
        for (int i = 0; i < ch.nu_planes; i++) {
            float r_eff = (fabsf(Dot(box.PCA[1].vector, ch.plane[i].GetVector3D())) +
                fabsf(Dot(box.PCA[2].vector, ch.plane[i].GetVector3D()))) * 0.5f;
            float dot1 = Dot(ch.plane[i], Q1);
            float dot2 = Dot(ch.plane[i], Q2);
            if (dot1 <= - r_eff && dot2 <= - r_eff)
                return false;
            if (i == ch.nu_planes - 1)
                return true;
            if (dot1 >= - r_eff && dot2 >= - r_eff)
                continue;
            // We now have that one of the dot products is less than - r_eff and the other is
            // greater than - r_eff. Calculate the point Q3 such that Dot(K, Q3) = - r_eff
            // and replace the exterior endpoint with it.
            Vector3D R = Q2 - Q1;
            float t = - (r_eff + dot1) / Dot(ch.plane[i], R);
            Vector3D Q3 = Q1 + t * R;
            if (dot1 <= - r_eff)
                Q1 = Q3;
            else
                Q2 = Q3;
        }
        return true;
    }
    // Standard box check.
    for (int i = 0; i < ch.nu_planes; i++) {
       float r_eff = (fabsf(Dot(box.PCA[0].vector, ch.plane[i].GetVector3D())) +
           fabsf(Dot(box.PCA[1].vector, ch.plane[i].GetVector3D())) + 
           fabsf(Dot(box.PCA[2].vector, ch.plane[i].GetVector3D()))) * 0.5f;
       if (Dot(ch.plane[i], box.center) <= - r_eff)
           return false;
    }
    return true;
}

// Intersection of AABB against convex hull. It can miss some cases of non-intersection.

static bool Intersects(const sreBoundingVolumeAABB& AABB, const sreBoundingVolumeConvexHull &ch) {
    Point3D center = (AABB.dim_min + AABB.dim_max) * 0.5f;
    Vector3D dim = AABB.dim_max - AABB.dim_min;
    for (int i = 0; i < ch.nu_planes; i++) {
        float r_eff = (
            fabsf(dim.x * ch.plane[i].x) +
            fabsf(dim.y * ch.plane[i].y) +
            fabsf(dim.z * ch.plane[i].z)
            ) * 0.5f;
        // It is important that center is of type Point3D (w coordinate treated as 1).
        if (Dot(ch.plane[i], center) <= - r_eff)
            return false;
    }
    return true;
}

// Intersection of a spherical sector (defined by center, axis, radius (length) and angular size)
// with a convex hull.

bool Intersects(const sreBoundingVolumeSphericalSector &spherical_sector,
const sreBoundingVolumeConvexHull &ch) {
    for (int i = 0; i < ch.nu_planes; i++) {
        float r_eff_squared;
        // Calculate the angle between the plane normal (pointing inside the hull)
        // and the sector axis.
        float dot = Dot(ch.plane[i].GetVector3D(), spherical_sector.axis);
        // When the angle is less or equal to the half angular size of the sector,
        // the effective radius (the radius from the sector's center in the direction
        // of the plane) is precisely the spherical sector radius.
        if (dot >= spherical_sector.cos_half_angular_size)
            r_eff_squared = sqrf(spherical_sector.radius);
        else if (dot <= - spherical_sector.sin_half_angular_size)
            // When the angle is 90 degrees or more greater than the half angular size,
            // the effective radius is precisely zero.
            r_eff_squared = 0;
        else {
            // Otherwise, r_eff varies, determined by the difference in distance to the
            // plane between the side of the spherical cap at the exterior end of the sector
            // and the sector center, which is associated with the sine of the angle between the
            // plane normal and the plane delimiting the nearest side (which is the angle
            // between the plane normal and the axis minus the half angular size).
            //
            // We can simply project the plane normal onto the sector axis to get this distance.
            Vector3D V = ProjectOnto(ch.plane[i].GetVector3D(), spherical_sector.axis);
            // Scale by the sector radius and use the squared distance to avoid square root
            // calculation.
            r_eff_squared = sqrf(spherical_sector.radius) * SquaredMag(V);
        }
        float dist = Dot(ch.plane[i], spherical_sector.center);
        if (dist <= 0 && sqrf(dist) >= r_eff_squared)
            return false;
    }
    return true;
}

// Intersection of a cylinder with a convex hull. Because of the use of square root
// calculations, this test may be somewhat expensive.

bool Intersects(const sreBoundingVolumeCylinder& cyl, const sreBoundingVolumeConvexHull &ch) {
    Point3D Q1 = cyl.center - 0.5f * cyl.length * cyl.axis;
    Point3D Q2 = cyl.center + 0.5f * cyl.length * cyl.axis;
    for (int i = 0; i < ch.nu_planes; i++) {
        float dot1 = Dot(ch.plane[i], Q1);
        float dot2 = Dot(ch.plane[i], Q2);
        float r_eff = cyl.radius * sqrtf(1.0 - sqrf(Dot(cyl.axis, ch.plane[i].GetVector3D())));
        if (dot1 <= - r_eff && dot2 <= - r_eff)
            return false;
        if (dot1 >= - r_eff && dot2 >= - r_eff)
            continue;
        Vector3D R = Q2 - Q1;
        float t = - (r_eff + dot1) / Dot(ch.plane[i], R);
        Point3D Q3 = Q1 + t * R;
        if (dot1 <= - r_eff)
            Q1 = Q3;
        else
            Q2 = Q3;
    }
    return true;
}

// Intersection between a half cylinder (that extends to infinity at one end) and
// a convex hull. In practice this is used when testing shadow volumes produced by
// directional lights against the view frustum.

bool Intersects(const sreBoundingVolumeHalfCylinder& hc, const sreBoundingVolumeConvexHull &ch) {
    Point3D Q1 = hc.endpoint;
    Point3D Q2;
    bool infinite = true;
    for (int i = 0; i < ch.nu_planes; i++) {
        // Calculate the distance between the endpoint and the plane.
        float dot1 = Dot(ch.plane[i], Q1);
        float r_eff = hc.radius * sqrtf(1.0f - sqrf(Dot(hc.axis, ch.plane[i].GetVector3D())));
        if (infinite) {
            // The infinite end of the half-cylinder hasn't been clipped yet.
            // Take the dot product between the axis direction and the plane normal.
            float dot2 = Dot(ch.plane[i].GetVector3D(), hc.axis);
            if (dot1 <= - r_eff && dot2 <= 0)
                // The half cylinder is completely outside the plane and extends away from the plane.
                return false;
            if (dot1 >= - r_eff && dot2 >= 0)
                // The half cylinder is not completely outside the plane and extends inside.
                // No conclusions can be drawn. Note that any part of the half cylinder's cap may
                // still be outside the plane. However, we cannot easily clip off that part
                // without potentially impacting subsequent plane tests.
                continue;
            if (i == ch.nu_planes - 1)
                 break;
            // The half cylinder overlaps the plane. If it extends inside, the cap is completely
            // outside the plane. If it extends away from the plane, at least part of the cap is
            // inside the plane.
            // Use the normalized axis direction as a hypothetical second endpoint to calculate
            // the intersection with the frustum plane.
            // When the half cylinder extends away from plane (dot2 <= 0), and the endpoint is
            // just outside the plane (- r_eff < dot1 < 0), so that only up to half of the cap is
            // inside the plane, chopping off the infinite part results a very small cylinder
            // that has the exterior cap just outside of the plane, with the interior cap unchanged.
            // (in the direction of the cylinder axis) so that the cylinder is just outside the
            // plane.
            float t = - (r_eff + dot1) / dot2;
            Point3D Q3 = Q1 + t * hc.axis;
            if (dot1 < - r_eff)
                // It is guaranteed that dot2 > 0, so the cylinder extends inside of the plane.
                // At least the cap is completely outside the plane; move the endpoint so that
                // just the cap (and not more) remains outside the plane.
                Q1 = Q3;
            else {
                // Since dot1 >= - r_eff, it is guaranteed that dot2 < 0, so the cylinder is
                // extending away from the plane. Chop off the infinite part so that only
                // the exterior cap is still outside the plane.
                Q2 = Q3;
                infinite = false;
            }
        }
        else {
            // We have a finite cylinder.
            float dot2 = Dot(ch.plane[i], Q2);
            if (dot1 <= - r_eff && dot2 <= - r_eff) {
                // The cylinder is completely outside the plane.
                return false;
            }
            if (dot1 >= - r_eff && dot2 >= - r_eff)
                // No conclusions can be drawn. Just the cylinder's exterior cap, but not more,
                // may be outside the plane. At least the whole cylinder up the exterior cap
                // is inside the plane (most likely the whole cylinder is inside the plane).
                continue;
            if (i == ch.nu_planes - 1)
                 break;
            // At least the whole exterior cap is outside plane. Chop off part of the cylinder
            // so that only the exterior cap is still outside the plane.
            Vector3D R = Q2 - Q1;
            float t = - (r_eff + dot1) / Dot(ch.plane[i].GetVector3D(), R);
            Point3D Q3 = Q1 + t * R;
            // Replace the exterior endpoint.
            if (dot1 < - r_eff)
                // It is guaranteed that dot2 > - r_eff, so Q1 is the exterior endpoint.
                Q1 = Q3;
            else
                Q2 = Q3;
        }
    }
    return true;
}

// Intersection test for a full convex hull with plane, center and plane radius information
// (this includes the sreBoundingVolumePyramid type) against a target convex hull (which does
// not need anything more than plane vectors, so may be a basic convex hull).
// The number of iterations required to determine definite intersection can be fairly large
// (number of target convex_hull planes * number of source convex hull planes). The target convex
// hull planes are iterated; when the convex hulls do not intersect, the test exits when
// the first target convex hull plane is encountered for which the (source) convex hull is fully
// outside.
//
// In practice, this is used to test intersection of a point/spot light shadow volume (pyramid)
// against the view frustum.

bool Intersects(const sreBoundingVolumeConvexHullFull& ch1,
const sreBoundingVolumeConvexHull& ch2) {
    // For each target convex hull plane, check whether the source convex hull is completely outside.
    for (int i = 0; i < ch2.nu_planes; i++) {
        // Calculate the distance between the source convex hull center and the target
        // convex hull plane.
        float dist = Dot(ch2.plane[i], ch1.center);
        if (dist > - ch1.min_radius)
            // The source convex hull is definitely at least partially inside the target
            // convex hull plane, so we can skip it.
            continue;
        if (dist <= - ch1.max_radius)
            // The source convex hull is definitely completely outside the target convex hull plane.
            return false;
        // At this point, we know the center of the source convex hull is outside the target
        // convex hull plane, but the source convex hull may still intersect the plane.
        // For each plane of the source convex hull, calculate the contribution to the effective
        // radius with respect to the target convex hull plane.
        float r_eff = 0;
        for (int j = 0; j < ch1.nu_planes; j++) {
            // Calculate the cosine of the angle between the target convex hull plane and the
            // source convex hull plane.
            float dot = Dot(ch2.plane[i].GetVector3D(), ch1.plane[j].GetVector3D());
            if (dot < 0)
                // The angle between the normal vectors of the two planes is greater than
                // 90 degrees, which means the source convex hull plane contributes to the
                // effective radius of the source convex hull with respect to the target
                // convex hull plane. Since dot is a negative value, subtract it multiplied
                // by the source plane radius (which is the distance from the source convex
                // hull center) so that the effective radius is increased with the relative
                // contribution from the source convex hull plane. 
                r_eff -= ch1.plane_radius[j] * dot;
        }
        if (dist <= - r_eff)
            // The source convex hull is completely outside the target convex hull plane.
            return false;
    }
    return true;
}

// Check whether the projection of the base plane of a pyramid (actually pyramid cone) with only vertex
// information and a normalized axis direction to infinity intersects with a frustum (without a far
// plane of course, otherwise the test would always return false). The half angular size of the pyramid
// cone is guaranteed to be less than 90 degrees.

bool Intersects(const sreBoundingVolumeInfinitePyramidBase& pyramid_cone,
const sreBoundingVolumeFrustum& fr, float cos_max_half_angular_size, float sin_max_half_angular_size) {
    // When the angle of the pyramid cone base plane normal with the near frustum plane is within the
    // upper bound of the frustum's angular size (in a corner), the pyramid base is guaranteed
    // to be inside the frustum.
    float cos_near_plane_angle = Dot(fr.plane[0].GetVector3D(), - pyramid_cone.axis);
    if (cos_near_plane_angle > cos_max_half_angular_size)
        return true;
    // Because we don't have the sine of the pyramid base angle, it's hard to compare
    // angles, and we must assume the pyramid's half angular size can be up to 90 degrees,
    // so the near plane angle must be > 90 degrees + the frustum's half angular size,
    // using cos(a + 90 degrees) = - sin(a). However, when the pyramid's half angular
    // size is smaller than 90 degrees - the frustum's half angular size, we can do better,
    // using cos(90 degrees - a) = sin(a).
    if (pyramid_cone.cos_half_angular_size <= sin_max_half_angular_size) {
        // In this case, the pyramid cannot intersect with the frustum when its base plane
        // normal points outside the near plane (near plane angle >= 90 degrees).
        if (cos_near_plane_angle < 0)
            return false;
    }
    else if (cos_near_plane_angle < - sin_max_half_angular_size)
        // The pyramid base is guaranteed to be entirely outside the near plane.
        return false;
    
    // Note that because a pyramid cone guarantees that all side edges
    // of the pyramid are the same length, its angular size was calculated without normalization.

#if 0
    // Since the frustum is symmetrical, the angle between the far plane and the side planes
    // is the same for each pair opposite side planes (left/right, top/bottom).
    // We can check the angle between the unnormalized pyramid base normal and the angle of
    // the left and right, and top and bottom planes, selecting either the left or right, and
    // either the bottom or top plane (depending on the orientation of the base plane normal).
    // We calculate the minimum dot product (max angle) between the unnormalized pyramid base normal
    // and each frustum side plane pair. Since the frustum normals are pointed inwards, the biggest
    // angle will represent the side within a frustum plane pair of the frustum that the pyramid
    // base can be oriented outside of. Because the angular extents of the pyramid are not yet
    // checked, we can't yet discriminate between left/right vs. bottom/top.
    int plane[2];
    // Select which one of the left and right side planes of the frustum the pyramid's base
    // can be outside of.
    float plane_dot_left_right = Dot(fr.plane[1].GetVector3D(), - pyramid_cone.axis);
    float dot = Dot(fr.plane[2].GetVector3D(), - pyramid_cone.axis);
    if (dot < plane_dot_left_right) {
        plane_dot_left_right = dot;
        plane[0] = 2;
    }
    else
        plane[0] = 1;
    // Select which one of the left and right side planes of the frustum the pyramid's base
    // can be outside of.
    float plane_dot_bottom_top = Dot(fr.plane[3].GetVector3D(), - pyramid_cone.axis);
    dot =  Dot(fr.plane[4].GetVector3D(), - pyramid_cone.axis);
    if (dot < plane_dot_bottom_top) {
        plane_dot_bottom_top = dot;
        plane[1] = 4;
    }
    else
        plane[1] = 3;
    // To do.
#endif

#if 1
//    printf("Half angular size of pyramid cone = %.1f (cos = %f)\n", acosf(pyramid_cone.cos_half_angular_size)
//        * 180.0f / M_PI, pyramid_cone.cos_half_angular_size);

    // We can check whether any of the lines representing these edges go outside frustum; if any line does
    // not go outside the frustum then the infinite pyramid is inside the frustum.
    // However, it is possible that the pyramid base "encloses" the frustum side planes.

    // When considering the angle between a line to infinity and a frustum side plane normal,
    // the line will go outside the plane when the angle is greater than 90 degrees.
    // Keep mask of whether any edge of the pyramid is outside each of the four frustum side planes.
    int pyramid_partially_outside_plane_mask = 0;
    int pyramid_completely_outside_plane_mask = 0xF;
    for (int i = 1; i < pyramid_cone.nu_vertices; i++) {
        Vector3D E = pyramid_cone.vertex[i] - pyramid_cone.vertex[0];
        int edge_inside_plane_mask = 0;
        // For each other frustum plane (we only check the side planes, we don't want to check the
        // far plane).
        for (int j = 1; j < 5; j++) {
            if (Dot(fr.plane[j].GetVector3D(), E) > 0)
                edge_inside_plane_mask |= 1 << (j - 1);
        }
        if (edge_inside_plane_mask == 0xF)
            // The infinite projection of the edge is inside the frustum.
            return true;
         // Update the mask indicating whether the pyramid is partially outside each frustum plane.
         pyramid_partially_outside_plane_mask |= (edge_inside_plane_mask ^ 0xF);
         // Update the mask indicating whether the pyramid is completely outside any frustum plane.
         pyramid_completely_outside_plane_mask &= (edge_inside_plane_mask ^ 0xF);
    }
    // Check whether the pyramid is entirely outside one of the side planes.
    if (pyramid_completely_outside_plane_mask != 0) {
//        printf("Pyramid cone completely outside frustum plane (mask = %d)\n",
//            pyramid_completely_outside_plane_mask);
        return false;
    }
    // Detect whether the pyramid base "encloses" either pair of frustum side planes.
//    if ((pyramid_partially_outside_plane_mask & 0x3) == 0x3 ||
//    (pyramid_partially_outside_plane_mask & 0xC) == 0xC)
//        return true;
//    else
//        return false;
    return true;
#else
    return true;
#endif
}

// Intersection of the infinite projection of a spherical sector with a frustum (without far plane).

bool Intersects(const sreBoundingVolumeInfiniteSphericalSector& spherical_sector,
const sreBoundingVolumeFrustum& fr, float cos_max_half_angular_size, float sin_max_half_angular_size) {
    float cos_near_plane_angle = Dot(fr.plane[0].GetVector3D(), spherical_sector.axis);
    // If the angle between the near plane normal and the spherical sector axis is smaller than
    // the frustum half angular size, then the infinite sector projection certainly intersects the
    // frustum.
    if (cos_near_plane_angle > cos_max_half_angular_size)
        return true;
    // To be fully implemented.
    return true;
}

#if 0

// Check whether the projection of the base plane of a pyramid (with defined base plane) to
// infinity intersects with a frustum (without a far plane of course, otherwise the test would
// always return false). The plane representing the base of the pyramid is the
// last defined plane. Only the normal vector of the pyramid base plane is used, so the
// pyramid data structure can be the same as the regular pyramid with a finite base plane.

bool Intersects(const sreBoundingVolumeInfinitePyramidBase& pyramid,
const sreBoundingVolumeFrustum& fr) {
    // We need plane information.
    if (pyramid.nu_planes == 0)
        return true;
    // First check the first plane of the frustum (near plane) to test whether the infinite
    // base is in front it (definitely not visible).
    if (Dot(fr.plane[0].GetVector3D(), pyramid.plane[pyramid.nu_planes - 1].GetVector3D()) > 0)
        return false;
    // For each other frustum plane (we only check the side planes, we don't want to check the
    // far plane).
    for (int i = 1; i < 5; i++) {
        // If the angle between the base normal vector and the frustum plane is smaller than
        // 45 degrees (i.e. cosine is greater than cos(45 degrees)), then the infinite base
        // plane is outside the frustum plane and the distance is minus infinity.
        if (Dot(fr.plane[i].GetVector3D(), pyramid.plane[pyramid.nu_planes - 1].GetVector3D())
        > cosf(M_PI / 8.0f))
            return false;
    }
    return true;
}

#endif

// Intersection tests against a sphere.

// When the following function returns SRE_COMPLETELY_INSIDE, it means that the sphere with
// the smallest radius is inside the other sphere.

static inline BoundsCheckResult QueryIntersectionUnified(const sreBoundingVolumeSphere& sphere1,
const sreBoundingVolumeSphere& sphere2) {
    float dist_squared = SquaredMag(sphere1.center - sphere2.center);
    if (dist_squared >= sqrf(sphere1.radius + sphere2.radius))
        // The two spheres do not intersect.
        return SRE_COMPLETELY_OUTSIDE;
    if (dist_squared <= sqrf(sphere2.radius - sphere1.radius))
        return SRE_COMPLETELY_INSIDE;
    return SRE_PARTIALLY_INSIDE;
}

// Test that seperates between which sphere is inside the other.

static inline BoundsCheckResult QueryIntersection(const sreBoundingVolumeSphere& sphere1,
const sreBoundingVolumeSphere& sphere2) {
    float dist_squared = SquaredMag(sphere1.center - sphere2.center);
    if (dist_squared >= sqrf(sphere1.radius + sphere2.radius))
        // The two spheres do not intersect.
        return SRE_COMPLETELY_OUTSIDE;
    if (dist_squared <= sqrf(sphere2.radius - sphere1.radius)) {
        if (sphere1.radius <= sphere2.radius)
            return SRE_COMPLETELY_INSIDE;
        else
            return SRE_COMPLETELY_ENCLOSES;
    }
    return SRE_PARTIALLY_INSIDE;
}

// Determine intersection of a box with plane information with a sphere. Due to the nature of
// the test, some non-intersections may be missed especially when the box is small and the
// sphere is large.

bool Intersects(const sreBoundingVolumeBox& box, const sreBoundingVolumeSphere& sphere) {
    for (int i = 0; i < 6; i++)
        if (Dot(box.plane[i], sphere.center) <= - sphere.radius)
            return false;
    return true;
}

// Intersection test of a box and a sphere that also returns whether the box
// is completely or partially inside the sphere. It may stil miss some cases
// when the box is completely outside the sphere (it returns partially inside
// in those cases).

BoundsCheckResult QueryIntersection(const sreBoundingVolumeBox& box,
const sreBoundingVolumeSphere& sphere) {
    if (!Intersects(box, sphere))
        return SRE_COMPLETELY_OUTSIDE;
    // Check whether all corners of the box in inside the sphere.
    if (!Intersects(box.GetCorner(0.5f, 0.5f, 0.5f), sphere))
        return SRE_PARTIALLY_INSIDE;
    if (!Intersects(box.GetCorner(- 0.5f, 0.5f, 0.5f), sphere))
        return SRE_PARTIALLY_INSIDE;
    if (!Intersects(box.GetCorner(0.5f, - 0.5f, 0.5f), sphere))
        return SRE_PARTIALLY_INSIDE;
    if (!Intersects(box.GetCorner(- 0.5f, - 0.5f, 0.5f), sphere))
        return SRE_PARTIALLY_INSIDE;
    if (!Intersects(box.GetCorner(0.5f, 0.5f, - 0.5f), sphere))
        return SRE_PARTIALLY_INSIDE;
    if (!Intersects(box.GetCorner(- 0.5f, 0.5f, - 0.5f), sphere))
        return SRE_PARTIALLY_INSIDE;
    if (!Intersects(box.GetCorner(0.5f, - 0.5f, - 0.5f), sphere))
        return SRE_PARTIALLY_INSIDE;
    if (!Intersects(box.GetCorner(- 0.5f, - 0.5f, - 0.5f), sphere))
        return SRE_PARTIALLY_INSIDE;
    return SRE_COMPLETELY_INSIDE;
}

// Quick intersection test of an AABB against a sphere. Uses the AABB of the sphere.
// Can miss some cases of non-intersection.

static bool Intersects(const sreBoundingVolumeAABB& AABB, const sreBoundingVolumeSphere& sphere) {
    if (- sphere.center.x + AABB.dim_max.x <= - sphere.radius)  // x-positive AABB plane.
        return false;
    if (sphere.center.x - AABB.dim_min.x <= - sphere.radius)    // x-negative.
        return false;
    if (- sphere.center.y + AABB.dim_max.y <= - sphere.radius)  // y-positive,
        return false;
    if (sphere.center.y - AABB.dim_min.y <= - sphere.radius)    // y-negative.
        return false;
    if (- sphere.center.z + AABB.dim_max.z <= - sphere.radius)  // z-positive,
        return false;
    if (sphere.center.z - AABB.dim_min.z <= - sphere.radius)    // z-negative.
        return false;
    return true;
}

// More detailed intersection test of an AABB against a sphere. Returns more exact information,
// and will detect more non-intersections than the function above.

static BoundsCheckResult QueryIntersection(const sreBoundingVolumeAABB& AABB,
const sreBoundingVolumeSphere& sphere) {
    // First perform a rough test using the sphere's AABB. This will catch AABB's that are
    // definitely completely outside of the sphere.
    if (!Intersects(AABB, sphere))
       return SRE_COMPLETELY_OUTSIDE;
    // Check whether each of the corners of the AABB is inside the sphere. To accurately discriminate
    // between partially inside, completely inside, and completely outside, all corners have to be
    // checked.
    int intersection_count;
    intersection_count = Intersects(Point3D(AABB.dim_min.x, AABB.dim_min.y, AABB.dim_min.z), sphere);
    intersection_count += Intersects(Point3D(AABB.dim_max.x, AABB.dim_min.y, AABB.dim_min.z), sphere);
    intersection_count += Intersects(Point3D(AABB.dim_min.x, AABB.dim_max.y, AABB.dim_min.z), sphere);
    intersection_count += Intersects(Point3D(AABB.dim_max.x, AABB.dim_max.y, AABB.dim_min.z), sphere);
    intersection_count += Intersects(Point3D(AABB.dim_min.x, AABB.dim_min.y, AABB.dim_max.z), sphere);
    intersection_count += Intersects(Point3D(AABB.dim_max.x, AABB.dim_min.y, AABB.dim_max.z), sphere);
    intersection_count += Intersects(Point3D(AABB.dim_min.x, AABB.dim_max.y, AABB.dim_max.z), sphere);
    intersection_count += Intersects(Point3D(AABB.dim_max.x, AABB.dim_max.y, AABB.dim_max.z), sphere);
    if (intersection_count == 8)
        return SRE_COMPLETELY_INSIDE;
    if (intersection_count != 0)
        return SRE_PARTIALLY_INSIDE;
    // When the corner intersection count is zero, we can't yet discriminate between partially
    // inside and completely outside, because the whole AABB might enclose the sphere, or
    // one or more faces of the AABB might intersect the sphere.
    // However, at this point if the AABB is completely outside the sphere, at least one of its
    // corners must be inside the border area between the sphere's AABB and the sphere
    // itself, since we earlier ensured that the ABBB intersects with the sphere's AABB. In this
    // case, all of the AABB's vertices must be in the same octant relative to the sphere's
    // center, because otherwise one of the edges of the AABB (leading from one of the corners that
    // is inside the border area) would intersect with the sphere.
    // In other words, if the sphere's center coordinates are outside the AABB for all coordinate
    // dimensions the AABB is completely outside. If the AABB encloses the sphere or one or faces
    // intersect with it (without one corner being inside the sphere), the sphere's center must be
    // inside the AABB for at least one coordinate dimension.
    if (sphere.center.x >= AABB.dim_min.x && sphere.center.x <= AABB.dim_max.x)
        return SRE_PARTIALLY_INSIDE;
    if (sphere.center.y >= AABB.dim_min.y && sphere.center.y <= AABB.dim_max.y)
        return SRE_PARTIALLY_INSIDE;
    if (sphere.center.z >= AABB.dim_min.z && sphere.center.z <= AABB.dim_max.z)
        return SRE_PARTIALLY_INSIDE;
    return SRE_COMPLETELY_OUTSIDE;
}

// Intersection tests against a cylinder.

static bool Intersects(const Point3D& P, const sreBoundingVolumeCylinder& cyl) {
    // Calculate the distance from P to the line defined by the cylinder's axis.
    float dist_Q_S_squared = SquaredMag(P - cyl.center);
    float d_squared = dist_Q_S_squared - sqrf(Dot(P - cyl.center, cyl.axis));
    if (d_squared >= sqrf(cyl.radius))
        return false;
    // Check whether the point is completely outside the bottom/top planes.
    float dist1 = Dot(P, Vector4D(cyl.axis, - Dot(cyl.axis, cyl.center -
        0.5f * cyl.length * cyl.axis)));
    if (dist1 <= 0)
        return false;
    float dist2 = Dot(P, Vector4D(- cyl.axis, - Dot(- cyl.axis, cyl.center +
        0.5f * cyl.length * cyl.axis)));
    if (dist2 <= 0)
        return false;
    return true;
}

// Intersection test of a sphere and a cylinder. It is an accurate test, and should
// be fairly quick.

static bool Intersects(const sreBoundingVolumeSphere& sphere, const sreBoundingVolumeCylinder& cyl) {
    // Calculate the distance from sphere's center to the line defined by the cylinder's axis.
    float dist_Q_S_squared = SquaredMag(sphere.center - cyl.center);
    float d_squared = dist_Q_S_squared - sqrf(Dot(sphere.center - cyl.center, cyl.axis));
    if (d_squared >= sqrf(sphere.radius + cyl.radius)) {
        // The sphere and the cylinder do not intersect.
        return false;
    }
    // Check whether the sphere is completely outside the bottom/top planes.
    float dist1 = Dot(sphere.center, Vector4D(cyl.axis, - Dot(cyl.axis, cyl.center -
        0.5f * cyl.length * cyl.axis)));
    if (dist1 <= - sphere.radius)
        return false;
    float dist2 = Dot(sphere.center, Vector4D(- cyl.axis, - Dot(- cyl.axis, cyl.center +
        0.5f * cyl.length * cyl.axis)));
    if (dist2 <= - sphere.radius)
        return false;
    return true;
}

// Intersection test of a sphere against a cylinder giving more detailed information.
// It should be accurate.

BoundsCheckResult QueryIntersection(const sreBoundingVolumeSphere& sphere,
const sreBoundingVolumeCylinder& cyl) {
    // Calculate the distance from sphere's center to the line defined by the cylinder's axis.
    float dist_Q_S_squared = SquaredMag(sphere.center - cyl.center);
    float d_squared = dist_Q_S_squared - sqrf(Dot(sphere.center - cyl.center, cyl.axis));
    if (d_squared >= sqrf(sphere.radius + cyl.radius)) {
        // The sphere and the cylinder do not intersect.
        return SRE_COMPLETELY_OUTSIDE;
    }
    // Check whether is sphere is completely outside the bottom and top planes.
    float dist1 = Dot(sphere.center, Vector4D(cyl.axis, - Dot(cyl.axis, cyl.center
         - 0.5f * cyl.length * cyl.axis)));
    if (dist1 <= - sphere.radius)
        return SRE_COMPLETELY_OUTSIDE;
    float dist2 = Dot(sphere.center, Vector4D(- cyl.axis, - Dot(- cyl.axis, cyl.center
         + 0.5f * cyl.length * cyl.axis)));
    if (dist2 <= - sphere.radius)
        return SRE_COMPLETELY_OUTSIDE;
    // We can now be sure that the sphere is at least partially inside the cylinder.
    // Now check whether the sphere is partially (not completely) inside in the top
    // and bottom planes. Note that we use < instead of <=, because if the sphere is
    // positioned against the exterior of the cylinder, we want to return
    // SRE_COMPLETELY_INSIDE if possible.
    if (dist1 < sphere.radius || dist2 < sphere.radius)
        return SRE_PARTIALLY_INSIDE;
    // If the sphere is completely inside the two planes, also check whether it is
    // completely within the cylinder radius.
    if (sphere.radius <= cyl.radius && d_squared <= sqrf(cyl.radius - sphere.radius))
        return SRE_COMPLETELY_INSIDE;
    return SRE_PARTIALLY_INSIDE;
}

// The following intersection tests for an oriented box or an AABB against a cylinder
// uses the plane by plane approach, clipping the cylinder against each plane of the box
// in succession, and exiting when the clipped cylinder is found to be completely outside
// a box plane.
//
// For an oriented box, one disadvantage is the use of a square root operation before
// every pair of planes.

bool Intersects(const sreBoundingVolumeBox& box, const sreBoundingVolumeCylinder& cyl) {
    Point3D Q1 = cyl.center - 0.5f * cyl.length * cyl.axis;
    Point3D Q2 = cyl.center + 0.5f * cyl.length * cyl.axis;
    // Process all six PCA planes of the box in pairs.
    for (int i = 0; i < 6; i += 2) {
        // Calculate the distances of both cylinder endpoints to the plane.
        float dot1 = Dot(box.plane[i], Q1);
        float dot2 = Dot(box.plane[i], Q2);
        float t;
        Point3D Q3;
        Vector3D R;
        float r_eff;
        r_eff = cyl.radius * sqrtf(1.0f - sqrf(Dot(cyl.axis, box.plane[i].GetVector3D())));
        if (dot1 <= - r_eff && dot2 <= - r_eff)
            return false;
        if (dot1 >= - r_eff && dot2 >= - r_eff)
            goto next_plane;
        R = Q2 - Q1;
        t = - (r_eff + dot1) / Dot(box.plane[i].GetVector3D(), R);
        Q3 = Q1 + t * R;
        if (dot1 <= - r_eff)
            Q1 = Q3;
        else
            Q2 = Q3;
next_plane :
        // Have to detect a special case for flat boxes with a T dimension of zero.
        // In this case the second T plane vector's normal and distance will be equal
        // to the negation of the first T plane (the planes overlap, but the normal
        // is reversed).
        if (i == 4 && box.plane[i] == - box.plane[i + 1]) {
            // We can reuse the distances calculated for the first T plane.
            // The cylinder may have been clipped, but we only need to check whether
            // the cylinder is completely on the outside the second T plane.
            // If the cylinder was clipped, it is certain that there is an intersection
            // with the flat plane; one of dot1 and dot2 would have been
            // < - r_eff, and the other would be > - r_eff, and the clipping will
            // have resulted in moving the distance of the exterior endpoint to
            // - r_eff. If the cylinder was not clipped, both distances must
            // already be >= - r_eff. For the cylinder to be completely outside the
            // second T plane, both distances (relative to the first plane) must
            // actually be >= r_eff.
            if (dot1 >= r_eff && dot2 >= r_eff)
                return false;
            return true;
        }
        // Calculate the distances of both cylinder endpoints to the plane.
        dot1 = Dot(box.plane[i + 1], Q1);
        dot2 = Dot(box.plane[i + 1], Q2);
        if (dot1 <= - r_eff && dot2 <= - r_eff) {
            return false;
        }
        if (i == 4)
            // We we have reached the last plane, there is no need for further
            // processing.
            break;
        if (dot1 >= - r_eff && dot2 >= - r_eff)
            continue;
        R = Q2 - Q1;
        t = (r_eff + dot1) / Dot(box.plane[i + 1].GetVector3D(), R);
        Q3 = Q1 + t * R;
        if (dot1 <= - r_eff)
            Q1 = Q3;
        else
            Q2 = Q3;
    }
    return true;
}

// Similar to the box test, but for AABBs. Precalculated cylinder axis coefficients can
// be used, avoiding square root calculations. The loop has been unrolled for each AABB
// plane. Although the code does not fold some contants (for example multiplication
// with vectors containing only + or - 1.0 or 0 can be largely eliminated), an optimizing
// compiler should be able to handle this.

bool Intersects(const sreBoundingVolumeAABB& AABB, const sreBoundingVolumeCylinder& cyl) {
        Point3D Q1 = cyl.center - 0.5f * cyl.length * cyl.axis;
        Point3D Q2 = cyl.center + 0.5f * cyl.length * cyl.axis;
        Vector4D box_plane;
        box_plane.Set(1.0f, 0, 0, - AABB.dim_min.x);
        float dot1 = Dot(box_plane, Q1);
        float dot2 = Dot(box_plane, Q2);
        float t;
        Point3D Q3;
        Vector3D R;
        float r_eff;
#if 1
        // Use precalculated axis coefficient, equal to sqrtf(1.0f - sqr(axis.x)).
        r_eff = cyl.radius * cyl.axis_coefficients.x;
#else
        r_eff = cyl.radius * sqrtf(1.0f - sqrf(Dot(cyl.axis, box_plane.GetVector3D())));
#endif
        if (dot1 <= - r_eff && dot2 <= - r_eff)
            return false;
        if (dot1 >= - r_eff && dot2 >= - r_eff)
            goto plane1;
        R = Q2 - Q1;
        // Calculate t as an offset from Q1 in terms of the distance between the
        // two endpoints, but corrected for the plane's orientation (the cylinder
        // endpoint has to be moved less as the angle between the cylinder axis
        // and the plane decreases from a maximum of 90 degrees), precisely so
        // that the new point will be at a distance - r_eff from the plane, which
        // is equal to offsetting Q1 by a distance of (- r_eff - dot1).
        t = - (r_eff + dot1) / Dot(box_plane.GetVector3D(), R);
        // Create Q3 so that distance from plane is - r_eff.
        Q3 = Q1 + t * R;
        if (dot1 <= - r_eff)
            Q1 = Q3;
        else
            Q2 = Q3;
plane1 :
        box_plane.Set(- 1.0f, 0, 0, AABB.dim_max.x);
        dot1 = Dot(box_plane, Q1);
        dot2 = Dot(box_plane, Q2);
        if (dot1 <= - r_eff && dot2 <= - r_eff) {
            return false;
        }
        if (dot1 >= - r_eff && dot2 >= - r_eff)
            goto plane2;
        R = Q2 - Q1;
        t = - (r_eff + dot1) / Dot(box_plane.GetVector3D(), R);
        Q3 = Q1 + t * R;
        if (dot1 <= - r_eff)
            Q1 = Q3;
        else
            Q2 = Q3;
plane2: 
        box_plane.Set(0, 1.0f, 0, - AABB.dim_min.y);;
        dot1 = Dot(box_plane, Q1);
        dot2 = Dot(box_plane, Q2);
#if 1
        r_eff = cyl.radius * cyl.axis_coefficients.y;
#else
        r_eff = cyl.radius * sqrtf(1.0f - sqrf(Dot(cyl.axis, box_plane.GetVector3D())));
#endif
        if (dot1 <= - r_eff && dot2 <= - r_eff)
            return false;
        if (dot1 >= - r_eff && dot2 >= - r_eff)
            goto plane3;
        R = Q2 - Q1;
        t = - (r_eff + dot1) / Dot(box_plane.GetVector3D(), R);
        Q3 = Q1 + t * R;
        if (dot1 <= - r_eff)
            Q1 = Q3;
        else
            Q2 = Q3;
plane3 :
        box_plane.Set(0, - 1.0f, 0, AABB.dim_max.y);
        dot1 = Dot(box_plane, Q1);
        dot2 = Dot(box_plane, Q2);
        if (dot1 <= - r_eff && dot2 <= - r_eff) {
            return false;
        }
        if (dot1 >= - r_eff && dot2 >= - r_eff)
            goto plane4;
        R = Q2 - Q1;
        t = - (r_eff + dot1) / Dot(box_plane.GetVector3D(), R);
        Q3 = Q1 + t * R;
        if (dot1 <= - r_eff)
            Q1 = Q3;
        else
            Q2 = Q3;
plane4: 
        box_plane.Set(0, 0, 1.0f, - AABB.dim_min.z);
        dot1 = Dot(box_plane, Q1);
        dot2 = Dot(box_plane, Q2);
#if 1
        r_eff = cyl.radius * cyl.axis_coefficients.z;
#else
        r_eff = cyl.radius * sqrtf(1.0f - sqrf(Dot(cyl.axis, box_plane.GetVector3D())));
#endif
        if (dot1 <= - r_eff && dot2 <= - r_eff)
            return false;
        if (dot1 >= - r_eff && dot2 >= - r_eff)
            goto plane5;
        R = Q2 - Q1;
        t = - (r_eff + dot1) / Dot(box_plane.GetVector3D(), R);
        Q3 = Q1 + t * R;
        if (dot1 <= - r_eff)
            Q1 = Q3;
        else
            Q2 = Q3;
plane5 :
        Vector4D box_plane5;
        box_plane5.Set(0, 0, - 1.0f, AABB.dim_max.z);
        dot1 = Dot(box_plane5, Q1);
        dot2 = Dot(box_plane5, Q2);
        // Have to detect a special case for boxes with a T dimension of zero.
        if (box_plane.w == - box_plane5.w && box_plane.GetVector3D() == - box_plane5.GetVector3D()) {
            if (dot1 < - r_eff && dot2 < - r_eff)
                return false;
           return true;
        }
        if (dot1 <= - r_eff && dot2 <= - r_eff) {
            return false;
        }
        return true;
}

// Bounding tests against a spherical sector (used for spot light bounding volumes).

// Test whether a point is inside a spherical sector.

static bool Intersects(const Point3D& P, const sreBoundingVolumeSphericalSector &spherical_sector) {
    // First perform a few basic plane checks.
    // Calculate the plane through the sector center with axis as normal.
    Vector4D K = Vector4D(spherical_sector.axis, - Dot(spherical_sector.axis, spherical_sector.center));
    // Calculate the distance of the point to the plane.
    float d_plane = Dot(K, P);
    if (d_plane <= 0 || d_plane >= spherical_sector.radius)
        // If the point is completely outside the plane, or it is completely beyond the radius
        // of the sector, it cannot intersect the sector.
        return false;
    Vector3D position_vector = P - spherical_sector.center;
    if (d_plane > spherical_sector.cos_half_angular_size * spherical_sector.radius) {
        // When the point is further from the plane than the spherical cap endpoint it may be in
        // the space just beyond the spherical cap (but inside the plane at the sector radius).
        // For this case simply perform a sphere test.
        if (SquaredMag(position_vector) > spherical_sector.radius)
            return false;
    }
    else {
        // Otherwise, calculate the squared distance of the point to the axis. It doesn't matter
        // on/ which side the point is; all that matters is the distance to the axis.
        float d_axis_squared = SquaredMag(position_vector) - sqrf(d_plane);
        // Since the radius of the sector is a linear function of d_plane up to the nearest
        // spherical cap, endpoint, we can check whether the point is outside the main part of
        // the spherical sector.
        if (d_axis_squared > sqrf(d_plane * spherical_sector.sin_half_angular_size))
            return false;
    }
    return true;
}

// Intersection of a sphere against a spherical sector (defined by center, radius (length) axis,
// and angular size). Maximum angular size is 180 degrees, but usually limited to 90 degrees
// (spotlights). This may use one square root calculation.

static BoundsCheckResult QueryIntersection(const sreBoundingVolumeSphere& sphere,
const sreBoundingVolumeSphericalSector &spherical_sector) {
    // The effective radius of the spherical sector with respect to the source sphere depends
    // on the vector between the source sphere's center and the spherical sector's center. When
    // it falls within the spherical_sector's angular range, it will be equal to the spherical
    // sector's radius. However, as the angle increases r_eff will gradually decrease and will
    // only be zero when the angle is greater or equal to the spherical sector's half angular range
    // plus 90 degrees.
    //
    // Since it's hard to avoid some kind of normalization of the center vector when
    // determining the angle with the axis and r_eff, we try an alternative method.

    // First perform a few basic plane checks.
    // Calculate the plane through the sector center with axis as normal.
    Vector4D K = Vector4D(spherical_sector.axis, - Dot(spherical_sector.axis, spherical_sector.center));
    // Calculate the distance of the sphere's center to the plane.
    float d_plane = Dot(K, sphere.center);
    if (d_plane <= - sphere.radius || d_plane >= spherical_sector.radius + sphere.radius)
        // If the sphere is completely outside the plane, or it is completely beyond the radius
        // of the sector, it cannot intersect the sector.
        return SRE_COMPLETELY_OUTSIDE;
    // We can calculate, within the plane formed by the axis and the vector between both centers,
    // a 2D line equation for the side of the sector that is closest to the the sphere's center.
    // We can use the equation in a 2D relative coordinate system with the origin at the
    // the sector's center, we represent the axis direction as the x positive direction
    // with the factor cos_half_angular_size providing the component for the side in the direction
    // of the axis, or distance from plane K. The second component (the y coordinate) represents
    // the distance from the axis in the direction of the sphere center, which will be proportional to
    // sin_half_angular_size in the line equation for the side.
    Point2D P;
    P.x = spherical_sector.cos_half_angular_size;
    P.y = spherical_sector.sin_half_angular_size;
    Vector3D center_vector = (sphere.center - spherical_sector.center);
    // Calculate the squared distance of the 3D sphere center to the axis. It doesn't matter on which
    // side the sphere is; all that matters is the distance to the axis.
    float d_axis_squared = SquaredMag(center_vector) - sqrf(d_plane);
    // P_center is the sphere center in our relative 2D coordinate system.
    // A square root calculation seems to be hard to avoid. The only alternative may be to use
    // cross products to derive the normal vector in the d_axis direction.
    Point2D P_center = Point2D(d_plane, sqrtf(d_axis_squared));
    // In order to fix P (which still has unit length) to the point that is closest to the sphere's
    // center, we can project P_center onto it (P_onto * Dot(P_onto, P_source)), so that it has the
    // same distance to the origin (the sector center).
    P = ProjectOnto(P_center, P);
    // When the sphere center is inside the sector, we have to detect that. Because our local
    // coordinate system only has positive x and y, we only need to check the y coordinate.
    bool inside = (P_center.y - P.y) < 0;
    // We now calculate the squared distance to the point P on the side of the sector that is closest
    // to the sphere's center.
    float d_squared = sqrf(P_center.x - P.x) + sqrf(P_center.y - P.y);
#if 0
    printf("d_axis = %f, d_sector_side = %f, inside = %d\n", sqrtf(d_axis_squared), sqrtf(d_squared),
        inside);
#endif
    if (inside)
        if (d_squared >= sqrf(sphere.radius))
            // Completely inside.
            return SRE_COMPLETELY_INSIDE;
        else
            // Partially inside.
            return SRE_PARTIALLY_INSIDE;
    else if (d_squared >= sqrf(sphere.radius))
        // The sphere is completely outside the sector.
        return SRE_COMPLETELY_OUTSIDE;
    else
        // Partially inside.
        return SRE_PARTIALLY_INSIDE;
}

// For just intersection, use the more detailed intersection test, it is not significantly
// slower than a less detailed test would be.

static bool Intersects(const sreBoundingVolumeSphere& sphere,
const sreBoundingVolumeSphericalSector &spherical_sector) {
    return QueryIntersection(sphere, spherical_sector) != SRE_COMPLETELY_OUTSIDE;
}


// A detailed intersection test for an AABB against a spherical sector is fairly complex (it
// could be implemented by treating the AABB as a convex hull).
// For now, only provide a test to determine whether an AABB is completely inside the sector.

static bool IsCompletelyInside(const sreBoundingVolumeAABB& AABB,
const sreBoundingVolumeSphericalSector &spherical_sector) {
    // Check whether each of the corners of the AABB is inside the sector.
    int intersection_count;
    intersection_count = Intersects(Point3D(AABB.dim_min.x, AABB.dim_min.y, AABB.dim_min.z),
        spherical_sector);
    intersection_count += Intersects(Point3D(AABB.dim_max.x, AABB.dim_min.y, AABB.dim_min.z),
        spherical_sector);
    intersection_count += Intersects(Point3D(AABB.dim_min.x, AABB.dim_max.y, AABB.dim_min.z),
        spherical_sector);
    intersection_count += Intersects(Point3D(AABB.dim_max.x, AABB.dim_max.y, AABB.dim_min.z),
        spherical_sector);
    intersection_count += Intersects(Point3D(AABB.dim_min.x, AABB.dim_min.y, AABB.dim_max.z),
        spherical_sector);
    intersection_count += Intersects(Point3D(AABB.dim_max.x, AABB.dim_min.y, AABB.dim_max.z),
        spherical_sector);
    intersection_count += Intersects(Point3D(AABB.dim_min.x, AABB.dim_max.y, AABB.dim_max.z),
        spherical_sector);
    intersection_count += Intersects(Point3D(AABB.dim_max.x, AABB.dim_max.y, AABB.dim_max.z),
        spherical_sector);
    if (intersection_count == 8)
        return true;
    return false;
}

// Intersection tests against a box.

// Whether a point is inside a box. Unlike tests of larger volumes against a box using the
// plane distance approach, this should be an exact test.

bool Intersects(const Point3D& P, const sreBoundingVolumeBox& box) {
    for (int i = 0; i < 6; i++)
        if (Dot(box.plane[i], P) <= 0)
            return false;
    return true;
}

// Intersection tests against an AABB.

// Intersection test of two AABBs.

bool IsCompletelyInside(const sreBoundingVolumeAABB& AABB1, const sreBoundingVolumeAABB& AABB2) {
    if (AABB1.dim_min.x < AABB2.dim_min.x)
        return false;
    if (AABB1.dim_max.x > AABB2.dim_max.x)
        return false;
    if (AABB1.dim_min.y < AABB2.dim_min.y)
        return false;
    if (AABB1.dim_max.y > AABB2.dim_max.y)
        return false;
    if (AABB1.dim_min.z < AABB2.dim_min.z)
        return false;
    if (AABB1.dim_max.z > AABB2.dim_max.z)
        return false;
    return true;
}

// Whether a sphere is completely inside an AABB.

static bool IsCompletelyInside(const sreBoundingVolumeSphere& sphere, const sreBoundingVolumeAABB& AABB) {
    sreBoundingVolumeAABB sphere_AABB;
    sphere_AABB.dim_min = Vector3D(sphere.center.x - sphere.radius, sphere.center.y - sphere.radius,
        sphere.center.z - sphere.radius);
    sphere_AABB.dim_max = Vector3D(sphere.center.x + sphere.radius, sphere.center.y + sphere.radius,
        sphere.center.z + sphere.radius);
    return IsCompletelyInside(sphere_AABB, AABB);
}

// Higher level intersection tests.

// Test intersection of a scene object against a convex hull. This is heavily used
// for visible object determination against the view frustum at the start of each frame.

bool Intersects(const sreObject& so, const sreBoundingVolumeConvexHull& ch) {
    if (so.model->bounds_flags & SRE_BOUNDS_PREFER_SPECIAL) {
        // Use the special bounding volume shapes of ellipsoid or cylinder when defined.
        if (so.bv_special.type == SRE_BOUNDING_VOLUME_ELLIPSOID)
            return Intersects(*so.bv_special.ellipsoid, ch);
        else
            return Intersects(*so.bv_special.cylinder, ch);
    }
    // When a spherical bounding volume is preferred for the object, use it.
    if (so.model->bounds_flags & SRE_BOUNDS_PREFER_SPHERE)
        return Intersects(so.sphere, ch);
    // Bounding box check. When the SRE_BOUNDS_IS_AXIS_ALIGNED flag is set, it means that
    // the object is static and the AABB defined for it is a tight fit (when the model
    // was created, the AABB was found to be not much worse than the oriented bounding box,
    // and when the object was added to the scene its rotation parameters were found to be
    // such that the AABB subsequently calculated for the object during octree creation
    // would still be a tight fit).
    if (so.box.flags & SRE_BOUNDS_IS_AXIS_ALIGNED)
         return Intersects(so.AABB, ch);
    else
         return Intersects(so.box, ch);
}

// Check whether the light volume of a light intersects with a convex hull
// (such as the frustum).

bool Intersects(const sreLight& light, const sreBoundingVolumeConvexHull& ch) {
    if (light.type & (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_DYNAMIC_SPOT_EXPONENT))
        // Perform a sphere check for a point source light, or a spot light
        // with a dynamic spot exponent, which is hard to pin down to a
        // cylinder.
        return Intersects(light.sphere, ch);
    if (light.type & SRE_LIGHT_SPOT)
        // Do a spherical sector check for spot lights.
        return Intersects(light.spherical_sector, ch);
    else
        // For beam lights, do a cylinder check.
        return Intersects(light.cylinder, ch);
}

// Test whether a scene object intersects a sphere. Used when testing an object
// against a light volume.

bool Intersects(const sreObject& so, const sreBoundingVolumeSphere& sphere) {
    // Always try a sphere check first.
    if (!Intersects(so.sphere, sphere))
        return false;
    // If the preferred bounding volume is a sphere, don't bother with box checks.
    if (so.model->bounds_flags & SRE_BOUNDS_PREFER_SPHERE)
        return true;
    // Perform a box check, which is however not extremely accurate (it can miss
    // non-intersections).
    if (so.box.flags & SRE_BOUNDS_IS_AXIS_ALIGNED)
        return Intersects(so.AABB, sphere);
    else
        return Intersects(so.box, sphere);
}

// Test intersection of a scene object against a light volume.

bool Intersects(const sreObject& so, const sreLight& light) {
    if (light.type & SRE_LIGHT_DIRECTIONAL)
        return true;
    // For point source lights, check the object against the light volume sphere.
    if (light.type & SRE_LIGHT_POINT_SOURCE)
        return Intersects(so, light.sphere);
    // Remaining cases are spot or beam light.
    // Always try a sphere-sphere check first (very quick).
    if (!Intersects(so.sphere, light.sphere))
        return false;
    if (light.type & SRE_LIGHT_SPOT) {
        // For spot lights, do a sphere check against the light volume spherical sector.
        if (!Intersects(so.sphere, light.spherical_sector))
            return false;
#if 0
        printf("Object %d intersects spotlight spherical sector for light %d.\n",
            so.id, light.id);
        printf("Spherical sector parameters: radius %f, cos half angle = %f, "
            "cap radius %f\n",
            light.spherical_sector.radius, light.spherical_sector.cos_half_angular_size,
            light.spherical_sector.sin_half_angular_size * light.spherical_sector.radius);
#endif
    }
    else
        // For beam lights, do a sphere check against the light volume cylinder
        // (which is fairly quick).
        if (!Intersects(so.sphere, light.cylinder))
            return false;
    // If the preferred bounding volume is a sphere (as opposed to box),
    // don't bother with box checks.
    if (so.model->bounds_flags & SRE_BOUNDS_PREFER_SPHERE)
        return true;
    // Check the bounding box of the object against the light volume cylinder,
    // which can be more expensive. For spot lights, it would be better to
    // have a box test against the spherical section.
    if (so.box.flags & SRE_BOUNDS_IS_AXIS_ALIGNED)
        return Intersects(so.AABB, light.cylinder);
    else
        return Intersects(so.box, light.cylinder);
}

// More detailed intersection test of a scene object against a light volume.

BoundsCheckResult QueryIntersection(const sreObject& so, const sreLight& light) {
    // Directional lights have an unbounded light volume (this function is not
    // normally called for directional lights, but it might still happen).
    if (light.type & SRE_LIGHT_DIRECTIONAL)
        return SRE_COMPLETELY_INSIDE;
    // Always try a sphere-sphere check first. Although a sphere does not accurately
    // bound a spot or beam light, it will quickly eliminate objects that are
    // well outside the light volume, and the test is fast.
    BoundsCheckResult r = QueryIntersectionUnified(so.sphere, light.sphere);
    if (r == SRE_COMPLETELY_OUTSIDE)
        return SRE_COMPLETELY_OUTSIDE;
    if (light.type & SRE_LIGHT_POINT_SOURCE) {
        if (r == SRE_COMPLETELY_INSIDE && so.sphere.radius <= light.sphere.radius)
            // For point source lights, the light volume is a sphere, so when the
            // object's bounding sphere is completely inside that's definite.
            return SRE_COMPLETELY_INSIDE;
        // If the preferred bounding volume of the object is a sphere, don't bother
        // with box checks.
        if (so.model->bounds_flags & SRE_BOUNDS_PREFER_SPHERE)
            return SRE_PARTIALLY_INSIDE;
        // Perform a detailed intersection test of the object's bounding
        // box against the light's sphere, preferring the AABB test when possible.
        // An AABB test will be more accurate (no or less missed non-intersections).
        if (so.box.flags & SRE_BOUNDS_IS_AXIS_ALIGNED)
            return QueryIntersection(so.AABB, light.sphere);
        else
            return QueryIntersection(so.box, light.sphere);
    }
    // For spot and beam lights, which use a spherical section or
    // cylinder as the primary bounding volume, first do a (relatively quick)
    // test of the object's bounding sphere against the light volume.
    if (light.type & SRE_LIGHT_SPOT)
        r = QueryIntersection(so.sphere, light.spherical_sector);
    else
        r = QueryIntersection(so.sphere, light.cylinder);
    // A result of completely inside or completely outside is definite.
    if (r != SRE_PARTIALLY_INSIDE)
        return r;
    // If a sphere is the preferred bounding volume of the object (as opposed
    // to box), there's no point trying box checks.
    if (so.model->bounds_flags & SRE_BOUNDS_PREFER_SPHERE)
        return SRE_PARTIALLY_INSIDE;
    // Check the bounding box of the object against the light volume cylinder.
    // We don't yet check whether the box is completely inside the cylinder.
    if (so.box.flags & SRE_BOUNDS_IS_AXIS_ALIGNED)
        // When possible, use the AABB check, which should be faster.
        if (Intersects(so.AABB, light.cylinder))
            return SRE_PARTIALLY_INSIDE;
        else
           return SRE_COMPLETELY_OUTSIDE;
    else
        if (Intersects(so.box, light.cylinder))
            return SRE_PARTIALLY_INSIDE;
        else
            return SRE_COMPLETELY_OUTSIDE;
}

// Full (slow) intersection test of every object vertex against a light volume. Useful
// for preprocessing.

BoundsCheckResult QueryIntersectionFull(const sreObject& so, const sreLight& light,
bool use_worst_case_bounds) {
    if (light.type & SRE_LIGHT_DIRECTIONAL)
        return SRE_COMPLETELY_INSIDE;
    // A bounding volume test may not be completely accurate, because it does not test
    // all LOD levels (bounding volumes are currently always based on LOD level zero). This
    // could lead to a bounding volume test returning SRE_COMPLETELY_INSIDE while a LOD
    // level other than zero actually has vertices that are outside the light volume.
    // Moreover, especially for more irregularly shaped models, it is conceivable that
    // a bounding volume test returns SRE_PARTIALLY_INSIDE while the object is actually
    // completely inside, and correctly identifying that will avoid some unnecessary
    // calculations during rendering such as geometry scissors calculations.
    // For accuracy, the vertices of all LOD models are considered. This has some drawbacks
    // (an actually used LOD model may be fully inside while but labeled partially inside
    // because one LOD model is partially inside). We consider only the LOD levels actually
    // used according to the object's LOD settings.
    int starting_level, ending_level;
    if (so.model->nu_lod_levels == 1) {
        starting_level = 0;
        ending_level = 0;
    }
    else
        if (so.lod_flags & SRE_LOD_FIXED) {
            starting_level = so.lod_level;
            ending_level = so.lod_level;
        }
        else {
            starting_level = so.lod_level;
            ending_level = so.model->nu_lod_levels - 1;
        }
    // Iterate all LOD levels used by the object.
    int count_outside = 0;
    int total_count = 0;
#if 0
    printf("LOD levels %d to %d\n", starting_level, ending_level);
#endif
    for (int lod_level = starting_level; lod_level <= ending_level; lod_level++) {
        sreLODModel *m = so.model->lod_model[lod_level];
        // Iterate the LOD model's vertices.
        for (int i = 0; i < m->nu_vertices; i++) {
            // Apply the model transformation for the object.
            Point3D P = (so.model_matrix * m->vertex[i]).GetPoint3D();
            // Test the vertex position against the light volume.
            // Count vertices that are outside the light volume.
            if (use_worst_case_bounds) {
                // At the moment, worst case bounds are always a sphere.
                if (!Intersects(P, light.worst_case_sphere))
                    count_outside++;
            }
            else if (light.type & SRE_LIGHT_POINT_SOURCE) {
#if 0
                printf("P: (%f, %f, %f), light position: (%f, %f, %f)\n", P.x, P.y, P.z,
                    light.sphere.center.x, light.sphere.center.y, light.sphere.center.z);
#endif
                if (!Intersects(P, light.sphere))
                    count_outside++;
            }
            else {
                // Light type is spot or beam.
                if (light.type & SRE_LIGHT_SPOT) {
                    if (!Intersects(P, light.spherical_sector))
                        count_outside++;
                }
                else {
                    if (!Intersects(P, light.cylinder))
                        count_outside++;
                }
            }
        }
        total_count += m->nu_vertices;
    }
#if 0
    printf("%d vertices counted, %d outside.\n", total_count, count_outside);
#endif
    if (count_outside == 0)
        // All vertices are inside the light volume.
        return SRE_COMPLETELY_INSIDE;
    if (count_outside != total_count)
        // Some vertices are inside, others outside the light volume.
        return SRE_PARTIALLY_INSIDE;
    // When all vertices are outside the light volume, it is still very possible that
    // the interior of a triangle is inside the light volume. Therefore we simply use
    // the bounding volume based intersection query. However, since we know
    // that the object is not completely inside the light volume, when the
    // bounding volume check returns completely inside, we can safely alter that to
    // partially inside.
    BoundsCheckResult r = QueryIntersection(so, light);
    if (r == SRE_COMPLETELY_INSIDE)
        return SRE_PARTIALLY_INSIDE;
    return r;
}

BoundsCheckResult QueryIntersectionFull(const sreObject& so, const sreLight& light) {
    return QueryIntersectionFull(so, light, false);
}

BoundsCheckResult QueryIntersection(const sreObject& so, const sreBoundingVolumeSphere& sphere) {
    // Always try a sphere check first.
    BoundsCheckResult r = QueryIntersectionUnified(so.sphere, sphere);
    if (r == SRE_COMPLETELY_OUTSIDE)
        return SRE_COMPLETELY_OUTSIDE;
    if (r == SRE_COMPLETELY_INSIDE && so.sphere.radius <= sphere.radius)
        return SRE_COMPLETELY_INSIDE;
    // If the preferred bounding volume is a sphere, don't bother with box checks.
    if (so.model->bounds_flags & SRE_BOUNDS_PREFER_SPHERE)
        return SRE_PARTIALLY_INSIDE;
    return QueryIntersection(so.box, sphere);
}

// Octree intersection tests. These return not only false or true, but give more detail
// (such as completely inside, partially inside or completely inside).

BoundsCheckResult QueryIntersection(const sreOctreeNodeBounds& octree_bounds,
const sreBoundingVolumeConvexHull& ch) {
    // Check whether the bounding sphere of the octree is inside, partially inside or outside.
    BoundsCheckResult r = QueryIntersection(octree_bounds.sphere, ch);
    if (r != SRE_PARTIALLY_INSIDE)
        // If the bounding sphere of the octree is completely inside or completely outside,
        // that is a definite result since the actual octree is smaller.
        return r;
    // If the bounding sphere was partially inside, do a more detailed check with the AABB.
    if (Intersects(octree_bounds.AABB, ch))
        return SRE_PARTIALLY_INSIDE;
    return SRE_COMPLETELY_OUTSIDE;
}

// Check whether the octree bounds intersect with a sphere.

BoundsCheckResult QueryIntersection(const sreOctreeNodeBounds& octree_bounds,
const sreBoundingVolumeSphere& sphere) {
    // First perform a sphere check, using the octree's bounding sphere.
    // This is not a definite test, but can quickly identify octrees that
    // have no chance of intersecting the sphere.
    BoundsCheckResult r = QueryIntersectionUnified(octree_bounds.sphere, sphere);
    if (r == SRE_COMPLETELY_OUTSIDE)
        return SRE_COMPLETELY_OUTSIDE;
    if (sphere.radius >= octree_bounds.sphere.radius) {
        // The sphere is at least as large as the octree's sphere.
        if (r == SRE_COMPLETELY_INSIDE)
            // The fact the unified sphere check returned SRE_COMPLETELY_INSIDE means the
            // octree is completely inside the sphere.
            return SRE_COMPLETELY_INSIDE;
    }
    // Calculate more detailed information about whether the octree is completely inside,
    // partially inside or completely outside the sphere. This calculation may need to
    // examine all eight octree corners, although it does perform a quicker check against
    // the sphere's AABB first.
    return QueryIntersection(octree_bounds.AABB, sphere);
}

// Check whether the octree bounds intersect with a light volume.

BoundsCheckResult QueryIntersection(const sreOctreeNodeBounds& octree_bounds, const sreLight& light) {
    if (light.type & SRE_LIGHT_DIRECTIONAL)
        return SRE_COMPLETELY_INSIDE;
    if (light.type & SRE_LIGHT_POINT_SOURCE)
        return QueryIntersection(octree_bounds, light.sphere);
    // The only remaining cases are a spot or beam light.
    // First perform a sphere check, using the octree's bounding sphere and the light's
    // bounding sphere. This is not a definite test, but can quickly identify octrees that
    // have no chance of intersecting the light volume.
    BoundsCheckResult r = QueryIntersectionUnified(octree_bounds.sphere, light.sphere);
    if (r == SRE_COMPLETELY_OUTSIDE)
        return SRE_COMPLETELY_OUTSIDE;
    // Check whether the octree's bounding sphere intersects the light's bounding volume.
    if (light.type & SRE_LIGHT_SPOT)
        r = QueryIntersection(octree_bounds.sphere, light.spherical_sector);
    else
        r = QueryIntersection(octree_bounds.sphere, light.cylinder);
    if (r != SRE_PARTIALLY_INSIDE)
        // Since the octree's bounding sphere encloses the AABB, any result equal to
        // SRE_COMPLETELY_INSIDE or SRE_COMPLETELY_OUTSIDE is a definite result; it
        // would also apply to the actual AABB of the octree.
        return r;
    // At this point we want to check for intersection using the actual AABB of the
    // octree.
    // For spot lights, it is better to use the spherical sector. Because a detailed test
    // is not yet implemented, only detect when the octree is completely inside.
    if (light.type & SRE_LIGHT_SPOT)
        if (IsCompletelyInside(octree_bounds.AABB, light.spherical_sector))
            return SRE_COMPLETELY_INSIDE;
    // Fully check the AABB against the bounding cylinder of the light. This can be a more
    // expensive test since at least one square root operation is performed.
    if (Intersects(octree_bounds.AABB, light.cylinder))
        // The octree could also be completely inside, but we don't yet check for that.
        return SRE_PARTIALLY_INSIDE;
    return SRE_COMPLETELY_OUTSIDE;
}

// The following is necessary for octree creation.

bool IsCompletelyInside(const sreLight& light, const sreBoundingVolumeAABB& AABB) {
    if (light.type & (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM)) {
        if (!(light.type & (SRE_LIGHT_DYNAMIC_SPOT_EXPONENT | SRE_LIGHT_DYNAMIC_DIRECTION))) {
            // Construct two spheres at the endpoints.
            sreBoundingVolumeSphere sphere;
            sphere.center = light.cylinder.center - 0.5f * light.cylinder.length * light.cylinder.axis;
            sphere.radius = light.cylinder.radius;
            if (!IsCompletelyInside(sphere, AABB))
                return false;
            sphere.center = light.cylinder.center + 0.5f * light.cylinder.length * light.cylinder.axis;
            sphere.radius = light.cylinder.radius;
            if (!IsCompletelyInside(sphere, AABB))
                return false;
            // Due to the octree's symmetry, if the endpoint spheres of the cylinder are inside,
            /// then the whole whole cylinder is inside.
            return true;
        }
        // Dynamic spot exponent or dynamic direction. We could improve on this for this in the
        // first case.
        return IsCompletelyInside(light.sphere, AABB);
    }
    // Point source light.
    return IsCompletelyInside(light.sphere, AABB);
}

// Intersection tests against frustum.

bool Intersects(const sreObject& so, const sreBoundingVolumeFrustum& fr) {
#if SRE_NU_FRUSTUM_PLANES == 6
    // There is a meaningful bounding sphere defined for the frustum.
    if (fr.nu_planes == 6 && !Intersects(so.sphere, fr.sphere))
        return false;
#endif
    return Intersects(so, (sreBoundingVolumeConvexHull)fr);
}

