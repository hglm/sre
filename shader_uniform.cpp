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
#define _USE_MATH_DEFINES
#include <math.h>
#include <malloc.h>
#include <float.h>
#ifdef OPENGL_ES2
#include <GLES2/gl2.h>
#endif
#ifdef OPENGL
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#include "win32_compat.h"
#include "sre.h"
#include "sre_internal.h"
#include "shader.h"

// Shadow cube map parameters.

float shadow_cube_segment_distance_scaling[6];

void GL3UpdateCubeShadowMapSegmentDistanceScaling(float *segment_distance_scaling) {
    for (int i = 0; i < 6; i++)
        shadow_cube_segment_distance_scaling[i] = segment_distance_scaling[i];
}

// Functions to update shader uniforms are defined below. This includes functions
// to update all relevant shader uniforms before each light or frame, and object
// specific shader uniform setting before the draw request. There is also initial
// uniform initialization just after loading for values that never change.

// Apart for setting up uniforms, textures are also bound to the relevant
// texture units when required.

// Common shader uniforms.

static void GL3InitializeShaderWithMVP(int loc, const sreObject& so) {
    Matrix4D MVP = sre_internal_view_projection_matrix * so.model_matrix;
    glUniformMatrix4fv(loc, 1, GL_FALSE, (const float *)&MVP);
}

static void GL3InitializeShaderWithModelMatrix(int loc, const sreObject& so) {
#ifdef OPENGL_ES2
    Matrix4D m = Matrix4D(so.model_matrix);
    glUniformMatrix4fv(loc, 1, GL_FALSE, (const float *)&m);
#else
    glUniformMatrix4x3fv(loc, 1, GL_FALSE, (const float *)&so.model_matrix);
#endif
}

static void GL3InitializeShaderWithModelRotationMatrix(int loc, const sreObject& so) {
    glUniformMatrix3fv(loc, 1, GL_FALSE, (const float *)&so.rotation_matrix);
}

static void GL3InitializeShaderWithViewProjectionMatrix(int loc) {
    glUniformMatrix4fv(loc, 1, GL_FALSE, (const float *)&sre_internal_view_projection_matrix);
}

static void GL3InitializeShaderWithMultiColor(int loc, const sreObject& so) {
    GLboolean multi_color;
    if (so.render_flags & SRE_OBJECT_MULTI_COLOR)
        multi_color = GL_TRUE;
    else
        multi_color = GL_FALSE;
    glUniform1i(loc, multi_color);
}

static void GL3InitializeShaderWithUseTexture(int loc, const sreObject& so) {
    GLboolean use_texture;
    if (so.render_flags & SRE_OBJECT_USE_TEXTURE)
        use_texture = GL_TRUE;
    else
        use_texture = GL_FALSE;
    glUniform1i(loc, use_texture);
}

static void GL3InitializeShaderWithViewpoint(int loc) {
    glUniform3f(loc, sre_internal_viewpoint.x, sre_internal_viewpoint.y, sre_internal_viewpoint.z);
}

static void GL3InitializeShaderWithLightPosition4ModelSpace(int loc, float *light_position_model_space) {
    glUniform4fv(loc, 1, light_position_model_space);
}

static void GL3InitializeShaderWithEmissionColor(int loc, const SceneObject& so) {
    glUniform3fv(loc, 1, (GLfloat *)&so.emission_color);
}

// Current light setting is not used currently; shaders only support one light per pass.
static void GL3InitializeShaderWithCurrentLight(int loc) {
    glUniform1i(loc, sre_internal_current_light_index); 
}

static void GL3InitializeShaderWithAmbientColor(int loc) {
    glUniform3f(loc, sre_internal_scene->ambient_color.r, sre_internal_scene->ambient_color.g,
        sre_internal_scene->ambient_color.b);
}

// Shadow map generating shader-specific uniforms.

#ifndef NO_SHADOW_MAP

static void GL3InitializeShadowMapShaderWithShadowMapMVP(int loc, const sreObject& so) {
    Matrix4D MVP = shadow_map_matrix * so.model_matrix;
    glUniformMatrix4fv(loc, 1, GL_FALSE, (const float *)&MVP);
}

static void GL3InitializeShadowMapShaderWithLightPosition(int loc) {
    glUniform3fv(loc, 1, (GLfloat *)&sre_internal_current_light->vector);
}

#endif

// Lighting-related uniforms.

static void GL3InitializeShaderWithDiffuseReflectionColor(int loc, const sreObject& so) {
    glUniform3fv(loc, 1, (GLfloat *)&so.diffuse_reflection_color);
}

static void GL3InitializeShaderWithSpecularReflectionColor(int loc, const sreObject& so) {
    glUniform3fv(loc, 1, (GLfloat *)&so.specular_reflection_color);
}

static void GL3InitializeShaderWithSpecularExponent(int loc, const sreObject& so) {
    glUniform1f(loc, so.specular_exponent);
}

static void GL3InitializeShaderWithDiffuseFraction(int loc, const sreObject& so) {
    glUniform1f(loc, so.diffuse_fraction);
}

static void GL3InitializeShaderWithRoughness(int loc, const sreObject& so) {
    glUniform2fv(loc, 1, (GLfloat *)&so.roughness_values);
}

static void GL3InitializeShaderWithRoughnessWeights(int loc, const sreObject& so) {
    glUniform2fv(loc, 1, (GLfloat *)&so.roughness_weights);
}

static void GL3InitializeShaderWithAnisotropic(int loc, const sreObject& so) {
    GLboolean value;
    if (so.anisotropic)
        value = GL_TRUE;
    else
        value = GL_FALSE;
    glUniform1i(loc, value);
}

// Multi-pass lighting shader light-related uniforms.
// The shaders use only one light per pass.

static void GL3InitializeMultiPassShaderWithLightPosition(int loc) {
    glUniform4fv(loc, 1, (GLfloat *)&sre_internal_current_light->vector);
}

static void GL3InitializeMultiPassShaderWithLightAttenuation(int loc) {
    float lightatt[4];
    lightatt[0] = sre_internal_current_light->attenuation.x;
    if (!sre_internal_light_attenuation_enabled) {
        if (sre_internal_current_light->type & SRE_LIGHT_LINEAR_ATTENUATION_RANGE)
            lightatt[0] = 1000000.0;
        lightatt[1] = 0;
        lightatt[2] = 0;
    }
    else {
        lightatt[1] = sre_internal_current_light->attenuation.y;
        lightatt[2] = sre_internal_current_light->attenuation.z;
    }
    if (sre_internal_current_light->type & (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM)) {
        if (sre_internal_current_light->type & SRE_LIGHT_BEAM) {
            lightatt[1] = 2.0;
            lightatt[2] = sre_internal_current_light->attenuation.y;
            lightatt[3] = sre_internal_current_light->attenuation.z;
        }
        else
            lightatt[1] = 1.0;
    }
    glUniform4fv(loc, 1, lightatt);
}

static void GL3InitializeMultiPassShaderWithLightColor(int loc) {
    glUniform3fv(loc, 1, (GLfloat *)&sre_internal_current_light->color);
}

static void GL3InitializeMultiPassShaderWithSpotlight(int loc) {
    glUniform4fv(loc, 1, (GLfloat *)&sre_internal_current_light->spotlight);
}

// The single-pass shaders use one light. The functions below that support multiple
// lights are disabled; we use the multi-pass functions instead to set light parameters.

static void GL3InitializeSinglePassShaderWithLightPosition(int loc) {
    GL3InitializeMultiPassShaderWithLightPosition(loc);
}

static void GL3InitializeSinglePassShaderWithLightAttenuation(int loc) {
    GL3InitializeMultiPassShaderWithLightAttenuation(loc);
}

static void GL3InitializeSinglePassShaderWithLightColor(int loc) {
    GL3InitializeMultiPassShaderWithLightColor(loc);
}

static void GL3InitializeSinglePassShaderWithSpotlight(int loc) {
    GL3InitializeMultiPassShaderWithSpotlight(loc);
}

#if 0

static void GL3InitializeShaderWithNumberOfLights(int loc) {
    glUniform1i(loc, sre_internal_scene->nu_active_lights);
}

static void GL3InitializeSinglePassShaderWithLightPosition(int loc) {
    float *lightpos = (float *)alloca(sizeof(float) *
        sre_internal_scene->nu_active_lights * 4);
    for (int i = 0; i < sre_internal_scene->nu_active_lights; i++) {
        lightpos[i * 4] = sre_internal_scene->global_light[
            sre_internal_scene->active_light[i]]->vector.x;
        lightpos[i * 4 + 1] = sre_internal_scene->global_light[
            sre_internal_scene->active_light[i]]->vector.y;
        lightpos[i * 4 + 2] = sre_internal_scene->global_light[
             sre_internal_scene->active_light[i]]->vector.z;
        lightpos[i * 4 + 3] = sre_internal_scene->global_light[
             sre_internal_scene->active_light[i]]->vector.w;
    }
    glUniform4fv(loc, sre_internal_scene->nu_active_lights, lightpos);
}


static void GL3InitializeSinglePassShaderWithLightAttenuation(int loc) {
    float *lightatt = (float *)alloca(sizeof(float) * sre_internal_scene->nu_active_lights * 4);
    for (int i = 0; i < sre_internal_scene->nu_active_lights; i++) {
        lightatt[i * 3] = sre_internal_scene->global_light[sre_internal_scene->active_light[i]]->attenuation.x;
        if (!sre_internal_light_attenuation_enabled) {
            if (sre_internal_scene->global_light[sre_internal_scene->active_light[i]]->type &
            SRE_LIGHT_LINEAR_ATTENUATION_RANGE)
                lightatt[i * 3] = 1000000.0;
            lightatt[i * 3 + 1] = 0;
            lightatt[i * 3 + 2] = 0;
        }
        else {
            lightatt[i * 3 + 1] = sre_internal_scene->global_light[sre_internal_scene->active_light[i]]->attenuation.y;
            lightatt[i * 3 + 2] = sre_internal_scene->global_light[sre_internal_scene->active_light[i]]->attenuation.z;
        }
        if (sre_internal_scene->global_light[sre_internal_scene->active_light[i]]->type & (SRE_LIGHT_SPOT |
        SRE_LIGHT_BEAM)) {
            if (sre_internal_scene->global_light[sre_internal_scene->active_light[i]]->type & SRE_LIGHT_BEAM) {
                // Set the light type (spot light or beam light).
                lightatt[i * 3 + 1] = 2.0;
                // Set the axial cut-off distance.
                lightatt[i * 3 + 2] =
                    sre_internal_scene->global_light[sre_internal_scene->active_light[i]]->attenuation.y;
                // Set the radial linear attenuation range.
                lightatt[i * 3 + 3] =
                    sre_internal_scene->global_light[sre_internal_scene->active_light[i]]->attenuation.z;
            }
            else
                lightatt[i * 3 + 1] = 1.0;  // Light type (spot light or beam light).
        }
    }
    glUniform4fv(loc, sre_internal_scene->nu_active_lights, lightatt);
}

static void GL3InitializeSinglePassShaderWithLightColor(int loc) {
    float *lightcol = (float *)alloca(sizeof(float) * sre_internal_scene->nu_active_lights * 3);
    for (int i = 0; i < sre_internal_scene->nu_active_lights; i++) {
        lightcol[i * 3] = sre_internal_scene->global_light[sre_internal_scene->active_light[i]]->color.r;
        lightcol[i * 3 + 1] = sre_internal_scene->global_light[sre_internal_scene->active_light[i]]->color.g;
        lightcol[i * 3 + 2] = sre_internal_scene->global_light[sre_internal_scene->active_light[i]]->color.b;
    }
    glUniform3fv(loc, sre_internal_scene->nu_active_lights, lightcol);
}

#endif

// Texture related uniforms, and texture binding.

static void GL3InitializeShaderWithObjectTexture(const sreObject& so) {
    glActiveTexture(GL_TEXTURE0);
    if (so.texture != NULL)
        glBindTexture(GL_TEXTURE_2D, so.texture->opengl_id);
    // When texture == NULL, the object has different textures for
    // each mesh. Binding will be delayed until the draw function.
}

static void GL3InitializeShaderWithUseNormalMap(int loc, const sreObject& so) {
    GLboolean use_normal_map;
    if (so.render_flags & SRE_OBJECT_USE_NORMAL_MAP)
        use_normal_map = GL_TRUE;
    else
        use_normal_map = GL_FALSE;
    glUniform1i(loc, use_normal_map);
}

static void GL3InitializeShaderWithObjectNormalMap(const sreObject& so) {
    glActiveTexture(GL_TEXTURE1);
    if (so.normal_map != NULL)
        glBindTexture(GL_TEXTURE_2D, so.normal_map->opengl_id);
    // When normal_map == NULL, the object has different normal maps for
    // each mesh. Binding will be delayed until the draw function.
}

static void GL3InitializeShaderWithScale(int loc, const sreObject& so) {
    glUniform1f(loc, so.texture3d_scale);
}

static void GL3InitializeShaderWithTexture3DType(int loc, const sreObject& so) {
    glUniform1i(loc, so.texture3d_type);
}

static void GL3InitializeShaderWithModelSubTexture(int id) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, id);
//    printf("ModelSubTexture (subsequent mesh): Texture id = %d\n", id);
}

static void GL3InitializeShaderWithModelSubNormalMap(int id) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, id);
//    printf("ModelSubNormalMap (subsequent mesh): Normal map id = %d\n", id);
}

static void GL3InitializeShaderWithUseSpecularMap(int loc, const sreObject& so) {
    GLboolean use_specular_map;
    if (so.render_flags & SRE_OBJECT_USE_SPECULARITY_MAP)
        use_specular_map = GL_TRUE;
    else
        use_specular_map = GL_FALSE;
    glUniform1i(loc, use_specular_map);
}

static void GL3InitializeShaderWithObjectSpecularMap(const sreObject& so) {
    glActiveTexture(GL_TEXTURE2);
    if (so.specularity_map != NULL)
        glBindTexture(GL_TEXTURE_2D, so.specularity_map->opengl_id);
    // When specularity_map == NULL, the object has different specularity maps for
    // each mesh. Binding will be delayed until the draw function.
}

static void GL3InitializeShaderWithModelSubSpecularMap(int id) {
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, id);
}

static void GL3InitializeShaderWithUseEmissionMap(int loc, const sreObject& so) {
    GLboolean use_emission_map;
    if (so.render_flags & SRE_OBJECT_USE_EMISSION_MAP)
        use_emission_map = GL_TRUE;
    else
        use_emission_map = GL_FALSE;
    glUniform1i(loc, use_emission_map);
}

static void GL3InitializeShaderWithObjectEmissionMap(const sreObject& so) {
    glActiveTexture(GL_TEXTURE3);
    if (so.emission_map != NULL)
        glBindTexture(GL_TEXTURE_2D, so.emission_map->opengl_id);
    // When emission_map == NULL, the object has different emission maps for
    // each mesh. Binding will be delayed until the draw function.
}

static void GL3InitializeShaderWithModelSubEmissionMap(int id) {
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, id);
}

static void GL3InitializeShaderWithUVTransform(int loc, const sreObject& so) {
    glUniformMatrix3fv(loc, 1, GL_FALSE, (GLfloat *)so.UV_transformation_matrix);
}

// Uniforms used in the halo shader.

static void GL3InitializeShaderWithAspectRatio(int loc) {
    glUniform1f(loc, sre_internal_aspect_ratio);
}

static void GL3InitializeShaderWithHaloSize(int loc, const sreObject& so) {
    glUniform1f(loc, so.halo_size / sre_internal_zoom);
}

static void GL3InitializeShaderWithId(int loc, const sreObject& so) {
    glUniform1i(loc, so.id);
}

#ifndef NO_SHADOW_MAP

// Shadow map-related uniforms setting before drawing each object in a light pass.

static void GL3InitializeShaderWithShadowMapTransformationMatrix(int loc, const sreObject& so) {
    Matrix4D transformation_matrix = shadow_map_lighting_pass_matrix * so.model_matrix;
    // Why const float *?
    glUniformMatrix4fv(loc, 1, GL_FALSE, (const float *)&transformation_matrix);
}

// Setting up before drawing objects for a light with shadow map support.

static void GL3InitializeShaderWithShadowMapTexture() {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, sre_internal_depth_texture);
}

static void GL3InitializeShaderWithSmallShadowMapTexture() {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, sre_internal_small_depth_texture);
}

static void GL3InitializeShaderWithCubeShadowMapTexture() {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D_ARRAY, sre_internal_depth_cube_map_texture);
}

void sreBindShadowMapTexture(Light *light) {
    // Note there's no need to call UseProgram here, because we only bind the shadow map texture to one
    // of the texture units (TEXTURE4).
    if (light->type & SRE_LIGHT_DIRECTIONAL)
        GL3InitializeShaderWithShadowMapTexture();
    else if (light->type & (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM))
        GL3InitializeShaderWithSmallShadowMapTexture();
    else
        GL3InitializeShaderWithCubeShadowMapTexture();
}

static void GL3InitializeShaderWithCubeSegmentDistanceScaling(int loc) {
    glUniform1fv(loc, 6, (GLfloat *)&shadow_cube_segment_distance_scaling[0]);
}

#endif

// HDR-related uniforms.

void GL3InitializeShaderWithAverageLumTextureSampler(int loc) {
    glUniform1i(loc, 1);
}

void GL3InitializeShaderWithHDRKeyValue(int loc) {
    glUniform1f(loc, sre_internal_HDR_key_value);
}

// One-time uniform initialization just after loading the shader. The
// shader program must be in use.
// Only uniforms that only need one-time initialization will actually be set.    

void sreInitializeLightingShaderUniformWithDefaultValue(int uniform_id, int loc) {
    switch (uniform_id) {
    case UNIFORM_TEXTURE_SAMPLER :
        glUniform1i(loc, 0);
        break;
    case UNIFORM_NORMAL_MAP_SAMPLER :
        glUniform1i(loc, 1);
        break;
    case UNIFORM_SPECULARITY_MAP_SAMPLER :
        glUniform1i(loc, 2);
        break;
    case UNIFORM_EMISSION_MAP_SAMPLER :
        glUniform1i(loc, 3);
        break;
    case UNIFORM_SHADOW_MAP_SAMPLER :
    case UNIFORM_CUBE_SHADOW_MAP_SAMPLER :
        glUniform1i(loc, 4);
        break;
    }
}

void sreInitializeMiscShaderUniformWithDefaultValue(int uniform_id, int loc) {
    switch (uniform_id) {
    case UNIFORM_MISC_TEXTURE_SAMPLER :
        glUniform1i(loc, 0);
        break;
    case UNIFORM_MISC_AVERAGE_LUM_SAMPLER :
        glUniform1i(loc, 1);
        break;
    // Note: The shadow map shader's shadow map textures are attached to the
    // framebuffer, so there's no need to initialize a sampler uniform. When
    // a transparent object texture is used with the shadow map shader, it will
    // already have been initialized as UNIFORM_MISC_TEXTURE_SAMPLER.
    case UNIFORM_MISC_ASPECT_RATIO :
        // When the shader are first initialized without demand-loading,
        // the aspect ratio may not yet be set up (equal to 0). When the
        // aspect ratio is set up or changed, the before-frame shader
        // initialization will set the uniform.
        // In case of a demand-loaded shader, this will set the current
        // aspect ratio.
        if (sre_internal_aspect_ratio > 0)
            GL3InitializeShaderWithAspectRatio(loc);
        break;
    }
}

enum MultiPassShaderSelection { SHADER0, SHADER1, SHADER2, SHADER3, SHADER4, SHADER5, SHADER6, SHADER7, SHADER8,
    SHADER9, SHADER10, SHADER11, SHADER12, SHADER13, SHADER14, SHADER15, SHADER16, SHADER17, SHADER18, SHADER19 };
enum SinglePassShaderSelection { SINGLE_PASS_SHADER0, SINGLE_PASS_SHADER1, SINGLE_PASS_SHADER2, SINGLE_PASS_SHADER3,
SINGLE_PASS_SHADER4, SINGLE_PASS_SHADER5, SINGLE_PASS_SHADER6, SINGLE_PASS_SHADER7 };


// Uniform initialization before each frame. This includes the camera viewpoint,
// ambient color, and for single-pass shaders light parameters.
// It would be better to initialize the shaders on a completely on-demand basis.

void GL3InitializeShadersBeforeFrame() {
    // Note: When multi-pass rendering is enabled, the only single-pass shader that may
    // be used is SINGLE_PASS_SHADER3 (for final pass objects), but it does not require
    // any uniform initialization before the frame (no viewpoint or ambient color needed).
    if (!sre_internal_multi_pass_rendering) {
        for (int i = 0; i < NU_SINGLE_PASS_SHADERS; i++) {
            // Skip before-frame initialization unloaded shaders.
            if (single_pass_shader[i].status != SRE_SHADER_STATUS_LOADED)
                continue;
            glUseProgram(single_pass_shader[i].program);
            if (single_pass_shader[i].uniform_mask & (1 << UNIFORM_VIEWPOINT))
                GL3InitializeShaderWithViewpoint(single_pass_shader[i].uniform_location[UNIFORM_VIEWPOINT]);
            if (single_pass_shader[i].uniform_mask & (1 << UNIFORM_AMBIENT_COLOR))
                GL3InitializeShaderWithAmbientColor(single_pass_shader[i].uniform_location[UNIFORM_AMBIENT_COLOR]);
#if 0
            // The following four initializations actually don't need to be done every frame, only on start-up.
            if (single_pass_shader[i].uniform_mask & (1 << UNIFORM_TEXTURE_SAMPLER))
                GL3InitializeShaderWithTexture(single_pass_shader[i].uniform_location[UNIFORM_TEXTURE_SAMPLER]);
            if (single_pass_shader[i].uniform_mask & (1 << UNIFORM_NORMAL_MAP_SAMPLER))
                GL3InitializeShaderWithNormalMap(single_pass_shader[i].uniform_location[UNIFORM_NORMAL_MAP_SAMPLER]);
            if (single_pass_shader[i].uniform_mask & (1 << UNIFORM_SPECULARITY_MAP_SAMPLER))
                GL3InitializeShaderWithSpecularMap(
                    single_pass_shader[i].uniform_location[UNIFORM_SPECULARITY_MAP_SAMPLER]);
            if (single_pass_shader[i].uniform_mask & (1 << UNIFORM_EMISSION_MAP_SAMPLER))
                GL3InitializeShaderWithEmissionMap(
                    single_pass_shader[i].uniform_location[UNIFORM_EMISSION_MAP_SAMPLER]);
#endif
            // If multi-pass rendering is enabled, the active_lights data structures are not filled in so
            // we must avoid the single-pass shader initialization of light parameters.
            if (sre_internal_multi_pass_rendering)
                continue;
            // Initialize the parameters for the single light.
            if (single_pass_shader[i].uniform_mask & (1 << UNIFORM_LIGHT_POSITION))
                GL3InitializeSinglePassShaderWithLightPosition(
                    single_pass_shader[i].uniform_location[UNIFORM_LIGHT_POSITION]);
            if (single_pass_shader[i].uniform_mask & (1 << UNIFORM_LIGHT_ATT))
                GL3InitializeSinglePassShaderWithLightAttenuation(
                    single_pass_shader[i].uniform_location[UNIFORM_LIGHT_ATT]);
            if (single_pass_shader[i].uniform_mask & (1 << UNIFORM_LIGHT_COLOR))
                GL3InitializeSinglePassShaderWithLightColor(single_pass_shader[i].uniform_location[UNIFORM_LIGHT_COLOR]);
            if (single_pass_shader[i].uniform_mask & (1 << UNIFORM_SPOTLIGHT))
                GL3InitializeSinglePassShaderWithSpotlight(single_pass_shader[i].uniform_location[UNIFORM_SPOTLIGHT]);
        }
    }
    if (sre_internal_multi_pass_rendering) {
        for (int i = 0; i < NU_LIGHTING_PASS_SHADERS; i++) {
            // Skip before-frame initialization unloaded shaders.
            if (lighting_pass_shader[i].status != SRE_SHADER_STATUS_LOADED)
                continue;
            glUseProgram(lighting_pass_shader[i].program);
            if (lighting_pass_shader[i].uniform_mask & (1 << UNIFORM_VIEWPOINT))
                GL3InitializeShaderWithViewpoint(lighting_pass_shader[i].uniform_location[UNIFORM_VIEWPOINT]);
            if (lighting_pass_shader[i].uniform_mask & (1 << UNIFORM_AMBIENT_COLOR))
                GL3InitializeShaderWithAmbientColor(lighting_pass_shader[i].uniform_location[UNIFORM_AMBIENT_COLOR]);
#if 0
            // The following initializations actually don't need to be done every frame, only on start-up.
            if (lighting_pass_shader[i].uniform_mask & (1 << UNIFORM_TEXTURE_SAMPLER))
                GL3InitializeShaderWithTexture(lighting_pass_shader[i].uniform_location[UNIFORM_TEXTURE_SAMPLER]);
            if (lighting_pass_shader[i].uniform_mask & (1 << UNIFORM_NORMAL_MAP_SAMPLER))
                GL3InitializeShaderWithNormalMap(lighting_pass_shader[i].uniform_location[UNIFORM_NORMAL_MAP_SAMPLER]);
            if (lighting_pass_shader[i].uniform_mask & (1 << UNIFORM_SPECULARITY_MAP_SAMPLER))
                GL3InitializeShaderWithSpecularMap(
                    lighting_pass_shader[i].uniform_location[UNIFORM_SPECULARITY_MAP_SAMPLER]);
            if (lighting_pass_shader[i].uniform_mask & (1 << UNIFORM_EMISSION_MAP_SAMPLER))
                GL3InitializeShaderWithEmissionMap(
                    lighting_pass_shader[i].uniform_location[UNIFORM_EMISSION_MAP_SAMPLER]);
#ifndef NO_SHADOW_MAP
            if (lighting_pass_shader[i].uniform_mask & (1 << UNIFORM_SHADOW_MAP_SAMPLER))
                GL3InitializeShaderWithShadowMap(
                    lighting_pass_shader[i].uniform_location[UNIFORM_SHADOW_MAP_SAMPLER]);
            if (lighting_pass_shader[i].uniform_mask & (1 << UNIFORM_CUBE_SHADOW_MAP_SAMPLER))
                GL3InitializeShaderWithCubeShadowMap(
                    lighting_pass_shader[i].uniform_location[UNIFORM_CUBE_SHADOW_MAP_SAMPLER]);
#endif
#endif
        }
    }

    if (misc_shader[SRE_MISC_SHADER_HALO].status == SRE_SHADER_STATUS_LOADED) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_HALO].program);
        GL3InitializeShaderWithViewProjectionMatrix(misc_shader[SRE_MISC_SHADER_HALO].uniform_location[UNIFORM_MISC_VIEW_PROJECTION_MATRIX]);
        if (sre_internal_aspect_changed)
            GL3InitializeShaderWithAspectRatio(
                misc_shader[SRE_MISC_SHADER_HALO].uniform_location[UNIFORM_MISC_ASPECT_RATIO]);
    }
    if (misc_shader[SRE_MISC_SHADER_PS].status == SRE_SHADER_STATUS_LOADED) {
       glUseProgram(misc_shader[SRE_MISC_SHADER_PS].program);
       GL3InitializeShaderWithViewProjectionMatrix(misc_shader[SRE_MISC_SHADER_PS].uniform_location[UNIFORM_MISC_VIEW_PROJECTION_MATRIX]);
       if (sre_internal_aspect_changed)
           GL3InitializeShaderWithAspectRatio(
               misc_shader[SRE_MISC_SHADER_PS].uniform_location[UNIFORM_MISC_ASPECT_RATIO]);
    }
#if 0
    // Some of this initialization should be done at initialization, not every frame.
    if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_SHADOW_MAP_TRANSPARENT].program);
        GL3InitializeShaderWithTexture(misc_shader[SRE_MISC_SHADER_SHADOW_MAP_TRANSPARENT].uniform_location[UNIFORM_MISC_TEXTURE_SAMPLER]);
        glUseProgram(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].program);
        GL3InitializeShaderWithTexture(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].uniform_location[UNIFORM_MISC_TEXTURE_SAMPLER]);
    }
    glUseProgram(misc_shader[SRE_MISC_SHADER_IMAGE_TEXTURE].program);
    GL3InitializeShaderWithTexture(misc_shader[SRE_MISC_SHADER_IMAGE_TEXTURE]
        .uniform_location[UNIFORM_MISC_TEXTURE_SAMPLER]);
    glUseProgram(misc_shader[SRE_MISC_SHADER_IMAGE_TEXTURE_ONE_COMPONENT].program);
    GL3InitializeShaderWithTexture(misc_shader[SRE_MISC_SHADER_IMAGE_TEXTURE_ONE_COMPONENT]
        .uniform_location[UNIFORM_MISC_TEXTURE_SAMPLER]);
    glUseProgram(misc_shader[SRE_MISC_SHADER_IMAGE_TEXTURE_ARRAY].program);
    GL3InitializeShaderWithTexture(misc_shader[SRE_MISC_SHADER_IMAGE_TEXTURE_ARRAY]
        .uniform_location[UNIFORM_MISC_TEXTURE_SAMPLER]);
    glUseProgram(misc_shader[SRE_MISC_SHADER_IMAGE_TEXTURE_ARRAY_ONE_COMPONENT].program);
    GL3InitializeShaderWithTexture(misc_shader[SRE_MISC_SHADER_IMAGE_TEXTURE_ARRAY_ONE_COMPONENT]
        .uniform_location[UNIFORM_MISC_TEXTURE_SAMPLER]);
#endif
#ifndef NO_HDR
#if 0
    if (misc_shader[SRE_MISC_SHADER_HDR_LOG_LUMINANCE].status == SRE_SHADER_STATUS_LOADED) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_HDR_LOG_LUMINANCE].program);
        GL3InitializeShaderWithTexture(misc_shader[SRE_MISC_SHADER_HDR_LOG_LUMINANCE].uniform_location[UNIFORM_MISC_TEXTURE_SAMPLER]);
    }
    if (misc_shader[SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE].status == SRE_SHADER_STATUS_LOADED) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE].program);
        GL3InitializeShaderWithTexture(misc_shader[SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE].uniform_location[UNIFORM_MISC_TEXTURE_SAMPLER]);
    }
    if (misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].status == SRE_SHADER_STATUS_LOADED) {
       glUseProgram(misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].program);
       GL3InitializeShaderWithTexture(
           misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].uniform_location[UNIFORM_MISC_TEXTURE_SAMPLER]);
       GL3InitializeShaderWithAverageLumTextureSampler(
           misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].uniform_location[UNIFORM_MISC_AVERAGE_LUM_SAMPLER]);
    }
    if (misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].status == SRE_SHADER_STATUS_LOADED) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].program);
        GL3InitializeShaderWithTexture(
            misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].uniform_location[UNIFORM_MISC_TEXTURE_SAMPLER]);
    }
#endif
    if (HDR_tone_map_shader[sre_internal_HDR_tone_mapping_shader].status == SRE_SHADER_STATUS_LOADED) {
        // Only the key value for a HDR shader is perhaps variable.
        glUseProgram(HDR_tone_map_shader[sre_internal_HDR_tone_mapping_shader].program);
        GL3InitializeShaderWithHDRKeyValue(HDR_tone_map_shader[sre_internal_HDR_tone_mapping_shader].
            uniform_location[UNIFORM_MISC_KEY_VALUE]);
#if 0
        GL3InitializeShaderWithTexture(HDR_tone_map_shader[sre_internal_HDR_tone_mapping_shader].
            uniform_location[UNIFORM_MISC_TEXTURE_SAMPLER]);
        GL3InitializeShaderWithAverageLumTextureSampler(HDR_tone_map_shader[sre_internal_HDR_tone_mapping_shader].
            uniform_location[UNIFORM_MISC_AVERAGE_LUM_SAMPLER]);
#endif
    }
#endif
}

class MultiPassShaderList {
public :
    MultiPassShaderSelection default_shader; // Unused.
    int nu_shaders;
    int shader[6];
};

static MultiPassShaderList shader_list_directional_standard = {
    SHADER4,
    4,
    { 0, 4, 5, 19 }
};

static MultiPassShaderList shader_list_directional_microfacet = {
    SHADER10,
    1,
    { 10 }
};

#if 0

static MultiPassShaderList shader_list_directional_shadow_map_standard = {
    SHADER12,
    2,
    { 12, 18 }
};

static MultiPassShaderList shader_list_directional_shadow_map_microfacet = {
    SHADER14,
    1,
    { 14 }
};

#else

static MultiPassShaderList shader_list_directional_shadow_map_standard = {
    SHADER12,
    6,
    { 0, 4, 5, 19, 12, 18 }
};

static MultiPassShaderList shader_list_directional_shadow_map_microfacet = {
    SHADER14,
    2,
    { 10, 14 }
};

#endif

static MultiPassShaderList shader_list_point_source = {
    SHADER0,
    6,
    { 0, 2, 3, 6, 8, 10 }
}; 

static MultiPassShaderList shader_list_spot_light_standard = {
    SHADER7,
    2,
    { 7, 9 }
};

static MultiPassShaderList shader_list_spot_light_microfacet = {
    SHADER11,
    1,
    { 11 }
};

#if 0

static MultiPassShaderList shader_list_spot_light_shadow_map_standard = {
    SHADER16,
    1,
    { 16 }
};

static MultiPassShaderList shader_list_spot_light_shadow_map_microfacet = {
    SHADER17,
    1,
    { 17 }
};

#else

static MultiPassShaderList shader_list_spot_light_shadow_map_standard = {
    SHADER16,
    3,
    { 7, 9, 16 }
};

static MultiPassShaderList shader_list_spot_light_shadow_map_microfacet = {
    SHADER17,
    2,
    { 11, 17 }
};

#endif

static MultiPassShaderList shader_list_point_source_linear_attenuation_range_standard = {
    SHADER7,
    2,
    { 7, 9 }
};

static MultiPassShaderList shader_list_point_source_linear_attenuation_range_microfacet = {
    SHADER11,
    1,
    { 11 }
};

#if 0

static MultiPassShaderList shader_list_point_source_linear_attenuation_range_shadow_map_standard = {
    SHADER13,
    1,
    { 13 }
};

static MultiPassShaderList shader_list_point_source_linear_attenuation_range_shadow_map_microfacet = {
    SHADER15,
    1,
    { 15 }
};

#else

static MultiPassShaderList shader_list_point_source_linear_attenuation_range_shadow_map_standard = {
    SHADER13,
    3,
    { 7, 9, 13 }
};

static MultiPassShaderList shader_list_point_source_linear_attenuation_range_shadow_map_microfacet = {
    SHADER15,
    2,
    { 11, 15 }
};

#endif

// Initialization of multi-pass shaders before each light. It would be better to initialize the
// shaders on a completety on-demand basis (the current implementation does initialize only the
// possible shaders for the light, which may not all be used).

void GL3InitializeShadersBeforeLight() {
    // This function is only called when multi-pass rendering is enabled, before each lighting pass.
    if (sre_internal_current_light_index == - 1)
        return;
    // With the new optimization where non-shadow map shaders may be used when shadow mapping is
    // enabled, more shaders have to be initialized before each light.
    MultiPassShaderList *list;
    if (sre_internal_current_light->type & SRE_LIGHT_DIRECTIONAL)
        if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING)
            if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                list = &shader_list_directional_shadow_map_microfacet;
            else
                list = &shader_list_directional_shadow_map_standard;
        else
            if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                list = &shader_list_directional_microfacet;
            else 
                list = &shader_list_directional_standard;
    else
        if (sre_internal_current_light->type & SRE_LIGHT_LINEAR_ATTENUATION_RANGE)
            if (sre_internal_current_light->type & (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM))
                if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING)
                    if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                        list = &shader_list_spot_light_shadow_map_microfacet;
                    else
                        list = &shader_list_spot_light_shadow_map_standard;
                else
                    if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                        list = &shader_list_spot_light_microfacet;
                    else 
                        list = &shader_list_spot_light_standard;
            else
                if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING)
                    if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                        list = &shader_list_point_source_linear_attenuation_range_shadow_map_microfacet;
                    else
                        list = &shader_list_point_source_linear_attenuation_range_shadow_map_standard;
                else
                    if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                        list = &shader_list_point_source_linear_attenuation_range_microfacet;
                    else 
                        list = &shader_list_point_source_linear_attenuation_range_standard;
        else
            list = &shader_list_point_source;
    for (int i = 0; i < list->nu_shaders; i++) {
        int j = list->shader[i];
        // Only initialize shaders that are loaded.
        if (lighting_pass_shader[j].status != SRE_SHADER_STATUS_LOADED)
            continue;
        glUseProgram(lighting_pass_shader[j].program);
        if (lighting_pass_shader[j].uniform_mask & (1 << UNIFORM_LIGHT_POSITION))
            GL3InitializeMultiPassShaderWithLightPosition(
                lighting_pass_shader[j].uniform_location[UNIFORM_LIGHT_POSITION]);
        if (lighting_pass_shader[j].uniform_mask & (1 << UNIFORM_LIGHT_ATT))
            GL3InitializeMultiPassShaderWithLightAttenuation(lighting_pass_shader[j].uniform_location[UNIFORM_LIGHT_ATT]);
        if (lighting_pass_shader[j].uniform_mask & (1 << UNIFORM_LIGHT_COLOR))
            GL3InitializeMultiPassShaderWithLightColor(lighting_pass_shader[j].uniform_location[UNIFORM_LIGHT_COLOR]);
        if (lighting_pass_shader[j].uniform_mask & (1 << UNIFORM_SPOTLIGHT))
            GL3InitializeMultiPassShaderWithSpotlight(lighting_pass_shader[j].uniform_location[UNIFORM_SPOTLIGHT]);
#ifndef NO_SHADOW_MAP
        if (lighting_pass_shader[j].uniform_mask & (1 << UNIFORM_SEGMENT_DISTANCE_SCALING))
            GL3InitializeShaderWithCubeSegmentDistanceScaling(
                lighting_pass_shader[j].uniform_location[UNIFORM_SEGMENT_DISTANCE_SCALING]);
#endif
    }
#ifndef NO_SHADOW_MAP
    if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING)
        sreBindShadowMapTexture(sre_internal_current_light);
#endif
}

#ifndef NO_SHADOW_MAP

// Initialization of shaders used to create shadow maps before each new shadow map is created.
// Note the checks whether the shaders are loaded should be unnecessary if the status of the
// shaders is properly checked when shadow mapping is enabled (just load them).

void GL3InitializeShadowMapShadersBeforeLight() {
    if (misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP].status == SRE_SHADER_STATUS_LOADED) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP].program);
        GL3InitializeShadowMapShaderWithLightPosition(
            misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP].uniform_location[UNIFORM_MISC_LIGHT_POSITION]);
    }
    if (misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].status == SRE_SHADER_STATUS_LOADED) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].program);
        GL3InitializeShadowMapShaderWithLightPosition(
           misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].
               uniform_location[UNIFORM_MISC_LIGHT_POSITION]);
    }
}

void GL3InitializeShadowMapShadersWithSegmentDistanceScaling(float scaling) {
    if (misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP].status == SRE_SHADER_STATUS_LOADED) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP].program);
        glUniform1f(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP].
            uniform_location[UNIFORM_MISC_SEGMENT_DISTANCE_SCALING], scaling);
    }
    if (misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].status == SRE_SHADER_STATUS_LOADED) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].program);
        glUniform1f(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].
            uniform_location[UNIFORM_MISC_SEGMENT_DISTANCE_SCALING], scaling);
    }
}

#endif

// Object-specific initialization of single-pass, multi-pass or halo shader.

// Lighting pass of multi-pass shader.

// The shader selected depends on the light type, so the current shader has to be
// cached for a few possible light types. The attribute mask is not affected by
// the light type (except for ambient light, which has its own shader info structure).
//
// Shader Light type                           Side conditions
// 14     DIRECTIONAL                          Shadow mapping, micro-facet
// 18     DIRECTIONAL                          Shadow mapping, not micro-facet, earth shader
// 12     DIRECTIONAL                          Shadow mapping, not micro-facet
// 17     SPOT/BEAM, LINEAR_ATTENUATION_RANGE         Shadow mapping, micro-facet
// 16     SPOT/BEAM, LINEAR_ATTENUATION_RANGE         Shadow mapping, not micro-facet
// 15     POINT_SOURCE, LINEAR_ATTENUATION_RANGE      Shadow mapping, micro-facet
// 13     POINT_SOURCE, LINEAR_ATTENUATION_RANGE      Shadow mapping, not micro-facet
// -      Not DIRECTIONAL, regular attenuation Shadow mapping (not implemented)
// 11     LINEAR_ATTENUATION_RANGE                    No shadow mapping, micro-facet
// 10     DIRECTIONAL or regular attenuation          No shadow mapping, micro-facet
// 7      LINEAR_ATTENUATION_RANGE             No shadow mapping, transparent texture, not micro-facet
// 0      DIRECTIONAL or regular attenuation   No shadow mapping, transparent texture, not micro-facet
// 5      DIRECTIONAL                          No shadow mapping, no transparent texture, not micro-facet,
//                                             regular texture only, no multi-color
// 19     DIRECTIONAL                          No shadow mapping, no transparent texture, not micro-facet,
//                                             earth shader
// 4      DIRECTIONAL                          No shadow mapping, no transparent texture, not micro-facet,
//                                             not regular texture only, no earth shader
// 9      NOT DIRECTIONAL, LINEAR_ATTENUATION_RANGE   No shadow mapping, no transparent texture,
//                                             not micro-facet, no texture/normal map/specularity map,
//                                             no multi-color
// 7      NOT DIRECTIONAL, LINEAR_ATTENUATION_RANGE   No shadow mapping, no transparent texture,
//                                             not micro-facet, texture/normal map/specularity map or
//                                             multi-color present
// 2      NOT DIRECTIONAL, regular attenuation No shadow mapping, no transparent texture, not micro-facet,
//                                             no texture/normal map/specularity map, multi-color enabled
// 3      NOT DIRECTIONAL, regular attenuation No shadow mapping, no transparent texture, not micro-facet,
//                                             regular texture only, no multi-color
// 6      NOT DIRECTIONAL, regular attenuation No shadow mapping, no transparent texture, not micro-facet,
//                                             normal map/specularity map or multi-color present
//
// Note: Spot/beam light are more or less assumed to always have a linear attenuation range.
//
// Given the same global settings (i.e. shadow mapping and/or micro-facet enabled), the light types
// should cover all possible shaders on a per-object basis.
//
// Required light types for shader selection:
//
// Global settings                             Light types
// Shadow mapping, micro-facet                 DIRECTIONAL, SPOT/BEAM, POINT_SOURCE
// Shadow mapping, not micro-facet             DIRECTIONAL, SPOT/BEAM, POINT_SOURCE
// No shadow mapping, micro-facet              LINEAR_ATTENUATION_RANGE, directional or regular atten.
// No shadow mapping, not micro-facet          DIRECTIONAL, LINEAR_ATTENUATION_RANGE, regular attenuation
//
// To cover all cases, the following are needed:
// DIRECTIONAL
// POINT_SOURCE with LINEAR_ATTENUATION_RANGE
// SPOT/BEAM with LINEAR_ATTENUATION_RANGE
// POINT_SOURCE with regular attenuation


static MultiPassShaderSelection sreSelectMultiPassShader(const sreObject& so) {
    MultiPassShaderSelection shader;
    int flags = so.render_flags;
    // Note: sreSelectMultiPassShadowMapShader should be used when shadow mapping is
    // actually required for the object.
#if 0
        if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING)
            if (sre_internal_current_light->type & SRE_LIGHT_DIRECTIONAL)
                // Directional light.
                if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                    shader = SHADER14;
                else
                    if (flags & SRE_OBJECT_EARTH_SHADER)
                        shader = SHADER18;
                    else
                        shader = SHADER12;
            else
                if (sre_internal_current_light->type &
                SRE_LIGHT_LINEAR_ATTENUATION_RANGE)
                    if (sre_internal_current_light->type &
                    (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM))
                        if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                            shader = SHADER17;
                        else
                            shader = SHADER16;
                    else
                        // Point source light with linear attenuation range. 
                        if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                            shader = SHADER15;
                        else
                            shader = SHADER13;
                else
                    // Point source light with traditional attenuation.
                    shader = SHADER6; // Not implemented yet.
        else // Not shadow mapping.
#endif
        if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
            if (sre_internal_current_light->type & SRE_LIGHT_LINEAR_ATTENUATION_RANGE)
                shader = SHADER11;
            else
                shader = SHADER10;
        else // Not micro-facet.
        if (sre_internal_shader_mask == 0x01 || (flags & SRE_OBJECT_TRANSPARENT_TEXTURE))
            // Optimized shaders have been disabled, or object has a transparent punchthrough texture.
            if (sre_internal_current_light->type & SRE_LIGHT_LINEAR_ATTENUATION_RANGE)
                shader = SHADER7;
            else
                shader = SHADER0;
        else
        if (sre_internal_current_light->type & SRE_LIGHT_DIRECTIONAL) {
            // Directional light.
            if ((flags & (SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP)) == SRE_OBJECT_USE_TEXTURE)
                shader = SHADER5;
            else
            if (flags & SRE_OBJECT_EARTH_SHADER)
                shader = SHADER19;
            else
                shader = SHADER4;
        }
        else // Point source light, beam or spot light.
        if (sre_internal_current_light->type & SRE_LIGHT_LINEAR_ATTENUATION_RANGE)
            // Linear attenuation range.
            if ((flags & (SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP)) == 0)
                shader = SHADER9;
            else
                shader = SHADER7;
        else // Not a linear attenuation range.
        if ((flags & (SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
        SRE_OBJECT_USE_SPECULARITY_MAP)) == SRE_OBJECT_MULTI_COLOR)
            shader = SHADER2;
        else
        if ((flags & (SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
        SRE_OBJECT_USE_SPECULARITY_MAP)) == SRE_OBJECT_USE_TEXTURE)
            shader = SHADER3;
        else
            shader = SHADER6;
        return shader;
}

// Select shader when shadow mapping is enabled, and is actually required for the object.

static MultiPassShaderSelection sreSelectMultiPassShadowMapShader(const sreObject& so) {
        MultiPassShaderSelection shader;
        int flags = so.render_flags;
            if (sre_internal_current_light->type & SRE_LIGHT_DIRECTIONAL)
                // Directional light.
                if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                    shader = SHADER14;
                else
                    if (flags & SRE_OBJECT_EARTH_SHADER)
                        shader = SHADER18;
                    else
                        shader = SHADER12;
            else
                if (sre_internal_current_light->type &
                SRE_LIGHT_LINEAR_ATTENUATION_RANGE)
                    if (sre_internal_current_light->type &
                    (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM))
                        if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                            shader = SHADER17;
                        else
                            shader = SHADER16;
                    else
                        // Point source light with linear attenuation range. 
                        if (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET)
                            shader = SHADER15;
                        else
                            shader = SHADER13;
                else
                    // Point source light with traditional attenuation.
                    shader = SHADER6; // Not implemented yet.
        return shader;
}


static void sreInitializeMultiPassShader(const sreObject& so, MultiPassShaderSelection shader) {
        // Handle demand-loading of lighting-pass shaders.
        if (lighting_pass_shader[shader].status != SRE_SHADER_STATUS_LOADED) {
            lighting_pass_shader[shader].Load();
            // Must make sure the before-frame and before-light initialization is done.
            // This could be optimized by only initializing the newly loaded shader.
            GL3InitializeShadersBeforeFrame();
            if (sre_internal_multi_pass_rendering)
                GL3InitializeShadersBeforeLight();
        }
        int flags = so.render_flags;
        switch (shader) {
        case SHADER0 :	// Complete lighting pass shader.
            glUseProgram(lighting_pass_shader[0].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[0].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[0].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[0].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[0].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[0].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[0].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[0].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[0].uniform_location[UNIFORM_USE_TEXTURE], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[0].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(lighting_pass_shader[0].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform( 
                   lighting_pass_shader[0].uniform_location[UNIFORM_UV_TRANSFORM], so);
            break;
        case SHADER1 : // Ambient-pass shader plus emission color and map.
            glUseProgram(lighting_pass_shader[1].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[1].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[1].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[1].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[1].uniform_location[UNIFORM_USE_TEXTURE], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithEmissionColor(lighting_pass_shader[1].uniform_location[UNIFORM_EMISSION_COLOR], so);
            GL3InitializeShaderWithUseEmissionMap(lighting_pass_shader[1].uniform_location[UNIFORM_USE_EMISSION_MAP], so);
            if (flags & SRE_OBJECT_USE_EMISSION_MAP)
                GL3InitializeShaderWithObjectEmissionMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_EMISSION_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[1].uniform_location[UNIFORM_UV_TRANSFORM], so);
            break;
        case SHADER2 : // Plain multi-color object lighting pass shader for point source lights.
            glUseProgram(lighting_pass_shader[2].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[2].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[2].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[2].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[2].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[2].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            break;
        case SHADER3 : // Plain texture-mapped object lighting pass shader for point source lights.
            glUseProgram(lighting_pass_shader[3].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[3].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[3].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[3].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[3].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[3].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[3].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUVTransform(
                lighting_pass_shader[3].uniform_location[UNIFORM_UV_TRANSFORM], so);
            break;
        case SHADER4 : // Complete lighting pass shader for directional lights.
            glUseProgram(lighting_pass_shader[4].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[4].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[4].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[4].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[4].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[4].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[4].uniform_location[UNIFORM_USE_TEXTURE], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[4].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[4].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[4].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(lighting_pass_shader[4].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[4].uniform_location[UNIFORM_UV_TRANSFORM], so);
            break;
        case SHADER5: // Plain texture mapped object lighting pass shader for directional lights.
            glUseProgram(lighting_pass_shader[5].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[5].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[5].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[5].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[5].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[5].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[5].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUVTransform(
                lighting_pass_shader[5].uniform_location[UNIFORM_UV_TRANSFORM], so);
            break;
        case SHADER6 :	// Complete lighting pass shader for point source lights.
            glUseProgram(lighting_pass_shader[6].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[6].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[6].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[6].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[6].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[6].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[6].uniform_location[UNIFORM_USE_TEXTURE], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[6].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[6].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[6].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(lighting_pass_shader[6].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[6].uniform_location[UNIFORM_UV_TRANSFORM], so);
            break;
        case SHADER7 : // Complete lighting pass shader for point source lights/spot lights/beam lights 
                       // with a linear attenuation range.
            glUseProgram(lighting_pass_shader[7].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[7].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[7].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[7].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[7].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[7].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[7].uniform_location[UNIFORM_USE_TEXTURE], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[7].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[7].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[7].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(lighting_pass_shader[7].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[7].uniform_location[UNIFORM_UV_TRANSFORM], so);
            break;
        case SHADER8 : // Plain Phong-shading lighting pass shader for point source lights.
            glUseProgram(lighting_pass_shader[8].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[8].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[8].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[8].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[8].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[8].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[8].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            break;
        case SHADER9 : // Plain Phong-shading lighting pass shader for point source lights/
                       // spot lights/beam lights with a linear attenuation range.
            glUseProgram(lighting_pass_shader[9].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[9].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[9].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[9].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[9].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[9].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[9].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            break;
        case SHADER10 :	// Complete lighting pass shader with microfacet reflection model.
            glUseProgram(lighting_pass_shader[10].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[10].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[10].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[10].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[10].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[10].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[10].uniform_location[UNIFORM_USE_TEXTURE], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[10].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[10].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(
                lighting_pass_shader[10].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[10].uniform_location[UNIFORM_UV_TRANSFORM], so);
            GL3InitializeShaderWithDiffuseFraction(
                lighting_pass_shader[10].uniform_location[UNIFORM_DIFFUSE_FRACTION], so);
            GL3InitializeShaderWithRoughness(
                lighting_pass_shader[10].uniform_location[UNIFORM_ROUGHNESS], so);
            GL3InitializeShaderWithRoughnessWeights(
                lighting_pass_shader[10].uniform_location[UNIFORM_ROUGHNESS_WEIGHTS], so);
            GL3InitializeShaderWithAnisotropic(
                lighting_pass_shader[10].uniform_location[UNIFORM_ANISOTROPIC], so);
            break;
        case SHADER11 :	// Complete lighting pass shader with microfacet reflection model for
                        // point lights/spot lights/beam lights with a linear attenuation range.
            glUseProgram(lighting_pass_shader[11].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[11].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[11].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[11].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[11].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[11].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[11].uniform_location[UNIFORM_USE_TEXTURE], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[11].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[11].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(
                lighting_pass_shader[11].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[11].uniform_location[UNIFORM_UV_TRANSFORM], so);
            GL3InitializeShaderWithDiffuseFraction(
                lighting_pass_shader[11].uniform_location[UNIFORM_DIFFUSE_FRACTION], so);
            GL3InitializeShaderWithRoughness(
                lighting_pass_shader[11].uniform_location[UNIFORM_ROUGHNESS], so);
            GL3InitializeShaderWithRoughnessWeights(
                lighting_pass_shader[11].uniform_location[UNIFORM_ROUGHNESS_WEIGHTS], so);
            GL3InitializeShaderWithAnisotropic(
                lighting_pass_shader[11].uniform_location[UNIFORM_ANISOTROPIC], so);
            break;
#ifndef NO_SHADOW_MAP
        case SHADER12 : // Shadow map, directional light.
            glUseProgram(lighting_pass_shader[12].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[12].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[12].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[12].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[12].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[12].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[12].uniform_location[UNIFORM_USE_TEXTURE], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[12].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[12].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[12].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(lighting_pass_shader[12].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[12].uniform_location[UNIFORM_UV_TRANSFORM], so);
            GL3InitializeShaderWithShadowMapTransformationMatrix(
                lighting_pass_shader[12].uniform_location[UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX], so);
            break;
        case SHADER13 : // Shadow cube-map, point source light with linear attenuation range.
            glUseProgram(lighting_pass_shader[13].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[13].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[13].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[13].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[13].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[13].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[13].uniform_location[UNIFORM_USE_TEXTURE], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[13].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[13].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[13].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(
                lighting_pass_shader[13].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[13].uniform_location[UNIFORM_UV_TRANSFORM], so);
            break;
        case SHADER14 : // Complete microfacet shadow map lighting pass shader for directional lights.
            glUseProgram(lighting_pass_shader[14].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[14].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[14].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[14].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[14].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[14].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[14].uniform_location[UNIFORM_USE_TEXTURE], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[14].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[14].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(lighting_pass_shader[14].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[14].uniform_location[UNIFORM_UV_TRANSFORM], so);
            GL3InitializeShaderWithDiffuseFraction(
                lighting_pass_shader[14].uniform_location[UNIFORM_DIFFUSE_FRACTION], so);
            GL3InitializeShaderWithRoughness(
                lighting_pass_shader[14].uniform_location[UNIFORM_ROUGHNESS], so);
            GL3InitializeShaderWithRoughnessWeights(
                lighting_pass_shader[14].uniform_location[UNIFORM_ROUGHNESS_WEIGHTS], so);
            GL3InitializeShaderWithAnisotropic(
                lighting_pass_shader[14].uniform_location[UNIFORM_ANISOTROPIC], so);
            GL3InitializeShaderWithShadowMapTransformationMatrix(
                lighting_pass_shader[14].uniform_location[UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX], so);
            break;
        case SHADER15 : // Complete microfacet shadow map lighting pass shader for point source lights
                        // with a linear attenuation range.
            glUseProgram(lighting_pass_shader[15].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[15].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[15].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[15].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[15].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[15].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[15].uniform_location[UNIFORM_USE_TEXTURE], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[15].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[15].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(lighting_pass_shader[15].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[15].uniform_location[UNIFORM_UV_TRANSFORM], so);
            GL3InitializeShaderWithDiffuseFraction(
                lighting_pass_shader[15].uniform_location[UNIFORM_DIFFUSE_FRACTION], so);
            GL3InitializeShaderWithRoughness(
                lighting_pass_shader[15].uniform_location[UNIFORM_ROUGHNESS], so);
            GL3InitializeShaderWithRoughnessWeights(
                lighting_pass_shader[15].uniform_location[UNIFORM_ROUGHNESS_WEIGHTS], so);
            GL3InitializeShaderWithAnisotropic(
                lighting_pass_shader[15].uniform_location[UNIFORM_ANISOTROPIC], so);
            break;
        case SHADER16 : // Shadow map, spot/beam light.
            glUseProgram(lighting_pass_shader[16].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[16].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[16].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[16].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[16].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[16].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[16].uniform_location[UNIFORM_USE_TEXTURE], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[16].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[16].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[16].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(lighting_pass_shader[16].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[16].uniform_location[UNIFORM_UV_TRANSFORM], so);
            GL3InitializeShaderWithShadowMapTransformationMatrix(
                lighting_pass_shader[16].uniform_location[UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX], so);
            break;
        case SHADER17 : // Shadow map, spot/beam light, micro-facet.
            glUseProgram(lighting_pass_shader[17].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[17].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[17].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[17].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[17].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithMultiColor(lighting_pass_shader[17].uniform_location[UNIFORM_MULTI_COLOR], so);
            GL3InitializeShaderWithUseTexture(lighting_pass_shader[17].uniform_location[UNIFORM_USE_TEXTURE], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[17].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            if (flags & SRE_OBJECT_USE_TEXTURE)
                GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithUseNormalMap(lighting_pass_shader[17].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
            if (flags & SRE_OBJECT_USE_NORMAL_MAP)
                GL3InitializeShaderWithObjectNormalMap(so);
            GL3InitializeShaderWithUseSpecularMap(
                lighting_pass_shader[17].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
            if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
                GL3InitializeShaderWithObjectSpecularMap(so);
            if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP))
               GL3InitializeShaderWithUVTransform(
                   lighting_pass_shader[17].uniform_location[UNIFORM_UV_TRANSFORM], so);
            GL3InitializeShaderWithDiffuseFraction(
                lighting_pass_shader[17].uniform_location[UNIFORM_DIFFUSE_FRACTION], so);
            GL3InitializeShaderWithRoughness(
                lighting_pass_shader[17].uniform_location[UNIFORM_ROUGHNESS], so);
            GL3InitializeShaderWithRoughnessWeights(
                lighting_pass_shader[17].uniform_location[UNIFORM_ROUGHNESS_WEIGHTS], so);
            GL3InitializeShaderWithAnisotropic(
                lighting_pass_shader[17].uniform_location[UNIFORM_ANISOTROPIC], so);
            GL3InitializeShaderWithShadowMapTransformationMatrix(
                lighting_pass_shader[17].uniform_location[UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX], so);
            break;
        case SHADER18 : // Earth shadow map, directional light.
            glUseProgram(lighting_pass_shader[18].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[18].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[18].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[18].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[18].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[18].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[18].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithObjectSpecularMap(so);
            GL3InitializeShaderWithObjectEmissionMap(so);
            GL3InitializeShaderWithShadowMapTransformationMatrix(
                lighting_pass_shader[18].uniform_location[UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX], so);
            break;
        case SHADER19 : // Earth shader, directional light.
            glUseProgram(lighting_pass_shader[19].program);
            GL3InitializeShaderWithMVP(lighting_pass_shader[19].uniform_location[UNIFORM_MVP], so);
            GL3InitializeShaderWithModelMatrix(lighting_pass_shader[19].uniform_location[UNIFORM_MODEL_MATRIX], so);
            GL3InitializeShaderWithModelRotationMatrix(
                lighting_pass_shader[19].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
            GL3InitializeShaderWithDiffuseReflectionColor(
                lighting_pass_shader[19].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularReflectionColor(
                lighting_pass_shader[19].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
            GL3InitializeShaderWithSpecularExponent(
                lighting_pass_shader[19].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
            GL3InitializeShaderWithObjectTexture(so);
            GL3InitializeShaderWithObjectSpecularMap(so);
            GL3InitializeShaderWithObjectEmissionMap(so);
            break;
#endif
        }
}

static SinglePassShaderSelection sreSelectSinglePassShader(const sreObject& so) {
    SinglePassShaderSelection shader;
    int flags = so.render_flags;
    if (flags & SRE_OBJECT_EMISSION_ONLY)
        if ((flags & (SRE_OBJECT_MULTI_COLOR |
        SRE_OBJECT_USE_EMISSION_MAP | SRE_OBJECT_USE_TEXTURE)) ==
        SRE_OBJECT_MULTI_COLOR) {
            shader = SINGLE_PASS_SHADER7;
        }
        else
            shader = SINGLE_PASS_SHADER3;
    else
    if (sre_internal_shader_mask == 0x01 || (flags & SRE_OBJECT_TRANSPARENT_TEXTURE))
        if (sre_internal_current_light->type & SRE_LIGHT_LINEAR_ATTENUATION_RANGE)
            shader = SINGLE_PASS_SHADER6;
        else
            shader = SINGLE_PASS_SHADER0;
    else
    if (sre_internal_current_light->type & SRE_LIGHT_DIRECTIONAL) {
        int map_flags = flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
            SRE_OBJECT_USE_SPECULARITY_MAP | SRE_OBJECT_USE_EMISSION_MAP);
        // Directional light
        if (map_flags == 0)
            shader = SINGLE_PASS_SHADER2;
        else if (map_flags == SRE_OBJECT_USE_TEXTURE)
            shader = SINGLE_PASS_SHADER4;
        else if (map_flags == (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP))
            shader = SINGLE_PASS_SHADER5;
        else
            shader = SINGLE_PASS_SHADER1;
    }
    else
        if (sre_internal_current_light->type
        & SRE_LIGHT_LINEAR_ATTENUATION_RANGE)
            shader = SINGLE_PASS_SHADER6;
        else
            shader = SINGLE_PASS_SHADER0;
    return shader;
}

static void sreInitializeSinglePassShader(const sreObject& so, SinglePassShaderSelection shader) {
    if (single_pass_shader[shader].status != SRE_SHADER_STATUS_LOADED) {
        // Demand-loaded shader.
        single_pass_shader[shader].Load();
        // Must make sure the before-frame initialization is done.
        // This could be optimized by only initializing the newly loaded shader.
        GL3InitializeShadersBeforeFrame();
    }
    int flags = so.render_flags;
    switch (shader) {
    case SINGLE_PASS_SHADER0 :	// Complete single pass pass shader.
        glUseProgram(single_pass_shader[0].program);
        GL3InitializeShaderWithMVP(single_pass_shader[0].uniform_location[UNIFORM_MVP], so);
        GL3InitializeShaderWithModelMatrix(single_pass_shader[0].uniform_location[UNIFORM_MODEL_MATRIX], so);
        GL3InitializeShaderWithModelRotationMatrix(
            single_pass_shader[0].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
        GL3InitializeShaderWithDiffuseReflectionColor(
            single_pass_shader[0].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
        GL3InitializeShaderWithMultiColor(single_pass_shader[0].uniform_location[UNIFORM_MULTI_COLOR], so);
        GL3InitializeShaderWithUseTexture(single_pass_shader[0].uniform_location[UNIFORM_USE_TEXTURE], so);
        GL3InitializeShaderWithSpecularReflectionColor(
            single_pass_shader[0].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
        GL3InitializeShaderWithSpecularExponent(
            single_pass_shader[0].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
        if (flags & SRE_OBJECT_USE_TEXTURE)
            GL3InitializeShaderWithObjectTexture(so);
        GL3InitializeShaderWithUseNormalMap(single_pass_shader[0].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
        if (flags & SRE_OBJECT_USE_NORMAL_MAP)
            GL3InitializeShaderWithObjectNormalMap(so);
        GL3InitializeShaderWithUseSpecularMap(single_pass_shader[0].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
        if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
            GL3InitializeShaderWithObjectSpecularMap(so);
        GL3InitializeShaderWithEmissionColor(single_pass_shader[0].uniform_location[UNIFORM_EMISSION_COLOR], so);
        GL3InitializeShaderWithUseEmissionMap(single_pass_shader[0].uniform_location[UNIFORM_USE_EMISSION_MAP], so);
        if (flags & SRE_OBJECT_USE_EMISSION_MAP)
            GL3InitializeShaderWithObjectEmissionMap(so);
        if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
        SRE_OBJECT_USE_SPECULARITY_MAP | SRE_OBJECT_USE_EMISSION_MAP))
            GL3InitializeShaderWithUVTransform(
                single_pass_shader[0].uniform_location[UNIFORM_UV_TRANSFORM], so);
        break;
    case SINGLE_PASS_SHADER1 : // Complete single pass shader for directional lights.
        glUseProgram(single_pass_shader[1].program);
        GL3InitializeShaderWithMVP(single_pass_shader[1].uniform_location[UNIFORM_MVP], so);
        GL3InitializeShaderWithModelMatrix(single_pass_shader[1].uniform_location[UNIFORM_MODEL_MATRIX], so);
        GL3InitializeShaderWithModelRotationMatrix(
            single_pass_shader[1].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
        GL3InitializeShaderWithDiffuseReflectionColor(
            single_pass_shader[1].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
        GL3InitializeShaderWithMultiColor(single_pass_shader[1].uniform_location[UNIFORM_MULTI_COLOR], so);
        GL3InitializeShaderWithUseTexture(single_pass_shader[1].uniform_location[UNIFORM_USE_TEXTURE], so);
        GL3InitializeShaderWithSpecularReflectionColor(
            single_pass_shader[1].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
        GL3InitializeShaderWithSpecularExponent(
            single_pass_shader[1].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
        if (flags & SRE_OBJECT_USE_TEXTURE)
            GL3InitializeShaderWithObjectTexture(so);
        GL3InitializeShaderWithUseNormalMap(single_pass_shader[1].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
        if (flags & SRE_OBJECT_USE_NORMAL_MAP)
            GL3InitializeShaderWithObjectNormalMap(so);
        GL3InitializeShaderWithUseSpecularMap(single_pass_shader[1].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
        if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
            GL3InitializeShaderWithObjectSpecularMap(so);
        GL3InitializeShaderWithEmissionColor(single_pass_shader[1].uniform_location[UNIFORM_EMISSION_COLOR], so);
        GL3InitializeShaderWithUseEmissionMap(single_pass_shader[1].uniform_location[UNIFORM_USE_EMISSION_MAP], so);
        if (flags & SRE_OBJECT_USE_EMISSION_MAP)
            GL3InitializeShaderWithObjectEmissionMap(so);
        if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
        SRE_OBJECT_USE_SPECULARITY_MAP | SRE_OBJECT_USE_EMISSION_MAP))
            GL3InitializeShaderWithUVTransform(
                single_pass_shader[1].uniform_location[UNIFORM_UV_TRANSFORM], so);
        break;
    case SINGLE_PASS_SHADER2 : // Phong-only single pass shader with no support for any maps
                               // (directional light).
         glUseProgram(single_pass_shader[2].program);
        GL3InitializeShaderWithMVP(single_pass_shader[2].uniform_location[UNIFORM_MVP], so);
        GL3InitializeShaderWithModelMatrix(single_pass_shader[2].uniform_location[UNIFORM_MODEL_MATRIX], so);
        GL3InitializeShaderWithModelRotationMatrix(
            single_pass_shader[2].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
        GL3InitializeShaderWithDiffuseReflectionColor(
            single_pass_shader[2].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
        GL3InitializeShaderWithMultiColor(single_pass_shader[2].uniform_location[UNIFORM_MULTI_COLOR], so);
        GL3InitializeShaderWithSpecularReflectionColor(
            single_pass_shader[2].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
        GL3InitializeShaderWithSpecularExponent(
            single_pass_shader[2].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
        GL3InitializeShaderWithEmissionColor(
            single_pass_shader[2].uniform_location[UNIFORM_EMISSION_COLOR], so);
        break;
    case SINGLE_PASS_SHADER3 : // Constant shader (emission color or map only). This shader is also
                               // for emission-only objects in the final pass of multi-pass rendering.
        glUseProgram(single_pass_shader[3].program);
        GL3InitializeShaderWithMVP(single_pass_shader[3].uniform_location[UNIFORM_MVP], so);
        GL3InitializeShaderWithEmissionColor(
            single_pass_shader[3].uniform_location[UNIFORM_EMISSION_COLOR], so);
        GL3InitializeShaderWithUseEmissionMap(
            single_pass_shader[3].uniform_location[UNIFORM_USE_EMISSION_MAP], so);
        if (flags & SRE_OBJECT_USE_EMISSION_MAP) {
            GL3InitializeShaderWithObjectEmissionMap(so);
            GL3InitializeShaderWithUVTransform(
                single_pass_shader[3].uniform_location[UNIFORM_UV_TRANSFORM], so);
        }
        break;
    case SINGLE_PASS_SHADER4 : // Phong texture-only single pass shader (directional light).
        glUseProgram(single_pass_shader[4].program);
        GL3InitializeShaderWithMVP(single_pass_shader[4].uniform_location[UNIFORM_MVP], so);
        GL3InitializeShaderWithModelMatrix(single_pass_shader[4].uniform_location[UNIFORM_MODEL_MATRIX], so);
        GL3InitializeShaderWithModelRotationMatrix(
            single_pass_shader[4].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
        GL3InitializeShaderWithDiffuseReflectionColor(
            single_pass_shader[4].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
        GL3InitializeShaderWithSpecularReflectionColor(
            single_pass_shader[4].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
        GL3InitializeShaderWithSpecularExponent(
            single_pass_shader[4].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
        GL3InitializeShaderWithObjectTexture(so);
        GL3InitializeShaderWithUVTransform(
            single_pass_shader[4].uniform_location[UNIFORM_UV_TRANSFORM], so);
        GL3InitializeShaderWithEmissionColor(
            single_pass_shader[4].uniform_location[UNIFORM_EMISSION_COLOR], so);
        break;
    case SINGLE_PASS_SHADER5 : // Phong texture plus normal map only single pass shader (directional light).
        glUseProgram(single_pass_shader[5].program);
        GL3InitializeShaderWithMVP(single_pass_shader[5].uniform_location[UNIFORM_MVP], so);
        GL3InitializeShaderWithModelMatrix(single_pass_shader[5].uniform_location[UNIFORM_MODEL_MATRIX], so);
        GL3InitializeShaderWithModelRotationMatrix(
            single_pass_shader[5].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
        GL3InitializeShaderWithDiffuseReflectionColor(
            single_pass_shader[5].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
        GL3InitializeShaderWithSpecularReflectionColor(
            single_pass_shader[5].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
        GL3InitializeShaderWithSpecularExponent(
            single_pass_shader[5].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
        GL3InitializeShaderWithObjectTexture(so);
        GL3InitializeShaderWithObjectNormalMap(so);
        GL3InitializeShaderWithUVTransform(
            single_pass_shader[5].uniform_location[UNIFORM_UV_TRANSFORM], so);
        GL3InitializeShaderWithEmissionColor(
            single_pass_shader[5].uniform_location[UNIFORM_EMISSION_COLOR], so);
        break;
    case SINGLE_PASS_SHADER6 :	// Complete single pass pass shader for point source lights with a linear
                                // attenuation range.
        glUseProgram(single_pass_shader[6].program);
        GL3InitializeShaderWithMVP(single_pass_shader[6].uniform_location[UNIFORM_MVP], so);
        GL3InitializeShaderWithModelMatrix(single_pass_shader[6].uniform_location[UNIFORM_MODEL_MATRIX], so);
        GL3InitializeShaderWithModelRotationMatrix(
            single_pass_shader[6].uniform_location[UNIFORM_MODEL_ROTATION_MATRIX], so);
        GL3InitializeShaderWithDiffuseReflectionColor(
            single_pass_shader[6].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
        GL3InitializeShaderWithMultiColor(single_pass_shader[6].uniform_location[UNIFORM_MULTI_COLOR], so);
        GL3InitializeShaderWithUseTexture(single_pass_shader[6].uniform_location[UNIFORM_USE_TEXTURE], so);
        GL3InitializeShaderWithSpecularReflectionColor(
            single_pass_shader[6].uniform_location[UNIFORM_SPECULAR_REFLECTION_COLOR], so);
        GL3InitializeShaderWithSpecularExponent(
            single_pass_shader[6].uniform_location[UNIFORM_SPECULAR_EXPONENT], so);
        if (flags & SRE_OBJECT_USE_TEXTURE)
            GL3InitializeShaderWithObjectTexture(so);
        GL3InitializeShaderWithUseNormalMap(single_pass_shader[6].uniform_location[UNIFORM_USE_NORMAL_MAP], so);
        if (flags & SRE_OBJECT_USE_NORMAL_MAP)
            GL3InitializeShaderWithObjectNormalMap(so);
        GL3InitializeShaderWithUseSpecularMap(single_pass_shader[6].uniform_location[UNIFORM_USE_SPECULARITY_MAP], so);
        if (flags & SRE_OBJECT_USE_SPECULARITY_MAP)
            GL3InitializeShaderWithObjectSpecularMap(so);
        GL3InitializeShaderWithEmissionColor(single_pass_shader[6].uniform_location[UNIFORM_EMISSION_COLOR], so);
        GL3InitializeShaderWithUseEmissionMap(single_pass_shader[6].uniform_location[UNIFORM_USE_EMISSION_MAP], so);
        if (flags & SRE_OBJECT_USE_EMISSION_MAP)
            GL3InitializeShaderWithObjectEmissionMap(so);
        if (flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
        SRE_OBJECT_USE_SPECULARITY_MAP | SRE_OBJECT_USE_EMISSION_MAP))
            GL3InitializeShaderWithUVTransform(
                single_pass_shader[6].uniform_location[UNIFORM_UV_TRANSFORM], so);
        break;  
    case SINGLE_PASS_SHADER7 : // Constant shader (with multi-color support).
        glUseProgram(single_pass_shader[7].program);
        GL3InitializeShaderWithMVP(single_pass_shader[7].uniform_location[UNIFORM_MVP], so);
        GL3InitializeShaderWithDiffuseReflectionColor(
            single_pass_shader[7].uniform_location[UNIFORM_DIFFUSE_REFLECTION_COLOR], so);
        GL3InitializeShaderWithMultiColor(single_pass_shader[7].uniform_location[UNIFORM_MULTI_COLOR], so);
        GL3InitializeShaderWithEmissionColor(
            single_pass_shader[7].uniform_location[UNIFORM_EMISSION_COLOR], so);
        break;
    }
}

void sreInitializeObjectShaderLightHalo(const sreObject& so) {
    if (so.render_flags & SRE_OBJECT_PARTICLE_SYSTEM) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_PS].program);
        GL3InitializeShaderWithEmissionColor(misc_shader[SRE_MISC_SHADER_PS].uniform_location[UNIFORM_MISC_BASE_COLOR], so);
        GL3InitializeShaderWithHaloSize(misc_shader[SRE_MISC_SHADER_PS].uniform_location[UNIFORM_MISC_HALO_SIZE], so);
        return;
    }
    // Light halo
    glUseProgram(misc_shader[SRE_MISC_SHADER_HALO].program);
    GL3InitializeShaderWithMVP(misc_shader[SRE_MISC_SHADER_HALO].uniform_location[UNIFORM_MISC_MVP], so);
    GL3InitializeShaderWithEmissionColor(misc_shader[SRE_MISC_SHADER_HALO].uniform_location[UNIFORM_MISC_BASE_COLOR], so);
    GL3InitializeShaderWithHaloSize(misc_shader[SRE_MISC_SHADER_HALO].uniform_location[UNIFORM_MISC_HALO_SIZE], so);
}

static inline void SetRenderFlags(sreObject& so) {
    so.render_flags = so.flags & sre_internal_object_flags_mask;
}

// Either select the shader for the object, or use the previously configured shader
// for the object, and then initialize the shader's uniforms.
// Take global rendering settings into account (potentially masking out or altering
// some object flags).

bool sreInitializeObjectShaderEmissionOnly(sreObject& so) {
    bool select_new_shader = (sre_internal_reselect_shaders ||
        so.current_shader[SRE_SHADER_LIGHT_TYPE_ALL] == - 1);
    if (select_new_shader) {
        SetRenderFlags(so);
        so.current_shader[SRE_SHADER_LIGHT_TYPE_ALL] = (int)SINGLE_PASS_SHADER3;
    }
    sreInitializeSinglePassShader(so, SINGLE_PASS_SHADER3);
    return select_new_shader;
}

bool sreInitializeObjectShaderSinglePass(sreObject& so) {
    bool select_new_shader = (sre_internal_reselect_shaders ||
        so.current_shader[SRE_SHADER_LIGHT_TYPE_ALL] == - 1);
    SinglePassShaderSelection s;
    if (select_new_shader) {
        SetRenderFlags(so);
        s = sreSelectSinglePassShader(so);
        so.current_shader[SRE_SHADER_LIGHT_TYPE_ALL] = (int)s;
    }
    else
        s = (SinglePassShaderSelection)so.current_shader[SRE_SHADER_LIGHT_TYPE_ALL];
    sreInitializeSinglePassShader(so, s);
    return select_new_shader;
}

bool sreInitializeObjectShaderAmbientPass(sreObject& so) {
    bool select_new_shader = (sre_internal_reselect_shaders ||
        so.current_shader[SRE_SHADER_LIGHT_TYPE_AMBIENT] == - 1);
    if (select_new_shader) {
        SetRenderFlags(so);
        // Use the multi-pass ambient shader.
        so.current_shader[SRE_SHADER_LIGHT_TYPE_AMBIENT] = (int)SHADER1;
    }
    sreInitializeMultiPassShader(so, SHADER1);
    return select_new_shader;
}

bool sreInitializeObjectShaderMultiPassLightingPass(sreObject& so) {
    int light_type = sre_internal_current_light->shader_light_type;
    bool select_new_shader = (sre_internal_reselect_shaders ||
        so.current_shader[light_type] == - 1);
    MultiPassShaderSelection s;
    if (select_new_shader) {
        SetRenderFlags(so);
        s = sreSelectMultiPassShader(so);
        so.current_shader[light_type] = (int)s;
    }
    else
        s = (MultiPassShaderSelection)so.current_shader[light_type];
    sreInitializeMultiPassShader(so, s);
    return select_new_shader;
}

// Select shader when shadow mapping is needed. Note that when shadow mapping
// is enabled but not needed for the object, the function above should be used
// to select a non-shadow map shader.

bool sreInitializeObjectShaderMultiPassShadowMapLightingPass(sreObject& so) {
    int light_type = sre_internal_current_light->shader_light_type;
    bool select_new_shader = (sre_internal_reselect_shaders ||
        so.current_shader_shadow_map[light_type] == - 1);
    MultiPassShaderSelection s;
    if (select_new_shader) {
        SetRenderFlags(so);
        s = sreSelectMultiPassShadowMapShader(so);
        so.current_shader_shadow_map[light_type] = (int)s;
    }
    else
        s = (MultiPassShaderSelection)so.current_shader_shadow_map[light_type];
    sreInitializeMultiPassShader(so, s);
    return select_new_shader;
}

// Initialize a sub-mesh of an object.

void sreInitializeShaderWithMesh(sreObject *so, sreModelMesh *mesh) {
    int render_flags = so->render_flags;
    if (render_flags & SRE_OBJECT_USE_TEXTURE)
        GL3InitializeShaderWithModelSubTexture(mesh->texture_opengl_id);
    if (render_flags & SRE_OBJECT_USE_NORMAL_MAP)
        GL3InitializeShaderWithModelSubNormalMap(mesh->normal_map_opengl_id);
    if (render_flags & SRE_OBJECT_USE_SPECULARITY_MAP)
        GL3InitializeShaderWithModelSubSpecularMap(mesh->specular_map_opengl_id);
    if (render_flags & SRE_OBJECT_USE_EMISSION_MAP)
        GL3InitializeShaderWithModelSubEmissionMap(mesh->emission_map_opengl_id);
}

#if 0
// Either select the shader for the object, or use the previously configured shader
// for the object, and then initialize the shader's uniforms.

bool sreInitializeObjectShader(sreObject& so) {
    bool select_new_shader = (sre_internal_reselect_shaders ||
        so.current_shader == - 1);
    if (so.renderer & (SRE_OBJECT_USE_LIGHTING_PASS | SRE_OBJECT_USE_AMBIENT_ONLY)) {
         MultiPassShaderSelection s;
         if (select_new_shader) {
             s = sreSelectMultiPassShader(so);
             so.current_shader = (int)s;
         }
         else
             s = (MultiPassShaderSelection)so.current_shader;
         sreInitializeMultiPassShader(so, s);
    }
    else if (so.renderer & RENDER_ELEMENT_LIGHT_HALO) {
         // Selecting a halo shader isn't much work (only two cases), so we don't
         // explictly store the selected shader.
         sreInitializeHaloShader(so);
         if (select_new_shader)
             // As long as so.current_shader is not equal to - 1 it's OK.
             so.current_shader = 0;
    }
    else {
         SinglePassShaderSelection s;
         if (select_new_shader) {
             s = sreSelectSinglePassShader(so);
             so.current_shader = (int)s;
         }
         else
             s = (SinglePassShaderSelection)so.current_shader;
         sreInitializeSinglePassShader(so, s);
    }
    return select_new_shader;
}
#endif

// Initialization of other shaders.

#if 0
void GL3InitializeTextShader(Color *colorp) {
    glUseProgram(misc_shader[SRE_MISC_SHADER_TEXT1].program);
    glUniform3fv(misc_shader[SRE_MISC_SHADER_TEXT1].uniform_location[UNIFORM_MISC_BASE_COLOR],
    1, (GLfloat *)colorp);
}
#endif

void GL3InitializeImageShader(int update_mask, sreImageShaderInfo *info, Vector4D *rect) {
    int shader;
    if (info->source_flags & SRE_IMAGE_SOURCE_FLAG_ONE_COMPONENT_SOURCE) {
        if (info->source_flags & SRE_IMAGE_SOURCE_FLAG_TEXTURE_ARRAY)
            shader = SRE_MISC_SHADER_IMAGE_TEXTURE_ARRAY_ONE_COMPONENT;
        else
            shader = SRE_MISC_SHADER_IMAGE_TEXTURE_ONE_COMPONENT;
    }
    else {
        if (info->source_flags & SRE_IMAGE_SOURCE_FLAG_TEXTURE_ARRAY)
            shader = SRE_MISC_SHADER_IMAGE_TEXTURE_ARRAY;
        else
            shader = SRE_MISC_SHADER_IMAGE_TEXTURE;
    }
    misc_shader[shader].Validate();
    glUseProgram(misc_shader[shader].program);
    if (update_mask & SRE_IMAGE_SET_RECTANGLE)
        glUniform4fv(misc_shader[shader].uniform_location[UNIFORM_MISC_RECTANGLE], 1,
            (GLfloat *)rect);
    if (update_mask & SRE_IMAGE_SET_COLORS) {
        glUniform4fv(misc_shader[shader].uniform_location[UNIFORM_MISC_MULT_COLOR], 1,
            (GLfloat *)&info->mult_color);
        glUniform4fv(misc_shader[shader].uniform_location[UNIFORM_MISC_ADD_COLOR], 1,
            (GLfloat *)&info->add_color);
    }
    if (update_mask & SRE_IMAGE_SET_TEXTURE_ARRAY_INDEX)
        glUniform1i(misc_shader[shader].uniform_location[UNIFORM_MISC_ARRAY_INDEX],
            info->array_index);
    if (update_mask & SRE_IMAGE_SET_TRANSFORM)
        glUniformMatrix3fv(misc_shader[shader].uniform_location[UNIFORM_MISC_UV_TRANSFORM], 1,
            GL_FALSE, (GLfloat *)&info->uv_transform);
    if (update_mask & SRE_IMAGE_SET_TEXTURE) {
        glActiveTexture(GL_TEXTURE0);
#ifndef OPENGL_ES2
        if (info->source_flags & SRE_IMAGE_SOURCE_FLAG_TEXTURE_ARRAY)
            glBindTexture(GL_TEXTURE_2D_ARRAY, info->opengl_id);
        else
#endif
            glBindTexture(GL_TEXTURE_2D, info->opengl_id);
    }
}

#define TEXT_ALLOW_UNALIGNED_INT_UNIFORM_ARRAY
#define TEXT_ALLOW_BYTE_ACCESS_BEYOND_STRING

void GL3InitializeTextShader(int update_mask, sreTextShaderInfo *info, Vector4D *rect,
const char *string, int length) {
#if !defined(TEXT_ALLOW_UNALIGNED_INT_UNIFORM_ARRAY) || !defined(TEXT_ALLOW_BYTE_ACCESS_BEYOND_STRING)
    unsigned int string_data[SRE_TEXT_MAX_TEXT_WIDTH / 4];
#endif
    int shader;
    if (info->font_format == SRE_FONT_FORMAT_32X8)
        shader = SRE_MISC_SHADER_TEXT_32X8;
    else
        shader = SRE_MISC_SHADER_TEXT_16X16;
    // When demand-loading is enabled, the shader will only be loaded at time of the
    // first draw request using the shader.
    misc_shader[shader].Validate();
    glUseProgram(misc_shader[shader].program);
    if (update_mask & SRE_IMAGE_SET_RECTANGLE)
        glUniform4fv(misc_shader[shader].uniform_location[UNIFORM_MISC_RECTANGLE], 1,
            (GLfloat *)rect);
    if (update_mask & SRE_TEXT_SET_STRING) {
        // Note: The size is limited to SRE_TEXT_MAX_TEXT_WIDTH.
        int size;
        int n;
        if (length > 0)
            n = length;
        else {
            n = 0;
            while (string[n] != '\0' && string[n] != '\n')
                n++;
        }
        // Size of the string in ints padded to int boundary.
        size = (n + 3) >> 2;
#if defined(TEXT_ALLOW_UNALIGNED_INT_UNIFORM_ARRAY) && defined(TEXT_ALLOW_BYTE_ACCESS_BEYOND_STRING)
        // When the length is specified, we can pass the raw pointer to glUniform. If it's not
        // specified, first determine the length. In both cases, memory access just beyond the
        // character string may happen (padded to the next 32-bit word) if the length is not
        // a multiple of four, and unaligned CPU memory access may happen. However, most modern
        // CPUs can cope with this automatically.
        //
        // This requires a shader change to work correctly on big-endian CPUs (which are very
        // rare on systems running OpenGL).
#ifdef OPENGL_ES2
        // Since glUniform with an unsigned type is not available in ES2, use the
        // the signed integer function (the results should be the same).
        glUniform1iv(misc_shader[shader].uniform_location[UNIFORM_MISC_STRING], size,
            (GLint *)string);
#else
        glUniform1uiv(misc_shader[shader].uniform_location[UNIFORM_MISC_STRING], size,
            (GLuint *)string);
#endif
#else

#if defined(TEXT_ALLOW_UNALIGNED_INT_UNIFORM_ARRAY)
        // Unaligned access allowed, but don't look beyond the string.
        if ((n & 3) == 0)
#else
#ifdef TEXT_ALLOW_BYTE_ACCESS_BEYOND_STRING
        // No unaligned access allowed, but byte access just beyond the string is allowed.
        if ((string & 3) == 0)
#else
        // No unaligned access allowed, and no byte access just beyond the string.
        if (((n & 3) | (string & 3)) == 0)
            // If the string is aligned on a word boundary, and is multiple of four characters
            // in length, passing the pointer direct should pose no problem in any circumstances.
#endif
#endif
            glUniform1uiv(misc_shader[shader].uniform_location[UNIFORM_MISC_STRING], size,
                (GLuint *)string);
        else {
            // In all other cases, we can use memcpy into our local buffer.
            memcpy(string_data, string, n);
            glUniform1uiv(misc_shader[shader].uniform_location[UNIFORM_MISC_STRING], (n + 3) >> 2,
                string_data);
        }
#endif
        // Usually, when a string is set there will no other parameters in the update mask apart
        // from the rectangle, which was already set. Check for an early exit.
        if ((update_mask & (SRE_IMAGE_SET_COLORS | SRE_IMAGE_SET_TEXTURE_ARRAY_INDEX |
        SRE_TEXT_SET_SCREEN_SIZE_IN_CHARS | SRE_IMAGE_SET_TEXTURE)) == 0)
            return;
    }
    if (update_mask & SRE_IMAGE_SET_COLORS) {
        glUniform4fv(misc_shader[shader].uniform_location[UNIFORM_MISC_MULT_COLOR], 1,
            (GLfloat *)&info->mult_color);
        glUniform4fv(misc_shader[shader].uniform_location[UNIFORM_MISC_ADD_COLOR], 1,
            (GLfloat *)&info->add_color);
    }
    if (update_mask & SRE_IMAGE_SET_TEXTURE_ARRAY_INDEX)
        glUniform1i(misc_shader[shader].uniform_location[UNIFORM_MISC_ARRAY_INDEX],
            info->array_index);
    if (update_mask & SRE_TEXT_SET_SCREEN_SIZE_IN_CHARS)
        glUniform2fv(misc_shader[shader].uniform_location[UNIFORM_MISC_SCREEN_SIZE_IN_CHARS], 1,
            (GLfloat *)&info->screen_size_in_chars);
    if (update_mask & SRE_IMAGE_SET_TEXTURE) {
#ifndef NO_SHADOW_MAP
        glActiveTexture(GL_TEXTURE0);
        if (info->source_flags & SRE_IMAGE_SOURCE_FLAG_TEXTURE_ARRAY)
            glBindTexture(GL_TEXTURE_2D_ARRAY, info->opengl_id);
        else
#endif
            glBindTexture(GL_TEXTURE_2D, info->opengl_id);
    }
}


// Initialization of shadow volume and shadow map generation shaders for each object.

void GL3InitializeShadowVolumeShader(const sreObject& so, const Vector4D& light_position_model_space) {
    glUseProgram(misc_shader[SRE_MISC_SHADER_SHADOW_VOLUME].program);
    GL3InitializeShaderWithMVP(
        misc_shader[SRE_MISC_SHADER_SHADOW_VOLUME].uniform_location[UNIFORM_MISC_MVP], so);
    GL3InitializeShaderWithLightPosition4ModelSpace(
        misc_shader[SRE_MISC_SHADER_SHADOW_VOLUME].uniform_location[UNIFORM_MISC_LIGHT_MODEL_SPACE],
        (float *)&light_position_model_space);
}

#ifndef NO_SHADOW_MAP

void GL3InitializeShadowMapShader(const sreObject& so) {
    if (so.render_flags & SRE_OBJECT_TRANSPARENT_TEXTURE) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_SHADOW_MAP_TRANSPARENT].program);
        GL3InitializeShadowMapShaderWithShadowMapMVP(
            misc_shader[SRE_MISC_SHADER_SHADOW_MAP_TRANSPARENT].uniform_location[UNIFORM_MISC_MVP], so);
        GL3InitializeShaderWithUVTransform(
            misc_shader[SRE_MISC_SHADER_SHADOW_MAP_TRANSPARENT].
            uniform_location[UNIFORM_MISC_UV_TRANSFORM], so);
        // When the texture is NULL, it is assumed that the object uses a mesh with different
        // textures for each sub-mesh, which will be bound later.
        if (so.texture != NULL) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, so.texture->opengl_id);
        }
    }
    else {
        glUseProgram(misc_shader[SRE_MISC_SHADER_SHADOW_MAP].program);
        GL3InitializeShadowMapShaderWithShadowMapMVP(
            misc_shader[SRE_MISC_SHADER_SHADOW_MAP].uniform_location[UNIFORM_MISC_MVP], so);
    }
}

void GL3InitializeCubeShadowMapShader(const sreObject& so) {
    if (so.render_flags & SRE_OBJECT_TRANSPARENT_TEXTURE) {
        glUseProgram(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].program);
        GL3InitializeShadowMapShaderWithShadowMapMVP(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT]
            .uniform_location[UNIFORM_MISC_MVP], so);
        GL3InitializeShaderWithModelMatrix(
            misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT]
            .uniform_location[UNIFORM_MISC_MODEL_MATRIX], so);
        GL3InitializeShaderWithUVTransform(
            misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].
            uniform_location[UNIFORM_MISC_UV_TRANSFORM], so);
        // When the texture is NULL, it is assumed that the object uses a mesh with different
        // textures for each sub-mesh, which will be bound later.
        if (so.texture != NULL) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, so.texture->opengl_id);
        }
    }
    else {
        glUseProgram(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP].program);
        GL3InitializeShadowMapShaderWithShadowMapMVP(misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP]
            .uniform_location[UNIFORM_MISC_MVP], so);
        GL3InitializeShaderWithModelMatrix(
            misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP].uniform_location[UNIFORM_MISC_MODEL_MATRIX], so);
    }
}

#endif

#ifndef NO_HDR

void GL3InitializeHDRLogLuminanceShader() {
    glUseProgram(misc_shader[SRE_MISC_SHADER_HDR_LOG_LUMINANCE].program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_color_texture);
}

void GL3InitializeHDRAverageLuminanceShader() {
    glUseProgram(misc_shader[SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE].program);
}

void GL3InitializeHDRAverageLuminanceShaderWithLogLuminanceTexture() {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_log_luminance_texture);
}

void GL3InitializeHDRAverageLuminanceShaderWithAverageLuminanceTexture(int i) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_average_luminance_texture[i]);
}

void GL3InitializeHDRLuminanceHistoryStorageShader() {
    glUseProgram(misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_average_luminance_texture[3]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_used_average_luminance_texture);
}

void GL3InitializeHDRLuminanceHistoryComparisonShader(int luminance_history_slot) {
    glUseProgram(misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].program);
    glUniform1i(misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].uniform_location[UNIFORM_MISC_LUMINANCE_HISTORY_SLOT],
        luminance_history_slot);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_luminance_history_texture);
}

void GL3InitializeHDRToneMapShader() {
    glUseProgram(HDR_tone_map_shader[sre_internal_HDR_tone_mapping_shader].program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_color_texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_used_average_luminance_texture);
//    glBindTexture(GL_TEXTURE_RECTANGLE, sre_internal_HDR_average_luminance_texture[3]); //DEBUG
}

#endif
