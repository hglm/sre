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

// Internal SRE library header file.

#include <cstdarg>

// Defined shader_matrix.cpp:

// The following matrices are used by different modules.
extern Matrix4D sre_internal_projection_matrix;
extern MatrixTransform sre_internal_view_matrix;
extern Matrix4D sre_internal_view_projection_matrix;
extern float sre_internal_aspect_ratio;

// In sre.cpp:

extern Point3D sre_internal_viewpoint;
extern float sre_internal_zoom;
extern Frustum *sre_internal_frustum;
extern int sre_internal_current_frame;
extern int sre_internal_current_light_index;
extern Light *sre_internal_current_light;
extern int sre_internal_object_flags_mask;
extern Scene *sre_internal_scene;
extern sreSwapBuffersFunc sre_internal_swap_buffers_func;
extern int sre_internal_window_width;
extern int sre_internal_window_height;
extern bool sre_internal_aspect_changed;
extern int sre_internal_reflection_model;
extern int sre_internal_shader_mask;
extern bool sre_internal_reselect_shaders;
extern bool sre_internal_invalidate_geometry_scissors_cache;
extern bool sre_internal_use_depth_clamping;
extern void (*sreDrawTextOverlayFunc)();
extern sreDefaultRNG *sre_internal_rng;

extern SRE_GLUINT sre_internal_depth_texture;
extern SRE_GLUINT sre_internal_depth_cube_map_texture;
extern SRE_GLUINT sre_internal_shadow_map_framebuffer;
extern SRE_GLUINT sre_internal_cube_shadow_map_framebuffer;
extern SRE_GLUINT sre_internal_small_depth_texture;
extern SRE_GLUINT sre_internal_small_shadow_map_framebuffer;
extern SRE_GLUINT sre_internal_HDR_color_texture;
extern SRE_GLUINT sre_internal_HDR_multisample_framebuffer;
extern SRE_GLUINT sre_internal_HDR_framebuffer;
extern SRE_GLUINT sre_internal_HDR_log_luminance_texture;
extern SRE_GLUINT sre_internal_HDR_log_luminance_framebuffer;
extern SRE_GLUINT sre_internal_HDR_average_luminance_texture[4];
extern SRE_GLUINT sre_internal_HDR_average_luminance_framebuffer[4];
extern SRE_GLUINT sre_internal_HDR_luminance_history_storage_framebuffer;
extern SRE_GLUINT sre_internal_HDR_luminance_history_texture;
extern SRE_GLUINT sre_internal_HDR_luminance_history_comparison_framebuffer;
extern SRE_GLUINT sre_internal_HDR_used_average_luminance_texture;

extern sreBoundingVolumeAABB sre_internal_shadow_map_AABB;
extern float sre_internal_near_plane_distance;
extern float sre_internal_far_plane_distance;
extern int sre_internal_max_silhouette_edges;
extern int sre_internal_max_shadow_volume_vertices;
extern SRE_GLUINT sre_internal_HDR_full_screen_vertex_buffer;
extern int sre_internal_shadows;
extern bool sre_internal_light_attenuation_enabled;
extern bool sre_internal_shadow_caster_volume_culling_enabled;
extern bool sre_internal_multi_pass_rendering;
extern int sre_internal_max_active_lights;
extern int sre_internal_scissors;
extern bool sre_internal_geometry_scissors_active;
extern bool sre_internal_shadow_volume_visibility_test;
extern int sre_internal_octree_type;
extern bool sre_internal_light_object_lists_enabled;
extern bool sre_internal_HDR_enabled;
extern float sre_internal_HDR_key_value;
extern int sre_internal_HDR_tone_mapping_shader;
extern int sre_internal_debug_message_level;
extern int sre_internal_shader_loading_mask;
extern const char *sre_internal_shader_path;
extern bool sre_internal_demand_load_shaders;
enum {
    SRE_INTERLEAVED_BUFFERS_DISABLED,
    SRE_INTERLEAVED_BUFFERS_ENABLED
};
extern int sre_internal_interleaved_vertex_buffers_mode;
extern bool sre_internal_shadow_volumes_disabled;
extern int sre_internal_visualized_shadow_map;
extern int sre_internal_shadow_volume_count;
extern int sre_internal_silhouette_count;
extern int sre_internal_rendering_flags;

extern Matrix3D *sre_internal_standard_UV_transformation_matrix;

// Defined in shader_matrix.cpp:
void GL3Perspective(float fov, float aspect, float nearp, float farp);
void GL3PerspectiveTweaked(float fov, float aspect, float nearp, float farp);
void GL3SkyPerspective(float fov, float aspect, float nearp, float farp);
void GL3LookAt(float viewpx, float viewpy, float viewpz, float lookx, float looky, float lookz, float upx, float upy,
    float upz);
void GL3CalculateShadowMapMatrix(Vector3D viewpoint, Vector3D light_direction, Vector3D normal_x, Vector3D normal_y,
Vector3D dim_min, Vector3D dim_max);
void GL3CalculateProjectionShadowMapMatrix(Vector3D viewp, Vector3D light_direction, Vector3D x_direction,
Vector3D y_direction, float zmax);
void GL3CalculateShadowMapMatrixAlwaysLight();
void GL3CalculateCubeShadowMapMatrix(Vector3D light_position, Vector3D zdir, Vector3D up_vector, float zmax);
void GL3CalculateGeometryScissorsMatrixAndSetViewport(const sreScissors& scissors);

// vertex_buffers.cpp

void GL3SetBillboard(SceneObject *so);
void GL3SetBillboardBounds(SceneObject *so);
void GL3SetParticleSystem(SceneObject *so);
void GL3SetParticleSystemBounds(SceneObject *so);

// draw_object.cpp

sreLODModel *sreCalculateLODModel(const SceneObject& so);
void sreDrawObjectSinglePass(SceneObject *so);
void sreDrawObjectFinalPass(SceneObject *so);
void sreDrawObjectAmbientPass(SceneObject *so);
void sreDrawObjectMultiPassLightingPass(SceneObject *so, bool shadow_map_required);

// shader_uniform.cpp

void GL3InitializeShadersBeforeFrame();
void GL3InitializeShadersBeforeLight();
void GL3InitializeShadowMapShadersBeforeLight();
void GL3InitializeShadowVolumeShader(const SceneObject& so, const Vector4D& light_position_object);
void GL3InitializeShadowMapShader(const SceneObject& so);
void GL3InitializeCubeShadowMapShader(const SceneObject& so);
void GL3UpdateCubeShadowMapSegmentDistanceScaling(float *segment_distance_scaling);
void GL3InitializeShadowMapShadersWithSegmentDistanceScaling(float scaling);
void GL3InitializeHDRLogLuminanceShader();
void GL3InitializeHDRAverageLuminanceShader();
void GL3InitializeHDRAverageLuminanceShaderWithLogLuminanceTexture();
void GL3InitializeHDRAverageLuminanceShaderWithAverageLuminanceTexture(int i);
void GL3InitializeHDRLuminanceHistoryStorageShader();
void GL3InitializeHDRLuminanceHistoryComparisonShader(int slot);
void GL3InitializeHDRToneMapShader();
void GL3InitializeOldTextShader(Color *colorp);

enum {
    SRE_IMAGE_SOURCE_FLAG_TEXTURE_ARRAY = 1,
    SRE_IMAGE_SOURCE_FLAG_ONE_COMPONENT_SOURCE = 2
};

#define SRE_NU_IMAGE_POSITION_BUFFERS 3

enum {
    SRE_IMAGE_POSITION_BUFFER_1X1 = 0,
    SRE_IMAGE_POSITION_BUFFER_4X4,
    SRE_IMAGE_POSITION_BUFFER_16X1
};

#define SRE_IMAGE_POSITION_BUFFER_FLAG_1X1 (1 << SRE_IMAGE_POSITION_BUFFER_1X1)
#define SRE_IMAGE_POSITION_BUFFER_FLAG_4X4 (1 << SRE_IMAGE_POSITION_BUFFER_4X4)
#define SRE_IMAGE_POSITION_BUFFER_FLAG_16X1 (1 << SRE_IMAGE_POSITION_BUFFER_16X1)

class sreImageShaderInfo {
public :
    int update_mask;
    SRE_GLUINT opengl_id;
    int buffer_flags;
    int source_flags;
    int array_index;
    Matrix3D uv_transform;
    Vector4D mult_color;
    Vector4D add_color;

    void ValidateImagePositionBuffers(int buffer_flags) const;
    void Initialize(int buffer_flags);
    void SetSource(int set_mask, SRE_GLUINT opengl_id, int array_index);
};

#ifndef OPENGL_ES2
#define SRE_TEXT_MAX_REQUEST_LENGTH 256
#else
// OpenGL-ES 2.0 shader can be more limited in terms of the number of characters
// per draw request, because each characters has to be stored in a full int uniform.
// Large strings are split into seperate requests.
#define SRE_TEXT_MAX_REQUEST_LENGTH 128
#endif

enum {
    SRE_FONT_FORMAT_32X8 = (32 | (8 << 8)),
    SRE_FONT_FORMAT_16X16 = (16 | (16 << 8))
};

class sreTextShaderInfo : public sreImageShaderInfo {
public :
    int font_format;
    Vector2D screen_size_in_chars;
};

// Defined in sre_uniform.cpp:
// Length specifies the length of the string in characters.
void GL3InitializeTextShader(int update_mask, sreTextShaderInfo *info, Vector4D *rect,
    const char *string, int length);
void GL3InitializeImageShader(int set_flags, sreImageShaderInfo *info, Vector4D *rect);

// Defined in shadow.cpp:
void sreRenderShadowVolumes(sreScene *scene, Light *light, Frustum& frustum);
void sreReportShadowCacheStats();
void sreResetShadowCacheStats();
void sreSetShadowCacheStatsInfo(sreShadowRenderingInfo *info);
void sreClearShadowCache();

// Defined in shadowmap.cpp:
bool GL3RenderShadowMapWithOctree(sreScene *scene, Light& light, Frustum &frustum);
void sreVisualizeDirectionalLightShadowMap(int light_index);
void sreVisualizeCubeMap(int light_index);
void sreVisualizeBeamOrSpotLightShadowMap(int light_index);

// Defined is lights.cpp:
void sreInitializeInternalShadowVolume();

#ifdef DEBUG_OPENGL
#define CHECK_GL_ERROR(s) sreCheckGLError(s);
#else
#define CHECK_GL_ERROR(s)
#endif

// This function check for the existence of OpenGL errors, and prints
// the given formatted string if an error has occurred. It flushes
// all previous errors. No newline is printed after the string.
void sreCheckGLError(const char *format, ...);

