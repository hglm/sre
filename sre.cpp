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
#include <math.h>
#include <limits.h>
#include <signal.h>
#ifdef OPENGL_ES2
#include <GLES2/gl2.h>
#endif
#ifdef OPENGL
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#include "sre.h"
#include "sre_internal.h"
#include "shader.h"

// Internal engine global variables.
Point3D sre_internal_viewpoint;
float sre_internal_zoom;
int sre_internal_current_frame = 0;
int sre_internal_current_light_index;
sreLight *sre_internal_current_light;
sreScene *sre_internal_scene;
sreSwapBuffersFunc sre_internal_swap_buffers_func;
int sre_internal_window_width;
int sre_internal_window_height;
bool sre_internal_aspect_changed;
sreFrustum *sre_internal_frustum;
int sre_internal_shader_mask = 0xFF;
// Whether object shader selection should be re-evaluated (for example after a
// global rendering settings change).
bool sre_internal_reselect_shaders;
bool sre_internal_invalidate_geometry_scissors_cache;
bool sre_internal_use_depth_clamping;
int sre_internal_shadow_volume_count;
int sre_internal_silhouette_count;
sreDefaultRNG *sre_internal_rng;
void (*sreDrawTextOverlayFunc)();
Matrix3D *sre_internal_standard_UV_transformation_matrix;

SRE_GLUINT sre_internal_depth_texture = 0;
SRE_GLUINT sre_internal_shadow_map_framebuffer = 0;
SRE_GLUINT sre_internal_depth_cube_map_texture = 0;
SRE_GLUINT sre_internal_cube_shadow_map_framebuffer = 0;
SRE_GLUINT sre_internal_small_depth_texture = 0;
SRE_GLUINT sre_internal_small_shadow_map_framebuffer = 0;
SRE_GLUINT sre_internal_HDR_multisample_color_renderbuffer = 0;
SRE_GLUINT sre_internal_HDR_multisample_depth_renderbuffer = 0;
SRE_GLUINT sre_internal_HDR_color_texture = 0;
SRE_GLUINT sre_internal_HDR_multisample_framebuffer = 0;
SRE_GLUINT sre_internal_HDR_framebuffer = 0;
SRE_GLUINT sre_internal_HDR_log_luminance_texture = 0;
SRE_GLUINT sre_internal_HDR_log_luminance_framebuffer = 0;
SRE_GLUINT sre_internal_HDR_average_luminance_texture[4];
SRE_GLUINT sre_internal_HDR_average_luminance_framebuffer[4];
SRE_GLUINT sre_internal_HDR_luminance_history_storage_framebuffer = 0;
SRE_GLUINT sre_internal_HDR_luminance_history_texture;
SRE_GLUINT sre_internal_HDR_luminance_history_comparison_framebuffer = 0;
SRE_GLUINT sre_internal_HDR_used_average_luminance_texture;
SRE_GLUINT sre_internal_HDR_full_screen_vertex_buffer;
static const GLfloat HDR_full_screen_vertex_buffer_data[12] = {
    -1.0, - 1.0, 1.0, 1.0, - 1.0, 1.0, - 1.0, - 1.0, 1.0, - 1.0, 1.0, 1.0 };

// Internal engine flags and settings that can be configured.
int sre_internal_shadows = SRE_SHADOWS_NONE;
int sre_internal_scissors = SRE_SCISSORS_GEOMETRY;
// Many rendering flags are consolidated into a single variable.
int sre_internal_rendering_flags = 0;
bool sre_internal_light_attenuation_enabled = true;
bool sre_internal_shadow_caster_volume_culling_enabled = true;
bool sre_internal_multi_pass_rendering = false;
int sre_internal_max_active_lights = SRE_DEFAULT_MAX_ACTIVE_LIGHTS;
int sre_internal_reflection_model = SRE_REFLECTION_MODEL_STANDARD;
bool sre_internal_geometry_scissors_active;
// int sre_internal_octree_type = SRE_OCTREE_MIXED_WITH_QUADTREE;
int sre_internal_octree_type = SRE_OCTREE_BALANCED;
bool sre_internal_light_object_lists_enabled = true;
bool sre_internal_HDR_enabled = false;
// The shadow map region for directional lights in camera space. Allow more space in the frustum view direction (- 300)
// compared to behind the viewpoint (100).
sreBoundingVolumeAABB sre_internal_shadow_map_AABB = {
    Vector3D(- 200.0, -200.0, -300.0), Vector3D(200.0, 200.0, 100.0)
};
float sre_internal_near_plane_distance = SRE_DEFAULT_NEAR_PLANE_DISTANCE;
float sre_internal_far_plane_distance = SRE_DEFAULT_FAR_PLANE_DISTANCE;
int sre_internal_max_silhouette_edges = SRE_DEFAULT_MAX_SILHOUETTE_EDGES;
int sre_internal_max_shadow_volume_vertices = SRE_DEFAULT_MAX_SHADOW_VOLUME_VERTICES;
float sre_internal_HDR_key_value = 0.2;
int sre_internal_HDR_tone_mapping_shader = SRE_TONE_MAP_LINEAR;
int sre_internal_debug_message_level = 0;
#if defined(SPLASH_SCREEN_BLACK)
int sre_internal_splash_screen = SRE_SPLASH_BLACK;
#elif defined(SPLASH_SCREEN_NONE)
int sre_internal_splash_screen = SRE_SPLASH_NONE;
#else
int sre_internal_splash_screen = SRE_SPLASH_LOGO;
#endif
void (*sre_internal_splash_screen_custom_function)();
#ifdef SHADER_LOADING_MASK
int sre_internal_shader_loading_mask = SHADER_LOADING_MASK;
#else
int sre_internal_shader_loading_mask = SRE_SHADER_MASK_ALL;
#endif
#ifndef SHADER_PATH
#define SHADER_PATH "./"
#endif
const char *sre_internal_shader_path = SHADER_PATH;
bool sre_internal_demand_load_shaders = false;
int sre_internal_interleaved_vertex_buffers_mode = SRE_INTERLEAVED_BUFFERS_DISABLED;
// Disabling shadow volumes permanently saves effort/GPU memory space
// when uploading model data.
bool sre_internal_shadow_volumes_disabled = false;
int sre_internal_object_flags_mask = SRE_OBJECT_FLAGS_MASK_FULL;
int sre_internal_visualized_shadow_map = - 1;
int sre_internal_max_texture_size;
int sre_internal_texture_detail_flags;

void sreSetShadowsMethod(int method) {
    if (method == SRE_SHADOWS_SHADOW_VOLUMES && sre_internal_shadow_volumes_disabled) {
       sreMessage(SRE_MESSAGE_WARNING,
           "Invalid shadow rendering method requested (shadow volumes are disabled).\n");
       return;
    }
#ifdef NO_SHADOW_MAP
    if (method == SRE_SHADOWS_SHADOW_MAPPING) {
        sreMessage(SRE_MESSAGE_WARNING,
           "Invalid shadow rendering method requested (shadow mapping is not supported)\n");
        return;    
    }
#endif
    if (method == SRE_SHADOWS_SHADOW_VOLUMES)
        sreValidateShadowVolumeShaders();
#ifdef NO_SHADOW_MAP
    else if (method == SRE_SHADOWS_SHADOW_MAPPING)
        sreValidateShadowMapShaders();
#endif
    sre_internal_shadows = method;
    sre_internal_reselect_shaders = true;
}

void sreEnableMultiPassRendering() {
    sre_internal_multi_pass_rendering = true;
    sre_internal_reselect_shaders = true;
}

void sreDisableMultiPassRendering() {
    sre_internal_multi_pass_rendering = false;
    sre_internal_reselect_shaders = true;
}

void sreSetMultiPassMaxActiveLights(int n) {
    sre_internal_max_active_lights = n;
    if (n == SRE_MAX_ACTIVE_LIGHTS_UNLIMITED)
        return; // Unlimited.
    if (sre_internal_max_active_lights > SRE_MAX_ACTIVE_LIGHTS)
        sre_internal_max_active_lights = SRE_MAX_ACTIVE_LIGHTS;
}

void sreSetObjectFlagsMask(int mask) {
    sre_internal_object_flags_mask = mask;
    sre_internal_reselect_shaders = true;
}

void sreSetShaderMask(int mask) {
    sre_internal_shader_mask = mask;
    sre_internal_reselect_shaders = true;
}

void sreSetReflectionModel(int model) {
    sre_internal_reflection_model = model;
    // Note: with demand-loading of shaders, the lighting
    // shaders are correctly loaded, only when they are
    // actually required.
    sre_internal_reselect_shaders = true;
}

void sreSetLightAttenuation(bool enabled) {
    sre_internal_light_attenuation_enabled = enabled;
    sre_internal_reselect_shaders = true;
}

void sreSetLightScissors(int mode) {
    if (mode == SRE_SCISSORS_GEOMETRY && sre_internal_scissors != SRE_SCISSORS_GEOMETRY)
        sre_internal_invalidate_geometry_scissors_cache = true;
    sre_internal_scissors = mode;
}

void sreSetShadowVolumeVisibilityTest(bool enabled) {
   if (enabled)
       sre_internal_rendering_flags |= SRE_RENDERING_FLAG_SHADOW_VOLUME_VISIBILITY_TEST;
   else {
       sre_internal_rendering_flags &= ~SRE_RENDERING_FLAG_SHADOW_VOLUME_VISIBILITY_TEST;
   }
   // The test can affect the cache; with the test enabled some shadow volumes may be
   // skipped entirely while for others the shadow volume uploaded to the GPU may have
   // fewer components.
   sreClearShadowCache();
}

void sreSetShadowVolumeDarkCapVisibilityTest(bool enabled) {
   if (enabled)
       sre_internal_rendering_flags |= SRE_RENDERING_FLAG_SHADOW_VOLUME_DARKCAP_VISIBILITY_TEST;
   else {
       sre_internal_rendering_flags &= ~SRE_RENDERING_FLAG_SHADOW_VOLUME_DARKCAP_VISIBILITY_TEST;
   }
   // The test can affect the cache for depth-fail shadow volumes, allowing the darkcap to be
   // skipped.
   sreClearShadowCache();
}

void sreSetShadowMapRegion(Point3D dim_min, Point3D dim_max) {
    sre_internal_shadow_map_AABB.dim_min = dim_min;
    sre_internal_shadow_map_AABB.dim_max = dim_max;
}

void sreSetOctreeType(int type) {
    sre_internal_octree_type = type;
}

void sreSetNearPlaneDistance(float dist) {
    sre_internal_near_plane_distance = dist;
}

void sreSetFarPlaneDistance(float dist) {
    sre_internal_far_plane_distance = dist;
}

void sreSetLightObjectLists(bool enabled) {
    sre_internal_light_object_lists_enabled = enabled;
}

void sreSetHDRRendering(bool flag) {
#ifndef OPENGL_ES2
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (flag)
        glEnable(GL_FRAMEBUFFER_SRGB);
    else
        glDisable(GL_FRAMEBUFFER_SRGB);
    sre_internal_HDR_enabled = flag;
    if (flag)
        sreValidateHDRShaders();
    sre_internal_reselect_shaders = true;
#endif
} 

void sreSetHDRKeyValue(float value) {
    sre_internal_HDR_key_value = value;
}

void sreSetHDRToneMappingShader(int i) {
#ifdef NO_HDR
    sreMessage(SRE_MESSAGE_WARNING,
            "sre: Invalid tone mapping shader request (HDR rendering is disabled).");
#else
    if (i >= 0 && i < SRE_NUMBER_OF_TONE_MAPPING_SHADERS) {
        sre_internal_HDR_tone_mapping_shader = i;
        sreValidateHDRShaders();
    }
    else
        sreMessage(SRE_MESSAGE_WARNING,
            "sre: Invalid tone mapping shader selected.");
#endif
}

int sreGetCurrentHDRToneMappingShader() {
    return sre_internal_HDR_tone_mapping_shader;
}

static const char *tone_mapping_shader_name[3] = {
    "Linear", "Reinhard", "Exponential"
};

const char *sreGetToneMappingShaderName(int i) {
    if (i >= 0 && i < SRE_NUMBER_OF_TONE_MAPPING_SHADERS)
        return tone_mapping_shader_name[i];
    return "Invalid";
}

int sreGetCurrentFrame() {
    return sre_internal_current_frame;
}

void sreSetDebugMessageLevel(int level) {
    sre_internal_debug_message_level = level;
}

void sreSetSplashScreen(int type, void (*SplashScreenFunction)()) {
    sre_internal_splash_screen = type;
    if (type == SRE_SPLASH_CUSTOM)
        sre_internal_splash_screen_custom_function = SplashScreenFunction;
}

void sreSetShaderLoadingMask(int mask) {
    sre_internal_shader_loading_mask = mask;
}

void sreSwapBuffers() {
    sre_internal_swap_buffers_func();
}

static const char *shadow_str[3] = {
    "No shadows", "Shadow volumes", "Shadow mapping" };

static const char *scissors_str[8] = {
    "Scissors disabled", "Light scissors", "Invalid", "Geometry scissors", "Invalid", "Geometry matrix scissors",
    "Invalid", "Invalid"
};

static const char *reflection_model_str[2] = {
    "Standard (Blinn-Phong per pixel lighting)",
    "Micro-facet"
};

static const char *no_yes_str[2] = { "No", "Yes" };

sreEngineSettingsInfo *sreGetEngineSettingsInfo() {
    sreEngineSettingsInfo *info = new sreEngineSettingsInfo;
    info->window_width = sre_internal_window_width;
    info->window_height = sre_internal_window_height;
#ifdef OPENGL
    info->opengl_version = SRE_OPENGL_VERSION_CORE;
#else
    info->opengl_version = SRE_OPENGL_VERSION_ES2;
#endif
    info->rendering_flags = sre_internal_rendering_flags;
    info->multi_pass_rendering = sre_internal_multi_pass_rendering;
    info->reflection_model = sre_internal_reflection_model;
    info->shadows_method = sre_internal_shadows;
    info->scissors_method = sre_internal_scissors;
    info->HDR_enabled = sre_internal_HDR_enabled;
    info->HDR_tone_mapping_shader = sre_internal_HDR_tone_mapping_shader;
    info->max_anisotropy = sreGetMaxAnisotropyLevel();
    info->max_visible_active_lights = sre_internal_max_active_lights;
    info->shadows_description = shadow_str[info->shadows_method];
    info->reflection_model_description = reflection_model_str[info->reflection_model];
    info->scissors_description = scissors_str[info->scissors_method];
    info->shader_path = sre_internal_shader_path;
    return info;
}

sreShadowRenderingInfo *sreGetShadowRenderingInfo() {
     sreShadowRenderingInfo *info = new sreShadowRenderingInfo;
     info->shadow_volume_count = sre_internal_shadow_volume_count;
     info->silhouette_count = sre_internal_silhouette_count;
     sreSetShadowCacheStatsInfo(info);
     return info;
}

void sreSetShaderPath(const char *path) {
    sre_internal_shader_path = path;
}

void sreSetDemandLoadShaders(bool flag) {
    sre_internal_demand_load_shaders = flag;
}

sreDefaultRNG *sreGetDefaultRNG() {
   return sre_internal_rng;
}

void sreSetVisualizedShadowMap(int light_index) {
    sre_internal_visualized_shadow_map = light_index;
}

void sreSetDrawTextOverlayFunc(void (*func)()) {
    sreDrawTextOverlayFunc = func;
}

void sreSetTriangleStripUseForShadowVolumes(bool enabled) {
#ifndef NO_PRIMITIVE_RESTART
    if (!GL_NV_primitive_restart)
#endif
        enabled = false;
    if (enabled)
        sre_internal_rendering_flags |= SRE_RENDERING_FLAG_USE_TRIANGLE_STRIPS_FOR_SHADOW_VOLUMES;
    else
        sre_internal_rendering_flags &= ~SRE_RENDERING_FLAG_USE_TRIANGLE_STRIPS_FOR_SHADOW_VOLUMES;
    sreClearShadowCache();
}

void sreSetTriangleFanUseForShadowVolumes(bool enabled) {
    if (enabled)
        sre_internal_rendering_flags |= SRE_RENDERING_FLAG_USE_TRIANGLE_FANS_FOR_SHADOW_VOLUMES;
    else
        sre_internal_rendering_flags &= ~SRE_RENDERING_FLAG_USE_TRIANGLE_FANS_FOR_SHADOW_VOLUMES;
    sreClearShadowCache();
}

void sreSetShadowVolumeCache(bool enabled) {
   if (enabled)
       sre_internal_rendering_flags |= SRE_RENDERING_FLAG_SHADOW_CACHE_ENABLED;
   else {
       if (sre_internal_rendering_flags & SRE_RENDERING_FLAG_SHADOW_CACHE_ENABLED)
           sreClearShadowCache();
       sre_internal_rendering_flags &= ~SRE_RENDERING_FLAG_SHADOW_CACHE_ENABLED;
   }
}

void sreSetForceDepthFailRendering(bool enabled) {
   if (enabled)
       sre_internal_rendering_flags |= SRE_RENDERING_FLAG_FORCE_DEPTH_FAIL;
   else {
       // It should not be necessary to reset the shadow volume cache; new depth fail-specific shadow
       // volumes will be requested and cached automatically.
       sre_internal_rendering_flags &= ~SRE_RENDERING_FLAG_FORCE_DEPTH_FAIL;
   }
}

void sreSetGlobalTextureDetailFlags(int set_mask, int flags) {
   if (set_mask & SRE_TEXTURE_DETAIL_SET_LEVEL) {
       sre_internal_texture_detail_flags &= ~SRE_TEXTURE_DETAIL_LEVEL_MASK;
       sre_internal_texture_detail_flags = flags  & SRE_TEXTURE_DETAIL_LEVEL_MASK;
   }
   if (set_mask & SRE_TEXTURE_DETAIL_SET_NPOT) {
       sre_internal_texture_detail_flags &= ~SRE_TEXTURE_DETAIL_NPOT_MASK;
       sre_internal_texture_detail_flags |= flags & SRE_TEXTURE_DETAIL_NPOT_MASK;
   }
}

int sreGetGlobalTextureDetailFlags() {
    return sre_internal_texture_detail_flags;
}

// Allocate UV transformation matrix that flips U or V.

Matrix3D *sreNewMirroringUVTransform(bool flip_u, bool flip_v) {
    Matrix3D *m = new Matrix3D;
    m->SetIdentity();
    if (flip_u)
        m->SetRow(0, Vector3D(- 1.0f, 0, 1.0f));
    if (flip_v)
        m->SetRow(1, Vector3D(0, - 1.0f, 1.0f));
    return m;
}

// Allocate UV transformation matrix that selects a region of a source texture. Any mirroring
// is applied before the region is selected.

Matrix3D *sreNewRegionUVTransform(Vector2D top_left, Vector2D bottom_right,
bool flip_u, bool flip_v) {
    Matrix3D *m = sreNewMirroringUVTransform(flip_u, flip_v);
    // Determine the size of the region.
    Vector2D size = bottom_right - top_left;
    // We have to scale and translate the coordinates so that ([0, 1], [0, 1]) is mapped to the
    // selected region of the (possibly mirrored) texture.
    m->Set(
        (*m)(0, 0) * size.x, 0, (*m)(0, 2) + (*m)(0, 0) * top_left.x,
        0, (*m)(1, 0) * size.y, (*m)(1, 2) + (*m)(1, 0) * top_left.y,
        0, 0, 1.0f);
    return m;
}

#define default_zoom 1.0

static GLint cube_map_target[6] = {
    GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
};

#ifndef OPENGL_ES2
static void SetupHDRFramebuffer() {
    glGenFramebuffers(1, &sre_internal_HDR_multisample_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_HDR_multisample_framebuffer);
    glGenRenderbuffers(1, &sre_internal_HDR_multisample_color_renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, sre_internal_HDR_multisample_color_renderbuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA16F, sre_internal_window_width, sre_internal_window_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
        sre_internal_HDR_multisample_color_renderbuffer);
    glGenRenderbuffers(1, &sre_internal_HDR_multisample_depth_renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, sre_internal_HDR_multisample_depth_renderbuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT32F, sre_internal_window_width,
        sre_internal_window_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
        sre_internal_HDR_multisample_depth_renderbuffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        sreFatalError("Error -- HDR multisample framebuffer not complete.");
    }

    glGenFramebuffers(1, &sre_internal_HDR_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sre_internal_HDR_framebuffer);
    glGenTextures(1, &sre_internal_HDR_color_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_color_texture);
    // Consider using the GL_R11F_G11F_B10F format.
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA16F, sre_internal_window_width, sre_internal_window_height, 0, 
        GL_RGBA, GL_HALF_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, sre_internal_HDR_color_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        sreFatalError("Error -- HDR framebuffer not complete.");
    }

    CHECK_GL_ERROR("Error after HDR render setup.\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
#endif

static void sreDrawSplashScreen() {
    // Draw at (0.20, 0.20), character size 0.20 x 0.60.
    Vector2D font_size = Vector2D(0.20, 0.60);
    sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE,
        NULL, &font_size);
    sreDrawTextN("SRE", 3, 0.20, 0.20);
}

void sreInitialize(int window_width, int window_height, sreSwapBuffersFunc swap_buffers_func) {
    // Initialize the default random number generator.
    sre_internal_rng = new sreDefaultRNG;

    // Initialize the internal bounding volume structures used for temporary shadow volumes.
    sreInitializeInternalShadowVolume();

    // Initialize the standard texture UV coordinate transformation matrix, which flips
    // the V coordinate.
    sre_internal_standard_UV_transformation_matrix = sreNewMirroringUVTransform(false, true);

    sre_internal_window_width = window_width;
    sre_internal_window_height = window_height;
    sre_internal_aspect_ratio = (float)window_width / window_height;
    sre_internal_swap_buffers_func = swap_buffers_func;

    glViewport(0, 0, window_width, window_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Initialize texture detail flags first because the text font may be loaded early.
    // Get maximum texture dimension in pixels.
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &sre_internal_max_texture_size);
    sre_internal_texture_detail_flags = 0;
    // These settings will be overriden later during initialization.
    sreSetGlobalTextureDetailFlags(SRE_TEXTURE_DETAIL_SET_LEVEL, SRE_TEXTURE_DETAIL_HIGH);
    sreSetGlobalTextureDetailFlags(SRE_TEXTURE_DETAIL_SET_NPOT, 0);

    // Note: Boolean rendering flags settings should be concentrated into the single variable
    // sre_internal_rendering_flags for efficiency.

    if (sre_internal_demand_load_shaders)
        sreMessage(SRE_MESSAGE_INFO, "Demand loading of shaders enabled.\n");

    // First load the text shader, but respect the shader loading mask.
    // When demand-loading is enabled, and the splash screen is off,
    // the text shader won't yet be loaded.
    if (sre_internal_shader_loading_mask & SRE_SHADER_MASK_TEXT) {
        // This function will initialize the text shaders.
        sreInitializeTextEngine();
    }
    if (sre_internal_shader_loading_mask & SRE_SHADER_MASK_IMAGE) {
        // This function will initialize the image shaders.
        sreInitializeImageEngine();
    }
    // Draw the splash screen.
    if (sre_internal_splash_screen != SRE_SPLASH_NONE) {
        glClear(GL_COLOR_BUFFER_BIT);
        if (sre_internal_splash_screen == SRE_SPLASH_LOGO &&
        (sre_internal_shader_loading_mask & SRE_SHADER_MASK_TEXT))
            sreDrawSplashScreen();
        else if (sre_internal_splash_screen == SRE_SPLASH_CUSTOM)
            sre_internal_splash_screen_custom_function();
        sreSwapBuffers();
    }
    // Initialize the other shaders. Note with demand-loading, most shaders may not
    // actually be loaded yet.
    sreInitializeShaders(sre_internal_shader_loading_mask & (~(SRE_SHADER_MASK_TEXT
        | SRE_SHADER_MASK_IMAGE)));

    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    if (sre_internal_shadows == SRE_SHADOWS_SHADOW_VOLUMES)
        sreValidateShadowVolumeShaders();

#ifndef NO_SHADOW_MAP
    // Set up render-to-texture framebuffer for shadow map.
    glGenTextures(1, &sre_internal_depth_texture);
    glBindTexture(GL_TEXTURE_2D, sre_internal_depth_texture);
    // 24-bit uint (standard) works, float works also, probably better (most likely same
    // buffer size and higher precision).
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, SRE_SHADOW_BUFFER_SIZE,
        SRE_SHADOW_BUFFER_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SRE_SHADOW_BUFFER_SIZE,
//         SRE_SHADOW_BUFFER_SIZE, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GLfloat border_color[4];
    border_color[0] = 1.0f;
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &border_color[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glGenFramebuffers(1, &sre_internal_shadow_map_framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_shadow_map_framebuffer);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sre_internal_depth_texture, 0);
    glDrawBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        sreFatalError("Error -- shadow map framebuffer not complete.");
    }

    // Set up render-to-cubemap framebuffer for shadow map cubemap (texture array).
    glGenTextures(1, &sre_internal_depth_cube_map_texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, sre_internal_depth_cube_map_texture);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, &border_color[0]);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
    // Half-float are probably sufficient for virtually all point lights, and should improve
    // performance. Cube map resolution is a more important parameter.
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT16, SRE_CUBE_SHADOW_BUFFER_SIZE,
        SRE_CUBE_SHADOW_BUFFER_SIZE, 6, 0, GL_DEPTH_COMPONENT, GL_HALF_FLOAT, 0);

    glGenFramebuffers(1, &sre_internal_cube_shadow_map_framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_cube_shadow_map_framebuffer);

    for (int i = 0; i < 6; i++) {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            sre_internal_depth_cube_map_texture, 0, i);
    }
    CHECK_GL_ERROR("Error after glFramebufferTextureLayer\n");
    glDrawBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        sreFatalError("Error -- cube shadow map framebuffer not complete.\n");
    }

    // Set up render-to-texture framebuffer for small shadow map used for spot and beam lights.
    glGenTextures(1, &sre_internal_small_depth_texture);
    glBindTexture(GL_TEXTURE_2D, sre_internal_small_depth_texture);
    // Half-float and float both work fine, as does 24-bit uint, although the shadow map
    // visualization for debugging currently has problems with the spotlight shadow maps.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, SRE_SMALL_SHADOW_BUFFER_SIZE,
          SRE_SMALL_SHADOW_BUFFER_SIZE, 0, GL_DEPTH_COMPONENT, GL_HALF_FLOAT, 0);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, SRE_SMALL_SHADOW_BUFFER_SIZE,
//        SRE_SMALL_SHADOW_BUFFER_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SRE_SMALL_SHADOW_BUFFER_SIZE,
//        SRE_SMALL_SHADOW_BUFFER_SIZE, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // We don't need a border color because clamping is done in the pixel shader.
    // Note: Using a border color may be faster by reducing conditional execution
    // in the shader.
// #define SPOT_BEAM_SHADOW_MAP_BORDER_COLOR
#ifdef SPOT_BEAM_SHADOW_MAP_BORDER_COLOR
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &border_color[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
#else
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif

    glGenFramebuffers(1, &sre_internal_small_shadow_map_framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_small_shadow_map_framebuffer);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
        sre_internal_small_depth_texture, 0);
    glDrawBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        sreFatalError("Error -- small (spot light) shadow map framebuffer not complete.\n");
    }

    if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING)
        sreValidateShadowMapShaders();
#endif

#ifndef NO_HDR
    // Set up render-to-texture framebuffer for HDR rendering.
    SetupHDRFramebuffer();
    // Set up intermediate textures for tone mapping.
    glGenFramebuffers(1, &sre_internal_HDR_log_luminance_framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_HDR_log_luminance_framebuffer);
    glGenTextures(1, &sre_internal_HDR_log_luminance_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_log_luminance_texture);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RG32F, 256, 256, 0, GL_RG, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE,
        sre_internal_HDR_log_luminance_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        sreFatalError("Error -- HDR log luminance framebuffer not complete.\n");
    }

    int size = 64;
    for (int i = 0; i < 4; i++) {
        glGenTextures(1, &sre_internal_HDR_average_luminance_texture[i]);
        glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_average_luminance_texture[i]);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RG32F, size, size, 0, GL_RG, GL_FLOAT, NULL);
        CHECK_GL_ERROR("Error after glTexImage2D for average luminance texture.\n");
        glGenFramebuffers(1, &sre_internal_HDR_average_luminance_framebuffer[i]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_HDR_average_luminance_framebuffer[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE,
            sre_internal_HDR_average_luminance_texture[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            sreFatalError("Error -- HDR average luminance framebuffer not complete.\n");
            exit(1);
        } 
        CHECK_GL_ERROR("Error after glFramebufferTexture2D.\n");
        size /= 4;
    }

    glGenFramebuffers(1, &sre_internal_HDR_luminance_history_storage_framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_HDR_luminance_history_storage_framebuffer);
    glGenTextures(1, &sre_internal_HDR_luminance_history_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_luminance_history_texture);
    // Initialize the luminance history texture with sane values.
    GLfloat data[16 * 4];
    for (int i = 0; i < 16; i++) {
        data[i * 4] = 0.4;
        data[i * 4 + 1] = 1.0;
        data[i * 4 + 2] = 0.4;
        data[i * 4 + 3] = 1.0;
    }
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA32F, 16, 1, 0, GL_RGBA, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE,
        sre_internal_HDR_luminance_history_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        sreFatalError("Error -- HDR luminance history storage framebuffer not complete.\n");
        exit(1);
    }

    glGenFramebuffers(1, &sre_internal_HDR_luminance_history_comparison_framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sre_internal_HDR_luminance_history_comparison_framebuffer);
    // The used_average_luminance texture is used both as an input to the luminance_history_storage shader and as a
    // destination for the luminance_history_comparison_shader.
    glGenTextures(1, &sre_internal_HDR_used_average_luminance_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_used_average_luminance_texture);
    data[0] = 0.4;
    data[1] = 1.0; 
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RG32F, 1, 1, 0, GL_RG, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE,
        sre_internal_HDR_used_average_luminance_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        sreFatalError("Error -- HDR luminance history comparison framebuffer not complete.\n");
        exit(1);
    }
    // Set up vertex array consisting of two 2D triangles to cover the whole screen.
    glGenBuffers(1, &sre_internal_HDR_full_screen_vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, sre_internal_HDR_full_screen_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(GLfloat) * 2, HDR_full_screen_vertex_buffer_data, GL_STATIC_DRAW);
    CHECK_GL_ERROR("Error after HDR full screen vertex buffer setup.\n");

    // If HDR is enabled at initialization, we need to make sure the shaders are loaded.
    if (sre_internal_HDR_enabled)
        sreValidateHDRShaders();
#endif

    // Switch back to window-system-provided framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#ifdef OPENGL_ES2
    // For OpenGL-ES2, we don't use GLEW, so get the extensions string.
    const GLubyte *extensions_str = glGetString(GL_EXTENSIONS);
#endif

    // Depth clamping is mainly useful for shadow volumes, but we still try to enable it for
    // all cases.
#if defined(NO_DEPTH_CLAMP)
    sre_internal_use_depth_clamping = false;
#else
    sre_internal_use_depth_clamping = true;
    if (!GL_ARB_depth_clamp) {
        sre_internal_use_depth_clamping = false;
        sreMessage(SRE_MESSAGE_WARNING, "GL_DEPTH_CLAMP not available.\n");
    }
#endif
    if (!sre_internal_use_depth_clamping)
        // Use a tweaked matrix to avoid precision problems for normalized device coordinates close to 1.
        GL3PerspectiveTweaked(60.0 * default_zoom, (float)window_width / window_height, sre_internal_near_plane_distance, - 1.0);
#if !defined(NO_DEPTH_CLAMP)
    else {
        glEnable(GL_DEPTH_CLAMP);
        CHECK_GL_ERROR("Error after enabling depth clamping.\n");
        // Set up perspective with infinite far plane.
        GL3Perspective(60.0 * default_zoom, (float)window_width / window_height, sre_internal_near_plane_distance, - 1.0);
    }
#endif

#ifndef NO_PRIMITIVE_RESTART
    // Enable primitive restart when available.
    if (GLEW_NV_primitive_restart) {
        // As a rule, the short primitive restart token is enabled.
        glPrimitiveRestartIndexNV(0xFFFF);
        glEnableClientState(GL_PRIMITIVE_RESTART_NV);
        CHECK_GL_ERROR("Error after enabling primitive restart.\n");
        sre_internal_rendering_flags |= SRE_RENDERING_FLAG_USE_TRIANGLE_STRIPS_FOR_SHADOW_VOLUMES;
    }
#endif

    sre_internal_rendering_flags |= SRE_RENDERING_FLAG_USE_TRIANGLE_FANS_FOR_SHADOW_VOLUMES;
    sre_internal_rendering_flags |= SRE_RENDERING_FLAG_SHADOW_CACHE_ENABLED;
    sre_internal_rendering_flags |= SRE_RENDERING_FLAG_SHADOW_VOLUME_VISIBILITY_TEST;
    // Do not enable the darkcap visibility test for now because of bugs.
//    sre_internal_rendering_flags |= SRE_RENDERING_FLAG_SHADOW_VOLUME_DARKCAP_VISIBILITY_TEST;

    const char *texture_detail_str;
#ifdef OPENGL_ES2
    // Set texture detail level to medium (reduce large textures).
    sreSetGlobalTextureDetailFlags(SRE_TEXTURE_DETAIL_SET_LEVEL, SRE_TEXTURE_DETAIL_LOW);
    // ES2 mandates limited NPOT support: no mipmaps, no wrap mode (clamp only).
    // Many devices support full NPOT: Adreno, Mali, but not PowerVR, determined by
    // GL_ARB_texture_non_power_of_two or GL_OES_texture_npot.
    if (strstr(extensions, "GL_OES_texture_npot") != NULL ||
    strstr(extensions, "GL_ARB_texture_non_power_of_two") != NULL)
        sreSetGlobalTextureDetailFlags(SRE_TEXTURE_DETAIL_SET_NPOT,
            SRE_TEXTURE_DETAIL_NPOT_FULL);
    else
        sreSetGlobalTextureDetailFlags(SRE_TEXTURE_DETAIL_SET_NPOT,
            SRE_TEXTURE_DETAIL_NPOT);
    texture_detail_str = "low (reduction of average-sized and large textures)";
#else
    // Set texture detail level to high (preserve original texture size when possible).
    sreSetGlobalTextureDetailFlags(SRE_TEXTURE_DETAIL_SET_LEVEL, SRE_TEXTURE_DETAIL_HIGH);
    // Assume an OpenGL 3/DX10 class CPU, supporting full NPOT textures.
    sreSetGlobalTextureDetailFlags(SRE_TEXTURE_DETAIL_SET_NPOT,
        SRE_TEXTURE_DETAIL_NPOT_FULL);
    texture_detail_str = "high (no reduction)";
#endif
    sreMessage(SRE_MESSAGE_INFO, "Maximum texture size %dx%d, global texture detail set to %s, "
       "NPOT mipmaps: %s, NPOT repeating textures: %s",
       sre_internal_max_texture_size, sre_internal_max_texture_size,
       texture_detail_str, no_yes_str[(sreGetGlobalTextureDetailFlags() &
           SRE_TEXTURE_DETAIL_NPOT_MIPMAPS) != 0], no_yes_str[(sreGetGlobalTextureDetailFlags()
           & SRE_TEXTURE_DETAIL_NPOT_MIPMAPS) != 0]);
    // Make sure all objects have their shader selected when first drawn.
    sre_internal_reselect_shaders = true;
    // Invalidate geometry scissors cache (not strictly required).
    sre_internal_invalidate_geometry_scissors_cache = true;
}

void sreResize(sreView *view, int window_width, int window_height) {
// Note: tweaked perspective matrix not supported.
    sre_internal_window_width = window_width;
    sre_internal_window_height = window_height;
    glViewport(0, 0, window_width, window_height);
    GL3Perspective(60.0 * view->GetZoom(),
       (float)window_width / window_height, sre_internal_near_plane_distance, - 1.0);
#ifndef NO_HDR
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &sre_internal_HDR_multisample_color_renderbuffer);
    glDeleteRenderbuffers(1, &sre_internal_HDR_multisample_depth_renderbuffer);
    glDeleteFramebuffers(1, &sre_internal_HDR_multisample_framebuffer);
    glDeleteTextures(1, &sre_internal_HDR_color_texture);
    glDeleteFramebuffers(1, &sre_internal_HDR_framebuffer);
//    glDeleteTextures(1, &sre_internal_HDR_log_luminance_texture);
//    glDeleteFramebuffers(1, &sre_internal_HDR_log_luminance_framebuffer);
//    glDeleteFramebuffers(1, &sre_internal_HDR_tone_map_framebuffer);
    SetupHDRFramebuffer();
#endif
}

void sreCheckGLError(const char *format, ...) {
    GLenum errorTmp = glGetError();
    if (errorTmp != GL_NO_ERROR) {
        if (sre_internal_debug_message_level != SRE_MESSAGE_QUIET) {
            va_list args;
            va_start(args, format);
            vprintf(format, args);
            va_end(args);
            fflush(stdout);
        }
        while (glGetError() != GL_NO_ERROR);
    }
}

void sreAbortOnGLError(const char *format, ...) {
    GLenum errorTmp = glGetError();
    if (errorTmp != GL_NO_ERROR) {
        va_list args;
        va_start(args, format);
        printf("(libsre) Unexpected OpenGL error: ");
        vprintf(format, args);
        va_end(args);
        fflush(stdout);
        raise(SIGABRT);
    }
}

void sreFatalError(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("(libsre) Unexpected fatal error: ");
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
    raise(SIGABRT);
}

static void sreDisplayMessage(int priority, const char *format, va_list args) {
    if (priority == SRE_MESSAGE_WARNING)
        printf("WARNING: ");
    else if (priority == SRE_MESSAGE_CRITICAL)
        printf("CRITICAL: ");
    vprintf(format, args);
}

void sreMessageNoNewline(int priority, const char *format, ...) {
    if (priority > sre_internal_debug_message_level)
        return;
    va_list args;
    va_start(args, format);
    sreDisplayMessage(priority, format, args);
    va_end(args);
    if (priority <= SRE_MESSAGE_WARNING)
        fflush(stdout);
}

void sreMessage(int priority, const char *format, ...) {
    if (priority > sre_internal_debug_message_level)
        return;
    va_list args;
    va_start(args, format);
    sreDisplayMessage(priority, format, args);
    va_end(args);
    printf("\n");
    if (priority <= SRE_MESSAGE_WARNING)
        fflush(stdout);
}

// Globally apply new zoom settings. Default field of view is 60 degrees.
// Note: tweaked perspective matrix not supported.

void sreApplyNewZoom(sreView *view) {
    // Some effects shaders that work in screen coordinates need access to the zoom factor.
    sre_internal_zoom = view->GetZoom();
    GL3Perspective(60.0f * sre_internal_zoom,
        (float)sre_internal_window_width / sre_internal_window_height,
        sre_internal_near_plane_distance, - 1.0);
}

// sreView

sreView::sreView() {
    zoom = 1.0;
    angles.z = 0;
    angles.x = 0;
    followed_object = 0;
    view_mode = SRE_VIEW_MODE_FOLLOW_OBJECT;
    movement_mode = SRE_MOVEMENT_MODE_STANDARD;
    // Resetting the last change frames should trigger calculation of the projection matrix
    // and frustum during the first subsequent scene->Render(view) call. 
    last_view_change = sre_internal_current_frame;
    last_projection_change = sre_internal_current_frame;
}

// Standard camera view mode involves a viewpoint and view angles (thetax, thetaz).

void sreView::SetViewModeStandard(Point3D _viewpoint) {
    // Detect when the parameters do not change.
    if (view_mode == SRE_VIEW_MODE_STANDARD && _viewpoint == viewpoint)
        return;
    view_mode = SRE_VIEW_MODE_STANDARD;
    viewpoint = _viewpoint;
    last_view_change = sre_internal_current_frame;
}

// Object-following camera view mode involves a scene object, view angles (thetax, thetaz),
// a viewpoint distance from the object and an additional offset for the viewpoint.

void sreView::SetViewModeFollowObject(int object_index, float distance, Vector3D offset) {
    // Detect when the parameters do not change. When the followed object moves,
    // the CameraHasChangedSinceLastFrame() function will detect that.
    if (view_mode == SRE_VIEW_MODE_FOLLOW_OBJECT &&
    object_index == followed_object && distance == following_distance &&
    offset == following_offset)
        return;
    followed_object = object_index;
    following_distance = distance;
    following_offset = offset;
    view_mode = SRE_VIEW_MODE_FOLLOW_OBJECT;
    last_view_change = sre_internal_current_frame;
}

// Look-at camera view mode involves a viewpoint location, a look-at location, and an up-vector
// to determine the tilt of the camera view.

void sreView::SetViewModeLookAt(Point3D _viewpoint, Point3D _view_lookat, Vector3D _view_upvector) {
    // Detect when the parameters do not change.
    if (view_mode == SRE_VIEW_MODE_LOOK_AT &&
    _viewpoint == viewpoint && _view_lookat == view_lookat && _view_upvector == view_upvector)
        return;
    view_mode = SRE_VIEW_MODE_LOOK_AT;
    viewpoint = _viewpoint;
    view_lookat = _view_lookat;
    view_upvector = _view_upvector;
    last_view_change = sre_internal_current_frame;
}

void sreView::SetViewAngles(Vector3D _angles) {
    // Detect when the angles do not change (the y-axis angle is ignored).
    if (_angles.x == angles.x && _angles.z == angles.z)
        return;
    angles = _angles;
    last_view_change = sre_internal_current_frame;
}

void sreView::RotateViewDirection(Vector3D angles_offset) {
    angles += angles_offset;
    angles.x = fmodf(angles.x, 360.0f);
    angles.y = fmodf(angles.y, 360.0f);
    angles.z = fmodf(angles.z, 360.0f);
    last_view_change = sre_internal_current_frame;
}

void sreView::SetZoom(float _zoom) {
    if (_zoom == zoom)
        return;
    zoom = _zoom;
    last_projection_change = sre_internal_current_frame;
}

static void PrintMatrix4D(Matrix4D &m) {
	printf("(%f %f %f %f, %f %f %f %f, %f %f %f %f, %f, %f, %f, %f)\n",
		m.n[0][0], m.n[0][1], m.n[0][2], m.n[0][3],
		m.n[1][0], m.n[1][1], m.n[1][2], m.n[1][3],
		m.n[2][0], m.n[2][1], m.n[2][2], m.n[2][3],
		m.n[3][0], m.n[3][1], m.n[3][2], m.n[3][3]);
}

// Set view direction and up vector based on current viewing angles.

void sreView::CalculateViewDirection() {
    Vector4D v4(0, 1, 0, 1);
    Matrix4D r1;
    r1.AssignRotationAlongZAxis(angles.z * M_PI / 180.0f);
    Matrix4D r2;
    r2.AssignRotationAlongXAxis(angles.x * M_PI / 180.0f);
//    PrintMatrix4D(r1);
//    PrintMatrix4D(r2);
//    view_direction = ((r1 * r2) * v4).GetVector3D();
    Matrix4D r1_times_r2 = r1 * r2;
//    PrintMatrix4D(r1_times_r2);
    view_direction = (r1_times_r2 * v4).GetVector3D();
//    printf("angles.z = %f, angles.x = %f, direction = (%f, %f, %f)\n", angles.z, angles.x,
//        view_direction.x, view_direction.y, view_direction.z);
    Vector4D v5(0, 0, 1, 1);
    view_upvector = (r1_times_r2 * v5).GetVector3D();
}

// Update lookat parameters (viewpoint, lookat position and up vector) and
// view direction based on current viewing mode. The followed object position
// must be specified when the viewing mode is SRE_VIEW_MODE_FOLLOW_OBJECT
// (otherwise, the argument is ignored).

void sreView::UpdateParameters(Point3D object_position) {
    if (view_mode == SRE_VIEW_MODE_LOOK_AT) {
        // Although not strictly necessary, calculate the view direction.
        view_direction = view_lookat - viewpoint;
        view_direction.Normalize();
        return;
    }
    // Calculate the viewing direction from angles.
    CalculateViewDirection();
    if (view_mode == SRE_VIEW_MODE_FOLLOW_OBJECT) {
        // View relative to an object position (for example from behind an object;
        // if distance is negative the view is from in front of the object).
        viewpoint = object_position - view_direction * following_distance +
            following_offset;
    }
    // When view_mode is SRE_VIEW_MODE_STANDARD, the currently defined viewpoint
    // is used.
    view_lookat = viewpoint + view_direction;
}

void sreView::SetMovementMode(sreMovementMode mode) {
    movement_mode = mode;
}

void sreView::SetForwardVector(const Vector3D& forward) {
    forward_vector = forward;
}

void sreView::SetAscendVector(const Vector3D& ascend) {
    ascend_vector = ascend;
}
