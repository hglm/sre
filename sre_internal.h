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

// Select optional use of explicit inline SIMD acceleration functions.

#ifndef NO_SIMD
#if defined(__SSE2__)
#define USE_SIMD
#define DST_SIMD_MODE_SSE2
#endif
#endif

// Defined in shader_matrix.cpp:

// The following matrices are used by different modules.
extern Matrix4D sre_internal_projection_matrix;
extern MatrixTransform sre_internal_view_matrix;
extern Matrix4D sre_internal_view_projection_matrix;
extern float sre_internal_aspect_ratio;

// In sre.cpp:

extern Point3D sre_internal_viewpoint;
extern float sre_internal_zoom;
extern sreFrustum *sre_internal_frustum;
extern int sre_internal_current_frame;
extern int sre_internal_current_light_index;
extern sreLight *sre_internal_current_light;
extern int sre_internal_object_flags_mask;
extern sreScene *sre_internal_scene;
extern sreSwapBuffersFunc sre_internal_swap_buffers_func;
extern int sre_internal_window_width;
extern int sre_internal_window_height;
extern bool sre_internal_aspect_changed;
extern int sre_internal_reflection_model;
extern int sre_internal_shader_selection;
extern bool sre_internal_reselect_shaders;
extern bool sre_internal_use_depth_clamping;
extern void (*sreDrawTextOverlayFunc)();
extern SRE_GLUINT sre_internal_depth_texture[SRE_MAX_SHADOW_MAP_LEVELS_OPENGL];
extern SRE_GLUINT sre_internal_shadow_map_framebuffer[SRE_MAX_SHADOW_MAP_LEVELS_OPENGL];
extern SRE_GLUINT sre_internal_depth_cube_map_texture[SRE_MAX_CUBE_SHADOW_MAP_LEVELS_OPENGL];
extern SRE_GLUINT sre_internal_cube_shadow_map_framebuffer[SRE_MAX_CUBE_SHADOW_MAP_LEVELS_OPENGL][6];
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
extern int sre_internal_octree_type;
extern bool sre_internal_light_object_lists_enabled;
extern bool sre_internal_HDR_enabled;
extern float sre_internal_HDR_key_value;
extern int sre_internal_HDR_tone_mapping_shader;
extern int sre_internal_debug_message_level;
extern int sre_internal_shader_loading_mask;
extern const char *sre_internal_shader_path;
extern bool sre_internal_demand_load_shaders;
extern bool sre_internal_invalidate_geometry_scissors_cache;
enum {
    SRE_INTERLEAVED_BUFFERS_DISABLED,
    SRE_INTERLEAVED_BUFFERS_ENABLED
};
extern int sre_internal_interleaved_vertex_buffers_mode;
extern int sre_internal_visualized_shadow_map;
extern int sre_internal_shadow_volume_count;
extern int sre_internal_silhouette_count;
extern int sre_internal_rendering_flags;
extern int sre_internal_max_texture_size;
extern int sre_internal_texture_detail_flags;
extern int sre_internal_max_shadow_map_size;
extern int sre_internal_current_shadow_map_index;
extern int sre_internal_nu_shadow_map_size_levels;
extern int sre_internal_max_cube_shadow_map_size;
extern int sre_internal_current_cube_shadow_map_index;
extern int sre_internal_nu_cube_shadow_map_size_levels;
extern SRE_GLUINT sre_internal_current_depth_cube_map_texture;
extern Vector4D sre_internal_current_shadow_map_dimensions;

extern Matrix3D *sre_internal_standard_UV_transformation_matrix;

// Defined in shader_matrix.cpp:
SRE_LOCAL void GL3Perspective(float fov, float aspect, float nearp, float farp);
SRE_LOCAL void GL3PerspectiveTweaked(float fov, float aspect, float nearp, float farp);
SRE_LOCAL void GL3SkyPerspective(float fov, float aspect, float nearp, float farp);
SRE_LOCAL void GL3LookAt(float viewpx, float viewpy, float viewpz, float lookx, float looky, float lookz, float upx, float upy,
    float upz);
SRE_LOCAL void GL3CalculateShadowMapMatrix(Vector3D viewpoint, Vector3D light_direction, Vector3D normal_x, Vector3D normal_y,
    Vector3D dim_min, Vector3D dim_max);
SRE_LOCAL void GL3CalculateProjectionShadowMapMatrix(Vector3D viewp, Vector3D light_direction, Vector3D x_direction,
    Vector3D y_direction, float zmax);
SRE_LOCAL void GL3CalculateShadowMapMatrixAlwaysLight();
SRE_LOCAL void GL3CalculateCubeShadowMapMatrix(Vector3D light_position, Vector3D zdir, Vector3D up_vector, float zmax);
SRE_LOCAL void GL3CalculateGeometryScissorsMatrixAndSetViewport(const sreScissors& scissors);

// vertex_buffers.cpp

SRE_LOCAL void GL3SetBillboard(sreObject *so);
SRE_LOCAL void GL3SetBillboardBounds(sreObject *so);
SRE_LOCAL void GL3SetParticleSystem(sreObject *so);
SRE_LOCAL void GL3SetParticleSystemBounds(sreObject *so);

// draw_object.cpp

SRE_LOCAL sreLODModel *sreCalculateLODModel(const sreObject& so);
SRE_LOCAL void sreDrawObjectSinglePass(sreObject *so);
SRE_LOCAL void sreDrawObjectFinalPass(sreObject *so);
SRE_LOCAL void sreDrawObjectAmbientPass(sreObject *so);
SRE_LOCAL void sreDrawObjectMultiPassLightingPass(sreObject *so, bool shadow_map_required);

// shader_uniform.cpp

SRE_LOCAL void GL3InitializeShadersBeforeFrame();
SRE_LOCAL void GL3InitializeShadersBeforeLight();
SRE_LOCAL void GL3InitializeCubeShadowMapShadersBeforeLight();
SRE_LOCAL void GL3InitializeShadowMapShadersBeforeLight(const Vector4D& dim);
SRE_LOCAL void GL3InitializeSpotlightShadowMapShadersBeforeLight();
SRE_LOCAL void GL3InitializeShadowVolumeShader(const sreObject& so, const Vector4D& light_position_object);
SRE_LOCAL void GL3InitializeShadowMapShader(const sreObject& so);
SRE_LOCAL void GL3InitializeShadowMapShaderNonClosedObject(const sreObject& so);
SRE_LOCAL void GL3InitializeSpotlightShadowMapShader(const sreObject& so);
SRE_LOCAL void GL3InitializeSpotlightShadowMapShaderNonClosedObject(const sreObject& so);
SRE_LOCAL void GL3InitializeSpotlightShadowMapShadersWithSegmentDistanceScaling();
SRE_LOCAL void GL3InitializeCubeShadowMapShader(const sreObject& so);
SRE_LOCAL void GL3UpdateShadowMapSegmentDistanceScaling(float segment_distance_scaling);
SRE_LOCAL void GL3InitializeCubeShadowMapShadersWithSegmentDistanceScaling();
SRE_LOCAL void sreBindShadowMapTexture(sreLight *light);
SRE_LOCAL void GL3InitializeHDRLogLuminanceShader();
SRE_LOCAL void GL3InitializeHDRAverageLuminanceShader();
SRE_LOCAL void GL3InitializeHDRAverageLuminanceShaderWithLogLuminanceTexture();
SRE_LOCAL void GL3InitializeHDRAverageLuminanceShaderWithAverageLuminanceTexture(int i);
SRE_LOCAL void GL3InitializeHDRLuminanceHistoryStorageShader();
SRE_LOCAL void GL3InitializeHDRLuminanceHistoryComparisonShader(int slot);
SRE_LOCAL void GL3InitializeHDRToneMapShader();
SRE_LOCAL void GL3InitializeOldTextShader(Color *colorp);

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
#if defined(GLES2_GLSL_NO_ARRAY_INDEXING)
// Without array indexing, the request length is severely limited.
#define SRE_TEXT_MAX_REQUEST_LENGTH 8
#else
#define SRE_TEXT_MAX_REQUEST_LENGTH 128
#endif
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
SRE_LOCAL void GL3InitializeTextShader(int update_mask, sreTextShaderInfo *info, Vector4D *rect,
    const char *string, int length);
SRE_LOCAL void GL3InitializeImageShader(int set_flags, sreImageShaderInfo *info, Vector4D *rect);

// Defined in shadow.cpp:
SRE_LOCAL void sreRenderShadowVolumes(sreScene *scene, sreLight *light, sreFrustum& frustum);
SRE_LOCAL void sreReportShadowCacheStats();
SRE_LOCAL void sreResetShadowCacheStats();
SRE_LOCAL void sreSetShadowCacheStatsInfo(sreShadowRenderingInfo *info);
SRE_LOCAL void sreClearShadowCache();

// Defined in shadowmap.cpp:
SRE_LOCAL bool GL3RenderShadowMapWithOctree(sreScene *scene, sreLight& light, sreFrustum &frustum);
SRE_LOCAL void sreVisualizeDirectionalLightShadowMap(int light_index);
SRE_LOCAL void sreVisualizeCubeMap(int light_index);
SRE_LOCAL void sreVisualizeBeamOrSpotLightShadowMap(int light_index);

// Defined is lights.cpp:
SRE_LOCAL void sreInitializeInternalShadowVolume();

// Image data structure for mipmaps.

class sreMipmapImage {
public :
	unsigned int *pixels;
	int width;
	int height;
	int extended_width;
	int extended_height;
	int alpha_bits;			// 0 for no alpha, 1 if alpha is limited to 0 and 0xFF, 8 otherwise.
	int nu_components;		// Indicates the number of components.
	int bits_per_component;		// 8 or 16.
	int is_signed;			// 1 if the components are signed, 0 if unsigned.
	int srgb;			// Whether the image is stored in sRGB format.
	int is_half_float;		// The image pixels are combinations of half-floats. The pixel size is 64-bit.
};

// Defined in mipmap.cpp:
SRE_LOCAL void generate_mipmap_level_from_original(sreMipmapImage *source_image, int level, sreMipmapImage *dest_image);
SRE_LOCAL void generate_mipmap_level_from_previous_level(sreMipmapImage *source_image, sreMipmapImage *dest_image);
SRE_LOCAL int count_mipmap_levels(sreMipmapImage *image);


// Error checking macro that is only defined if the DEBUG_OPENGL build flag was set.
#ifdef DEBUG_OPENGL
#define CHECK_GL_ERROR(s) sreCheckGLError(s);
#else
#define CHECK_GL_ERROR(s)
#endif

// This function check for the existence of OpenGL errors, and prints
// the given formatted string if an error has occurred. It flushes
// all previous errors. No newline is printed after the string.
SRE_LOCAL void sreCheckGLError(const char *format, ...);
// Abort when a GL error is detected.
SRE_LOCAL void sreAbortOnGLError(const char *format, ...);

// File read/write functions that check for errors and exit gracefully if one occurs.
// Abitrarily defined in binary_model_file.cpp.
SRE_LOCAL void fread_with_check(void *ptr, size_t size, size_t nmemb, FILE *stream);
SRE_LOCAL void fwrite_with_check(void *ptr, size_t size, size_t nmemb, FILE *stream);

