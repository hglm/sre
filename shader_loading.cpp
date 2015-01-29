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


// Generic vertex attribute names.
const char *attribute_str[5] = { "position_in", "texcoord_in", "normal_in", "tangent_in", "color_in" };

// Multi-pass lighting shader definition.

const char *uniform_str[MAX_UNIFORMS] = {
    "MVP", "model_matrix", "model_rotation_matrix", "diffuse_reflection_color_in",
    "use_multi_color_in", "use_texture_map_in", "shadow_map_dimensions_in", "ambient_color_in",
    "viewpoint_in",
#if 1
    "light_parameters_in", "", "",
#else
    "light_position_in",
    "light_att_in", "light_color_in",
#endif
    "specular_reflection_color_in", "specular_exponent_in",
    "texture_map_in",
    "use_normal_map_in", "normal_map_in", "use_specular_map_in", "specular_map_in",
    "emission_color_in",
    "use_emission_map_in", "emission_map_in", "diffuse_fraction_in", "roughness_in",
    "roughness_weights_in",
    "anisotropic_in", "shadow_map_transformation_matrix", "shadow_map_in", "cube_shadow_map_in",
    "segment_distance_scaling_in", "spotlight_in", "uv_transform_in" };

class ShaderInfo {
public :
    const char *name;
    unsigned int uniform_mask;
    int attribute_mask;
};

// Slighly shorter notation.
#define ATTRIBUTE_POSITION SRE_ATTRIBUTE_POSITION
#define ATTRIBUTE_TEXCOORDS SRE_ATTRIBUTE_TEXCOORDS
#define ATTRIBUTE_NORMAL SRE_ATTRIBUTE_NORMAL
#define ATTRIBUTE_TANGENT SRE_ATTRIBUTE_TANGENT
#define ATTRIBUTE_COLOR SRE_ATTRIBUTE_COLOR

static ShaderInfo multi_pass_shader_info[NU_MULTI_PASS_SHADERS] = {
    // SHADER0
    { "Complete multi-pass lighting shader", UNIFORM_MASK_COMMON ^ (
    (1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER)),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Ambient multi-pass lighting shader", (1 << UNIFORM_MVP) | (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) |
    (1 << UNIFORM_USE_MULTI_COLOR) | (1 << UNIFORM_USE_TEXTURE_MAP) | (1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_TEXTURE_MAP_SAMPLER) | ((unsigned int)1 << UNIFORM_UV_TRANSFORM) | (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_COLOR) },
    { "Plain multi-color object multi-pass lighting shader for local lights with class attenuation",
    (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) | (1 << UNIFORM_VIEWPOINT) |
    UNIFORM_LIGHT_PARAMETERS_MASK,
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_COLOR) },
    { "Plain texture mapped object multi-pass lighting shader for local lights "
    "with classic attenuation", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_VIEWPOINT) |
    UNIFORM_LIGHT_PARAMETERS_MASK | (1 << UNIFORM_TEXTURE_MAP_SAMPLER) | ((unsigned int)1 << UNIFORM_UV_TRANSFORM),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) },
    // SHADER4
    { "Complete multi-pass lighting shader for directional lights", UNIFORM_MASK_COMMON ^ (
    (1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER)),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Plain texture mapped object multi-pass lighting shader for directional lights", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_VIEWPOINT) |
    (UNIFORM_LIGHT_PARAMETERS_MASK) |
    (1 << UNIFORM_TEXTURE_MAP_SAMPLER) | ((unsigned int)1 << UNIFORM_UV_TRANSFORM),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) },
    // SHADER6
    { "Complete multi-pass lighting shader for point source lights", UNIFORM_MASK_COMMON ^
    ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER)),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    // SHADER7
    { "Multi-pass lighting shader for point lights with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER))),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    // SHADER8
    { "Multi-pass lighting shader for spot lights",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER))),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    // SHADER9
    { "Plain Phong-shaded object multi-pass lighting shader for point lights with a linear attenuation range",
    (1 << UNIFORM_MVP) | (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_VIEWPOINT) | UNIFORM_LIGHT_PARAMETERS_MASK ,
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL) },
    { "Complete microfacet multi-pass lighting shader for directional lights",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER) | (1 << UNIFORM_SPECULAR_EXPONENT))) | (1 << UNIFORM_DIFFUSE_FRACTION) |
    (1 << UNIFORM_ROUGHNESS) | (1 << UNIFORM_ROUGHNESS_WEIGHTS) | (1 << UNIFORM_ANISOTROPIC),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Complete microfacet multi-pass lighting shader for point/spot/beam lights with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER) | (1 << UNIFORM_SPECULAR_EXPONENT))) |
    (1 << UNIFORM_DIFFUSE_FRACTION) |
    (1 << UNIFORM_ROUGHNESS) | (1 << UNIFORM_ROUGHNESS_WEIGHTS) | (1 << UNIFORM_ANISOTROPIC),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
#ifndef NO_SHADOW_MAP
    { "Complete shadow map multi-pass lighting shader for directional lights", (UNIFORM_MASK_COMMON ^ (
    (1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER))) | (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) |
    (1 << UNIFORM_SHADOW_MAP_SAMPLER) | (1 << UNIFORM_SHADOW_MAP_DIMENSIONS),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    /// SHADER13
    { "Complete shadow map multi-pass lighting shader for point source light with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER))) |
    (1 << UNIFORM_CUBE_SHADOW_MAP_SAMPLER) | (1 << UNIFORM_SEGMENT_DISTANCE_SCALING),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Complete microfacet shadow map multi-pass lighting shader for directional lights",
    (UNIFORM_MASK_COMMON ^ (
    (1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER) | (1 << UNIFORM_SPECULAR_EXPONENT))) |
    (1 << UNIFORM_DIFFUSE_FRACTION) |
    (1 << UNIFORM_ROUGHNESS) | (1 << UNIFORM_ROUGHNESS_WEIGHTS) | (1 << UNIFORM_ANISOTROPIC) |
    (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) | (1 << UNIFORM_SHADOW_MAP_SAMPLER) |
    (1 << UNIFORM_SHADOW_MAP_DIMENSIONS),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Complete microfacet shadow map multi-pass lighting shader for point source light with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER) |
    (1 << UNIFORM_SPECULAR_EXPONENT))) |
    (1 << UNIFORM_DIFFUSE_FRACTION) |
    (1 << UNIFORM_ROUGHNESS) | (1 << UNIFORM_ROUGHNESS_WEIGHTS) | (1 << UNIFORM_ANISOTROPIC) |
    (1 << UNIFORM_CUBE_SHADOW_MAP_SAMPLER) | (1 << UNIFORM_SEGMENT_DISTANCE_SCALING),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    // SHADER16
    { "Complete shadow map multi-pass lighting shader for spot light with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER))) |
    (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) | (1 << UNIFORM_SHADOW_MAP_SAMPLER) |
    /* (1 << UNIFORM_SHADOW_MAP_DIMENSIONS) | */ (1 << UNIFORM_SEGMENT_DISTANCE_SCALING),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Complete microfacet shadow map multi-pass lighting shader for spot light with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_SPECULAR_EXPONENT) |
    (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER))) |
    (1 << UNIFORM_DIFFUSE_FRACTION) |
    (1 << UNIFORM_ROUGHNESS) | (1 << UNIFORM_ROUGHNESS_WEIGHTS) | (1 << UNIFORM_ANISOTROPIC) |
    (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) | (1 << UNIFORM_SHADOW_MAP_SAMPLER) |
    /* (1 << UNIFORM_SHADOW_MAP_DIMENSIONS) | */ (1 << UNIFORM_SEGMENT_DISTANCE_SCALING),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Complete shadow map multi-pass lighting shader for beam light with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER))) |
    (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) | (1 << UNIFORM_SHADOW_MAP_SAMPLER) |
    (1 << UNIFORM_SHADOW_MAP_DIMENSIONS),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Complete microfacet shadow map multi-pass lighting shader for beam light with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_SPECULAR_EXPONENT) |
    (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER))) |
    (1 << UNIFORM_DIFFUSE_FRACTION) |
    (1 << UNIFORM_ROUGHNESS) | (1 << UNIFORM_ROUGHNESS_WEIGHTS) | (1 << UNIFORM_ANISOTROPIC) |
    (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) | (1 << UNIFORM_SHADOW_MAP_SAMPLER) |
    (1 << UNIFORM_SHADOW_MAP_DIMENSIONS),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Earth shadow map multi-pass lighting shader for directional light", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_VIEWPOINT) |
    (1 << UNIFORM_LIGHT_PARAMETERS) | (1 << UNIFORM_SPECULAR_REFLECTION_COLOR) |
    (1 << UNIFORM_SPECULAR_EXPONENT) | (1 << UNIFORM_TEXTURE_MAP_SAMPLER) |
    (1 << UNIFORM_SPECULARITY_MAP_SAMPLER) | (1 << UNIFORM_EMISSION_MAP_SAMPLER) |
    (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) | (1 << UNIFORM_SHADOW_MAP_SAMPLER) |
    (1 << UNIFORM_SHADOW_MAP_DIMENSIONS),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) },
#endif
    { "Earth multi-pass lighting shader for directional light", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) |
    (1 << UNIFORM_MODEL_ROTATION_MATRIX) | (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) |
    (1 << UNIFORM_VIEWPOINT) | (1 << UNIFORM_LIGHT_PARAMETERS) |
    (1 << UNIFORM_SPECULAR_REFLECTION_COLOR) |
    (1 << UNIFORM_SPECULAR_EXPONENT) | (1 << UNIFORM_TEXTURE_MAP_SAMPLER) |
    (1 << UNIFORM_SPECULARITY_MAP_SAMPLER) | (1 << UNIFORM_EMISSION_MAP_SAMPLER),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) }
};

const char *multi_pass_shader_prologue[NU_MULTI_PASS_SHADERS] = {
    // Complete versatile lighting pass shader for local lights with support for all
    // options except emission color and map (obsolete)
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define GENERAL_LOCAL_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n",
    // Complete ambient pass shader.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define COLOR_IN\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define EMISSION_MAP_OPTION\n"
    "#define AMBIENT_COLOR_IN\n"
    "#define EMISSION_COLOR_IN\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define NO_SMOOTH_SHADING\n"
    "#define TEXTURE_ALPHA\n",
    // Lighting pass shader for plain multi-color objects for local lights.
    "#define NORMAL_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define MULTI_COLOR_FIXED\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define GENERAL_LOCAL_LIGHT\n",
    // Lighting pass shader for plain textured objects for local lights.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define TEXTURE_MAP_FIXED\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define GENERAL_LOCAL_LIGHT\n",
    // Complete versatile lighting pass shader for directional lights.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define DIRECTIONAL_LIGHT\n",
    // Plain texture mapped object lighting pass shader for directional lights.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define TEXTURE_MAP_FIXED\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define DIRECTIONAL_LIGHT\n",
    // Complete lighting pass shader with support for all options except emission color and map,
    // for local lights.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define GENERAL_LOCAL_LIGHT\n",
    // SHADER7
    // Lighting pass shader with support for all options except emission color and map, for
    // point light sources with a linear attenuation range.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define POINT_SOURCE_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n",
    // SHADER8
    // Multi-pass lighting shader for spot lights
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define SPOT_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n",
    // SHADER9
    // Lighting pass shader for plain phong-shaded objects for point source light with a
    // linear attenuation range.
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define POINT_SOURCE_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n",
    // Complete microfacet lighting pass shader for directional lights
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define MICROFACET\n"
    "#define DIRECTIONAL_LIGHT\n",
    // Complete microfacet lighting pass shader for local light sources with a linear attenuation range.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define GENERAL_LOCAL_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n"
    "#define MICROFACET\n",
#ifndef NO_SHADOW_MAP
    // Complete shadow map lighting pass shader with support for all options except emission color and map,
    // for directional lights.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define DIRECTIONAL_LIGHT\n"
    "#define SHADOW_MAP\n",
    // Complete shadow map lighting pass shader for point source light with a linear attenuation range
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define POINT_SOURCE_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n"  
    "#define SHADOW_CUBE_MAP\n",
    // Complete microfacet shadow map lighting pass shader for directional lights
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define DIRECTIONAL_LIGHT\n"
    "#define MICROFACET\n"
    "#define SHADOW_MAP\n",
    // Complete microfacet shadow map lighting pass shader for point source light with a linear attenuation range.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define POINT_SOURCE_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n"  
    "#define MICROFACET\n"
    "#define SHADOW_CUBE_MAP\n",
    // Complete shadow map lighting pass shader for spot lights with a linear attenuation range
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define SPOT_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n" 
//    "#define SHADOW_MAP\n"
    "#define SPOT_LIGHT_SHADOW_MAP\n",
    // Complete microfacet shadow map lighting pass shader for spot lights with a linear attenuation range
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define SPOT_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n"
    "#define MICROFACET\n" 
//    "#define SHADOW_MAP\n"
    "#define SPOT_LIGHT_SHADOW_MAP\n",
    // Complete shadow map multi-pass lighting shader for beam light with a linear attenuation range
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define BEAM_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n" 
    "#define SHADOW_MAP\n",
    // Complete microfacet shadow map multi-pass lighting shader for beam light
    // with a linear attenuation range.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define BEAM_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n"
    "#define MICROFACET\n" 
    "#define SHADOW_MAP\n",
    // Earth shadow map lighting pass shader for directional light
    "#define TEXCOORD_IN\n"
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define TEXTURE_MAP_FIXED\n"
    "#define SPECULARITY_MAP_FIXED\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define DIRECTIONAL_LIGHT\n"
    "#define SHADOW_MAP\n"
    "#define EARTH_SHADER\n",
#endif
    // Earth lighting pass shader for directional light
    "#define TEXCOORD_IN\n"
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define TEXTURE_MAP_FIXED\n"
    "#define SPECULARITY_MAP_FIXED\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define DIRECTIONAL_LIGHT\n"
    "#define EARTH_SHADER\n"
};

// Single pass lighting shader definition.

static ShaderInfo single_pass_shader_info[NU_SINGLE_PASS_SHADERS] = {
    { "Complete single pass shader for local lights with a linear attenuation range",
    UNIFORM_MASK_COMMON,
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Complete single pass shader for directional light", UNIFORM_MASK_COMMON,
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Single-pass phong-only shader for directional light", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_USE_MULTI_COLOR) |
    (1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_VIEWPOINT) | (UNIFORM_LIGHT_PARAMETERS_MASK) |
    (1 << UNIFORM_EMISSION_COLOR),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_COLOR) },
    { "Single-pass (final pass) constant shader", (1 << UNIFORM_MVP) | (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER) |
    ((unsigned int)1 << UNIFORM_UV_TRANSFORM),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) },
    { "Single-pass phong texture-only shader for directional light", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_VIEWPOINT) | (UNIFORM_LIGHT_PARAMETERS_MASK) |
    (1 << UNIFORM_TEXTURE_MAP_SAMPLER) | ((unsigned int)1 << UNIFORM_UV_TRANSFORM) | (1 << UNIFORM_EMISSION_COLOR),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) },
    { "Single-pass phong texture plus normal map-only shader for directional light", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_VIEWPOINT) | (UNIFORM_LIGHT_PARAMETERS_MASK) |
    (1 << UNIFORM_TEXTURE_MAP_SAMPLER) | ((unsigned int)1 << UNIFORM_UV_TRANSFORM) | (1 << UNIFORM_NORMAL_MAP_SAMPLER) |
    (1 << UNIFORM_EMISSION_COLOR),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) },
    { "Complete single pass shader for local lights (point, beam, spot) with a linear "
    "attenuation range",
    UNIFORM_MASK_COMMON,
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Single-pass constant shader with multi-color support",
    (1 << UNIFORM_MVP) | (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_MULTI_COLOR),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_COLOR) },
};

const char *single_pass_shader_prologue[NU_SINGLE_PASS_SHADERS] = {
    // Complete versatile single pass shader for local lights with support for all options.
    "#define SINGLE_PASS\n"
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define AMBIENT_COLOR_IN\n"
    "#define EMISSION_COLOR_IN\n"
    "#define EMISSION_MAP_OPTION\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define GENERAL_LOCAL_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n",
    // Complete versatile single pass shader for directional lights with support for all options.
    "#define SINGLE_PASS\n"
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define AMBIENT_COLOR_IN\n"
    "#define EMISSION_COLOR_IN\n"
    "#define EMISSION_MAP_OPTION\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define DIRECTIONAL_LIGHT\n",
    // Phong shading-only single pass shader for directional lights (no support for any maps).
    "#define SINGLE_PASS\n"
    "#define NORMAL_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define AMBIENT_COLOR_IN\n"
    "#define EMISSION_COLOR_IN\n"
    "#define DIRECTIONAL_LIGHT\n",
    // Constant shading only single pass shader with support for emission color and maps only.
    // Used for the final pass in multi-pass rendering.
    "#define SINGLE_PASS\n"
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define TEXCOORD_VAR\n"
    "#define EMISSION_COLOR_IN\n"
    "#define EMISSION_MAP_OPTION\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define NO_SMOOTH_SHADING\n"
    "#define EMISSION_MAP_ALPHA\n",
    // Phong texture map single pass shader for directional light.
    "#define SINGLE_PASS\n"
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define TEXTURE_MAP_FIXED\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define AMBIENT_COLOR_IN\n"
    "#define EMISSION_COLOR_IN\n"
    "#define DIRECTIONAL_LIGHT\n",
    // Single-pass phong texture plus normal map-only shader for directional light.
    "#define SINGLE_PASS\n"
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define TEXTURE_MAP_FIXED\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_FIXED\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define AMBIENT_COLOR_IN\n"
    "#define EMISSION_COLOR_IN\n"
    "#define DIRECTIONAL_LIGHT\n",
    // Complete single pass shader for local lights (point, beam, spot) with a linear attenuation range.
    "#define SINGLE_PASS\n"
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define TANGENT_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_MAP_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_MAP_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define AMBIENT_COLOR_IN\n"
    "#define EMISSION_COLOR_IN\n"
    "#define EMISSION_MAP_OPTION\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define GENERAL_LOCAL_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n",
    // Constant shading-only single pass shader (no lighting or texture)
    // supporting multi-color and emission color. Diffuse reflection color
    // is added to emission color.
    "#define SINGLE_PASS\n"
    "#define COLOR_IN\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define EMISSION_COLOR_IN\n"
    "#define NO_SMOOTH_SHADING\n"
    "#define ADD_DIFFUSE_TO_EMISSION\n"
};

static char *newstrcat(const char *s1, const char *s2) {
    char *s = new char[strlen(s1) + strlen(s2) + 1];
    strcpy(s, s1);
    strcat(s, s2);
    return s;
}

static void AddPrologueDefinition(const char *definition, char* &prologue) {
        char *old_prologue = prologue;
        char *new_prologue = newstrcat(prologue, definition);
        delete [] old_prologue;
        prologue = new_prologue;
}

enum {
    SHADER_CREATION_FLAG_MULTI_PASS = 0x1,
    SHADER_CREATION_FLAG_NO_SMOOTH_SHADING = 0x2,
    SHADER_CREATION_FLAG_AMBIENT_AND_EMISSION_COLOR = 0x4,
    SHADER_CREATION_FLAG_TEXTURE_MAP_FIXED = 0x8,
    SHADER_CREATION_FLAG_TEXTURE_MAP_OPTION = 0x10,
    SHADER_CREATION_FLAG_NORMAL_MAP_FIXED = 0x20,
    SHADER_CREATION_FLAG_NORMAL_MAP_OPTION = 0x40,
    SHADER_CREATION_FLAG_SPECULARITY_MAP_FIXED = 0x80,
    SHADER_CREATION_FLAG_SPECULARITY_MAP_OPTION = 0x100,
    SHADER_CREATION_FLAG_EMISSION_MAP_FIXED = 0x200,
    SHADER_CREATION_FLAG_EMISSION_MAP_OPTION = 0x400,
    SHADER_CREATION_MASK_ANY_TEXTURE = 0x7F8,
    SHADER_CREATION_FLAG_MULTI_COLOR_FIXED = 0x1000,
    SHADER_CREATION_FLAG_MULTI_COLOR_OPTION = 0x2000,
    SHADER_CREATION_FLAG_MICROFACET = 0x4000,
    SHADER_CREATION_FLAG_TEXTURE_MAP_ALPHA = 0x8000,
    SHADER_CREATION_FLAG_EMISSION_MAP_ALPHA = 0x10000,
    SHADER_CREATION_FLAG_NO_SPECULAR = 0x20000
};

static const char *light_type_definition_str[4] = {
   "#define DIRECTIONAL_LIGHT\n",
   "#define POINT_SOURCE_LIGHT\n",
   "#define SPOT_LIGHT\n",
   "#define BEAM_LIGHT\n"
};

// Create based on flags and light type, storing the shader code prologue text (newly allocated)
// and the uniform mask;

static void CreateShaderDefinitions(int flags, int light_type_index, char* &prologue,
unsigned int &uniform_mask, int &attribute_mask) {
    prologue = new char[1];
    prologue[0] = '\0';
    uniform_mask |= (1 << UNIFORM_MVP);
    attribute_mask |= SRE_POSITION_MASK;

    if (!(flags & SHADER_CREATION_FLAG_MULTI_PASS))
        AddPrologueDefinition("#define SINGLE_PASS\n", prologue);
    AddPrologueDefinition(light_type_definition_str[light_type_index], prologue);
    if (light_type_index != SRE_LIGHT_TYPE_DIRECTIONAL) {
        AddPrologueDefinition("#define LINEAR_ATTENUATION_RANGE\n", prologue);
    }
    if (flags & SHADER_CREATION_FLAG_NO_SMOOTH_SHADING)
        AddPrologueDefinition("#define NO_SMOOTH_SHADING\n", prologue);
    else {
        AddPrologueDefinition("#define LIGHT_PARAMETERS\n", prologue);
        AddPrologueDefinition("#define NORMAL_IN\n", prologue);
        AddPrologueDefinition("#define NORMAL_VAR\n", prologue);
        AddPrologueDefinition("#define POSITION_WORLD_VAR\n", prologue);
        AddPrologueDefinition("#define VIEWPOINT_IN\n", prologue);
        uniform_mask |= (1 << UNIFORM_LIGHT_PARAMETERS) |
            (1 <<UNIFORM_VIEWPOINT) | (1 << UNIFORM_MODEL_MATRIX) |
            (1 << UNIFORM_MODEL_ROTATION_MATRIX);
        uniform_mask |= (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR);
        if (!(flags & SHADER_CREATION_FLAG_NO_SPECULAR)) {
            AddPrologueDefinition("#define NO_SPECULAR\n", prologue);
            uniform_mask |= (1 << UNIFORM_SPECULAR_REFLECTION_COLOR) |
                (1 << UNIFORM_SPECULAR_EXPONENT);
        }
        attribute_mask |= SRE_NORMAL_MASK;
   }

    if (flags & SHADER_CREATION_FLAG_TEXTURE_MAP_FIXED)
        AddPrologueDefinition("#define TEXTURE_MAP_FIXED\n", prologue);
    if (flags & SHADER_CREATION_FLAG_TEXTURE_MAP_OPTION) {
        AddPrologueDefinition("#define TEXTURE_MAP_OPTION\n", prologue);
        uniform_mask |= 1 << UNIFORM_USE_TEXTURE_MAP;
    }
    if (flags & (SHADER_CREATION_FLAG_TEXTURE_MAP_FIXED |
    SHADER_CREATION_FLAG_TEXTURE_MAP_OPTION))
        AddPrologueDefinition("#define TEXTURE_MAP_SAMPLER\n", prologue);
    if (flags & SHADER_CREATION_FLAG_TEXTURE_MAP_ALPHA)
        AddPrologueDefinition("#define TEXTURE_MAP_ALPHA\n", prologue);

    if (flags & SHADER_CREATION_FLAG_NORMAL_MAP_FIXED)
        AddPrologueDefinition("#define NORMAL_MAP_FIXED\n", prologue);
    if (flags & SHADER_CREATION_FLAG_NORMAL_MAP_OPTION) {
        AddPrologueDefinition("#define NORMAL_MAP_OPTION\n", prologue);
        uniform_mask |= 1 << UNIFORM_USE_NORMAL_MAP;
    }
    if (flags & (SHADER_CREATION_FLAG_NORMAL_MAP_FIXED | SHADER_CREATION_FLAG_NORMAL_MAP_OPTION)) {
        AddPrologueDefinition("#define NORMAL_MAP_SAMPLER\n", prologue);
        AddPrologueDefinition("#define TBN_MATRIX_VAR\n", prologue);
    }
    if (flags & SHADER_CREATION_FLAG_SPECULARITY_MAP_FIXED)
        AddPrologueDefinition("#define SPECULARITY_MAP_FIXED\n", prologue);
    if (flags & SHADER_CREATION_FLAG_SPECULARITY_MAP_OPTION)
        AddPrologueDefinition("#define SPECULARITY_MAP_OPTION\n", prologue);
    if (flags & (SHADER_CREATION_FLAG_SPECULARITY_MAP_FIXED |
    SHADER_CREATION_FLAG_SPECULARITY_MAP_OPTION)) {
        AddPrologueDefinition("#define SPECULARITY_MAP_SAMPLER\n", prologue);
        uniform_mask |= 1 << UNIFORM_USE_SPECULARITY_MAP;
    }

    if (flags & SHADER_CREATION_MASK_ANY_TEXTURE) {
        AddPrologueDefinition("#define TEXCOORD_IN\n", prologue);
        AddPrologueDefinition("#define TEXCOORD_VAR\n", prologue);
        AddPrologueDefinition("#define UV_TRANSFORM\n", prologue);
        uniform_mask |= 1 << UNIFORM_UV_TRANSFORM;
        attribute_mask |= SRE_TEXCOORDS_MASK;
    }

    if (flags & SHADER_CREATION_FLAG_AMBIENT_AND_EMISSION_COLOR) {
        AddPrologueDefinition("#define AMBIENT_COLOR_IN\n"
            "#define EMISSION_COLOR_IN\n", prologue);
        uniform_mask |= 1 << UNIFORM_AMBIENT_COLOR;
    }
    if (flags & SHADER_CREATION_FLAG_EMISSION_MAP_FIXED)
        AddPrologueDefinition("#define EMISSION_MAP_FIXED\n", prologue);
    if (flags & SHADER_CREATION_FLAG_EMISSION_MAP_OPTION) {
        AddPrologueDefinition("#define EMISSION_MAP_OPTION\n", prologue);
        uniform_mask |= 1 << UNIFORM_USE_EMISSION_MAP;
    }
    if (flags & (SHADER_CREATION_FLAG_EMISSION_MAP_FIXED |
    SHADER_CREATION_FLAG_EMISSION_MAP_OPTION))
        AddPrologueDefinition("#define EMISSION_MAP_SAMPLER\n", prologue);
    if (flags & SHADER_CREATION_FLAG_EMISSION_MAP_ALPHA)
        AddPrologueDefinition("#define EMISSION_MAP_ALPHA\n", prologue);

    if (flags & SHADER_CREATION_FLAG_MULTI_COLOR_FIXED)
        AddPrologueDefinition("#define MULTI_COLOR_FIXED\n", prologue);
    if (flags & SHADER_CREATION_FLAG_MULTI_COLOR_OPTION) {
        AddPrologueDefinition("#define MULTI_COLOR_OPTION\n", prologue);
        uniform_mask |= 1 << UNIFORM_USE_MULTI_COLOR;
    }
    if (flags & (SHADER_CREATION_FLAG_MULTI_COLOR_FIXED |
    SHADER_CREATION_FLAG_MULTI_COLOR_OPTION)) {
        AddPrologueDefinition("#define COLOR_IN\n", prologue);
        attribute_mask |= SRE_COLOR_MASK;
    }

    if ((flags & (SHADER_CREATION_FLAG_NORMAL_MAP_FIXED | SHADER_CREATION_FLAG_NORMAL_MAP_OPTION))
    || (flags & SHADER_CREATION_FLAG_MICROFACET)) {
        AddPrologueDefinition("#define TANGENT_IN\n", prologue);
        attribute_mask |= SRE_TANGENT_MASK;
    }
}

static void printShaderInfoLog(GLuint obj) {
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);
    if (infologLength > 0) {
        infoLog = (char *)malloc(infologLength);
        glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
	sreMessage(SRE_MESSAGE_ERROR, "%s",infoLog);
        free(infoLog);
    }
}


// Shader loading.

static void printProgramInfoLog(GLuint obj) {
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    glGetProgramiv(obj, GL_INFO_LOG_LENGTH,&infologLength);
    if (infologLength > 0) {
        infoLog = (char *)malloc(infologLength);
        glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
	sreMessage(SRE_MESSAGE_ERROR, "%s",infoLog);
        free(infoLog);
    }
}

/* A simple function that will read a file into an allocated char pointer buffer */
static char* filetobuf(const char *file) {
    FILE *fptr;
    long length;
    char *buf;
    fptr = fopen(file, "rb"); /* Open file for reading */
    if (!fptr) /* Return NULL on failure */
        return NULL;
    fseek(fptr, 0, SEEK_END); /* Seek to the end of the file */
    length = ftell(fptr);
    buf = (char *)new char[length + 1];
    fseek(fptr, 0, SEEK_SET); /* Go back to the beginning of the file */
    fread_with_check(buf, length, 1, fptr); /* Read the contents of the file in to the buffer */
    fclose(fptr);
    buf[length] = 0; /* Null terminator */
    return buf;
}

sreShader::sreShader() {
    status = SRE_SHADER_STATUS_UNINITIALIZED;
}

#define SHADER_DIRECTORY_DEFAULT_PATH 0
#define SHADER_DIRECTORY_BUILTIN      1
#define SHADER_DIRECTORY_END_MARKER   - 1

// Search order for shaders. First look in the default path, then look for built-in shaders.

static int shader_directory_table[] = {
    SHADER_DIRECTORY_DEFAULT_PATH,
    SHADER_DIRECTORY_BUILTIN,
    SHADER_DIRECTORY_END_MARKER
};

void sreShader::Initialize(const char *_name, int _type, int _uniform_mask, int _attribute_mask,
const char * _vfilename, const char *_ffilename, const char *_prologue) {
    if (status != SRE_SHADER_STATUS_UNINITIALIZED) {
        sreFatalError("Error (sreShader::Initialize()) -- shader not uninitialized.\n");
        exit(1);
    }
    name = _name;
    type = _type;
    uniform_mask = _uniform_mask;
    attribute_mask = _attribute_mask;
    vfilename = _vfilename;
    ffilename = _ffilename;
    prologue = _prologue;
    status = SRE_SHADER_STATUS_INITIALIZED;
    // Demand loading only enabled for lighting shaders for now.
//    if (!sre_internal_demand_load_shaders || !(type & SRE_SHADER_MASK_LIGHTING))
    // Demand loading enabled for most shaders.
    if (!sre_internal_demand_load_shaders)
        Load();
}

// Old style function that requires some fields to be already initialized.
void sreShader::Initialize(const char *vertex_shader_filename, const char *fragment_shader_filename,
const char *pr_str) {
    if (status != SRE_SHADER_STATUS_UNINITIALIZED) {
        sreFatalError("Error (InitializeShader()) -- shader not uninitialized.\n");
    }
    vfilename = vertex_shader_filename;
    ffilename = fragment_shader_filename;
    prologue = pr_str;
    status = SRE_SHADER_STATUS_INITIALIZED;
    // Demand loading only enabled for lighting shaders for now.
//    if (!sre_internal_demand_load_shaders || !(type & SRE_SHADER_MASK_LIGHTING))
    // Demand loading enabled for most shaders.
    if (!sre_internal_demand_load_shaders)
        Load();
}

void sreShader::Load() {
    if (status == SRE_SHADER_STATUS_UNINITIALIZED) {
        sreFatalError(
            "Error -- sreShader::Initialize() should be called before LoadShader().\n");
    }

    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    if (v == 0) {
        sreFatalError("Error allocating vertex shader %s\n", name);
    }
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);	
    if (f == 0) {
        sreFatalError("Error allocating fragment shader %s\n", name);
    }

    bool builtin = false;
    char *vertexsource = NULL;
    char *fragmentsource = NULL;
    for (int sdir = 0;; sdir++) {
        if (shader_directory_table[sdir] == SHADER_DIRECTORY_END_MARKER) {
            sreFatalError("Error - shader file not found (%s, %s).\n", 
                vfilename, ffilename);
         }
#ifdef SHADERS_BUILTIN
        if (shader_directory_table[sdir] == SHADER_DIRECTORY_BUILTIN) {
            for (int i = 0; i < sre_nu_builtin_shader_sources; i++) {
                if (strcmp(sre_builtin_shader_table[i].shader_filename, vfilename) == 0)
                   vertexsource = (char *)sre_builtin_shader_table[i].shader_source;
                if (strcmp(sre_builtin_shader_table[i].shader_filename, ffilename) == 0)
                    fragmentsource = (char *)sre_builtin_shader_table[i].shader_source;
                if (vertexsource != NULL && fragmentsource != NULL) {
                   builtin = true;
                   break;
                }
            }
           if (builtin)
               break;
        }
#endif

        if (shader_directory_table[sdir] == SHADER_DIRECTORY_DEFAULT_PATH) {
            bool found = false;
            // Prepend the shader path name.
            char *vertex_path = new char[strlen(sre_internal_shader_path) + strlen(vfilename) + 1];
            strcpy(vertex_path, sre_internal_shader_path);
            strcat(vertex_path, vfilename);
            char *fragment_path = new char[strlen(sre_internal_shader_path) + strlen(ffilename) + 1];
            strcpy(fragment_path, sre_internal_shader_path);
            strcat(fragment_path, ffilename);

            vertexsource = filetobuf(vertex_path);
            if (vertexsource != NULL) {
                fragmentsource = filetobuf(fragment_path);
                if (fragmentsource == NULL) {
                    free(vertexsource);
                    sreMessage(SRE_MESSAGE_WARNING,
                        "Fragment shader missing (%s).", fragment_path);
                }
                else
                    found = true;
            }
            delete [] vertex_path;
            delete [] fragment_path;
            if (found)
                break;
        }
    }
    const char *origin_str;
    if (builtin)
        origin_str = "(built-in)";
    else
        origin_str = "(default path)";
    sreMessage(SRE_MESSAGE_INFO, "Loading shader %s %s", name, origin_str);

    char *vertex_source_str[1];
    vertex_source_str[0] = new char[strlen(prologue) + strlen(vertexsource) + 1];
    char *fragment_source_str[1];
    fragment_source_str[0] = new char[strlen(prologue) + strlen(fragmentsource) + 1];
    strcpy(vertex_source_str[0], prologue);
    strcat(vertex_source_str[0], vertexsource);
    strcpy(fragment_source_str[0], prologue);
    strcat(fragment_source_str[0], fragmentsource);
    if (!builtin) {
        delete [] vertexsource;
        delete [] fragmentsource;
    }

    glShaderSource(v, 1, (const char **)&vertex_source_str[0], NULL);
    glShaderSource(f, 1, (const char **)&fragment_source_str[0], NULL);
    glCompileShader(v);
    glCompileShader(f);

    program = glCreateProgram();
    if (f == 0) {
        sreFatalError("Error creating shader program.\n");
    }

//    glUseProgram(0);

    glAttachShader(program, v);
    glAttachShader(program ,f);

    BindAttributes();

    glLinkProgram(program);
    GLint params;
    glGetProgramiv(program, GL_LINK_STATUS, &params);
    if (params == GL_FALSE) {
        sreMessage(SRE_MESSAGE_ERROR,
            "Shader program link unsuccesful (%s, %s).\n",  vfilename, ffilename);
	sreMessage(SRE_MESSAGE_ERROR, "Vertex shader code:\n%s", vertex_source_str[0]);
	sreMessage(SRE_MESSAGE_ERROR, "Fragment shader code:\n%s", fragment_source_str[0]);
        sreMessage(SRE_MESSAGE_ERROR, "Vertex shader log:");
        printShaderInfoLog(v);
        sreMessage(SRE_MESSAGE_ERROR, "Fragment shader log:\n");
        printShaderInfoLog(f);
        sreMessage(SRE_MESSAGE_ERROR, "Shader program log:\n");
        printProgramInfoLog(program);
        sreFatalError("Loading of shader failed:\n%s", name);
    }

    delete [] vertex_source_str[0];
    delete [] fragment_source_str[0];

    // Also bind uniform locations.
    if (type & (SRE_SHADER_MASK_LIGHTING_SINGLE_PASS | SRE_SHADER_MASK_LIGHTING_MULTI_PASS))
        InitializeUniformLocationsLightingShader();
    else
        InitializeUniformLocationsMiscShader();

    status = SRE_SHADER_STATUS_LOADED;

    SetDefaultUniformValues();
}

void sreShader::BindAttributes() {
    for (int i = 0; i < SRE_NU_VERTEX_ATTRIBUTES; i++) {
        if (attribute_mask & (1 << i))
             glBindAttribLocation(program, i, attribute_str[i]);

        if (sre_internal_debug_message_level >= 2 && glGetError() != GL_NO_ERROR) {
            sreFatalError("OpenGL error occurred after BindAttribs for shader %s, attribute %d.\n",
                name, i);
            exit(1);
        }
    }
    if (glGetError() != GL_NO_ERROR) {
        sreFatalError("OpenGL error occurred after BindAttribs.\n");
        exit(1);
    }
}

void sreShader::InitializeUniformLocationsLightingShader() {
    for (int j = 0; j < MAX_UNIFORMS; j++)
        if (uniform_mask & (1 << j)) {
            uniform_location[j] = glGetUniformLocation(program, uniform_str[j]);
            if (uniform_location[j] == - 1) {
                sreFatalError("Error getting uniform location for '%s' from shader '%s'.\n", uniform_str[j], name);
                exit(1);
            }
        }
}

// Initialization of uniforms that only need to be initialized once.
void sreShader::SetDefaultUniformValues() {
    // Fortunately, the default values that a uniform needs to be initialized
    // with are generally the same for different shaders given the same uniform id.
    // We only have to seperate lighting and miscellaneous shaders.
    int n;
    if (type & (SRE_SHADER_MASK_LIGHTING_SINGLE_PASS | SRE_SHADER_MASK_LIGHTING_MULTI_PASS))
        n = MAX_UNIFORMS;
    else
        n = MAX_MISC_UNIFORMS;
    glUseProgram(program);
    for (int i = 0; i < n; i++)
        // Only uniforms that need only one-time initialization will actually be set.    
        if (uniform_mask & (1 << i)) {
            if (type & (SRE_SHADER_MASK_LIGHTING_SINGLE_PASS |
            SRE_SHADER_MASK_LIGHTING_MULTI_PASS))
                sreInitializeLightingShaderUniformWithDefaultValue(i, uniform_location[i]);
            else
                // Misc shader.
               sreInitializeMiscShaderUniformWithDefaultValue(i, uniform_location[i]);
        }
    glUseProgram(0);
}

// Array of uniform identifiers for miscellaneous shaders.

static const char *uniform_misc_str[MAX_MISC_UNIFORMS] = {
    "MVP", "light_pos_model_space_in", "view_projection_matrix",
    "base_color_in", "aspect_ratio_in", "halo_size_in", "texture_in", "light_position_in",
    "model_matrix", "segment_distance_scaling_in", "average_lum_in", "slot_in",
    "key_value_in", "array_in", "rectangle_in", "uv_transform_in", "mult_color_in",
    "add_color_in", "screen_size_in_chars_in", "string_in", "use_emission_map_in",
    "shadow_map_dimensions_in"
};

class MiscShaderInfo {
public :
    const char *name;
    int type;
    int uniform_mask;
    int attribute_mask;
    const char *vsource;
    const char *fsource;
    const char *prologue;
};

static const MiscShaderInfo misc_shader_info[] = {
    {
    "Text shader (16x16 font texture)",
    SRE_SHADER_MASK_TEXT,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_RECTANGLE) |
    (1 << UNIFORM_MISC_MULT_COLOR) | (1 << UNIFORM_MISC_ADD_COLOR) |
    (1 << UNIFORM_MISC_SCREEN_SIZE_IN_CHARS) | (1 << UNIFORM_MISC_STRING),
    (1 << ATTRIBUTE_POSITION),
    "gl3_image.vert", "gl3_text2.frag",
    "#define TEXT_SHADER\n#define ONE_COMPONENT\n"
#ifdef OPENGL_ES2
    "#define MAX_TEXT_LENGTH 128\n"
    "#define FONT_TEXTURE_COLUMNS 16\n#define FONT_TEXTURE_ROWS 16\n"
#ifdef GLES2_GLSL_NO_ARRAY_INDEXING
    "#define NO_ARRAY_INDEXING\n"
#endif
#ifdef GLES2_GLSL_LIMITED_UNIFORM_INT_PRECISION
    "#define LIMITED_UNIFORM_INT_PRECISION\n"
#endif
#ifdef FLOATING_POINT_TEXT_STRING
    "#define FLOATING_POINT_TEXT_STRING\n"
#endif
#else
    "#define MAX_TEXT_LENGTH 256\n"
    "#define FONT_TEXTURE_COLUMNS 16u\n#define FONT_TEXTURE_ROWS 16u\n"
#endif
    },
    {
    "Text shader (32x8 font texture)",
    SRE_SHADER_MASK_TEXT,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_RECTANGLE) |
    (1 << UNIFORM_MISC_MULT_COLOR) | (1 << UNIFORM_MISC_ADD_COLOR) |
    (1 << UNIFORM_MISC_SCREEN_SIZE_IN_CHARS) | (1 << UNIFORM_MISC_STRING),
    (1 << ATTRIBUTE_POSITION),
    "gl3_image.vert", "gl3_text2.frag",
    "#define TEXT_SHADER\n#define ONE_COMPONENT\n"
#ifdef OPENGL_ES2
    "#define MAX_TEXT_LENGTH 128\n"
    "#define FONT_TEXTURE_COLUMNS 32\n#define FONT_TEXTURE_ROWS 8\n"
#ifdef GLES2_GLSL_NO_ARRAY_INDEXING
    "#define NO_ARRAY_INDEXING\n"
#endif
#ifdef GLES2_GLSL_LIMITED_UNIFORM_INT_PRECISION
    "#define LIMITED_UNIFORM_INT_PRECISION\n"
#endif
#ifdef FLOATING_POINT_TEXT_STRING
    "#define FLOATING_POINT_TEXT_STRING\n"
#endif
#else
    "#define MAX_TEXT_LENGTH 256\n"
    "#define FONT_TEXTURE_COLUMNS 32u\n#define FONT_TEXTURE_ROWS 8u\n"
#endif
    },
    {
    "2D texture image shader",
    SRE_SHADER_MASK_IMAGE,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) |
    (1 << UNIFORM_MISC_RECTANGLE) | (1 << UNIFORM_MISC_UV_TRANSFORM) |
    (1 << UNIFORM_MISC_MULT_COLOR) | (1 << UNIFORM_MISC_ADD_COLOR),
    (1 << ATTRIBUTE_POSITION),
    "gl3_image.vert", "gl3_image.frag", "#define UV_TRANSFORM\n"
    },
    {
    "2D texture image shader (one component)",
    SRE_SHADER_MASK_IMAGE,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER)|
    (1 << UNIFORM_MISC_RECTANGLE) | (1 << UNIFORM_MISC_UV_TRANSFORM) |
    (1 << UNIFORM_MISC_MULT_COLOR) | (1 << UNIFORM_MISC_ADD_COLOR),
    (1 << ATTRIBUTE_POSITION),
    "gl3_image.vert", "gl3_image.frag", "#define UV_TRANSFORM\n#define ONE_COMPONENT\n"
    },
#ifndef OPENGL_ES2
    {
    "2D texture array image shader",
    SRE_SHADER_MASK_IMAGE,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) |
    (1 << UNIFORM_MISC_ARRAY_INDEX) |
    (1 << UNIFORM_MISC_RECTANGLE) | (1 << UNIFORM_MISC_UV_TRANSFORM) |
    (1 << UNIFORM_MISC_MULT_COLOR) | (1 << UNIFORM_MISC_ADD_COLOR),
    (1 << ATTRIBUTE_POSITION),
    "gl3_image.vert", "gl3_image.frag", "#define UV_TRANSFORM\n#define TEXTURE_ARRAY\n"
    },
    {
    "2D texture array image shader (one component)",
    SRE_SHADER_MASK_IMAGE,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) |
    (1 << UNIFORM_MISC_ARRAY_INDEX) |
    (1 << UNIFORM_MISC_RECTANGLE) | (1 << UNIFORM_MISC_UV_TRANSFORM) |
    (1 << UNIFORM_MISC_MULT_COLOR) | (1 << UNIFORM_MISC_ADD_COLOR),
    (1 << ATTRIBUTE_POSITION),
    "gl3_image.vert", "gl3_image.frag",
        "#define UV_TRANSFORM\n#define TEXTURE_ARRAY\n#define ONE_COMPONENT\n"
    },
#endif
    {
    "Shadow volume shader",
    SRE_SHADER_MASK_SHADOW_VOLUME,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_LIGHT_MODEL_SPACE),
    (1 << ATTRIBUTE_POSITION),
    "gl3_shadow_volume.vert", "gl3_shadow_volume.frag", ""
    },
#ifndef NO_SHADOW_MAP
    {
    "Shadow map shader",
    SRE_SHADER_MASK_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP),
    (1 << ATTRIBUTE_POSITION),
    "gl3_shadow_map.vert", "gl3_shadow_map.frag", ""
    },
    {
    "Shadow map shader (non-closed object)",
    SRE_SHADER_MASK_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_SHADOW_MAP_DIMENSIONS),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL),
    "gl3_shadow_map.vert", "gl3_shadow_map.frag", "#define ADD_BIAS\n"
    },
    {
    "Shadow map shader for transparent textures",
    SRE_SHADER_MASK_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_TEXTURE_SAMPLER) |
    (1 << UNIFORM_MISC_UV_TRANSFORM),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS),
    "gl3_shadow_map.vert", "gl3_shadow_map.frag", "#define TEXTURE_ALPHA\n#define UV_TRANSFORM\n"
    },
    // Spotlight now use similar shadow map format to a point light cube map side.
    {
    "Shadow map shader (spotlights)",
    SRE_SHADER_MASK_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_LIGHT_POSITION) |
    (1 << UNIFORM_MISC_MODEL_MATRIX) | (1 << UNIFORM_MISC_SEGMENT_DISTANCE_SCALING),
    (1 << ATTRIBUTE_POSITION),
    "gl3_shadow_map.vert", "gl3_shadow_map.frag",  "#define SPOTLIGHT\n"
    },
#if 0
    {
    "Shadow map shader (spotlights) (non-closed objects)",
    SRE_SHADER_MASK_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_SHADOW_MAP_DIMENSIONS),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL),
    "gl3_shadow_map.vert", "gl3_shadow_map.frag",  "#define SPOTLIGHT\n#define ADD_BIAS\n"
    },
#endif
    {
    "Shadow map shader for transparent textures (spotlights)",
    SRE_SHADER_MASK_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_LIGHT_POSITION) |
    (1 << UNIFORM_MISC_MODEL_MATRIX) | (1 << UNIFORM_MISC_SEGMENT_DISTANCE_SCALING) |
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_UV_TRANSFORM),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS),
    "gl3_shadow_map.vert", "gl3_shadow_map.frag",
    "#define SPOTLIGHT\n#define TEXTURE_ALPHA\n#define UV_TRANSFORM\n"
    },
    {
    "Shadow cube-map shader",
    SRE_SHADER_MASK_CUBE_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_LIGHT_POSITION) |
    (1 << UNIFORM_MISC_MODEL_MATRIX) | (1 << UNIFORM_MISC_SEGMENT_DISTANCE_SCALING),
    (1 << ATTRIBUTE_POSITION),
     "gl3_shadow_map.vert", "gl3_shadow_map.frag", "#define CUBE_MAP\n"   
    },
    {
    "Shadow cube-map shader for transparent textures",
    SRE_SHADER_MASK_CUBE_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_UV_TRANSFORM) |
    (1 << UNIFORM_MISC_LIGHT_POSITION) | (1 << UNIFORM_MISC_MODEL_MATRIX) |
    (1 << UNIFORM_MISC_SEGMENT_DISTANCE_SCALING),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS),
     "gl3_shadow_map.vert", "gl3_shadow_map.frag",
     "#define CUBE_MAP\n#define TEXTURE_ALPHA\n#define UV_TRANSFORM\n"   
    },
#endif
    {
    "Halo shader (single and particle system)",
    SRE_SHADER_MASK_EFFECTS,
    (1 << UNIFORM_MISC_VIEW_PROJECTION_MATRIX) |
    (1 << UNIFORM_MISC_BASE_COLOR) | (1 << UNIFORM_MISC_ASPECT_RATIO) | (1 << UNIFORM_MISC_HALO_SIZE),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL),
    "gl3_billboard.vert", "gl3_halo.frag", "#define HALO\n"
    },
    {
    "Billboard shader (single and particle system)",
    SRE_SHADER_MASK_EFFECTS,
    (1 << UNIFORM_MISC_VIEW_PROJECTION_MATRIX) |
    (1 << UNIFORM_MISC_BASE_COLOR) | (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_USE_EMISSION_MAP) |
    (1 << UNIFORM_MISC_UV_TRANSFORM),
    (1 << ATTRIBUTE_POSITION) | (1 < ATTRIBUTE_TEXCOORDS),
    "gl3_billboard.vert", "gl3_billboard.frag", ""
    },
#ifndef NO_HDR
    {
    "HDR log luminance shader",
    SRE_SHADER_MASK_HDR,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER),
    (1 << ATTRIBUTE_POSITION),
    "gl3_HDR_log_lum.vert", "gl3_HDR_log_lum.frag", ""
    },
    {
    "HDR average log luminance shader",
    SRE_SHADER_MASK_HDR,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER),
    (1 << ATTRIBUTE_POSITION),
    "gl3_HDR_average_lum.vert", "gl3_HDR_average_lum.frag", "",
    },
    {
    "HDR luminance history storage shader",
    SRE_SHADER_MASK_HDR,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_AVERAGE_LUM_SAMPLER),
    (1 << ATTRIBUTE_POSITION),
    "gl3_HDR_lum_history_storage.vert", "gl3_HDR_lum_history_storage.frag", "",
    },
    {
    "HDR luminance history shader",
    SRE_SHADER_MASK_HDR,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_LUMINANCE_HISTORY_SLOT),
    (1 << ATTRIBUTE_POSITION),
    "gl3_HDR_lum_history_comparison.vert", "gl3_HDR_lum_history_comparison.frag", ""
    },
#endif
};

void sreShader::InitializeUniformLocationsMiscShaderNew() {
    int count = 0;
    for (int j = 0; j < MAX_MISC_UNIFORMS; j++)
        if (uniform_mask & (1 << j)) {
            uniform_location[count] = glGetUniformLocation(program, uniform_misc_str[j]);
            if (uniform_location[count] == - 1) {
                sreFatalError("Error getting uniform location for '%s' from shader '%s'.\n",
                    uniform_misc_str[j], name);
                exit(1);
            }
            count++;
        }
}

void sreShader::InitializeUniformLocationsMiscShader() {
    for (int j = 0; j < MAX_MISC_UNIFORMS; j++)
        if (uniform_mask & (1 << j)) {
            uniform_location[j] = glGetUniformLocation(program, uniform_misc_str[j]);
            if (uniform_location[j] == - 1) {
                sreFatalError("Error getting uniform location for '%s' from shader '%s'.\n", uniform_misc_str[j], name);
                exit(1);
            }
        }
}

// Actual shader initialization.

sreShader multi_pass_shader[NU_MULTI_PASS_SHADERS];
sreShader single_pass_shader[NU_SINGLE_PASS_SHADERS];
sreShader misc_shader[SRE_NU_MISC_SHADERS];
sreShader HDR_tone_map_shader[SRE_NUMBER_OF_TONE_MAPPING_SHADERS];

static void sreInitializeMiscShaders(int mask) {
    for (int i = 0; i < SRE_NU_MISC_SHADERS; i++)
        if (misc_shader_info[i].type & mask)
            misc_shader[i].Initialize(
                misc_shader_info[i].name,
                misc_shader_info[i].type,
                misc_shader_info[i].uniform_mask,
                misc_shader_info[i].attribute_mask,
                misc_shader_info[i].vsource,
                misc_shader_info[i].fsource,
                misc_shader_info[i].prologue);
}

static void sreInitializeTextShader() {
    sreInitializeMiscShaders(SRE_SHADER_MASK_TEXT);
#if 0
    misc_shader[SRE_MISC_SHADER_TEXT1].Initialize(
        "Text shader",
        SRE_SHADER_MASK_TEXT,                                   // Type
        (1 << UNIFORM_MISC_TEXTURE_SAMPLER) |                   // Uniform mask
        (1 << UNIFORM_MISC_BASE_COLOR),
        (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS), // Attribute mask
        "gl3_text.vert", "gl3_text.frag", "");                  // Filenames, prologue
#endif
}

static void sreInitializeImageShader() {
    sreInitializeMiscShaders(SRE_SHADER_MASK_IMAGE);
}

static void sreInitializeShadowVolumeShaders() {
    sreInitializeMiscShaders(SRE_SHADER_MASK_SHADOW_VOLUME);
#if 0
    misc_shader[SRE_MISC_SHADER_SHADOW_VOLUME].Initialize(
        "Shadow volume shader",
        SRE_SHADER_MASK_SHADOW_VOLUME,
        (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_LIGHT_MODEL_SPACE),
        (1 << ATTRIBUTE_POSITION),
        "gl3_shadow_volume.vert", "gl3_shadow_volume.frag", "");
#endif
}

static void sreInitializeShadowMapShaders() {
#ifndef NO_SHADOW_MAP
    sreInitializeMiscShaders(SRE_SHADER_MASK_SHADOW_MAP);
#endif
}

static void sreInitializeCubeShadowMapShaders() {
#ifndef NO_SHADOW_MAP
    sreInitializeMiscShaders(SRE_SHADER_MASK_CUBE_SHADOW_MAP);
#endif
}

static void sreInitializeEffectsShaders() {
    sreInitializeMiscShaders(SRE_SHADER_MASK_EFFECTS);
}

static const char *tone_map_prologue[] = {
     "#define TONE_MAP_LINEAR\n", "#define TONE_MAP_REINHARD\n",
     "#define TONE_MAP_EXPONENTIAL\n"
};

static void sreInitializeHDRShaders() {
#ifndef NO_HDR
    sreInitializeMiscShaders(SRE_SHADER_MASK_HDR);
    for (int i = 0; i < SRE_NUMBER_OF_TONE_MAPPING_SHADERS; i++) {
        char name[64];
        sprintf(name, "HDR %s tone mapping shader", sreGetToneMappingShaderName(i));
        HDR_tone_map_shader[i].Initialize(
            strdup(name),
            SRE_SHADER_MASK_HDR,
            (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_AVERAGE_LUM_SAMPLER) |
            (1 << UNIFORM_MISC_KEY_VALUE),
            (1 << ATTRIBUTE_POSITION),
            "gl3_HDR_tone.vert", "gl3_HDR_tone.frag", tone_map_prologue[i]);
    }
#endif 
}

static void AddDirectionalLightSpillOverDefinition(char* &prologue) {
#ifdef ENABLE_DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR
        const char *spill_over_definition = "#define ENABLE_DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR\n";
        AddPrologueDefinition(spill_over_definition, prologue);
#endif
}

static const char *light_parameter_definitions =
    "#define NU_LIGHT_PARAMETERS_DIRECTIONAL 6\n"
    "#define NU_LIGHT_PARAMETERS_POINT 7\n"
    "#define NU_LIGHT_PARAMETERS_SPOT 11\n"
    "#define NU_LIGHT_PARAMETERS_BEAM 13\n"
    "#define NU_LIGHT_PARAMETERS_LOCAL 15\n"
    "#define NU_LIGHT_PARAMETERS_MAX 16\n"
    "#define LIGHT_POSITION_X 0\n"
    "#define LIGHT_POSITION_Y 1\n"
    "#define LIGHT_POSITION_Z 2\n"
    "#define LIGHT_COLOR_R 3\n"
    "#define LIGHT_COLOR_G 4\n"
    "#define LIGHT_COLOR_B 5\n"
    "#define LIGHT_LINEAR_ATTENUATION_RANGE 6\n"
    "#define LIGHT_AXIS_DIRECTION_X 7\n"
    "#define LIGHT_AXIS_DIRECTION_Y 8\n"
    "#define LIGHT_AXIS_DIRECTION_Z 9\n"
    "#define DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR 6\n"
    "#define SPOT_LIGHT_EXPONENT 10\n"
    "#define BEAM_LIGHT_AXIS_CUT_OFF_DISTANCE 10\n"
    "#define BEAM_LIGHT_RADIUS 11\n"
    "#define BEAM_LIGHT_RADIAL_LINEAR_ATTENUATION_RANGE 12\n"
    "#define LOCAL_LIGHT_TYPE 10\n"
    "#define LOCAL_LIGHT_SPOT_EXPONENT 11\n"
    "#define LOCAL_LIGHT_BEAM_AXIS_CUT_OFF_DISTANCE 12\n"
    "#define LOCAL_LIGHT_BEAM_RADIUS 13\n"
    "#define LOCAL_LIGHT_BEAM_RADIAL_LINEAR_ATTENUATION_RANGE 14\n";

static void AddLightParameterDefinitions(char *&prologue) {
    AddPrologueDefinition(light_parameter_definitions, prologue);
}

static void sreInitializeMultiPassLightingShaders() {
    // New style shader loading for lighting shaders.
    for (int i = 0; i < NU_MULTI_PASS_SHADERS; i++) {
        // Do not load shadow map or cube shadow map lighting shaders when
        // the shadow map feature is not supported.
        if (i == 13 || i == 15) {
            if (!(sre_internal_rendering_flags & SRE_RENDERING_FLAG_CUBE_SHADOW_MAP_SUPPORT))
                continue;
        }
        else if (i >= 12 && i <= 18 &&
        !(sre_internal_rendering_flags & SRE_RENDERING_FLAG_SHADOW_MAP_SUPPORT))
            continue;
        char *prologue = new char[strlen(multi_pass_shader_prologue[i]) + 1];
        strcpy(prologue, multi_pass_shader_prologue[i]);
#ifdef COMPRESS_COLOR_ATTRIBUTE
        const char *color_attribute_definition = "#define COMPRESS_COLOR_ATTRIBUTE\n";
        AddPrologueDefinition(color_attribute_definition, prologue);
#endif
#if defined(OPENGL_ES2) && defined(USE_REFLECTION_VECTOR_GLES2)
        const char *reflection_vector_definition = "#define USE_REFLECTION_VECTOR_GLES2\n";
        AddPrologueDefinition(reflection_vector_definition, prologue);
#endif
        AddDirectionalLightSpillOverDefinition(prologue);
        AddLightParameterDefinitions(prologue);
//	sreMessage(SRE_MESSAGE_INFO, "%s", prologue);
        multi_pass_shader[i].Initialize(
            multi_pass_shader_info[i].name,
            SRE_SHADER_MASK_LIGHTING_MULTI_PASS,
            multi_pass_shader_info[i].uniform_mask,
            multi_pass_shader_info[i].attribute_mask,
            "gl3_lighting_pass.vert", "gl3_lighting_pass.frag",
            prologue);
        delete [] prologue;
    }
}

static void sreInitializeSinglePassLightingShaders() {
    for (int i = 0; i < NU_SINGLE_PASS_SHADERS; i++) {
        char *prologue = new char[strlen(single_pass_shader_prologue[i]) + 1];
        strcpy(prologue, single_pass_shader_prologue[i]);
#ifdef COMPRESS_COLOR_ATTRIBUTE
        const char *color_attribute_definition = "#define COMPRESS_COLOR_ATTRIBUTE\n";
        AddPrologueDefinition(color_attribute_definition, prologue);
#endif
#if defined(OPENGL_ES2) && defined(USE_REFLECTION_VECTOR_GLES2)
        const char *reflection_vector_definition = "#define USE_REFLECTION_VECTOR_GLES2\n";
        AddPrologueDefinition(reflection_vector_definition, prologue);
#endif
        AddDirectionalLightSpillOverDefinition(prologue);
        AddLightParameterDefinitions(prologue);
        single_pass_shader[i].Initialize(
            single_pass_shader_info[i].name,
            SRE_SHADER_MASK_LIGHTING_SINGLE_PASS,
            single_pass_shader_info[i].uniform_mask,
            single_pass_shader_info[i].attribute_mask,
            "gl3_lighting_pass.vert", "gl3_lighting_pass.frag",
            prologue);
        delete [] prologue;
    }
}

// This function is called by sreInitialize(). Depending on the demand-loading
// setting, most shaders may not actually be loaded yet.

void sreInitializeShaders(int shader_mask) {
    if (shader_mask & SRE_SHADER_MASK_TEXT)
        sreInitializeTextShader();
    if (shader_mask & SRE_SHADER_MASK_IMAGE)
        sreInitializeImageShader();
    if (shader_mask & SRE_SHADER_MASK_SHADOW_VOLUME)
        sreInitializeShadowVolumeShaders();
    if (shader_mask & SRE_SHADER_MASK_SHADOW_MAP)
        sreInitializeShadowMapShaders();
    if (shader_mask & SRE_SHADER_MASK_CUBE_SHADOW_MAP)
        sreInitializeCubeShadowMapShaders();
    if (shader_mask & SRE_SHADER_MASK_EFFECTS)
        sreInitializeEffectsShaders();
    if (shader_mask & SRE_SHADER_MASK_HDR)
        sreInitializeHDRShaders();
    if (shader_mask & SRE_SHADER_MASK_LIGHTING_SINGLE_PASS)
        sreInitializeSinglePassLightingShaders();
    if (shader_mask & SRE_SHADER_MASK_LIGHTING_MULTI_PASS)
        sreInitializeMultiPassLightingShaders();
}

// Functions to validate shaders (make sure they are loaded) when shadow
// or HDR settings are changed with demand-loading shaders.

void sreValidateShadowVolumeShaders() {
   misc_shader[SRE_MISC_SHADER_SHADOW_VOLUME].Validate();
}

#ifndef NO_SHADOW_MAP

void sreValidateShadowMapShaders() {
   misc_shader[SRE_MISC_SHADER_SHADOW_MAP].Validate();
   misc_shader[SRE_MISC_SHADER_SHADOW_MAP_NON_CLOSED_OBJECT].Validate();
   misc_shader[SRE_MISC_SHADER_SHADOW_MAP_TRANSPARENT].Validate();
}

void sreValidateSpotlightShadowMapShaders() {
   misc_shader[SRE_MISC_SHADER_SPOTLIGHT_SHADOW_MAP].Validate();
//   misc_shader[SRE_MISC_SHADER_PROJECTION_SHADOW_MAP_NON_CLOSED_OBJECT].Validate();
   misc_shader[SRE_MISC_SHADER_SPOTLIGHT_SHADOW_MAP_TRANSPARENT].Validate();
}

void sreValidateCubeShadowMapShaders() {
    misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP].Validate();
    misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].Validate();
}

#endif


#ifndef NO_HDR

void sreValidateHDRShaders() {
    misc_shader[SRE_MISC_SHADER_HDR_LOG_LUMINANCE].Validate();
    misc_shader[SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE].Validate();
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].Validate();
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].Validate();
    HDR_tone_map_shader[sre_internal_HDR_tone_mapping_shader].Validate();
}

#endif
