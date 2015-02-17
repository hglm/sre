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


/* User API for the SRE library. */

#ifndef __SRE_H__
#define __SRE_H__

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
  #define SRE_HELPER_DLL_IMPORT __declspec(dllimport)
  #define SRE_HELPER_DLL_EXPORT __declspec(dllexport)
  #define SRE_HELPER_DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define SRE_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define SRE_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define SRE_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define SRE_HELPER_DLL_IMPORT
    #define SRE_HELPER_DLL_EXPORT
    #define SRE_HELPER_DLL_LOCAL
  #endif
#endif

// Now we use the generic helper definitions above to define SRE_API and SRE_LOCAL.
// SRE_API is used for the public API symbols. It either DLL imports or DLL exports (or does nothing
// for static build). SRE_LOCAL is used for non-api symbols.

#ifdef SRE_DLL // defined if SRE is compiled as a DLL
  #ifdef SRE_DLL_EXPORTS // defined if we are building the SRE DLL (instead of using it)
    #define SRE_API SRE_HELPER_DLL_EXPORT
  #else
    #define SRE_API SRE_HELPER_DLL_IMPORT
  #endif // SRE_DLL_EXPORTS
  #define SRE_LOCAL SRE_HELPER_DLL_LOCAL
#else // SRE_DLL is not defined this means SRE is a static lib.
  #define SRE_API
  #define SRE_LOCAL
#endif // SRE_DLL

// Hack to avoid having to always include all GL header files. Should be safe under all reasonable assumptions.
#define SRE_GLUINT unsigned int
#define SRE_GLINT int

// dstRandom.h, dstDynamicArray.h and dstMatrixMath.h are provided by the external
// DataSetTurbo library.
#include <dstRandom.h>
#include <dstDynamicArray.h>
#include <dstMatrixMath.h>
#include <dstColor.h>

typedef Matrix4x3RM MatrixTransform;

// Note: sreMatrixMath.h also provides alignment macro DST_ALIGNED(n)
#include "sreBoundingVolume.h"

#define SRE_MAX_ACTIVE_LIGHTS_UNLIMITED 2147483647
// When multi-pass rendering is used, the following value defines the default maximum number
// of active lights. A value of INT_MAX (SRE_MAX_ACTIVE_LIGHTS_UNLIMITED) means the number
// of visible active lights is unlimited. With single-pass rendering, the maximum number of
// active lights is always one.
#define SRE_DEFAULT_MAX_ACTIVE_LIGHTS SRE_MAX_ACTIVE_LIGHTS_UNLIMITED
// Absolute limit for the amount of active lights calculated with multi-pass
// rendering, if the number of lights is not unlimited.
#define SRE_MAX_ACTIVE_LIGHTS 256
// Default temporary limits for the number of visible objects/lights during rendering. If the scene
// contains less objects, the actual number of objects will be used initially. If capacity runs out,
// it is automatically increased (these settings do not place the actual limit, which is only limited
// by system memory).
#define SRE_DEFAULT_MAX_VISIBLE_OBJECTS 256
#define SRE_DEFAULT_MAX_FINAL_PASS_OBJECTS 128
#define SRE_DEFAULT_MAX_SHADOW_CASTER_OBJECTS 256
#define SRE_DEFAULT_MAX_VISIBLE_LIGHTS 64
// Default temporary limits for the stencil shadow implementation. Capacity is dynamically increased when
// needed.
#define SRE_DEFAULT_MAX_SHADOW_VOLUME_VERTICES 8192
#define SRE_DEFAULT_MAX_SILHOUETTE_EDGES 2048
// Maximum number of levels-of-detail per object.
#define SRE_MAX_LOD_LEVELS 4
// The distance of the near plane.
#define SRE_DEFAULT_NEAR_PLANE_DISTANCE 1.0f
// If SRE_NU_FRUSTUM_PLANES is 5, for view frustum culling purposes the far clipping plane has
// an infinite distance. If it is 6 objects beyond the far plane are culled. 
#define SRE_NU_FRUSTUM_PLANES 6
// The distance of the far plane. Because we use an infinite projection matrix, the engine optionally
// culls objects against this plane but the GPU does not clip against it. 
#define SRE_DEFAULT_FAR_PLANE_DISTANCE 2000.0f
// The projected size in normalized device coordinates below which octree nodes are culled.
#define SRE_OCTREE_SIZE_CUTOFF 0.01f
// The projected size below which objects are culled.
#define SRE_OBJECT_SIZE_CUTOFF 0.002f
// The projected size below which light volumes are culled.
#define SRE_LIGHT_VOLUME_SIZE_CUTOFF 0.05f
// The projected size below which LOD levels are triggered by default.
#define SRE_LOD_LEVEL_1_THRESHOLD 0.064f
#define SRE_LOD_LEVEL_2_THRESHOLD 0.032f
#define SRE_LOD_LEVEL_3_THRESHOLD 0.016f
// The projected size/area below which geometry scissors are skipped.
#define SRE_GEOMETRY_SCISSORS_OBJECT_SIZE_THRESHOLD 0.8f
#define SRE_GEOMETRY_SCISSORS_OBJECT_AREA_THRESHOLD (0.4f * 0.4f)
#define SRE_GEOMETRY_SCISSORS_LIGHT_AREA_THRESHOLD (0.5f * 0.5f)
// The size of the shadow map in pixels (n x n) for directional lights, spot lights and beam lights.
#define SRE_MAX_SHADOW_MAP_SIZE_OPENGL 2048
#define SRE_MAX_SHADOW_MAP_SIZE_GLES2 1024
// The projected size above which the largest shadow map is triggered for spot and beam lights;
// the threshold is halved for each subsequent level with a shadow map half the size.
#define SRE_MAX_SHADOW_MAP_SIZE_THRESHOLD_OPENGL 5.12f
#define SRE_MAX_SHADOW_MAP_SIZE_THRESHOLD_GLES2 2.56f
// The maximum number of shadow map levels.
#define SRE_MAX_SHADOW_MAP_LEVELS_OPENGL 6
#define SRE_MAX_SHADOW_MAP_LEVELS_GLES2 5
// The largest size of the shadow maps for point source light cube maps (GL 3+).
#define SRE_MAX_CUBE_SHADOW_MAP_SIZE_OPENGL 2048
// The largest size of the shadow maps for point source light cube maps (GL-ES 2.0).
#define SRE_MAX_CUBE_SHADOW_MAP_SIZE_GLES2 512
// The projected size above which the largest cube shadow map is triggered; the threshold is
// halved for each subsequent level with a cube shadow map half the size.
#define SRE_MAX_CUBE_SHADOW_MAP_SIZE_THRESHOLD_OPENGL 5.12f
#define SRE_MAX_CUBE_SHADOW_MAP_SIZE_THRESHOLD_GLES2 1.28f
// The maximum number of cube shadow map levels.
#define SRE_MAX_CUBE_SHADOW_MAP_LEVELS_OPENGL 6
#define SRE_MAX_CUBE_SHADOW_MAP_LEVELS_GLES2 4
#define SRE_MIN_SHADOW_CUBE_MAP_NEAR_PLANE_DISTANCE 0.01f
// The maximum depth for the octrees used for scene entities (objects and lights).
#define SRE_MAX_OCTREE_DEPTH 12


enum {
     TEXTURE_TYPE_NORMAL = 0, TEXTURE_TYPE_SRGB, TEXTURE_TYPE_LINEAR,
    TEXTURE_TYPE_TRANSPARENT, TEXTURE_TYPE_TRANSPARENT_EXTEND_TO_ALPHA,
    TEXTURE_TYPE_WILL_MERGE_LATER, TEXTURE_TYPE_NORMAL_MAP,
    TEXTURE_TYPE_SPECULARITY_MAP, TEXTURE_TYPE_TABLE,
    SRE_TEXTURE_TYPE_FLAG_USE_UNCOMPRESSED_TEXTURE = 0x400,
    SRE_TEXTURE_TYPE_FLAG_WRAP_REPEAT = 0x800,
    SRE_TEXTURE_TYPE_FLAG_KEEP_DATA = 0x1000,
    SRE_TEXTURE_TYPE_FLAG_NO_UPLOAD = 0x2000,
    SRE_TEXTURE_TYPE_FLAGS_MASK = 0x3C00 };

enum {
    TEXTURE_FORMAT_RAW = 0, TEXTURE_FORMAT_RAW_RGBA8, TEXTURE_FORMAT_RAW_RGB8,
    TEXTURE_FORMAT_RAW_SRGBA8, TEXTURE_FORMAT_RAW_SRGB8, TEXTURE_FORMAT_RAW_R8,
    TEXTURE_FORMAT_COMPRESSED = 0x100,
    TEXTURE_FORMAT_ETC1 = 0x100, TEXTURE_FORMAT_DXT1, TEXTURE_FORMAT_SRGB_DXT1,
    TEXTURE_FORMAT_DXT1A, TEXTURE_FORMAT_SRGB_DXT1A,
    TEXTURE_FORMAT_ETC2_RGB8, TEXTURE_FORMAT_SRGB_BPTC, TEXTURE_FORMAT_BPTC,
    TEXTURE_FORMAT_BPTC_FLOAT, TEXTURE_FORMAT_RGTC1, TEXTURE_FORMAT_RGTC2,
    TEXTURE_FORMAT_SIGNED_RGTC1, TEXTURE_FORMAT_SIGNED_RGTC2
};


enum { SRE_TEXTURE_FILTER_NEAREST, SRE_TEXTURE_FILTER_LINEAR, SRE_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR };
#define SRE_TEXTURE_FLAG_SET_FILTER          1
#define SRE_TEXTURE_FLAG_SET_ANISOTROPY      2
#define SRE_TEXTURE_FLAG_ENABLE_WRAP_REPEAT  0x10
#define SRE_TEXTURE_FLAG_DISABLE_WRAP_REPEAT 0x20

class SRE_API sreTexture {
public:
    int width;
    int height;
    int bytes_per_pixel;
    int bit_depth;
    int nu_components;
    int format;
    unsigned int *data;
    int opengl_id;
    int type;
    unsigned int largest_level_width;

    // Use of constructors in applications to create textures is deprecated.
    // use sreCreateTexture() instead.
    sreTexture();
    // Create a texture (and upload it to the GPU, normally) from a
    // texture file (.png, .ktx, .dds). basepathname should exclude any
    // extension; the desired extensions will be automatically looked for,
    // in order of preference.
    sreTexture(const char *pathname_without_ext, int type);
    // Create a standard RGBA8 texture of size w x h, memory is allocated
    // in system memory (sreTexture::data) but it is not uploaded to the
    // GPU.
    sreTexture(int w, int h);
    ~sreTexture();
    void ClearData();
    void Load(const char *pathname_without_ext, int type);
    // Upload a single-level texture to the GPU.
    void UploadGL(int flags);
    unsigned int LookupPixel(int x, int y);
    void TextureLookupNearest(float u, float v, Color& c); 
    void SetPixel(int x, int y, unsigned int value);
    void MergeTransparencyMap(sreTexture *t);
    void ConvertFrom24BitsTo32Bits();
    void LoadPNG(const char *pathname, int flags);
    bool LoadKTX(const char *pathname, int flags);
    void LoadDDS(const char *pathname, int flags);
    void ChangeParameters(int flags, int filter, float anisotropy);
private :
    void CalculateTargetSize(int& target_width, int& target_height,
        int &nu_levels_to_skip, int flags);
    void SelectMipmaps(int nu_mipmaps, int& power_of_two_count, int& nu_mipmaps_used,
        int& target_width, int& target_height, int& nu_levels_skipped, int flags);
    void ApplyTextureDetailSettings(int flags);
    void GenerateMipmapLevels(int starting_level, int& nu_levels, sreTexture **textures);
};

class sreShadowVolume : public sreBoundingVolume {
public :
    int light;

    sreShadowVolume() { }
};

// Triangle in a model.

class SRE_API sreModelTriangle {
public:
    int vertex_index[3];
    Vector3D normal;

    void AssignVertices(int i1, int i2, int i3) {
        vertex_index[0] = i1;
        vertex_index[1] = i2;
        vertex_index[2] = i3;
    }
    void InvertVertices() {
        int temp = vertex_index[0];
        vertex_index[0] = vertex_index[2];
        vertex_index[2] = temp;
    }
};

// For optional geometry preprocessing, polygons may be used.

class SRE_API sreModelPolygon {
public:
    int *vertex_index;
    Vector3D normal;
    int nu_vertices;

    sreModelPolygon();
    void InitializeWithSize(int n);
    void AddVertex(int i);
};

// Edge information is used for shadow volumes.

class SRE_API ModelEdge {
public:
    int vertex_index[2];   // The two vertices.
    int triangle_index[2]; // One or two triangles that the edge is part of.
};

// Vertex attribute identifiers.

#define SRE_NU_VERTEX_ATTRIBUTES 5

enum {
    SRE_ATTRIBUTE_POSITION = 0,
    SRE_ATTRIBUTE_TEXCOORDS = 1,
    SRE_ATTRIBUTE_NORMAL = 2,
    SRE_ATTRIBUTE_TANGENT = 3,
    SRE_ATTRIBUTE_COLOR = 4
};

// These attribute flags apply to base models (sreBaseModel) and the derived sreLODModel
// (sreBaseModel::flags) where they define the attribute data defined for the model.
// The attributes mask bit are also used for sreLODModel::instance_flags and the
// dynamic_flags parameter in the vertex buffer initialization functions, and in
// sreAttributeInfo.
// Additional sreBaseModel::flags about the closedness of the model are also defined.

enum {
    SRE_POSITION_MASK = (1 << SRE_ATTRIBUTE_POSITION),
    SRE_TEXCOORDS_MASK = (1 << SRE_ATTRIBUTE_TEXCOORDS),
    SRE_NORMAL_MASK = (1 << SRE_ATTRIBUTE_NORMAL),
    SRE_TANGENT_MASK = (1 << SRE_ATTRIBUTE_TANGENT),
    SRE_COLOR_MASK = (1 << SRE_ATTRIBUTE_COLOR),
    SRE_ALL_ATTRIBUTES_MASK = ((1 << SRE_NU_VERTEX_ATTRIBUTES) - 1),
    // Whether every exterior part of the model is covered by an outward-facing triangle.
    SRE_LOD_MODEL_NOT_CLOSED = 0x100,
    // Whether the model has any holes (which has consequences for its silhouette and shadow).
    SRE_LOD_MODEL_CONTAINS_HOLES = 0x200,
    // Whether the model is limited to a single plane.
    SRE_LOD_MODEL_SINGLE_PLANE = 0x400,
    // Whether the model is almost perfectly closed when the SRE_LOD_MODEL_NOT_CLOSED flag is
    // set. Such models can be treated as closed models in some cases (e.g. shadow mapping)
    // while having to be treated as non-closed models in other cases (e.g. stencil shadows).
    SRE_LOD_MODEL_ALMOST_CLOSED = 0x800
};

enum {
    SRE_SORTING_HINT_XYZ = 0,
    SRE_SORTING_HINT_YXZ = 1,
    SRE_SORTING_HINT_ZXY = 2,
    SRE_SORTING_HINT_XZY = 3,
    SRE_SORTING_HINT_YZX = 4,
    SRE_SORTING_HINT_ZYX = 5,
    SRE_SORTING_HINT_FIRST_ASCENDING = 0,
    SRE_SORTING_HINT_FIRST_DESCENDING = 6,
    SRE_SORTING_HINT_SECOND_ASCENDING = 0,
    SRE_SORTING_HINT_SECOND_DESCENDING = 12,
    SRE_SORTING_HINT_THIRD_ASCENDING = 0,
    SRE_SORTING_HINT_THIRD_DESCENDING = 24,
    SRE_SORTING_HINT_DO_NOT_SORT = 0xFE,
    SRE_SORTING_HINT_UNDEFINED = 0xFF
};

// The base model class with basic geometry and attribute value information.

class SRE_API sreBaseModel {
public :
    int nu_vertices;
    int nu_triangles;
    union {
        Point3DPadded *vertex;
        Point3DPadded *position;
    };
    sreModelTriangle *triangle;
    Vector3D *vertex_normal;
    Vector4D *vertex_tangent;
    Point2D *texcoords;
    Color *colors;
    int flags;
    unsigned char sorting_dimension;
    unsigned char cache_coherency_sorting_hint;
    // Instancing (indicates which attributes are not shared with another model).
    int instance_flags;

    sreBaseModel();
    sreBaseModel(int nu_vertices, int nu_triangles, int flags);
    ~sreBaseModel();
    void SetAttributeFlags(int _flags) {
        flags = (flags & (~SRE_ALL_ATTRIBUTES_MASK)) | _flags;
    }
    void SetNuVertices(int n) {
        nu_vertices = n;
    }
    // Assign vertex attribute data. Makes sure the array is aligned on a 16-byte boundary.
    void SetPositions(Point3DPadded *_positions);
    void SetTexcoords(Point2D *_texcoords);
    void SetColors(Color *_colors);
    void SetTangents(Vector4D *_tangents);
    void CalculateNormals();
    void CalculateNormals(int start_vertex_index, int nu_vertices, bool verbose);
    void CalculateNormalsNotSmooth();
    void CalculateNormalsNotSmooth(int start_vertex_index, int nu_vertices);
    void CalculateTriangleNormals();
    void CalculateVertexTangentVectors();
    void CalculateTangentVectors();
    void RemapVertices(int *vertex_mapping, int n, int *vertex_mapping2);
    void MergeIdenticalVertices(int *saved_indices);
    void MergeIdenticalVertices();
    void CloneGeometry(sreBaseModel *clone);
    void Clone(sreBaseModel *clone);
    void RemoveEmptyTriangles();
    void RemoveUnusedVertices(int *saved_indices);
    void RemoveUnusedVertices();
    void SortVertices(int dimension);
    void SortVerticesOptimalDimension();
    void WeldVertices();
    void ReduceTriangleCount(float max_surface_roughness, float cost_threshold,
        bool check_vertex_normals, float vertex_normal_threshold, int *saved_indices);
    void ReduceTriangleCount(float max_surface_roughness, float cost_threshold,
        bool check_vertex_normals, float vertex_normal_threshold);
    uint64_t CalculateCacheCoherency();
    // Bounding volume calculation.
    void CalculatePrincipalComponents(srePCAComponent *PCA, Point3D& center) const;
    void CalculatePCABoundingSphere(const srePCAComponent *PCA, sreBoundingVolumeSphere& sphere) const;
    void CalculatePCABoundingEllipsoid(const srePCAComponent *PCA,
        sreBoundingVolumeEllipsoid& ellipsoid) const;
    void CalculatePCABoundingCylinder(const srePCAComponent *PCA,
        sreBoundingVolumeCylinder& cylinder) const;
    void CalculateAABB(sreBoundingVolumeAABB& AABB) const;
};

// Object and model-specific shader attribute information.
//
// When applied to a model, it defines all the possible attribute buffers defined
// for the model, and their configuration (non-interleaved and/or up to three
// interleaved attribute buffers). The offsets and strides for interleaved buffers
// are uniquely defined by their attribute mask.
//
// When applied to a cached shader configuration (which differs depending on
// light type, and shadow map use) for an object, it defines the
// attribute configuration for the specific shader configuration for the object.
// It is always a subset of the full set of attributes supported by the model.

#define SRE_NU_OBJECT_ATTRIBUTE_INFO_ARRAYS 3

enum {
    SRE_OBJECT_ATTRIBUTE_LIST_NON_INTERLEAVED = 0,
    SRE_OBJECT_ATTRIBUTE_LIST_INTERLEAVED,
    SRE_OBJECT_ATTRIBUTE_INFO_INTERLEAVED,
};

class sreAttributeInfo {
public :
    // This 32-bit integer encodes the attribute masks used by a model or a shader
    // configuration for an object for each of group of attributes:
    // - Bits 0-7 always contain the attribute mask for the non-interleaved vertex buffers;
    //   When it is equal to zero there are no non-interleaved attributes.
    // - Bits 8-15 contain the attribute mask for the first interleaved vertex buffer (0).
    //   When it is equal to zero there are no interleaved attributes.
    // - Bits 16-23 and 24-31 contain the attribute mask for any subsequent interleaved
    //   vertex buffers (1 and 2); the maximum allowed number of seperate interleaved
    //   vertex buffers used is three.
    //
    unsigned int attribute_masks;
};

// Object-specific vertex attribute buffer information.
// When there is an interleaved vertex buffer in an object shader configuration, an
// interleaved attribute mask does not fully describe the offsets and strides that have to
// be used when setting up the interleaved attribute buffers, because not all
// attributes defined in the model's interleaved vertex buffer may be used. In this case,
// the object attribute buffer set-up function must use the interleaved attribute
// information of the model to derive the offsets and stride. This information is cached
// below.

class sreObjectAttributeInfo : public sreAttributeInfo {
public :
    // This integer contains, for each one of the three possible interleaved vertex
    // buffers for the object shader configuration defined above, the full attribute
    // mask of the corresponding actual interleaved vertex buffer of the model used. Note
    // that the index (0, 1, or 2) of an interleaved vertex buffer for the object may
    // differ from the one in the model.
    // - Bits 0-7 are unused.
    // - Bits 8-15, 16-23 and 24-31 define the full attribute masks for the actual interleaved
    //   model attribute buffer referred to by the corresponding bits in attribute_masks above.
    unsigned int model_attribute_masks;

    // Set the attribute info for a object shader configuration based on the provided attribute
    // mask and the model's attribute info.
    void Set(unsigned int attribute_mask, const sreAttributeInfo& model_attribute_info);
};

// Model sub-meshes are supported (with per-mesh source textures).

class SRE_API sreModelMesh {
public:
    int starting_vertex;
    int nu_vertices;
    SRE_GLUINT texture_opengl_id;
    SRE_GLUINT normal_map_opengl_id;
    SRE_GLUINT specular_map_opengl_id;
    SRE_GLUINT emission_map_opengl_id;
};

// These flags apply to LOD models (sreLODModel). The flags field is defined
// in the sreBaseModel class.
enum {
    // Whether the model is actually of the derived class sreLODModelShadowVolume
    // which has the extra shadow volume and edge information fields.
    SRE_LOD_MODEL_IS_SHADOW_VOLUME_MODEL = 0x1000,
    // Whether extruded vertex positions for shadow volumes should not be generated
    // and edge information calculated when the model is uploaded to the GPU when
    // shadow volumes not disabled and SRE_LOD_MODEL_IS_SHADOW_VOLUME_MODEL is set.
    SRE_LOD_MODEL_NO_SHADOW_VOLUME_SUPPORT = 0x2000,
    // Whether calculated edge information is actually present (generally true when
    // shadow volumes are globally enabled, SRE_LOD_MODEL_IS_SHADOW_VOLUME_MODEL is set,
    // and the SRE_LOD_MODEL_NO_SHADOW_VOLUME_SUPPORT flag was not set for the model).
    SRE_LOD_MODEL_HAS_EDGE_INFORMATION = 0x4000,
    // Model is a billboard (includes billboards, halos and particle systems).
    SRE_LOD_MODEL_BILLBOARD = 0x8000,
    // Model is a halo (either single or a particle system).
    SRE_LOD_MODEL_LIGHT_HALO = 0x10000,
    // Model is a fluid model of derived class sreLODModelFluid.
    SRE_LOD_MODEL_IS_FLUID_MODEL = 0x20000,
    // Model has been uploaded to the GPU.
    SRE_LOD_MODEL_UPLOADED = 0x100000,
    // The following flag is deprecated. Use dynamic_flags instead.
    SRE_LOD_MODEL_VERTEX_BUFFER_DYNAMIC = 0x4000000,
};

// An extension of sreBaseModel for models that can be uploaded to the GPU.
// Used for Level-Of-Detail sub-models.

class SRE_API sreLODModel : public sreBaseModel {
public :
    // OpenGL 3+ identifiers.
    // When one or more interleaved attribute buffers are used,
    // some of the attribute buffer IDs will be identical.
    SRE_GLUINT GL_attribute_buffer[SRE_NU_VERTEX_ATTRIBUTES];
    SRE_GLUINT GL_element_buffer;
    int GL_indexsize;
    // Identification.
    int id; // Encoded as the id of the parent sreModel id * 10 + the LOD level.
    bool referenced; // LOD model is referred to by a scene object.
    // Multiple meshes.
    int nu_meshes;
    sreModelMesh *mesh;
    // Vertex attribute information (for non-interleaved buffers, and up to
    // three interleaved buffers).
    sreAttributeInfo attribute_info;

    // Constructors and allocation.
    sreLODModel();
    sreLODModel *AllocateNewOfSameType() const;
    sreLODModel *CreateCopy() const;
    // Vertex buffer creation.
    void UploadToGPU(int attribute_mask, int dynamic_flags);
    void DeleteFromGPU();
    void NewVertexBuffers(int attribute_mask, int dynamic_flags, Vector4D *positions, bool shadow);
    void NewVertexBufferInterleaved(int attribute_mask, Vector4D *positions, bool shadow);
    // Setting up GL vertex attribute array pointers before drawing an object (the attribute
    // info passed is object-specific, and may be a subset of the model's attributes).
    void SetupAttributesNonInterleaved(sreObjectAttributeInfo *info) const;
    void SetupAttributesInterleaved(sreObjectAttributeInfo *info) const;
};

// LOD model with support for shadow volumes (which can have with extruded vertices
// and edge information). When the sreLODObject is of this type, the
// SRE_LOD_MODEL_IS_SHADOW_VOLUME_MODEL flag should be set.

class SRE_API sreLODModelShadowVolume : public sreLODModel {
public :
    // Shadow support.
    int vertex_index_shadow_offset;
    // Edge information for shadow volumes.
    int nu_edges;
    ModelEdge *edge;

    sreLODModelShadowVolume();
    ~sreLODModelShadowVolume();
    // Edge handling.
    void CalculateEdges();
    void RemoveUnnecessaryEdges();
    void DestroyEdges();
};

// Fluid data used for fluid models.

class SRE_API sreFluid {
private :
    int	width;
    int	height;			
    float k1, k2, k3;
public :
    Vector3D *buffer[2];
    int renderBuffer;
    Vector3D *normal;
    Vector3D *tangent;

    sreFluid(int n, int m, float d, float t, float c, float mu);
    ~sreFluid();	
    void Evaluate(void);
    void CreateDisturbance(int x, int y, float z);
};

class SRE_API sreLODModelFluid : public sreLODModel {
public :
    sreFluid *fluid;

    sreLODModelFluid();
    ~sreLODModelFluid();
    void Evaluate();
    void UpdateVertexBuffers();
};


// Definitions for sreModel::model_flags (not to be confused with the flags field
// of the LOD models).

enum {
    // Whether shadow volumes for the model are configured (implies that all
    // LOD sub-models are of the type sreLODModelShadowVolume and have fully
    // configured shadow volume support).
    SRE_MODEL_SHADOW_VOLUMES_CONFIGURED = 0x1,
    // The LOD model (usually a single one) is a billboard.
    SRE_MODEL_BILLBOARD = 0x2,
    // The LOD model is a light halo (SRE_MODEL_BILLBOARD should also be set).
    SRE_MODEL_LIGHT_HALO = 0x4,
    // The LOD model is a particle system (SRE_MODEL_BILLBOARD should also be set,
    // SRE_MODEL_LIGHT_HALO may be set).
    // The vertex position attribute of the LOD model represent all combined (disjoint) triangles
    // of the particle system and the normal attribute additionally holds the center position of
    // the billboard to which the particle triangle belongs.
    SRE_MODEL_PARTICLE_SYSTEM = 0x8,
};

// Bounding volume flags for a model (sreModel::bounds_flags). Either
// SRE_BOUNDS_PREFER_SPHERE, BOUNDS_PREFER_BOX or BOUNDS_PREFER_BOX_LINE_SEGMENT
// is mandatory. Optionally if SRE_BOUNDS_PREFER_SPECIAL is set it indicates the
// preferred bounding volume is stored in bv_special_type;
// SRE_BOUNDS_PREFER_AABB is set when an AABB check is preferable over a regular
// (oriented) box check (independent of the other bounds flags).
// When a model is instantiated as an object, the bounds flags of the model are
// copied into so.box.flags (although only the BOUNDS_PREFER_BOX and
// BOUNDS_PREFER_BOX_LINE_SEGMENT are relevant to this field), and additionally for
// the static objects the flag SRE_BOUNDS_IS_AXIS_ALIGNED is set in so.box.flags if
// the model has SRE_BOUNDS_PREFER_AABB set and the object instantiation is not
// rotated or is rotated in such a way (multiples of 90 degrees) that an AABB
// (when calculated during octree creation) will still be optimal.

enum {
    SRE_BOUNDS_PREFER_SPHERE = 1,
    SRE_BOUNDS_PREFER_BOX = 2,
    SRE_BOUNDS_PREFER_BOX_LINE_SEGMENT = 4,
    SRE_BOUNDS_PREFER_AABB = 8,
    SRE_BOUNDS_PREFER_SPECIAL = 16,
    SRE_BOUNDS_IS_AXIS_ALIGNED = 32,
    SRE_BOUNDS_SPECIAL_SRE_COLLISION_SHAPE = 64
};

enum {
    SRE_COLLISION_SHAPE_STATIC, SRE_COLLISION_SHAPE_CONVEX_HULL,
    SRE_COLLISION_SHAPE_SPHERE, SRE_COLLISION_SHAPE_BOX,
    SRE_COLLISION_SHAPE_CYLINDER, SRE_COLLISION_SHAPE_ELLIPSOID,
    SRE_COLLISION_SHAPE_CAPSULE
};

// The highest-level model class, which is seperate and does not derive from the
// sreBase/LODModel classes. The actual base model data/GPU information is contained
// in the lod_model[] array of pointers to LOD sub-models. At least lod_model[0] must
// exist.
//
// Instances of this class can be used as a model parameter for an object.
// It contains things like bounding volumes, collision shapes and LOD information.

class SRE_API sreModel {
public:
    int model_flags;
    // Identification.
    int id;
    bool referenced;   // Model is referred to by a scene object.
    // Preprocessing stage structures.
    int nu_polygons;
    bool is_static;
    sreModelPolygon *polygon;
    // Bounding volume variables.
    int bounds_flags;
    // PCA components (R, S and T). Includes normalized PCA vector (PCA[i].vector)
    // and its size (PCA[i].size).
    srePCAComponent PCA[3];
    // Sphere and oriented bounding box are always present.
    sreBoundingVolumeSphere sphere;
    Point3D box_center; // OBB center; box uses PCA components.
    // Optional special bounding volume.
    sreBoundingVolume bv_special;
    // AABB.
    sreBoundingVolumeAABB AABB;
    // Collision/physics structures.
    int collision_shape_static;   // Collision shape type for static instances of this object.
    int collision_shape_dynamic;  // Collision shape type for dynamic instances of this object.
    sreBoundingVolume *special_collision_shape; // Parameters for special collision shapes.
    // Level of detail management.
    int nu_lod_levels;
    // Theshold scaling inherent to the LOD model. This will be compounded with any
    // treshold scaling defined for the referring sreObject.
    float lod_threshold_scaling;
    sreLODModel *lod_model[SRE_MAX_LOD_LEVELS];

    sreModel();
    ~sreModel();
    sreModel *CreateNewInstance() const;
    void CalculatePrincipalComponents();
    void CalculateBoundingSphere();
    void CalculateBoundingBox();
    void CalculateBoundingEllipsoid(sreBoundingVolumeEllipsoid& ellipsoid) const;
    void CalculateBoundingCylinder(sreBoundingVolumeCylinder& cylinder) const;
    void SetBoundingCollisionShapeCapsule(const sreBoundingVolumeCapsule& capsule);
    void CalculateAABB();
    void CalculateBounds();
    void SetOBBWithAABBBounds(const sreBoundingVolumeAABB& _AABB);
    void Triangulate();
    void InsertPolygonVertex(int p, int i, const Point3D& Q, float t);
    void GetMaxExtents(sreBoundingVolumeAABB *AABB, float *max_dim);
    void SetLODModelFlags(int flag_mask);
    void ClearLODModelFlags(int flag_mask);
};

// Data structures for lights.

enum {
    // Light type flags.
    SRE_LIGHT_DIRECTIONAL = 1,
    SRE_LIGHT_POINT_SOURCE = 2,
    SRE_LIGHT_SPOT = 4,
    SRE_LIGHT_BEAM = 8,
    // The light attenuation is a linear function of the distance from the light
    // (generally always enabled).
    SRE_LIGHT_LINEAR_ATTENUATION_RANGE = 16,
    // The position of a point, spot or beam light is not fixed.
    SRE_LIGHT_DYNAMIC_POSITION = 32,
    // The direction of a directional, spot or beam light is not fixed.
    SRE_LIGHT_DYNAMIC_DIRECTION = 64,
    // The spot exponent of a spot light is not fixed.
    SRE_LIGHT_DYNAMIC_SPOT_EXPONENT = 128,
    // The light attenuation (range) is not fixed.
    SRE_LIGHT_DYNAMIC_ATTENUATION = 256,
    // The light's geometrical shadow volume changes shape or size for a static object.
    SRE_LIGHT_DYNAMIC_SHADOW_VOLUME = 0x10000,
    // The light's light volume changes shape or size for a static object.
    SRE_LIGHT_DYNAMIC_LIGHT_VOLUME = 0x20000,
    // The worst case bounding sphere of the light volume defines the maximum sphere of
    // influence an otherwise variable light.
    SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE = 0x40000,
    // A worst case spherical sector is defined for an otherwise variable light. This is not
    // yet fully implemented.
    SRE_LIGHT_WORST_CASE_BOUNDS_SECTOR = 0x80000,
    // A list of static objects affected by the light has been calculated.
    SRE_LIGHT_STATIC_OBJECTS_LIST = 0x100000,
    // A list of static shadow-casting objects for the light has been calculated.
    SRE_LIGHT_STATIC_SHADOW_CASTER_LIST = 0x200000
};

enum {
   SRE_LIGHT_TYPE_DIRECTIONAL = 0,
   SRE_LIGHT_TYPE_POINT_SOURCE,
   SRE_LIGHT_TYPE_SPOT,
   SRE_LIGHT_TYPE_BEAM
};

// The number of the light types that can affect the selected shader for multi-pass
// rendering is limited.

#define SRE_NU_SHADER_LIGHT_TYPES 6

enum {
    SRE_SHADER_LIGHT_TYPE_DIRECTIONAL = 0,
    SRE_SHADER_LIGHT_TYPE_POINT_SOURCE_LINEAR_ATTENUATION,
    SRE_SHADER_LIGHT_TYPE_POINT_SOURCE_REGULAR_ATTENUATION,
    SRE_SHADER_LIGHT_TYPE_SPOT,
    SRE_SHADER_LIGHT_TYPE_BEAM,
    SRE_SHADER_LIGHT_TYPE_AMBIENT
};

// Use the first slot for single pass rendering or non-lighting shaders.
#define SRE_SHADER_LIGHT_TYPE_ALL SRE_SHADER_LIGHT_TYPE_DIRECTIONAL

class SRE_API sreLight {
public:
    int type;             // This field should be renamed flags.
    int id;
    char shader_light_type;
    char type_index;
    Vector4D vector;      // Direction for directional lights (w == 0), positions for other lights (w == 1.0).
    Vector3D attenuation; // Constant, linear and quadratic terms, or a single linear attenation range.
    Color color;          // RGB values from 0 to beyond 1.0.
    Vector4D spotlight;   // Spotlight direction, plus exponent in w coordinate. For beam light direction, plus
                          // circular radius in w coordinate.
    float spill_over_factor; // Light spill-over factor for directional lights.
    sreBoundingVolumeSphere sphere;
    sreBoundingVolumeSphericalSector spherical_sector;
    sreBoundingVolumeCylinder cylinder;
    sreBoundingVolumeAABB AABB; // For static positional lights.
    // Worst case bounding volume for variable lights.
    sreBoundingVolumeSphere worst_case_sphere;
    sreBoundingVolumeSphericalSector worst_case_sector;
    // Light volume object array for static position lights. The objects that are partially inside
    // inside the light volume come first, followed by the objects that are completely inside
    // the light volume.
    int nu_light_volume_objects;
    int nu_light_volume_objects_partially_inside;
    int *light_volume_object;
    int *shadow_caster_object;
    int nu_shadow_caster_objects;
    // State variables for shadow volume cache optimization.
    int most_recent_shadow_volume_change;
    bool changing_every_frame;
    float projected_size;
    // Set to true when there are no shadow receivers for the current frame when shadow mapping
    // is enabled.
    bool shadow_map_required;

    sreLight();
    ~sreLight();
    void CalculateSpotLightCylinderRadius(); // For spot lights.
    void CalculateBoundingVolumes();
    void CalculateLightVolumeAABB(sreBoundingVolumeAABB& AABB_out);
    void CalculateWorstCaseLightVolumeAABB(sreBoundingVolumeAABB& AABB_out);
    // Whether the GPU shadow volumes of the light's shadow casters are changing every frame due
    // to light changes. This reflects changes to the GPU's shadow volumes that are extruded to
    // infinity; just changing the range (attenuation) of the light does not affect it, while that
    // does affect the geometrical (capped) shadow volumes for positional lights.
    bool ShadowVolumeIsChangingEveryFrame(int current_frame) const {
        return changing_every_frame && (most_recent_shadow_volume_change == current_frame); 
    }
};

class sreFrustum;

// Scissors region used for GPU scissors optimization.

enum sreScissorsRegionType{
    SRE_SCISSORS_REGION_EMPTY,
    SRE_SCISSORS_REGION_UNDEFINED,
    SRE_SCISSORS_REGION_DEFINED
};

class sreScissors {
public :
    float left, right, bottom, top;  // Given in normalized device coordinates ([-1, 1]).
    double near, far; // Given as [0, 1].

    void SetFullRegion() {
        left = - 1.0f;
        right = 1.0f;
        bottom = - 1.0f;
        top = 1.0f;
    }
    void SetFullRegionAndDepthBounds() {
        SetFullRegion();
        near = 0;
        far = 1.0f;
    }
    void SetEmptyRegion() {
        near = 1.0f;
        far = 0;
        left = 1.0f;
        right = - 1.0f;
        bottom = 1.0f;
        top = - 1.0f;
    }
    void ClampRegionToScreen() {
        if (right > 1.0f)
           right = 1.0f;
        if (left < - 1.0f)
           left = - 1.0f;
        if (top > 1.0f)
           top = 1.0f;
        if (bottom < - 1.0f)
           bottom = - 1.0f;
    }
    void ClampRegionAndDepthBounds() {
        ClampRegionToScreen();
        if (far > 1.0f)
           far = 1.0f;
        if (near < 0)
           near = 0;
    }
    void ClampEmptyRegion() {
        if (near > far)
            near = far = 0;
        if (left > right)
            left = right = 0;
        if (bottom > top)
            bottom = top = 0;
    }
    void UpdateRegionWithIntersection(const sreScissors& scissors) {
        left = maxf(left, scissors.left);
        right = minf(right, scissors.right);
        bottom = maxf(bottom, scissors.bottom);
        top = minf(top, scissors.top);
    }
    void UpdateRegionWithUnion(const sreScissors& scissors) {
        left = minf(left, scissors.left);
        right = maxf(right, scissors.right);
        bottom = minf(bottom, scissors.bottom);
        top = maxf(top, scissors.top);
    }
    bool RegionIsEmpty() const {
        return left >= right || bottom >= top;
    }
    bool IsEmptyOrOutside() const {
        if (near >= far)
            return true;
        if (left >= right)
            return true;
        if (bottom >= top)
            return true;
        if (left >= 1.0f)
            return true;
        if (right <= - 1.0f)
            return true;
        if (bottom >= 1.0f)
            return true;
        if (top <= - 1.0f)
            return true;
        return false; 
    }
    bool ScissorsRegionIsEqual(const sreScissors& scissors) const {
        return left == scissors.left && right == scissors.right &&
            bottom == scissors.bottom && top == scissors.top;
    }
    bool DepthBoundsAreEqual(const sreScissors& scissors) const {
        return near == scissors.near && far == scissors.far;
    }
    float Area() const {
        if (RegionIsEmpty())
            return 0.0f;
        return (right - left) * (top - bottom);
    }
    void UpdateWithProjectedPoint(float x, float y, double z);
    void UpdateWithWorldSpaceBoundingHull(Point3DPadded *P, int n);
    bool UpdateWithWorldSpaceBoundingBox(Point3DPadded *P, int n, const sreFrustum& frustum);
    bool UpdateWithWorldSpaceBoundingPolyhedron(Point3DPadded *P, int n, const sreFrustum& frustum);
    sreScissorsRegionType UpdateWithWorldSpaceBoundingPyramid(Point3DPadded *P, int n, const sreFrustum& frustum);
    void SetGL();
    void Print();
};

// Level-Of-Detail flags for an object (sreObject::lod_flags).

enum {
    SRE_LOD_DYNAMIC = 1,
    SRE_LOD_FIXED = 2,
    SRE_LOD_MAX_LIMITED = 4,
    SRE_LOD_PHYSICS_HIGHEST = 8
};

// Object position/orientation change flags (used in sreObject::rapid_change_flags).

enum {
    SRE_OBJECT_POSITION_CHANGE = 1,
    SRE_OBJECT_TRANSFORMATION_CHANGE = 2
};

class sreSceneEntityList;

// These flags apply to scene objects (sreObject::flags).
enum {
    // Texture/attribute flags.
    SRE_OBJECT_USE_TEXTURE = 0x1,
    SRE_OBJECT_USE_NORMAL_MAP = 0x2,
    SRE_OBJECT_USE_SPECULARITY_MAP = 0x4,
    SRE_OBJECT_USE_EMISSION_MAP = 0x8,
    SRE_OBJECT_MULTI_COLOR = 0x10,
    SRE_OBJECT_TRANSPARENT_TEXTURE = 0x20,
    SRE_OBJECT_TRANSPARENT_EMISSION_MAP = 0x40,
    SRE_OBJECT_3D_TEXTURE = 0x80,
    SRE_OBJECT_EMISSION_ONLY = 0x100,
    SRE_OBJECT_EMISSION_ADD_DIFFUSE_REFLECTION_COLOR = 0x200,
    // Hidden surface removal/shadows.
    // Infinite distance objects do not occlude anything and are not included in
    // the main static scenery octree.
    SRE_OBJECT_INFINITE_DISTANCE = 0x1000,
    SRE_OBJECT_NO_BACKFACE_CULLING = 0x2000,
    // This flag must be set for every object that can cast shadows.
    SRE_OBJECT_CAST_SHADOWS = 0x4000,
    // Object never occludes any other object (except INFINITE_DISTANCE objects)
    // from any allowable camera viewpoint.
    SRE_OBJECT_NOT_OCCLUDING = 0x8000,
    // Always use the object shadow cache (as opposed to model shadow cache which
    // has limited space), even for directional lights. Use when multiple objects
    // with different fixed rotation/scaling of the same model are used. For
    // dynamic objects that are affected by physics, this flag is automatically
    // set when scene->AddObject() is called.
    SRE_OBJECT_USE_OBJECT_SHADOW_CACHE = 0x10000,
    // Physics/movement.
    SRE_OBJECT_DYNAMIC_POSITION = 0x20000, // The position is not fixed. (Implies physics?)
    SRE_OBJECT_NO_PHYSICS = 0x40000,       // No physics (no collisions/movement).
    SRE_OBJECT_KINEMATIC_BODY = 0x80000,
    // Miscellaneous.
    SRE_OBJECT_EARTH_SHADER = 0x100000,
    SRE_OBJECT_BILLBOARD = 0x200000,      // Object is a billboard (with a single billboard model).
    SRE_OBJECT_LIGHT_HALO = 0x400000,     // For a halo, SRE_OBJECT_BILLBOARD should not be set.
    // Object is a particle system, which is a set of billboards.
    SRE_OBJECT_PARTICLE_SYSTEM = 0x800000,
    // Object is hidden (not rendered), but can interact in terms of physics or serve
    // as control object or attachment point for lights, and may cast shadows.
    SRE_OBJECT_HIDDEN = 0x1000000,
    // Object is animated. For semi-static objects like fluid (DYNAMIC_POSITION not set), 
    // reprocessing is skipped.
    SRE_OBJECT_ANIMATED = 0x2000000,
    // The open side of an object with a non-closed model is always hidden from lights.
    SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_LIGHT = 0x4000000,
    // The open side of an object with a non-closed model is always hidden from view.
    SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_VIEW = 0x8000000
};

class sreScissorsCacheEntry : public sreScissors {
public :
    int light_id;
};

// The main object class (an object in the scene); refers to the model used,
// and contains all other non-model specific information.

class SRE_API sreObject {
public:
    // Pointer to the object definition of which sreObject is an instantiation.
    sreModel *model;
    int id;
    bool exists;
    // Instantiation parameters for world space position.
    Point3D position;
    Vector3D rotation;
    float scaling;
    bool only_translation;
    // Misc. attributes.
    int flags;        // Object flags
    int render_flags; // Object flags after applying global settings mask.
    // Cached shader information for each light type that needs to be seperated.
    int current_shader[SRE_NU_SHADER_LIGHT_TYPES];
    // Because we would like to select a non-shadow map shader when shadow
    // mapping is enabled, for example when there are no light casters for the light
    // or the object doesn't receive shadows, maintain a seperate list of shadow-map
    // shaders.
    int current_shader_shadow_map[SRE_NU_SHADER_LIGHT_TYPES];
    sreObjectAttributeInfo attribute_info;   // Lighting pass vertex attribute information.
    sreObjectAttributeInfo attribute_info_ambient_pass; // Ambient pass attribute information.
    sreObjectAttributeInfo attribute_info_shadow_map; // Attribute info for shadow map shaders.
    // Level-of-detail settings.
    // lod_flags determines whether the LOD level is fixed to one level or dynamically
    // determined.
    unsigned char lod_flags;
    unsigned char min_lod_level; // The lowest LOD level used.
    unsigned char max_lod_level; // The highest LOD level used.
    unsigned char physics_lod_level; // The LOD level used for physics geometry.
    // A value higher than 1.0 means the LOD threshold are scaled up
    // (less detailed LOD levels will be triggered for smaller on-screen
    // size). Only applies to dynamic LOD.
    float lod_threshold_scaling;
    // Whether the object has a light attached to it.
    int attached_light;
    Vector3D attached_light_model_position;
    Vector3D attached_light_model_direction;
    // Texture and lighting attributes.
    Color diffuse_reflection_color;
    Color specular_reflection_color;
    float specular_exponent;
    Color emission_color;
    float diffuse_fraction;
    Vector2D roughness_values;
    Vector2D roughness_weights;
    bool anisotropic;
    sreTexture *texture;
    sreTexture *specularity_map;
    sreTexture *normal_map;
    sreTexture *emission_map;
    // UV transformation matrix allows mirroring or selecting region within source texture
    // to apply to the object.
    const Matrix3D *UV_transformation_matrix;
    float texture3d_scale;
    int texture3d_type;
    float billboard_width;
    float billboard_height;
    float halo_size;
    // Particle system.
    int nu_particles;
    Vector3D *particles;  // Displacements from the particle system position.
    // Model-space transformation matrices.
    MatrixTransform model_matrix;    // Transforms from model space to world space.
    Matrix4D inverted_model_matrix;  // Transforms from world space to model space.
    Matrix3D rotation_matrix;        // Just the rotation (can be used for normals).
    // Bounding volumes (dynamic)
    sreBoundingVolumeSphere sphere;
    sreBoundingVolumeBox box;
    sreBoundingVolume bv_special;
    // Static axis-aligned box bounding volume.
    sreBoundingVolumeAABB AABB;
    // Collision detection/physics variables.
    float mass;
    Vector3D collision_shape_center_offset;
    Matrix3D *original_rotation_matrix;
    sreSceneEntityList *octree_list;
    // Keeping track of the frequency of position and orientation changes.
    int most_recent_position_change;
    int most_recent_transformation_change;
    int most_recent_change;
    int rapid_change_flags;
    // Precalculated static shadow volumes (pyramids or half cylinders).
    int nu_shadow_volumes;
    sreShadowVolume **shadow_volume;
    // Rendering attributes.
    // Cache of geometry scissors for static lights.
    sreScissorsCacheEntry *geometry_scissors_cache;
    // The object-specific order of the static light currently being rendered.
    int static_light_order;
    // Time stamp to determine whether the first object-affecting light of a new frame
    // has been reached.
    int geometry_scissors_cache_timestamp;
    float projected_size;
    // The frame number when the object was last determined to be visible.
    int most_recent_frame_visible;

    sreObject();
    ~sreObject();
    sreModel *ConvertToStaticScenery() const;
    void CalculateAABB();
    bool IntersectsWithLightVolume(const sreLight& light) const;
    BoundsCheckResult CalculateGeometryScissors(const sreLight& light, const sreFrustum &frustum,
        sreScissors& scissors);
    bool CalculateShadowVolumeScissors(const sreLight& light, const sreFrustum& frustum,
        const sreShadowVolume& sv, sreScissors& shadow_volume_scissors) const;
    sreBoundingVolumeType CalculateShadowVolumePyramid(const sreLight& light, Point3D *Q,
        int &n_convex_hull) const;
    sreBoundingVolumeType CalculatePointSourceOrSpotShadowVolume(const sreLight& light, Point3D *Q,
        int &n_convex_hull, Vector3D& axis, float& radius, float& cos_half_angular_size) const;
    sreBoundingVolumeType CalculateShadowVolumeHalfCylinderForDirectionalLight(const sreLight &light,
        Point3D &E, float& cylinder_radius, Vector3D& cylinder_axis) const;
    sreBoundingVolumeType CalculateShadowVolumeCylinderForBeamLight(
        const sreLight& light, Point3D& center, float& length, Vector3D& cylinder_axis,
        float& cylinder_radius) const;
    void AddShadowVolume(sreShadowVolume *sv);
    sreShadowVolume *LookupShadowVolume(int light_index) const;
    void CalculateTemporaryShadowVolume(const sreLight& light, sreShadowVolume **sv) const;
    bool IsChangingPositionEveryFrame(int current_frame) const {
        return (rapid_change_flags & SRE_OBJECT_POSITION_CHANGE)
            && (most_recent_position_change == current_frame); 
    }
    bool IsChangingTransformationEveryFrame(int current_frame) const {
        return (rapid_change_flags & SRE_OBJECT_TRANSFORMATION_CHANGE)
            && (most_recent_transformation_change == current_frame); 
    }
    bool IsChangingEveryFrame(int current_frame) const {
        return IsChangingPositionEveryFrame(current_frame) ||
            IsChangingTransformationEveryFrame(current_frame);
    }
    bool PositionHasChangedSinceLastFrame(int current_frame) const {
        return most_recent_position_change == current_frame;
    }
};

// Linked list of integer ids (used for objects in a few cases; however
// most data structures are array-based for performance).

class sreObjectListElement {
public :
    int so;
    sreObjectListElement *next;
};

class sreObjectList {
public :
   sreObjectListElement *head;
   sreObjectListElement *tail;

   sreObjectList();
   void AddElement(int so);
   void DeleteElement(int so);
   int Pop();
   void MakeEmpty();
};

// Types for octree storage (only used during octree creation).

enum sreSceneEntityType { SRE_ENTITY_OBJECT, SRE_ENTITY_LIGHT };

class sreSceneEntity {
public :
    int type;
    union {
        sreObject *so;
        sreLight *light;
    };
};

class sreSceneEntityListElement {
public :
    sreSceneEntity *entity;
    sreSceneEntityListElement *next;
};

class sreSceneEntityList {
public :
   sreSceneEntityListElement *head;
   sreSceneEntityListElement *tail;

   sreSceneEntityList();
   void AddElement(sreSceneEntity *entity);
   void DeleteElement(sreSceneEntity *entity);
   void DeletesreObject(sreObject *so);
   sreSceneEntity *Pop();
   void MakeEmpty();
};

class sreOctreeNodeBounds {
public :
    sreBoundingVolumeAABB AABB;
    sreBoundingVolumeSphere sphere;
};

// Optimized octree.

class SRE_API sreFastOctree {
public :
    sreOctreeNodeBounds *node_bounds;
    unsigned int *array;

    int GetNumberOfOctants(int offset) const {
	return array[offset];
    }
    int GetNumberOfOctantsStrict(int offset) const {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || !defined(__BYTE_ORDER__)
        return array[offset] & 0xFF;
#else
        return (char)array[offset];
#endif
    }
    void GetEntity(int offset, sreSceneEntityType& type, int& index) const {
        unsigned int value = array[offset];
        if (value & 0x80000000) {
            type = SRE_ENTITY_LIGHT;
            index = value & 0x7FFFFFFF;
            return;
        }
        type = SRE_ENTITY_OBJECT;
        index = value;
    }
    void Destroy();
};

// The viewing frustum type.

#define SRE_LIGHT_POSITION_IN_NEAR_PLANE 1
#define SRE_LIGHT_POSITION_IN_FRONT_OF_NEAR_PLANE 2
#define SRE_LIGHT_POSITION_BEHIND_NEAR_PLANE 4
#define SRE_LIGHT_POSITION_POINT_LIGHT 8

class sreFrustum {
public:
    // The most recent frame when the frustum changed.
    int most_recent_frame_changed;
    bool changing_every_frame;
    sreBoundingVolumeFrustum frustum_world;
    sreBoundingVolumeFrustum frustum_eye;
    sreBoundingVolumeFrustum frustum_without_far_plane_world;
    // Volumes calculated for the current light.
    sreBoundingVolumeConvexHull near_clip_volume;
    sreBoundingVolumeConvexHull shadow_caster_volume;
//    Plane shadow_caster_plane[12];
    int shadow_caster_edge[8][2];
    // Projection parameters for the frustum.
    float ratio, angle, nearD, farD;
    float nw, nh, fw, fh;
    float e; // Image plane distance.
    float cos_max_half_angular_size;
    float sin_max_half_angular_size;
    // Light near clip-volume variables.
    int light_position_type;
    // Light scissors region variables.
    sreScissors scissors;
    // Shadow caster volume variables.
    int nu_shadow_caster_edges;
    // Maximum extents of region in world space in which objects cast shadows for a directional
    // light when shadow mapping is enabled. Updated by Calculate() function based on the
    // library's internally defined eye-space shadow map region.
    sreBoundingVolumeAABB shadow_map_region_AABB;

    sreFrustum();
    // Set frustum projection parameters.
    void SetParameters(float angle, float ratio, float nearD, float farD);
    // Calculate frustum plane data based on current view matrix and projection parameters.
    // The shadow map region for shadow mapping (directional lights) may also be calculated.
    void Calculate();
    void CalculateNearClipVolume(const Vector4D& lightpos);
    void CalculateShadowCasterVolume(const Vector4D& lightpos, int nu_frustum_planes);
    void CalculateLightScissors(sreLight *light);
    // sreFrustum-specific intersection tests.
    // The const behind the function definition means the sreFrustum structure remains constant.
    bool ObjectIntersectsNearClipVolume(const sreObject& so) const;
    bool ShadowVolumeIsOutsideFrustum(sreShadowVolume& sv) const;
    bool DarkCapIsOutsideFrustum(sreShadowVolume& sv) const;
    // Whether object is visible in the current frame, based on time-stamp comparison
    // of when the frustum last changed and the object's most recent visibility determination.
    bool ObjectIsVisibleInCurrentFrame(const sreObject& so, int current_frame) const {
        if (so.flags & SRE_OBJECT_DYNAMIC_POSITION)
            // For dynamic objects, visibility is determined each frame.
            return so.most_recent_frame_visible == current_frame;
        else
            // For static objects, visibility is only determined when the frustum changes.
            return so.most_recent_frame_visible >= most_recent_frame_changed;
    }
    bool IsChangingEveryFrame(int current_frame) const {
        return changing_every_frame && (most_recent_frame_changed == current_frame); 
    }
};

// Camera viewpoint and movement control type.

enum sreViewMode { SRE_VIEW_MODE_STANDARD, SRE_VIEW_MODE_FOLLOW_OBJECT, SRE_VIEW_MODE_LOOK_AT };
enum sreMovementMode { SRE_MOVEMENT_MODE_STANDARD, SRE_MOVEMENT_MODE_USE_FORWARD_AND_ASCEND_VECTOR,
    SRE_MOVEMENT_MODE_NONE };

class SRE_API sreView {
    // Fields declared privately because changes are detected by setting functions.
private :
    int last_view_change;       // Frame when the view mode was last changed.
    int last_projection_change; // Frame when the projection was last changed.
    sreViewMode view_mode;     // Camera view mode.
    int followed_object;
    float following_distance;
    Vector3D following_offset;
    float zoom;
    Vector3D angles;        // View angles; the y-axis angle is currently ignored.
    Point3D viewpoint;
    Point3D view_lookat;
    Vector3D view_upvector;
    Vector3D view_direction; // Calculated from either viewing angles, or
                             // viewpoint/lookat position (SRE_VIEW_MODE_LOOK_AT).
    sreMovementMode movement_mode;
    Vector3D forward_vector; // For movement.
    Vector3D ascend_vector;

public :
    sreView();
    void SetViewModeStandard(Point3D viewpoint);
    void SetViewModeFollowObject(int object_index, float distance, Vector3D offset);
    void SetViewModeLookAt(Point3D viewpoint, Point3D view_lookat, Vector3D view_upvector);
    void SetViewAngles(Vector3D angles);
    void RotateViewDirection(Vector3D angles_offset);
    void SetZoom(float _zoom);
    // Update viewpoint, lookat position, up vector and view direction based on current view mode.
    // If view mode is SRE_VIEW_MODE_FOLLOW_OBJECT, the object's positions must be specified;
    // otherwise the argument is ignored.
    void UpdateParameters(Point3D object_position);
    void CalculateViewDirection();
    sreViewMode GetViewMode() const {
        return view_mode;
    }
    Point3D GetViewPoint() const {
        return viewpoint;
    }
    void GetViewAngles(Vector3D &_angles) const {
       _angles = angles;
    }
    float GetZoom() const {
        return zoom;
    }
    Point3D GetLookatPosition() const {
        return view_lookat;
    }
    Vector3D GetUpVector() const {
        return view_upvector;
    }
    int GetFollowedObject() const {
        return followed_object;
    }
    void GetFollowedObjectParameters(float& distance, Vector3D& offset_vector) const {
        distance = following_distance;
        offset_vector = following_offset;
    }
    bool ViewHasChangedSinceLastFrame(int current_frame) const {
        return (last_view_change == current_frame);
    }
    bool ProjectionHasChangedSinceLastFrame(int current_frame) const {
        return (last_projection_change == current_frame);
    }
    bool MostRecentFrameChanged() {
        if (last_view_change > last_projection_change)
            return last_view_change;
        else
            return last_projection_change;
    }
    void SetMovementMode(sreMovementMode mode);
    void SetForwardVector(const Vector3D& forward);
    void SetAscendVector(const Vector3D& ascend);
    sreMovementMode GetMovementMode() const {
        return movement_mode;
    }
    Vector3D GetForwardVector() const {
        return forward_vector;
    }
    Vector3D GetAscendVector() const {
        return ascend_vector;
    }
};

// Flags for sreScene::PrepareForRendering(flags).

enum {
    SRE_PREPARE_PREPROCESS = 1,
    SRE_PREPARE_UPLOAD_ALL_MODELS = 2,
    SRE_PREPARE_UPLOAD_NO_MODELS = 4,
    SRE_PREPARE_REUSE_OCTREES = 8
};

// The top-level scene class, contain arrays of the objects and lights in the scene,
// octree information, a model registry, data structures used during rendering, and
// state variables used during scene construction.
;
typedef dstCastDynamicArray <sreModel *, void *, uint32_t, dstPointerArray> sreModelPointerArray;

class SRE_API sreScene {
public:
    int nu_objects;

    // The objects in the scene.
    sreObject **object;
    // A registry of all higher-level models.
    sreModelPointerArray models;
//    int nu_models;
//    sreModel **model;
//    int max_models;
    int max_objects;
    int nu_lod_models;
    sreFastOctree fast_octree_static;
    sreFastOctree fast_octree_static_infinite_distance;
    sreFastOctree fast_octree_dynamic;
    sreFastOctree fast_octree_dynamic_infinite_distance;
    int nu_root_node_objects;
    // Array of (non-final pass) visible objects, updated each frame.
    int *visible_object;
    // Array of final-pass visible objects, updated each frame.
    int *final_pass_object;
    // Array of shadow caster objects, updated for each light within each frame.
    dstIntArray shadow_caster_array;
    // Array of visible lights, updated each frame.
    dstIntArray visible_light_array;
    int nu_visible_objects;
    int nu_final_pass_objects;
    int max_visible_objects;
    int max_final_pass_objects;
    // Lights
    Color ambient_color;
    int nu_lights;
    int max_scene_lights;
    sreLight **light;
    // Active lights for shaders that are limited by the number of lights.
    int nu_active_lights;
    int active_light[SRE_MAX_ACTIVE_LIGHTS];
    // List of deleted scene objects.
    sreObjectList *deleted_ids;
    // State variables used during scene construction.
    Color current_diffuse_reflection_color;
    int current_flags;
    Color current_specular_reflection_color;
    float current_specular_exponent;
    sreTexture *current_texture;
    sreTexture *current_specularity_map;
    sreTexture *current_normal_map;
    sreTexture *current_transparency_map;
    sreTexture *current_emission_map;
    // The UV transformation matrix is a dynamically allocated structure.
    const Matrix3D *current_UV_transformation_matrix;
    Color current_emission_color;
    int current_texture3d_type;
    float current_texture3d_scale;
    float current_billboard_width;
    float current_billboard_height;
    float current_halo_size;
    float current_mass;
    float current_diffuse_fraction;
    Vector2D current_roughness_values;
    Vector2D current_roughness_weights;
    bool current_anisotropic;
    int current_lod_flags;
    int current_min_lod_level;
    int current_max_lod_level;
    float current_lod_threshold_scaling;
    int current_physics_lod_level;

    sreScene(int max_objects, int max_models, int max_lights);
    ~sreScene();
    // Make scene empty. Models are not affected.
    void ClearObjectsAndLights();
    // Free all models.
    void ClearModels();
     // Functions used for scene construction.
    void SetColor(Color color); // Same SetDiffuseReflectionColor
    void SetFlags(int flags);
    void SetDiffuseReflectionColor(Color color);
    void SetSpecularReflectionColor(Color color);
    void SetSpecularExponent(float exponent);
    void SetTexture(sreTexture *texture);
    void SetSpecularityMap(sreTexture *texture);
    void SetNormalMap(sreTexture *texture);
    void SetTransparencyMap(sreTexture *texture);
    void SetEmissionColor(Color color);
    void SetEmissionMap(sreTexture *texture);
    void SetTexture3DTypeAndScale(int type, float scale);
    // Set UV transformation matrix, which must be dynamically allocated. NULL selects the
    // standard (identity) transformation.
    void SetUVTransform(Matrix3D *matrix);
    void SetBillboardSize(float width, float height);
    void SetHaloSize(float size);
    void SetMass(float mass);
    void SetMicrofacetParameters(float diffuse_fraction, float roughness_value1, float weigth1,
        float roughness_value2, float weight2, bool anisotropic);
    // Set level of detail for object, referring to the levels of detail in the model.
    // The minimum (most detailed) level is min_level; if the SRE_LOD_FIXED flag is set
    // only min_level is used. If SRE_LOD_DYNAMIC is set, higher, less detailed levels
    // (up to the maximum defined in the model) will be used as the on-screen size of the
    // object decreases; a threshold scaling value higher than 1.0 will accelerate the
    // selection of less detailed LOD levels. If the SRE_LOD_MAX_LIMITED flag is set with
    // SRE_LOD_DYNAMIC, max_level specifies the highest (least detailed) level to be used
    // which may not be the highest level. Finally, physics_level is the LOD level used
    // for static scenery physics collision detection (for objects that have a geometrical
    // collision shape, this is ignored).
    void SetLevelOfDetail(int flags, int min_level, int max_level, float threshold_scaling,
        int physics_level);
    // AddObject returns the model id which can be used to subsequently change object
    // parameters.
    // The following function is deprecated; used the following one.
    int AddObject(sreModel *model, float dx, float dy, float dz, float rot_x, float rot_y,
        float rot_z, float scaling);
    // Add an object instantiation of a model at position pos, with specified rotation
    // angles (radians) and scaling.
    int AddObject(sreModel *model, Point3D pos, Vector3D rot_along_axi, float scaling);
    // Lights.
    void SetAmbientColor(Color color);
    int AddDirectionalLight(int flags, Vector3D direction, Color color);
    int AddPointSourceLight(int flags, Point3D position, float linear_range, Color color);
    int AddSpotLight(int flags, Point3D position, Vector3D direction, float exponent,
        float linear_range, Color color);
    int AddBeamLight(int flags, Point3D position, Vector3D direction, float beam_radius,
        float radial_linear_range, float cutoff_distance, float linear_range, Color color);
    void SetDirectionalLightSpillOverFactor(int i, float factor) const;
    void ChangeDirectionalLightDirection(int i, Vector3D direction) const;
    void ChangeLightPosition(int i, Point3D position) const;
    void ChangeLightColor(int i, Color color) const;
    void ChangeSpotOrBeamLightDirection(int i, Vector3D direction) const;
    void ChangePointSourceLightAttenuation(int i, float range) const;
    void ChangeSpotLightAttenuationAndExponent(int i, float range, float exponent) const;
    void ChangeBeamLightAttenuation(int i, float beam_radius, float radial_linear_range,
        float cutoff_distance, float linear_range) const;
    // Enable a worst-case bounding sphere for a positional light's light volume for lights
    // with variable direction (SRE_LIGHT_DYNAMIC_DIRECTION), attenuation
    // (SRE_LIGHT_DYNAMIC_ATTENUATION) or even position (SRE_LIGHT_DYNAMIC_POSITION) that is bounded
    // within a fixed space. This should be used during scene creation. Since this defines
    // the objects that are included in the fixed list of static objects within the light
    // volume that is used during rendering, when the total number of static objects in this
    // volume is much larger than the typical number of objects in the actual light volume,
    // an excessive number of light volume intersection tests may be performed each frame; in
    // that case worst case bounds should not be set, so that optimized traversal of the scene
    // octrees will be used to determine objects within the light volume. When a beam or spot light
    // is added with just SRE_LIGHT_DYNAMIC_DIRECTION, the worst case bounding sphere will
    // already be enabled assuming full angular range of the axis.
    void SetLightWorstCaseBounds(int i, const sreBoundingVolumeSphere& sphere);
    // Set a worst case bounding volume for a point light with variable range and position. Since
    // both parameters are defined in terms of a sphere, the resulting worst case volume will
    // also be a sphere.
    void SetPointLightWorstCaseBounds(int i, float max_range,
        const sreBoundingVolumeSphere& position_sphere);
    // Set a worst case bounding volume (which may be just a sphere, ideally it would
    // be more optimized) for a spot light with variable direction, spot light exponent, range or
    // position; max_direction_angle is in radians, position sphere represents the space within
    // which the light's center position may be moved (for a fixed position, use a sphere centered
    // at the position with radius of zero).
    void SetSpotLightWorstCaseBounds(int i, float max_direction_angle, float min_exponent,
        float max_range, const sreBoundingVolumeSphere& position_sphere);
    // Set a worst case bounding volume (which may be just a sphere, ideally it would
    // be more optimized) for a beam light with variable direction, range (length), beam radius or
    // position; the max_direction_angle in radians is relative to the direction it was created
    // with and the position sphere represents the space within which the light's center (source)
    // position may be moved.
    void SetBeamLightWorstCaseBounds(int i, float max_direction_angle, float max_range,
        float max_beam_radius, const sreBoundingVolumeSphere& position_sphere);
    void CalculateWholeSceneActiveLights(sreView *view, int max_lights);
    void CalculateVisibleActiveLights(sreView *view, int max_lights);
    void CheckLightCapacity();
    void RegisterLight(sreLight *l);
    // Handling of objects (functions used internally).
    void InstantiateObject(int object_index) const;
    void InstantiateObjectRotationMatrixAlreadySet(int object_index) const;
    void FinishObjectInstantiation(sreObject& so, bool rotated) const;
    void DeleteObject(int object_index);
    int AddParticleSystem(sreModel *model, int nu_particles, Point3D center,
        float worst_case_bounding_sphere_radius, Vector3D *particles);
    // Changing object properties. They only affect the sreObject, so they declared const.
    void ChangePosition(int object_index, float x, float y, float z) const;
    void ChangePosition(int object_index, Point3D position) const;
    void ChangeRotation(int object_index, float rotx, float roty, float rotz) const;
    void ChangeRotationMatrix(int object_index, const Matrix3D& rot) const;
    void ChangePositionAndRotation(int object_index, float x, float y, float z, float rotx, float roty,
        float rotz) const;
    void ChangePositionAndRotationMatrix(int object_index, float x, float y, float z,
        const Matrix3D& rot) const;
    void ChangeDiffuseReflectionColor(int object_index, Color color) const;
    void ChangeSpecularReflectionColor(int object_index, Color color) const;
    void ChangeSpecularExponent(int object_index, float exponent) const;
    void ChangeEmissionColor(int object_index, Color color) const;
    void ChangeMicrofacetParameters(int object_index, float diffuse_fraction,
        float roughness_value1, float weight1, float roughness_value2, float weight2,
        bool anisotropic) const;
    void ChangeBillboardSize(int object_index, float bb_width, float bb_height) const;
    void ChangeHaloSize(int object_index, float size) const;
    // Attach a light to an object the specified position in model space. For spot or
    // beam lights the model space-relative light direction is also specified, which
    // overrides the light's original direction.
    void AttachLight(int object_index, int light_index, Vector3D model_position,
        Vector3D model_light_direction) const;
    void InvalidateShaders(int object_index) const;
    void InvalidateLightingShaders(int object_index) const;
    // sreModel processing functions used for preprocessing and when uploading models to the GPU.
    bool EliminateTJunctionsForModels(sreModel& o1, const sreModel& o2) const;
    void DetermineStaticIntersectingObjects(const sreFastOctree& fast_oct, int array_index, int model_index,
        const sreBoundingVolumeAABB& AABB, int *static_object_belonging_to_object, int &nu_intersecting_objects,
        int *intersecting_objects) const;
    void EliminateTJunctions();
    void Triangulate();
    void Preprocess();
    void RemoveUnreferencedModels();
    void MarkAllModelsReferenced() const;
    void UploadModels() const;
    void PrepareForRendering(unsigned int prepare_flags);
    // Octree creation and static light volume objects calculation.
    void CreateOctrees();
    void ClearOctrees();
    void DetermineStaticLightVolumeIntersectingObjects(const sreFastOctree& fast_oct, int array_index,
        const sreLight& light, int &nu_intersecting_objects, int *intersecting_object) const;
    void CalculateStaticLightObjectLists();
    // Model objects.
    void RegisterModel(sreModel *m);
    // Main rendering function. Renders the scene from the given view parameters and calls the
    // text overlay function.
    void Render(sreView *view);
    bool CameraHasChangedSinceLastFrame(sreView *view, int current_frame) {
        if (view->ViewHasChangedSinceLastFrame(current_frame) ||
        view->ProjectionHasChangedSinceLastFrame(current_frame))
            return true;
        if (view->GetViewMode() == SRE_VIEW_MODE_FOLLOW_OBJECT)
            if (object[view->GetFollowedObject()]->PositionHasChangedSinceLastFrame(current_frame))
                return true;
        return false;
    }
    // Internal functions used during rendering.
    void DetermineObjectIsVisible(sreObject& so, const sreFrustum& f, BoundsCheckResult r);
    void CheckVisibleLightCapacity();
    void DetermineFastOctreeNodeVisibleEntities(const sreFastOctree& fast_oct,
        const sreFrustum& frustum, BoundsCheckResult bounds_check_result,
        int array_index, int nu_entities);
    void DetermineVisibleEntitiesInFastOctree(const sreFastOctree& fast_octree, int array_index,
        const sreFrustum& f, BoundsCheckResult r);
    void DetermineVisibleEntitiesInFastOctreeNonRootNode(const sreFastOctree& fast_octree, int array_index,
        const sreFrustum& f, BoundsCheckResult r);
    void DetermineVisibleEntitiesInFastOctreeRootNode(const sreFastOctree& fast_octree, int array_index,
        const sreFrustum& f, BoundsCheckResult r);
    void DetermineVisibleEntitiesInFastStrictOptimizedOctree(const sreFastOctree& fast_oct,
        const sreOctreeNodeBounds& node_bounds, int array_index, const sreFrustum& frustum,
        BoundsCheckResult bounds_check_result);
    void DetermineVisibleEntitiesInFastStrictOptimizedOctreeNonRootNode(const sreFastOctree& fast_oct,
        const sreOctreeNodeBounds& node_bounds, int array_index, const sreFrustum& frustum,
        BoundsCheckResult bounds_check_result);
    void DetermineVisibleEntitiesInFastStrictOptimizedOctreeRootNode(const sreFastOctree& fast_oct,
        int array_index, const sreFrustum& frustum, BoundsCheckResult bounds_check_result);
    void DetermineVisibleEntities(const sreFrustum& f);
    void RenderVisibleObjectsSinglePass(const sreFrustum&) const;
    void RenderFinalPassObjectsSinglePass(const sreFrustum&) const;
    void RenderVisibleObjectsAmbientPass(const sreFrustum&) const;
    void RenderVisibleObjectsLightingPass(const sreFrustum& f, const sreLight& light) const;
    void RenderFinalPassObjectsMultiPass(const sreFrustum& f) const;
    void UpdateGeometryScissorsCacheData(const sreFrustum& frustum, const sreLight& light) const;
    // Render lighting passes with shadow volumes or shadow mapping. Will change the shadow caster
    // array in sreScene for each light.
    void RenderLightingPasses(sreFrustum *f, sreView *view);
    void RenderLightingPassesNoShadow(sreFrustum *f, sreView *view);
    void ApplyGlobalTextureParameters(int flags, int filter, float anisotropy);
    void sreVisualizeShadowMap(int light_index, sreFrustum *frustum);
    void InvalidateGeometryScissorsCache() const;
    // Physics.
    void DoBulletPhysics(double previous_time, double current_time) const;
    void BulletApplyCentralImpulse(int object_index, const Vector3D& v) const;
    void BulletGetLinearVelocity(int soi, Vector3D *v_out) const;
    void BulletChangePosition(int soi, Point3D position) const;
    void BulletChangeVelocity(int soi, Vector3D velocity) const;
    void BulletChangeRotationMatrix(int soi, const Matrix3D& rot_matrix) const;
};

enum { SRE_OPENGL_VERSION_CORE = 0, SRE_OPENGL_VERSION_ES2 };

// Rendering settings flags.
enum {
    // Whether there is support for shadow volumes. Disabling increase performance
    // somewhat.
    SRE_RENDERING_FLAG_SHADOW_VOLUME_SUPPORT = 0x1,
    // Use shadow volume cache for models and objects in GPU memory.
    SRE_RENDERING_FLAG_SHADOW_CACHE_ENABLED = 0x100,
    // Use triangle strips for point/spot shadow volume sides.
    SRE_RENDERING_FLAG_USE_TRIANGLE_STRIPS_FOR_SHADOW_VOLUMES = 0x200,
    // Use triangle fans for dir./beam shadow volume sides.
    SRE_RENDERING_FLAG_USE_TRIANGLE_FANS_FOR_SHADOW_VOLUMES = 0x400,
    // Force depth-fail stencil shadow volume rendering (slower).
    SRE_RENDERING_FLAG_FORCE_DEPTH_FAIL = 0x800,
    // Enable geometrical visibility test for shadow volumes when using stencil
    // shadows.
    SRE_RENDERING_FLAG_SHADOW_VOLUME_VISIBILITY_TEST = 0x1000,
    // Enable geometrical visibility test for shadow volume darkcap (infinite
    // projection of shadow volume) when using with depth-fail rendering.
    // stencil shadows.
    SRE_RENDERING_FLAG_SHADOW_VOLUME_DARKCAP_VISIBILITY_TEST = 0x2000,
    // Enable caching of geometry scissors regions when the frustum does not change.
    SRE_RENDERING_FLAG_GEOMETRY_SCISSORS_CACHE_ENABLED = 0x4000,
    // Whether shadow maps are supported.
    SRE_RENDERING_FLAG_SHADOW_MAP_SUPPORT = 0x8000,
    // Whether cube shadow maps (point lights) are supported.
    SRE_RENDERING_FLAG_CUBE_SHADOW_MAP_SUPPORT = 0x10000,
};

class SRE_API sreEngineSettingsInfo {
public :
    int window_width, window_height;
    int opengl_version;
    int rendering_flags;
    bool multi_pass_rendering;
    int reflection_model;
    int shadows_method;
    int scissors_method;
    bool HDR_enabled;
    int HDR_tone_mapping_shader;
    float max_anisotropy;
    int max_visible_active_lights;
    const char *reflection_model_description;
    const char *shadows_description;
    const char *scissors_description;
    const char *shader_path;
};

class SRE_API sreShadowRenderingInfo {
public :
    int shadow_volume_count;
    int silhouette_count;
    int object_cache_total_entries;
    int object_cache_entries_used;
    int object_cache_total_vertex_count;
    int object_cache_hits;
    int object_cache_misses;
    int object_cache_entries_depthfail;
    int object_cache_hits_depthfail;
    int model_cache_total_entries;
    int model_cache_entries_used;
    int model_cache_total_vertex_count;
    int model_cache_hits;
    int model_cache_misses;
    int model_cache_entries_depthfail;
    int model_cache_hits_depthfail;
};

typedef void (*sreSwapBuffersFunc)();

// The following functions must be called before sreInitialize().
SRE_API void sreSetDemandLoadShaders(bool enabled);
SRE_API void sreSetMaxShadowMapSize(int size);
// SRE library initialization function.
SRE_API void sreInitialize(int window_width, int window_height, sreSwapBuffersFunc swap_buffers_func);
SRE_API void sreResize(sreView *view, int window_width, int window_heigth);
SRE_API void sreApplyNewZoom(sreView *view);
enum { SRE_SHADOWS_NONE, SRE_SHADOWS_SHADOW_VOLUMES, SRE_SHADOWS_SHADOW_MAPPING };
SRE_API void sreSetShadowsMethod(int type);
SRE_API void sreEnableMultiPassRendering();
SRE_API void sreDisableMultiPassRendering();
// Set maximum visible lights in multi-pass rendering mode.
SRE_API void sreSetMultiPassMaxActiveLights(int n);
// A global mask can be applied to object flags.
#define SRE_OBJECT_FLAGS_MASK_FLAT (SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_EMISSION_ONLY | 0xFFFFF000);
#define SRE_OBJECT_FLAGS_MASK_NO_LIGHTING (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_MULTI_COLOR | \
    SRE_OBJECT_USE_EMISSION_MAP | SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_TRANSPARENT_TEXTURE | \
    SRE_OBJECT_TRANSPARENT_EMISSION_MAP | SRE_OBJECT_3D_TEXTURE | SRE_OBJECT_EMISSION_ONLY \
    | 0xFFFFF000)
#define SRE_OBJECT_FLAGS_MASK_NO_TEXTURES (SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_3D_TEXTURE | \
    SRE_OBJECT_EMISSION_ONLY | 0xFFFFF000)
#define SRE_OBJECT_FLAGS_MASK_PHONG_ONLY (SRE_OBJECT_USE_NORMAL_MAP | SRE_OBJECT_USE_SPECULARITY_MAP | \
    SRE_OBJECT_MULTI_COLOR | 0xFFFFF000)
#define SRE_OBJECT_FLAGS_MASK_FULL 0xFFFFFFFF
SRE_API void sreSetObjectFlagsMask(int mask);
SRE_API void sreSetShaderMask(int mask); // Obsolete; maintained for compatibility.
enum {
    SRE_SHADER_SELECTION_UNOPTIMIZED = 0x01,
    SRE_SHADER_SELECTION_ALL = 0xFF
};
SRE_API void sreSetShaderSelection(int value);
enum { SRE_REFLECTION_MODEL_STANDARD, SRE_REFLECTION_MODEL_MICROFACET };
SRE_API void sreSetReflectionModel(int model);
SRE_API void sreSetLightAttenuation(bool enabled);
enum { SRE_SCISSORS_NONE = 0, SRE_SCISSORS_LIGHT = 1, SRE_SCISSORS_GEOMETRY = 3, SRE_SCISSORS_GEOMETRY_MATRIX = 5 };
enum { SRE_SCISSORS_LIGHT_MASK = 1, SRE_SCISSORS_GEOMETRY_MASK = 2, SRE_SCISSORS_GEOMETRY_MATRIX_MASK = 4 };
SRE_API void sreSetLightScissors(int mode);
// Enable shadow volume visibility test (but not the darkcap visibility test).
SRE_API void sreSetShadowVolumeVisibilityTest(bool enabled);
SRE_API void sreSetShadowVolumeDarkCapVisibilityTest(bool enabled);
SRE_API void sreSetShadowMapRegion(Point3D dim_min, Point3D dim_max);
enum { SRE_OCTREE_STRICT, SRE_OCTREE_STRICT_OPTIMIZED, SRE_OCTREE_BALANCED, SRE_QUADTREE_XY_STRICT,
SRE_QUADTREE_XY_STRICT_OPTIMIZED, SRE_QUADTREE_XY_BALANCED, SRE_OCTREE_MIXED_WITH_QUADTREE };
SRE_API void sreSetOctreeType(int type);
SRE_API void sreSetNearPlaneDistance(float dist);
SRE_API void sreSetFarPlaneDistance(float dist);
SRE_API void sreSetLightObjectLists(bool enabled);
SRE_API void sreSetHDRRendering(bool enabled);
SRE_API void sreSetHDRKeyValue(float value);
#define SRE_NUMBER_OF_TONE_MAPPING_SHADERS 3
enum { SRE_TONE_MAP_LINEAR = 0, SRE_TONE_MAP_REINHARD, SRE_TONE_MAP_EXPONENTIAL };
SRE_API void sreSetHDRToneMappingShader(int i);
SRE_API int sreGetCurrentHDRToneMappingShader();
SRE_API const char *sreGetToneMappingShaderName(int i);
SRE_API int sreGetCurrentFrame();
enum { SRE_SPLASH_NONE, SRE_SPLASH_BLACK, SRE_SPLASH_LOGO, SRE_SPLASH_CUSTOM };
SRE_API void sreSetSplashScreen(int type, void (*SplashScreenFunction)());
SRE_API void sreSwapBuffers();
SRE_API sreEngineSettingsInfo *sreGetEngineSettingsInfo();
SRE_API sreShadowRenderingInfo *sreGetShadowRenderingInfo();
SRE_API void sreSetVisualizedShadowMap(int light_index);
SRE_API void sreSetDrawTextOverlayFunc(void (*func)());
SRE_API void sreSetTriangleStripUseForShadowVolumes(bool enabled);
SRE_API void sreSetTriangleFanUseForShadowVolumes(bool enabled);
SRE_API void sreSetShadowVolumeCache(bool enabled);
SRE_API void sreSetForceDepthFailRendering(bool enabled);
SRE_API void sreSetShadowVolumeSupport(bool enabled);
SRE_API void sreSetGeometryScissorsCache(bool enabled);
// Global texture detail settings.
enum {
    // Use original texture size.
    SRE_TEXTURE_DETAIL_HIGH = 1,
    // Reduce large textures (e.g. use half-size).
    SRE_TEXTURE_DETAIL_MEDIUM = 2,
    // Reduce large textures significantly.
    SRE_TEXTURE_DETAIL_LOW = 4,
    SRE_TEXTURE_DETAIL_VERY_LOW = 8,
    SRE_TEXTURE_DETAIL_LEVEL_MASK = 15,
    // Non-power-of-two texture flags.
    // Whether at least limited NPOT textures are supported.
    SRE_TEXTURE_DETAIL_NPOT = 0x100,
    // Whether multiple mipmap levels are supported with NPOT textures.
    SRE_TEXTURE_DETAIL_NPOT_MIPMAPS = 0x200,
    // Whether full repeat/wrap mode NPOT textures are supported.
    SRE_TEXTURE_DETAIL_NPOT_WRAP = 0x400,
    // Whether mipmapped compressed NPOT textures are supported.
    SRE_TEXTURE_DETAIL_NPOT_MIPMAPS_COMPRESSED = 0x800,
    SRE_TEXTURE_DETAIL_NPOT_FULL = 0xF00,
    SRE_TEXTURE_DETAIL_NPOT_MASK = 0xF00
};
enum {
    SRE_TEXTURE_DETAIL_SET_LEVEL = 1,
    SRE_TEXTURE_DETAIL_SET_NPOT = 2
};
SRE_API void sreSetGlobalTextureDetailFlags(int set_mask, int flags);
SRE_API int sreGetGlobalTextureDetailFlags();
enum {
    SRE_OPENGL_PLATFORM_GLES2, SRE_OPENGL_PLATFORM_GL3
};
SRE_API int sreGetOpenGLPlatform();

class sreFont;

// Defined in texture.cpp:

SRE_API sreTexture *sreGetStandardTexture();
SRE_API sreTexture *sreGetStandardTextureWrapRepeat();
// Create a checkboard texture of size w x h consisting of two clors, with fields
// of size bw x bh.
SRE_API sreTexture *sreCreateCheckerboardTexture(int type, int w, int h, int bw, int bh,
    Color color0, Color color1);
SRE_API sreTexture *sreCreateStripesTexture(int type, int w, int h, int bh,
    Color color0, Color color1);
SRE_API sreTexture *sreCreateTexture(const char *pathname, int type);
SRE_API sreTexture *sreCreateTextureLimitLevelWidth(const char *pathname, int type,
    int max_level_width);
SRE_API float sreGetMaxAnisotropyLevel();
SRE_API sreTexture *sreCreateTextTexture(const char *str, sreFont *font);

// Defined in text.cpp:

class sreFont {
public:
    sreTexture *tex;
    int chars_horizontal; // Width of the font texture in characters.
    int chars_vertical;   // Height of the font texture in characters.
    int shift;            // When not 0, chars_horizontal is equal to (1 << shift).
    float char_width;     // Width of each character in texture coordinates.
    float char_height;    // Height of each character in texture coordinates.

    sreFont() { }
    sreFont(const char *texture_name, int chars_x, int chars_y);
    void SetFiltering(int filtering) const;
};

enum {
    SRE_IMAGE_SET_TEXTURE = 1,
    SRE_IMAGE_SET_TEXTURE_ARRAY_INDEX = 2,
     // Set texture array as source (also sets array index).
    SRE_IMAGE_SET_TEXTURE_ARRAY = 3,
    SRE_IMAGE_SET_COLORS = 4,
    SRE_IMAGE_SET_TRANSFORM = 8,
    SRE_IMAGE_SET_RECTANGLE = 16,
    SRE_IMAGE_SET_ALL = 31,
    // Extension for text2 shader.
    SRE_TEXT_SET_SCREEN_SIZE_IN_CHARS = 32,
    SRE_TEXT_SET_STRING = 64,
    SRE_TEXT_SET_ALL = 127,
    // Special flag that sets one component source texture configuration
    // (used with SET_TEXTURE or SET_TEXTURE_ARRAY).
    SRE_IMAGE_SET_ONE_COMPONENT_SOURCE = 0x10000,
    SRE_IMAGE_SET_SOURCE_FLAGS_ONLY = 0x20000,
};

#define SRE_TEXT_SET_FONT_SIZE SRE_TEXT_SET_SCREEN_SIZE_IN_CHARS

// Blending constants are shared between image and text shaders.
enum { SRE_IMAGE_BLEND_ADDITIVE = 0, SRE_IMAGE_BLEND_OPAQUE = 1 };
#define SRE_TEXT_FLAG_OPAQUE SRE_IMAGE_BLEND_OPAQUE

SRE_API void sreInitializeImageEngine();
SRE_API void sreSetImageParameters(int set_mask, const Vector4D *colors, const Matrix3D *m);
SRE_API void sreSetImageSource(int set_mask, SRE_GLUINT opengl_id, int array_index);
SRE_API void sreDrawImage(float x, float y, float w, float h);
// Blending setting is immediately applied, works with image and text functions.
SRE_API void sreSetImageBlendingMode(int mode);

SRE_API void sreInitializeTextEngine();
SRE_API void sreSetTextParameters(int set_mask, const Vector4D *colors, const Vector2D *font_size);
SRE_API sreFont *sreSetFont(sreFont *font);
SRE_API sreFont *sreGetStandardFont();
SRE_API sreFont *sreCreateSystemMemoryFont(const char *pathname, int chars_x, int chars_y);
SRE_API void sreDrawText(const char *string, float x, float y);
SRE_API void sreDrawTextN(const char *string, int n, float x, float y);
SRE_API void sreDrawTextCentered(const char *text, float x, float y, float w);

// Defined in shader_loading.cpp:
#define SRE_SHADER_MASK_TEXT                 1
#define SRE_SHADER_MASK_LIGHTING_SINGLE_PASS 2
#define SRE_SHADER_MASK_LIGHTING_MULTI_PASS  4
#define SRE_SHADER_MASK_SHADOW_VOLUME        8
#define SRE_SHADER_MASK_SHADOW_MAP           16
#define SRE_SHADER_MASK_EFFECTS              32
#define SRE_SHADER_MASK_HDR                  64
#define SRE_SHADER_MASK_IMAGE                128
#define SRE_SHADER_MASK_LIGHTING             6
#define SRE_SHADER_MASK_CUBE_SHADOW_MAP      0x100
#define SRE_SHADER_MASK_ALL                  0x1FF
SRE_API void sreSetShaderLoadingMask(int mask);

// Model loading/creation.

// Create new LOD model with optional shadow volume support if it is enabled in global settings.
SRE_API sreLODModel *sreNewLODModel();
SRE_API sreLODModel *sreNewLODModelNoShadowVolume();

enum {
    SRE_MODEL_FILE_TYPE_OBJ = 1,
    SRE_MODEL_FILE_TYPE_SRE_BINARY
};
enum {
    // Force use the native SRE model loading function even when assimp is available.
    SRE_MODEL_LOAD_FLAG_USE_SRE = 1,
    SRE_MODEL_LOAD_FLAG_NO_VERTEX_NORMALS = 2,
    SRE_MODEL_LOAD_FLAG_NO_TEXCOORDS = 4,
    SRE_MODEL_LOAD_FLAG_NO_COLORS = 8,
    SRE_MODEL_LOAD_FLAG_NO_TANGENTS = 16,
};
SRE_API sreModel *sreReadModelFromFile(sreScene *scene, const char *pathname, int model_type, int load_flags);
SRE_API sreModel *sreReadMultiDirectoryModelFromFile(sreScene *scene, const char *pathname,
const char *base_path, int model_type, int load_flags);
// This function is used by sreReadMultiDirectoryModelFromFile when assimp is available and the
// SRE_MODEL_LOAD_FLAG_USE_SRE flag is not set.
SRE_API sreModel *sreReadModelFromAssimpFile(sreScene *scene, const char *pathname, const char *base_path,
    int load_flags);
// Note: Loading a LOD model directly currently does not use assimp and relies on native support.
SRE_API sreLODModel *sreReadLODModelFromFile(const char *pathname, int model_type, int load_flags);
SRE_API sreLODModel *sreReadMultiDirectoryLODModelFromFile(const char *pathname, const char *base_path,
    int model_type, int load_flags);
// Read a LOD model from SRE's internal binary file format (.srebinarylodmodel).
SRE_API sreLODModel *sreReadLODModelFromSREBinaryLODModelFile(const char *pathname,
    int load_flags);
// Read a model from SRE's internal binary file format (.srebinarymodel).
SRE_API sreModel *sreReadModelFromSREBinaryModelFile(sreScene *scene, const char *pathname,
    int load_flags);
// Save a LOD model or model to the internal binary format. The parameter save_flags
// corresponds to load_flags and can be used to omit certain attributes.
SRE_API void sreSaveLODModelToSREBinaryLODModelFile(sreLODModel *lm, const char *pathname,
    int save_flags);
SRE_API void sreSaveModelToSREBinaryModelFile(sreModel *m, const char *pathname,
    int save_flags);

SRE_API sreModel *sreCreateFluidModel(sreScene *scene, int width, int height, float d,
    float t, float c, float mu, float texcoords_scaling);
SRE_API void sreEvaluateModelFluid(sreModel *m);
SRE_API void sreCreateModelFluidDisturbance(sreModel *m, int x, int y, float z);

// Defined in standard_objects.cpp:
SRE_API sreModel *sreCreateSphereModel(sreScene *scene, float oblateness);
SRE_API sreModel *sreCreateSphereModelSimple(sreScene *scene, float oblateness);
SRE_API sreModel *sreCreateBillboardModel(sreScene *scene, bool is_halo);
SRE_API sreModel *sreCreateParticleSystemModel(sreScene *scene, int n, bool is_halo);
SRE_API sreModel *sreCreateUnitBlockModel(sreScene *scene);
enum { RAMP_TOWARDS_BACK, RAMP_TOWARDS_FRONT, RAMP_TOWARDS_LEFT, RAMP_TOWARDS_RIGHT };
SRE_API sreModel *sreCreateRampModel(sreScene *scene, float xdim, float ydim, float zdim, int type);
SRE_API sreModel *sreCreateRingsModel(sreScene *scene, float min_radius, float max_radius);
SRE_API sreModel *sreCreateCheckerboardModel(sreScene *scene, int size,
    float unit_size, Color color1, Color color2);
#define TORUS_RADIUS 5
#define TORUS_RADIUS2 2
SRE_API sreModel *sreCreateTorusModel(sreScene *scene);
SRE_API sreModel *sreCreateGratingModel(sreScene *scene, int NU_HOLES_X, int NU_HOLES_Y, float BORDER_WIDTH, float GAP_WIDTH,
float BAR_WIDTH, float THICKNESS);
#define SRE_BLOCK_NO_LEFT 1
#define SRE_BLOCK_NO_RIGHT 2
#define SRE_BLOCK_NO_FRONT 4
#define SRE_BLOCK_NO_BACK 8
#define SRE_BLOCK_NO_BOTTOM 16
#define SRE_BLOCK_NO_TOP 32
SRE_API sreModel *sreCreateBlockModel(sreScene *scene, float xdim, float ydim, float zdim, int flags);
SRE_API sreModel *sreCreateRepeatingRectangleModel(sreScene *scene, float size, float unit_size);
SRE_API sreModel *sreCreateCenteredXPlaneRectangleModel(sreScene *scene, float dimx, float dimz);
SRE_API sreModel *sreCreateCenteredYPlaneRectangleModel(sreScene *scene, float dimx, float dimz);
SRE_API sreModel *sreCreateCenteredZPlaneRectangleModel(sreScene *scene, float dimx, float dimy);
SRE_API sreModel *sreCreateCylinderModel(sreScene *scene, float zdim, bool include_top,
    bool include_bottom);
SRE_API sreModel *sreCreateEllipsoidModel(sreScene *scene, float radius_y, float radius_z);
SRE_API sreModel *sreCreateCapsuleModel(sreScene *scene, float cap_radius, float length,
    float radius_y, float radius_z);
SRE_API sreModel *sreCreateCompoundModel(sreScene *scene, bool has_texcoords, bool has_tangents,
    int lod_flags);
SRE_API void sreAddToCompoundModel(sreModel *compound_model, sreModel *o, Point3D position,
    Vector3D rotation, float scaling);
SRE_API void sreFinalizeCompoundModel(sreScene *scene, sreModel *compound_model);
enum {
    SRE_MULTI_COLOR_FLAG_SHARE_RESOURCES = 1,  // Share model geometry and OpenGL buffer resources.
    SRE_MULTI_COLOR_FLAG_ASSIGN_RANDOM = 2,    // Assign random colors form the given set.
    SRE_MULTI_COLOR_FLAG_NEW_RANDOM = 4        // Create new random colors for every triangle.
};
SRE_API sreModel *sreCreateNewMultiColorModel(sreScene *scene, sreModel *m, int color_flags,
    int nu_colors, Color *colors);

/// More helper functions for scene construction.

// Allocate UV transformation matrix that flips U or V.
SRE_API Matrix3D *sreNewMirroringUVTransform(bool flip_u, bool flip_v);
// Allocate UV transformation matrix that selects a region of a source texture. Any mirroring
// is applied after the region has been selected.
SRE_API Matrix3D *sreNewRegionUVTransform(Vector2D top_left, Vector2D bottom_right,
    bool flip_u, bool flip_v);

// Errors/messages/logging. Used by the library, but can also be used by
// the application.

SRE_API void sreSetDebugMessageLevel(int level);

#ifdef __GNU_C__
// This function prints a formatted string and raises an interrupt, so that
// it can function as a debugging breakpoint as well as quitting the program.
// A newline appended after the string.
SRE_API void sreFatalError(const char *format, ...) __attribute__ ((noreturn));
#else
SRE_API void sreFatalError(const char *format, ...);
#endif

// Messages will only be displayed if the priority is smaller than or equal
// to the configured debug message level (sreSetDebugMessageLevel).
enum {
    // When the debug message level is equal to - 3, in principle no text
    // output should occur.
    SRE_MESSAGE_ERROR = - 4,
    SRE_MESSAGE_QUIET = - 3,
    SRE_MESSAGE_CRITICAL = - 2,
    SRE_MESSAGE_WARNING = - 1,
    // Priority levels 0 to 3 are information (0 corresponds to the
    // least frequent information that always displayed by default with
    // sre_internal_debug_message_level of 0, level 3 corresponds to very
    // frequent information at logging level).
    SRE_MESSAGE_INFO = 0,
    SRE_MESSAGE_SPARSE_LOG = 1,
    SRE_MESSAGE_LOG = 2,
    SRE_MESSAGE_VERBOSE_LOG = 3
};
// Display a message depending on the priority level. A newline is appended
// if the message is printed.
SRE_API void sreMessage(int priority, const char *format, ...);
SRE_API void sreMessageNoNewline(int priority, const char *format, ...);

#endif

