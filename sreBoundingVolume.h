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


// Bounding volume data types.

class srePCAComponent {
public :
    Vector3D vector;  // Normalized PCA component vector.
    float size;       // Size (magnitude) of the PCA component.
};

// For some purposes (such as oriented bounding boxes), it is more
// efficient to store the PCA components in scaled format.

class srePCAComponentScaled {
public :
    Vector3D vector;    // Unnormalized (scaled) PCA component vector.
    float scale_factor; // Scale factor to obtain normalized vector.

    inline Vector3D GetNormal() const {
        return vector * scale_factor;
    }
    inline bool SizeIsZero() const {
        // Size of zero is encoded as a scale factor of - 1.0.
        return scale_factor < 0.0f;
    }
    inline void SetSizeZero() {
        vector.Set(0, 0, 0);
        scale_factor = - 1.0f;
    }
};

// Result values for QueryIntersection(A, B) tests.
// For example, SRE_COMPLETELY_INSIDE means A is completely inside B.

#define SRE_INTERSECT_MASK 1
#define SRE_INSIDE_MASK 2
#define SRE_ENCLOSES_MASK 4
#define SRE_BOUNDS_UNDEFINED_MASK 8
#define SRE_BOUNDS_DO_NOT_CHECK_MASK 16

enum BoundsCheckResult {
    SRE_COMPLETELY_OUTSIDE = 0,
    SRE_PARTIALLY_INSIDE = SRE_INTERSECT_MASK,
    SRE_COMPLETELY_INSIDE = SRE_INTERSECT_MASK | SRE_INSIDE_MASK,
    SRE_COMPLETELY_ENCLOSES = SRE_INTERSECT_MASK | SRE_ENCLOSES_MASK,
    // Bounds not yet defined, calculation is allowed.
    SRE_BOUNDS_UNDEFINED = SRE_BOUNDS_UNDEFINED_MASK,
    // Do not recalculate bounds flag.
    SRE_BOUNDS_DO_NOT_CHECK = SRE_BOUNDS_DO_NOT_CHECK_MASK
};

#define SRE_BOUNDS_EQUAL_AND_TEST_ALLOWED(result, value) \
    ((result) == (value))

#define SRE_BOUNDS_NOT_EQUAL_AND_TEST_ALLOWED(result, value) \
    ((result) != (value) && ((result) & SRE_BOUNDS_DO_NOT_CHECK_MASK) == 0)

class sreBoundingVolumeLineSegment {
public :
    Point3D E1;
    Point3D E2;
};

class sreBoundingVolumeSphere {
public :
    Point3D center;
    float radius;
};

class sreBoundingVolumeEllipsoid {
public :
    Point3D center;
    // The scale field of the PCA is not used.
    srePCAComponentScaled PCA[3];
};

// A spherical sector is like a cone but has a spherical cap.

class sreBoundingVolumeSphericalSector : public sreBoundingVolumeSphere {
public :
    Vector3D axis;
    float cos_half_angular_size;
    float sin_half_angular_size;

};

class sreBoundingVolumeInfiniteSphericalSector : public sreBoundingVolumeSphericalSector {
};

class sreBoundingVolumeBox {
public :
    srePCAComponentScaled PCA[3];
    // When one component (T) is zero in size, the scaled component will be
    // a zero vector and not contain direction information. So we have to
    // store the normalized T direction seperately.
    Vector3D T_normal;
    int flags;
    Point3D center;
    Vector4D plane[6];

    void CalculatePlanes();
    // Get a corner position of the box. Factors must be 0.5 or - 0.5.
    inline Point3D GetCorner(float R_factor, float S_factor, float T_factor) const {
        return center + PCA[0].vector * R_factor + PCA[1].vector * S_factor +
            PCA[2].vector * T_factor;
    }
    void ConstructVertices(Point3D *P, int& n) const;
};

class sreBoundingVolumeAABB {
public :
    Vector3D dim_min;
    Vector3D dim_max;
};

// A hull is defined with a set of vertex positions.

class sreBoundingVolumeHull {
public :
    int nu_vertices;
    Point3D *vertex;

    void AllocateStorage(int n_vertices);
};

// Minimal convex hull with just the plane vectors. Usually sufficient when it is the
// target argument of an intersection test.

class sreBoundingVolumeConvexHull {
public :
    Vector4D *plane;
    int nu_planes;

    sreBoundingVolumeConvexHull() { }
    sreBoundingVolumeConvexHull(int n_planes);
    void AllocateStorage(int n_planes);
};

// Convex hull that also stores the vertex positions (hull).

class sreBoundingVolumeConvexHullWithVertices : public sreBoundingVolumeConvexHull {
public :
    sreBoundingVolumeHull hull;

    sreBoundingVolumeConvexHullWithVertices() { }
    sreBoundingVolumeConvexHullWithVertices(int n_vertices, int n_planes);
    void AllocateStorage(int n_vertices, int n_planes);
};

// Convex hull with vertices that also includes a center position and the distances of
// each plane to the center, as well as the minimum and maximum plane distance. These
// values are convenient when the convex hull is tested for intersection against another
// convex hull.

class sreBoundingVolumeConvexHullFull : public sreBoundingVolumeConvexHullWithVertices {
public :
    Point3D center;
    float *plane_radius;
    float min_radius;
    float max_radius;

    sreBoundingVolumeConvexHullFull() { }
    sreBoundingVolumeConvexHullFull(int n_vertices, int n_planes);
    void AllocateStorage(int n_vertices, int n_planes);
};

// Convex hull with all the information in sreBoundingVolumeConvexHullFull that adds
// a plane definition array. This array identifies how many vertices each plane has,
// and the vertex index of the vertices of the plane. The array is a contiguous array
// of integers starting with the number of vertices in the first plane, then the vertex
// indices of each of its vertices, followed by the number of vertices in the second
// plane and it vertex indices, and so on, with data included for all planes (nu_planes).

class sreBoundingVolumeConvexHullConfigurable : public sreBoundingVolumeConvexHullFull {
public :
    int *plane_definitions;
};

// For pyramids, we only need the a hull. This data type is currently unused.

class sreBoundingVolumePyramid : public sreBoundingVolumeHull {
public :
    // Values filled in by CompleteParameters().
    Vector3D base_normal;
    float cos_half_angular_size;
};

// For point light shadow volumes, we actually use a pyramid cone (consisting of an apex and
// a set of base vertices). The length of each pyramid side edges is the same (equal to radius).
// The value cos_half_angular_size, filled in by CompleteParameters(), 

class sreBoundingVolumePyramidCone : public sreBoundingVolumeHull {
public :
    Vector3D axis;
    float radius;
    // Value filled in by CompleteParameters().
    float cos_half_angular_size;
};

class sreBoundingVolumeInfinitePyramidBase : public sreBoundingVolumePyramidCone {
};

class sreBoundingVolumeFrustum : public sreBoundingVolumeConvexHullWithVertices {
public :
    sreBoundingVolumeSphere sphere;
};

class sreBoundingVolumeCylinder {
public :
    Point3D center;
    float length;
    Vector3D axis;
    float radius;
    // Store precalculated square root of (1.0 - (square of axis coordinate)).
    // This helps intersection tests of an AABB against the cylinder. Generally
    // only used when the cylinder defines the light bounding volume of a spot or
    // beam light.
    Vector3D axis_coefficients;

    void CalculateAxisCoefficients();
};

class sreBoundingVolumeHalfCylinder {
public :
    Point3D endpoint;
    float radius;
    Vector3D axis;
};

class sreBoundingVolumeCapsule {
public :
    float radius;
    float length;
    Vector3D center;
    Vector3D axis;
    float radius_y;
    float radius_z;
};

// Generalized bounding volume. Used for shadow volumes.

enum sreBoundingVolumeType {
    SRE_BOUNDING_VOLUME_UNDEFINED, SRE_BOUNDING_VOLUME_EMPTY,
    SRE_BOUNDING_VOLUME_EVERYWHERE, SRE_BOUNDING_VOLUME_SPHERE,
    SRE_BOUNDING_VOLUME_ELLIPSOID, SRE_BOUNDING_VOLUME_BOX,
    SRE_BOUNDING_VOLUME_CYLINDER,
    // A bounding volume of the following type is assumed to be of the type
    // sreBoundingVolumeConvexHullConfigurable.
    SRE_BOUNDING_VOLUME_CONVEX_HULL,
    SRE_BOUNDING_VOLUME_PYRAMID, SRE_BOUNDING_VOLUME_PYRAMID_CONE,
    SRE_BOUNDING_VOLUME_SPHERICAL_SECTOR, SRE_BOUNDING_VOLUME_HALF_CYLINDER,
    SRE_BOUNDING_VOLUME_CAPSULE
};

class sreBoundingVolume {
public :
    sreBoundingVolumeType type;
    bool is_complete;
    union {
    sreBoundingVolumeSphere *sphere;
    sreBoundingVolumeEllipsoid *ellipsoid;
    sreBoundingVolumeBox *box;
    sreBoundingVolumeCylinder *cylinder;
    sreBoundingVolumeConvexHullFull *convex_hull_full;
    // The configurable convex hull includes plane definitions (vertex lists).
    sreBoundingVolumeConvexHullConfigurable *convex_hull_configurable;
    sreBoundingVolumePyramid *pyramid;
    sreBoundingVolumePyramidCone *pyramid_cone;
    sreBoundingVolumeSphericalSector *spherical_sector;
    sreBoundingVolumeHalfCylinder *half_cylinder;
    sreBoundingVolumeCapsule *capsule;
    };

    sreBoundingVolume() { }
    ~sreBoundingVolume();
    void Clear(); 
    // Only the bounding volume types relevant to shadow volumes are fully implemented.
    void SetEmpty();
    void SetEverywhere();
    void SetPyramid(const Point3D *P, int nu_vertices);
    void SetPyramidCone(const Point3D *P, int nu_vertices, const Vector3D& axis, float radius,
        float cos_half_angular_size);
    void SetSphericalSector(const Vector3D& axis, float radius, float cos_half_angular_size);
    void SetHalfCylinder(const Point3D& E, float cylinder_radius, const Vector3D& cylinder_axis);
    void SetCylinder(const Point3D& center, float length, const Vector3D& axis, float radius);
    void CompleteParameters();
};
