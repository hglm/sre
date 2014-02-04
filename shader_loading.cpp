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

const char *uniform_str[MAX_UNIFORMS] = {  "MVP", "model_matrix", "model_rotation_matrix", "diffuse_reflection_color_in",
    "multi_color_in", "use_texture_in", "current_light_in", "ambient_color_in", "viewpoint_in", "light_position_in",
    "light_att_in", "light_color_in", "specular_reflection_color_in", "specular_exponent_in", "texture_in",
    "use_normal_map_in", "normal_map_in", "use_specular_map_in", "specular_map_in", "emission_color_in",
    "use_emission_map_in", "emission_map_in", "diffuse_fraction_in", "roughness_in", "roughness_weights_in",
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

static ShaderInfo lighting_pass_shader_info[NU_LIGHTING_PASS_SHADERS] = {
    { "Complete multi-pass lighting shader", UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_CURRENT_LIGHT) |
    (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER)),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Ambient multi-pass lighting shader", (1 << UNIFORM_MVP) | (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) |
    (1 << UNIFORM_MULTI_COLOR) | (1 << UNIFORM_USE_TEXTURE) | (1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_TEXTURE_SAMPLER) | ((unsigned int)1 << UNIFORM_UV_TRANSFORM) | (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_COLOR) },
    { "Plain multi-color object multi-pass lighting shader", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) | (1 << UNIFORM_VIEWPOINT) |
    UNIFORM_LIGHT_PARAMETERS_MASK,
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_COLOR) },
    { "Plain texture mapped object multi-pass lighting shader", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_VIEWPOINT) |
    UNIFORM_LIGHT_PARAMETERS_MASK | (1 << UNIFORM_TEXTURE_SAMPLER) | ((unsigned int)1 << UNIFORM_UV_TRANSFORM),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) },
    { "Complete multi-pass lighting shader for directional lights", UNIFORM_MASK_COMMON ^ (
    (1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_CURRENT_LIGHT) | (1 << UNIFORM_LIGHT_ATT) |
    (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER)),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Plain texture mapped object multi-pass lighting shader for directional lights", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_VIEWPOINT) |
    (UNIFORM_LIGHT_PARAMETERS_MASK ^ (1 << UNIFORM_LIGHT_ATT)) |
    (1 << UNIFORM_TEXTURE_SAMPLER) | ((unsigned int)1 << UNIFORM_UV_TRANSFORM),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) },
    { "Complete multi-pass lighting shader for point source lights", UNIFORM_MASK_COMMON ^
    ((1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_CURRENT_LIGHT) |
    (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER)),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Complete multi-pass lighting shader for point/spot/beam lights with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_CURRENT_LIGHT) |
    (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER))) |
    (1 << UNIFORM_SPOTLIGHT),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Plain Phong-shaded object multi-pass lighting shader for point lights", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_VIEWPOINT) | UNIFORM_LIGHT_PARAMETERS_MASK,
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL) },
    { "Plain Phong-shaded object multi-pass lighting shader for point/spot/beam lights with a linear attenuation range",
    (1 << UNIFORM_MVP) | (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_VIEWPOINT) | UNIFORM_LIGHT_PARAMETERS_MASK |
    (1 << UNIFORM_SPOTLIGHT),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL) },
    { "Complete microfacet multi-pass lighting shader", (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_CURRENT_LIGHT) | (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER) | (1 << UNIFORM_SPECULAR_EXPONENT))) | (1 << UNIFORM_DIFFUSE_FRACTION) |
    (1 << UNIFORM_ROUGHNESS) | (1 << UNIFORM_ROUGHNESS_WEIGHTS) | (1 << UNIFORM_ANISOTROPIC),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Complete microfacet multi-pass lighting shader for point/spot/beam lights with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_CURRENT_LIGHT) | (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER) | (1 << UNIFORM_SPECULAR_EXPONENT))) | (1 << UNIFORM_SPOTLIGHT) |
    (1 << UNIFORM_DIFFUSE_FRACTION) |
    (1 << UNIFORM_ROUGHNESS) | (1 << UNIFORM_ROUGHNESS_WEIGHTS) | (1 << UNIFORM_ANISOTROPIC),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Complete shadow map multi-pass lighting shader for directional lights", (UNIFORM_MASK_COMMON ^ (
    (1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_LIGHT_ATT) |
    (1 << UNIFORM_CURRENT_LIGHT) | (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER))) | (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) |
    (1 << UNIFORM_SHADOW_MAP_SAMPLER),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Complete shadow map multi-pass lighting shader for point lights with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_CURRENT_LIGHT) | (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER))) | (1 << UNIFORM_SPOTLIGHT) |
    (1 << UNIFORM_CUBE_SHADOW_MAP_SAMPLER) | (1 << UNIFORM_SEGMENT_DISTANCE_SCALING),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Complete microfacet shadow map multi-pass lighting shader for directional lights",
    (UNIFORM_MASK_COMMON ^ (
    (1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_LIGHT_ATT) |
    (1 << UNIFORM_CURRENT_LIGHT) | (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER) | (1 << UNIFORM_SPECULAR_EXPONENT))) | (1 << UNIFORM_DIFFUSE_FRACTION) |
    (1 << UNIFORM_ROUGHNESS) | (1 << UNIFORM_ROUGHNESS_WEIGHTS) | (1 << UNIFORM_ANISOTROPIC) |
    (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) | (1 << UNIFORM_SHADOW_MAP_SAMPLER),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Complete microfacet shadow map multi-pass lighting shader for point light with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_CURRENT_LIGHT) | (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER) | (1 << UNIFORM_SPECULAR_EXPONENT))) | (1 << UNIFORM_SPOTLIGHT) |
    (1 << UNIFORM_DIFFUSE_FRACTION) |
    (1 << UNIFORM_ROUGHNESS) | (1 << UNIFORM_ROUGHNESS_WEIGHTS) | (1 << UNIFORM_ANISOTROPIC) |
    (1 << UNIFORM_CUBE_SHADOW_MAP_SAMPLER) | (1 << UNIFORM_SEGMENT_DISTANCE_SCALING),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Complete shadow map multi-pass lighting shader for spot or beam light with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_CURRENT_LIGHT) | (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER))) | (1 << UNIFORM_SPOTLIGHT) |
    (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) | (1 << UNIFORM_SHADOW_MAP_SAMPLER),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Complete microfacet shadow map multi-pass lighting shader for spot or beam light with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ ((1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_SPECULAR_EXPONENT) |
    (1 << UNIFORM_CURRENT_LIGHT) | (1 << UNIFORM_EMISSION_COLOR) | (1 << UNIFORM_USE_EMISSION_MAP) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER))) |
    (1 << UNIFORM_SPOTLIGHT) | (1 << UNIFORM_DIFFUSE_FRACTION) |
    (1 << UNIFORM_ROUGHNESS) | (1 << UNIFORM_ROUGHNESS_WEIGHTS) | (1 << UNIFORM_ANISOTROPIC)  |
    (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) | (1 << UNIFORM_SHADOW_MAP_SAMPLER),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Earth shadow map multi-pass lighting shader for directional light", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_VIEWPOINT) | (1 << UNIFORM_LIGHT_POSITION) |
    (1 << UNIFORM_LIGHT_COLOR) | (1 << UNIFORM_SPECULAR_REFLECTION_COLOR) |
    (1 << UNIFORM_SPECULAR_EXPONENT) | (1 << UNIFORM_TEXTURE_SAMPLER) |
    (1 << UNIFORM_SPECULARITY_MAP_SAMPLER) | (1 << UNIFORM_EMISSION_MAP_SAMPLER) |
    (1 << UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX) | (1 << UNIFORM_SHADOW_MAP_SAMPLER),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) },
    { "Earth multi-pass lighting shader for directional light", (1 << UNIFORM_MVP) | (1 << UNIFORM_MODEL_MATRIX) |
    (1 << UNIFORM_MODEL_ROTATION_MATRIX) | (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_VIEWPOINT) |
    (1 << UNIFORM_LIGHT_POSITION) | (1 << UNIFORM_LIGHT_COLOR) | (1 << UNIFORM_SPECULAR_REFLECTION_COLOR) |
    (1 << UNIFORM_SPECULAR_EXPONENT) | (1 << UNIFORM_TEXTURE_SAMPLER) | (1 << UNIFORM_SPECULARITY_MAP_SAMPLER) |
    (1 << UNIFORM_EMISSION_MAP_SAMPLER),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) }
};

const char *lighting_pass_shader_prologue[NU_LIGHTING_PASS_SHADERS] = {
    // Complete versatile lighting pass shader with support for all options except emission color and map.
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n",
    // Complete ambient pass shader.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define COLOR_IN\n"
    "#define TEXCOORD_VAR\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define TEXTURE_OPTION\n"
    "#define EMISSION_MAP_OPTION\n"
    "#define AMBIENT_COLOR_IN\n"
    "#define EMISSION_COLOR_IN\n"
    "#define TEXTURE_SAMPLER\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define NO_SMOOTH_SHADING\n"
    "#define TEXTURE_ALPHA\n",
    // Lighting pass shader for plain multi-color objects for point source lights.
    "#define NORMAL_IN\n"
    "#define COLOR_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define MULTI_COLOR_FIXED\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define POINT_SOURCE_LIGHT\n",
    // Lighting pass shader for plain textured objects for point source lights.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define TEXTURE_FIXED\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define POINT_SOURCE_LIGHT\n",
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define DIRECTIONAL_LIGHT\n",
    // Plain texture mapped object lighting pass shader for directional lights.
    "#define TEXCOORD_IN\n"
    "#define UV_TRANSFORM\n"
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define TEXTURE_FIXED\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define DIRECTIONAL_LIGHT\n",
    // Complete lighting pass shader with support for all options except emission color and map,
    // for point source lights.
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define POINT_SOURCE_LIGHT\n",
    // Complete lighting pass shader with support for all options except emission color and map, for
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define POINT_SOURCE_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n",
    // Lighting pass shader for plain phong-shaded objects for point source lights.
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define POINT_SOURCE_LIGHT\n",
    // Lighting pass shader for plain phong-shaded objects for point source lights with a
    // linear attenuation range.
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define POINT_SOURCE_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n",
    // Complete microfacet lighting pass shader
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define MICROFACET\n",
    // Complete microfacet lighting pass shader for point light sources with a linear attenuation range.
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define POINT_SOURCE_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n"
    "#define MICROFACET\n",
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define DIRECTIONAL_LIGHT\n"
    "#define SHADOW_MAP\n",
    // Complete shadow map lighting pass shader for point light with a linear attenuation range
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define DIRECTIONAL_LIGHT\n"
    "#define MICROFACET\n"
    "#define SHADOW_MAP\n",
    // Complete microfacet shadow map lighting pass shader for point lights with a linear attenuation range.
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define POINT_SOURCE_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n"  
    "#define SHADOW_MAP\n"
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define POINT_SOURCE_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n"
    "#define MICROFACET\n" 
    "#define SHADOW_MAP\n"
    "#define SPOT_LIGHT_SHADOW_MAP\n",
    // Earth shadow map lighting pass shader for directional light
    "#define TEXCOORD_IN\n"
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define TEXTURE_FIXED\n"
    "#define SPECULARITY_MAP_FIXED\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define DIRECTIONAL_LIGHT\n"
    "#define SHADOW_MAP\n"
    "#define EARTH_SHADER\n",
    // Earth lighting pass shader for directional light
    "#define TEXCOORD_IN\n"
    "#define NORMAL_IN\n"
    "#define POSITION_WORLD_VAR\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define TEXTURE_FIXED\n"
    "#define SPECULARITY_MAP_FIXED\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define DIRECTIONAL_LIGHT\n"
    "#define EARTH_SHADER\n"
};

// Single pass lighting shader definition.

static ShaderInfo single_pass_shader_info[NU_SINGLE_PASS_SHADERS] = {
    { "Complete single pass shader", UNIFORM_MASK_COMMON ^ (1 << UNIFORM_CURRENT_LIGHT),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) | (1 << ATTRIBUTE_COLOR) },
    { "Complete single pass shader for directional light", UNIFORM_MASK_COMMON ^
    ((1 << UNIFORM_CURRENT_LIGHT) | (1 << UNIFORM_LIGHT_ATT)),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Single-pass phong-only shader for directional light", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_MULTI_COLOR) |
    (1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_VIEWPOINT) | (UNIFORM_LIGHT_PARAMETERS_MASK ^
    (1 << UNIFORM_LIGHT_ATT)) | (1 << UNIFORM_EMISSION_COLOR),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_COLOR) },
    { "Single-pass (final pass) constant shader", (1 << UNIFORM_MVP) | (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_USE_EMISSION_MAP) | (1 << UNIFORM_EMISSION_MAP_SAMPLER) |
    ((unsigned int)1 << UNIFORM_UV_TRANSFORM),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) },
    { "Single-pass phong texture-only shader for directional light", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_AMBIENT_COLOR) |
    (1 << UNIFORM_VIEWPOINT) | (UNIFORM_LIGHT_PARAMETERS_MASK ^ (1 << UNIFORM_LIGHT_ATT)) |
    (1 << UNIFORM_TEXTURE_SAMPLER) | ((unsigned int)1 << UNIFORM_UV_TRANSFORM) | (1 << UNIFORM_EMISSION_COLOR),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) },
    { "Single-pass phong texture plus normal map-only shader for directional light", (1 << UNIFORM_MVP) |
    (1 << UNIFORM_MODEL_MATRIX) | (1 << UNIFORM_MODEL_ROTATION_MATRIX) |
    (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | (1 << UNIFORM_AMBIENT_COLOR) | (1 << UNIFORM_VIEWPOINT) |
    (UNIFORM_LIGHT_PARAMETERS_MASK ^ (1 << UNIFORM_LIGHT_ATT)) |
    (1 << UNIFORM_TEXTURE_SAMPLER) | ((unsigned int)1 << UNIFORM_UV_TRANSFORM) | (1 << UNIFORM_NORMAL_MAP_SAMPLER) |
    (1 << UNIFORM_EMISSION_COLOR),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) | (1 << ATTRIBUTE_TANGENT) },
    { "Complete single pass shader for point lights with a linear attenuation range",
    (UNIFORM_MASK_COMMON ^ (1 << UNIFORM_CURRENT_LIGHT)) | (1 << UNIFORM_SPOTLIGHT),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS) | (1 << ATTRIBUTE_NORMAL) |
    (1 << ATTRIBUTE_TANGENT) |
    (1 << ATTRIBUTE_COLOR) },
    { "Single-pass constant shader with multi-color support",
    (1 << UNIFORM_MVP) | (1 << UNIFORM_EMISSION_COLOR) |
    (1 << UNIFORM_MULTI_COLOR),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_COLOR) },
};

const char *single_pass_shader_prologue[NU_SINGLE_PASS_SHADERS] = {
    // Complete versatile single pass shader with support for all options.
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define AMBIENT_COLOR_IN\n"
    "#define EMISSION_COLOR_IN\n"
    "#define EMISSION_MAP_OPTION\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n",
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
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
    "#define TEXTURE_FIXED\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
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
    "#define TEXTURE_FIXED\n"
    "#define NORMAL_VAR\n"
    "#define TEXCOORD_VAR\n"
    "#define TBN_MATRIX_VAR\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_FIXED\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define AMBIENT_COLOR_IN\n"
    "#define EMISSION_COLOR_IN\n"
    "#define DIRECTIONAL_LIGHT\n",
    // Complete single pass shader for point lights with a linear attenuation range.
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
    "#define TEXTURE_OPTION\n"
    "#define NORMAL_MAP_OPTION\n"
    "#define SPECULARITY_MAP_OPTION\n"
    "#define VIEWPOINT_IN\n"
    "#define LIGHT_PARAMETERS\n"
    "#define TEXTURE_SAMPLER\n"
    "#define NORMAL_MAP_SAMPLER\n"
    "#define SPECULARITY_MAP_SAMPLER\n"
    "#define AMBIENT_COLOR_IN\n"
    "#define EMISSION_COLOR_IN\n"
    "#define EMISSION_MAP_OPTION\n"
    "#define EMISSION_MAP_SAMPLER\n"
    "#define TEXTURE_ALPHA\n"
    "#define POINT_SOURCE_LIGHT\n"
    "#define LINEAR_ATTENUATION_RANGE\n",
    // Constant shading-only single pass shader (no lighting or texture)
    // supporting multi-color and emission color
    "#define SINGLE_PASS\n"
    "#define COLOR_IN\n"
    "#define MULTI_COLOR_OPTION\n"
    "#define EMISSION_COLOR_IN\n"
    "#define NO_SMOOTH_SHADING\n"
    "#define ADD_DIFFUSE_TO_EMISSION\n"
};

static void printShaderInfoLog(GLuint obj) {
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);
    if (infologLength > 0) {
        infoLog = (char *)malloc(infologLength);
        glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
	printf("%s\n",infoLog);
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
	printf("%s\n",infoLog);
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
    fread(buf, length, 1, fptr); /* Read the contents of the file in to the buffer */
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
        printf("Error (sreShader::Initialize()) -- shader not uninitialized.\n");
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
        printf("Error (InitializeShader()) -- shader not uninitialized.\n");
        exit(1);
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
    printf("Loading shader %s ", name);
    if (status == SRE_SHADER_STATUS_UNINITIALIZED) {
        printf("Error -- sreShader::Initialize() should be called before LoadShader().\n");
        exit(1);
    }

    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    if (v == 0) {
        printf("Error creating vertex shader %s\n", name);
        exit(1); 
    }
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);	
    if (f == 0) {
        printf("Error creating fragment shader %s\n", name);
        exit(1); 
    }

    bool builtin = false;
    char *vertexsource = NULL;
    char *fragmentsource = NULL;
    for (int sdir = 0;; sdir++) {
        if (shader_directory_table[sdir] == SHADER_DIRECTORY_END_MARKER) {
            printf("Error - shader file not found (%s, %s).\n", 
                vfilename, ffilename);
            exit(1);
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
                    printf("Fragment shader missing (%s).", fragment_path);
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
    if (builtin)
        printf("(built-in)\n");
    else
        printf("(default path)\n");


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
        printf("Error creating program.\n");
        exit(1); 
    }

//    glUseProgram(0);

    glAttachShader(program, v);
    glAttachShader(program ,f);

    BindAttributes();

    glLinkProgram(program);
    GLint params;
    glGetProgramiv(program, GL_LINK_STATUS, &params);
    if (params == GL_FALSE) {
        printf("Shader program link unsuccesful (%s, %s).\n", vfilename, ffilename);
        printf("Vertex shader log:\n");
        printShaderInfoLog(v);
        printf("Fragment shader log:\n");
        printShaderInfoLog(f);
        printf("Shader program log:\n");
        printProgramInfoLog(program);
        exit(1);
    }

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
            printf("OpenGL error occurred after BindAttribs for shader %s, attribute %d.\n",
                name, i);
            exit(1);
        }
    }
    if (glGetError() != GL_NO_ERROR) {
        printf("OpenGL error occurred after BindAttribs.\n");
        exit(1);
    }
}

void sreShader::InitializeUniformLocationsLightingShader() {
    for (int j = 0; j < MAX_UNIFORMS; j++)
        if (uniform_mask & (1 << j)) {
            uniform_location[j] = glGetUniformLocation(program, uniform_str[j]);
            if (uniform_location[j] == - 1) {
                printf("Error getting uniform location for '%s' from shader '%s'.\n", uniform_str[j], name);
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
            if (type & (SRE_SHADER_MASK_LIGHTING_SINGLE_PASS | SRE_SHADER_MASK_LIGHTING_MULTI_PASS))
                sreInitializeLightingShaderUniformWithDefaultValue(i, uniform_location[i]);
            else
                // Misc shader.
               sreInitializeMiscShaderUniformWithDefaultValue(i, uniform_location[i]);
        }
    glUseProgram(0);
}

// Array of uniform identifiers for miscellaneous shaders.

static const char *uniform_misc_str[] = {
    "MVP", "light_pos_model_space_in", "view_projection_matrix",
    "base_color_in", "aspect_ratio_in", "halo_size_in", "texture_in", "light_position_in",
    "model_matrix", "segment_distance_scaling_in", "average_lum_in", "slot_in",
    "key_value_in", "array_in", "rectangle_in", "uv_transform_in", "mult_color_in",
    "add_color_in", "screen_size_in_chars_in", "string_in" };

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
#if 0
    {
    "Old text shader",
    SRE_SHADER_MASK_TEXT,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_BASE_COLOR),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS),
    "gl3_text.vert", "gl3_text.frag", ""
    },
#endif
    {
    "Text shader (16x16 font texture)",
    SRE_SHADER_MASK_TEXT,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_RECTANGLE) |
    (1 << UNIFORM_MISC_MULT_COLOR) | (1 << UNIFORM_MISC_ADD_COLOR) |
    (1 << UNIFORM_MISC_SCREEN_SIZE_IN_CHARS) | (1 << UNIFORM_MISC_STRING),
    (1 << ATTRIBUTE_POSITION),
    "gl3_image.vert", "gl3_text2.frag",
    "#define TEXT_SHADER\n#define ONE_COMPONENT\n#define MAX_TEXT_LENGTH 256\n"
#ifdef OPENGL_ES2
    "#define FONT_TEXTURE_COLUMNS 16\n#define FONT_TEXTURE_ROWS 16\n"
#else
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
    "#define TEXT_SHADER\n#define ONE_COMPONENT\n#define MAX_TEXT_LENGTH 256\n"
#ifdef OPENGL_ES2
    "#define FONT_TEXTURE_COLUMNS 32\n#define FONT_TEXTURE_ROWS 8\n"
#else
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
    {
    "2D texture array image shader",
    SRE_SHADER_MASK_IMAGE,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) |
#ifndef OPENGL_ES2
    (1 << UNIFORM_MISC_ARRAY_INDEX) |
#endif
    (1 << UNIFORM_MISC_RECTANGLE) | (1 << UNIFORM_MISC_UV_TRANSFORM) |
    (1 << UNIFORM_MISC_MULT_COLOR) | (1 << UNIFORM_MISC_ADD_COLOR),
    (1 << ATTRIBUTE_POSITION),
    "gl3_image.vert", "gl3_image.frag", "#define UV_TRANSFORM\n#define TEXTURE_ARRAY\n"
    },
    {
    "2D texture array image shader (one component)",
    SRE_SHADER_MASK_IMAGE,
    (1 << UNIFORM_MISC_TEXTURE_SAMPLER) |
#ifndef OPENGL_ES2
    (1 << UNIFORM_MISC_ARRAY_INDEX) |
#endif
    (1 << UNIFORM_MISC_RECTANGLE) | (1 << UNIFORM_MISC_UV_TRANSFORM) |
    (1 << UNIFORM_MISC_MULT_COLOR) | (1 << UNIFORM_MISC_ADD_COLOR),
    (1 << ATTRIBUTE_POSITION),
    "gl3_image.vert", "gl3_image.frag",
        "#define UV_TRANSFORM\n#define TEXTURE_ARRAY\n#define ONE_COMPONENT\n"
    },
    {
    "Shadow volume shader",
    SRE_SHADER_MASK_SHADOW_VOLUME,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_LIGHT_MODEL_SPACE),
    (1 << ATTRIBUTE_POSITION),
    "gl3_shadow_volume.vert", "gl3_shadow_volume.frag", ""
    },
    {
    "Shadow map shader",
    SRE_SHADER_MASK_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP),
    (1 << ATTRIBUTE_POSITION),
    "gl3_shadow_map.vert", "gl3_shadow_map.frag", ""
    },
    {
    "Shadow map shader for transparent textures",
    SRE_SHADER_MASK_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_UV_TRANSFORM),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS),
    "gl3_shadow_map.vert", "gl3_shadow_map.frag", "#define TEXTURE_ALPHA\n#define UV_TRANSFORM\n"
    },
    {
    "Shadow cube-map shader",
    SRE_SHADER_MASK_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_LIGHT_POSITION) |
    (1 << UNIFORM_MISC_MODEL_MATRIX) | (1 << UNIFORM_MISC_SEGMENT_DISTANCE_SCALING),
    (1 << ATTRIBUTE_POSITION),
     "gl3_shadow_map.vert", "gl3_shadow_map.frag", "#define CUBE_MAP\n"   
    },
    {
    "Shadow cube-map shader for transparent textures",
    SRE_SHADER_MASK_SHADOW_MAP,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_TEXTURE_SAMPLER) | (1 << UNIFORM_MISC_UV_TRANSFORM) |
    (1 << UNIFORM_MISC_LIGHT_POSITION) | (1 << UNIFORM_MISC_MODEL_MATRIX) |
    (1 << UNIFORM_MISC_SEGMENT_DISTANCE_SCALING),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS),
     "gl3_shadow_map.vert", "gl3_shadow_map.frag",
     "#define CUBE_MAP\n#define TEXTURE_ALPHA\n#define UV_TRANSFORM\n"   
    },
    {
    "Halo shader",
    SRE_SHADER_MASK_EFFECTS,
    (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_VIEW_PROJECTION_MATRIX) |
    (1 << UNIFORM_MISC_BASE_COLOR) | (1 << UNIFORM_MISC_ASPECT_RATIO) | (1 << UNIFORM_MISC_HALO_SIZE),
    (1 << ATTRIBUTE_POSITION),
    "gl3_halo.vert", "gl3_halo.frag", ""
    },
    {
    "Particle system shader",
    SRE_SHADER_MASK_EFFECTS,
    (1 << UNIFORM_MISC_VIEW_PROJECTION_MATRIX) |
    (1 << UNIFORM_MISC_BASE_COLOR) | (1 << UNIFORM_MISC_ASPECT_RATIO) | (1 << UNIFORM_MISC_HALO_SIZE),
    (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL),
    "gl3_ps.vert", "gl3_ps.frag", ""
    },
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
};

void sreShader::InitializeUniformLocationsMiscShaderNew() {
    int count = 0;
    for (int j = 0; j < MAX_MISC_UNIFORMS; j++)
        if (uniform_mask & (1 << j)) {
            uniform_location[count] = glGetUniformLocation(program, uniform_misc_str[j]);
            if (uniform_location[count] == - 1) {
                printf("Error getting uniform location for '%s' from shader '%s'.\n",
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
                printf("Error getting uniform location for '%s' from shader '%s'.\n", uniform_misc_str[j], name);
                exit(1);
            }
        }
}

// Actual shader initialization.

sreShader lighting_pass_shader[NU_LIGHTING_PASS_SHADERS];
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
#if 0
    shadow_map_shader.name = "Shadow map shader";
    shadow_map_shader.type = SRE_SHADER_MASK_SHADOW_MAP;
    shadow_map_shader.uniform_mask = (1 << UNIFORM_MISC_MVP);
    shadow_map_shader.attribute_mask = (1 << ATTRIBUTE_POSITION);
    shadow_map_shader.Initialize("gl3_shadow_map.vert", "gl3_shadow_map.frag", "");
    shadow_map_transparent_shader.name = "Shadow map shader for transparent textures";
    shadow_map_transparent_shader.type = SRE_SHADER_MASK_SHADOW_MAP;
    shadow_map_transparent_shader.uniform_mask = (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_TEXTURE_SAMPLER);
    shadow_map_transparent_shader.attribute_mask = (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS);
    shadow_map_transparent_shader.Initialize("gl3_shadow_map.vert", "gl3_shadow_map.frag", "#define TEXTURE_ALPHA\n");
    cube_shadow_map_shader.name = "Shadow cube-map shader";
    cube_shadow_map_shader.type = SRE_SHADER_MASK_SHADOW_MAP;
    cube_shadow_map_shader.uniform_mask = (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_LIGHT_POSITION) |
        (1 << UNIFORM_MISC_MODEL_MATRIX) | (1 << UNIFORM_MISC_SEGMENT_DISTANCE_SCALING);
    cube_shadow_map_shader.attribute_mask = (1 << ATTRIBUTE_POSITION);
    cube_shadow_map_shader.Initialize("gl3_shadow_map.vert", "gl3_shadow_map.frag", "#define CUBE_MAP\n");
    misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].name = "Shadow cube-map shader for transparent textures";
    misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].type = SRE_SHADER_MASK_SHADOW_MAP;
    misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].uniform_mask = (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_TEXTURE_SAMPLER) |
        (1 << UNIFORM_MISC_LIGHT_POSITION) | (1 << UNIFORM_MISC_MODEL_MATRIX) |
        (1 << UNIFORM_MISC_SEGMENT_DISTANCE_SCALING);
    misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].attribute_mask = (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_TEXCOORDS);
    misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].Initialize("gl3_shadow_map.vert", "gl3_shadow_map.frag",
        "#define CUBE_MAP\n#define TEXTURE_ALPHA\n");
#endif
#endif
}

static void sreInitializeEffectsShaders() {
    sreInitializeMiscShaders(SRE_SHADER_MASK_EFFECTS);
#if 0
    halo_shader.name = "Halo shader";
    halo_shader.type = SRE_SHADER_MASK_EFFECTS;
    halo_shader.uniform_mask = (1 << UNIFORM_MISC_MVP) | (1 << UNIFORM_MISC_VIEW_PROJECTION_MATRIX) |
        (1 << UNIFORM_MISC_BASE_COLOR) | (1 << UNIFORM_MISC_ASPECT_RATIO) | (1 << UNIFORM_MISC_HALO_SIZE);
    halo_shader.attribute_mask = (1 << ATTRIBUTE_POSITION);
    halo_shader.Initialize("gl3_halo.vert", "gl3_halo.frag", "");
    ps_shader.name = "Particle system shader";
    ps_shader.type = SRE_SHADER_MASK_EFFECTS;
    ps_shader.uniform_mask = (1 << UNIFORM_MISC_VIEW_PROJECTION_MATRIX) |
        (1 << UNIFORM_MISC_BASE_COLOR) | (1 << UNIFORM_MISC_ASPECT_RATIO) | (1 << UNIFORM_MISC_HALO_SIZE);
    ps_shader.attribute_mask = (1 << ATTRIBUTE_POSITION) | (1 << ATTRIBUTE_NORMAL);
    ps_shader.Initialize("gl3_ps.vert", "gl3_ps.frag", "");
#endif
}

static const char *tone_map_prologue[] = {
     "#define TONE_MAP_LINEAR\n", "#define TONE_MAP_REINHARD\n", "#define TONE_MAP_EXPONENTIAL\n"
};

static void sreInitializeHDRShaders() {
#ifndef NO_HDR
    sreInitializeMiscShaders(SRE_SHADER_MASK_HDR);
#if 0
    misc_shader[SRE_MISC_SHADER_HDR_LOG_LUMINANCE].name = "HDR log luminance shader";
    misc_shader[SRE_MISC_SHADER_HDR_LOG_LUMINANCE].type = SRE_SHADER_MASK_HDR;
    misc_shader[SRE_MISC_SHADER_HDR_LOG_LUMINANCE].uniform_mask = (1 << UNIFORM_MISC_TEXTURE_SAMPLER);
    misc_shader[SRE_MISC_SHADER_HDR_LOG_LUMINANCE].attribute_mask = (1 << ATTRIBUTE_POSITION);
    misc_shader[SRE_MISC_SHADER_HDR_LOG_LUMINANCE].Initialize("gl3_HDR_log_lum.vert", "gl3_HDR_log_lum.frag", "");
    misc_shader[SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE].name = "HDR average log luminance shader";
    misc_shader[SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE].type = SRE_SHADER_MASK_HDR;
    misc_shader[SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE].uniform_mask = (1 << UNIFORM_MISC_TEXTURE_SAMPLER);
    misc_shader[SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE].attribute_mask = (1 << ATTRIBUTE_POSITION);
    misc_shader[SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE].Initialize("gl3_HDR_average_lum.vert", "gl3_HDR_average_lum.frag", "");
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].name = "HDR luminance history storage shader";
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].type = SRE_SHADER_MASK_HDR;
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].uniform_mask = (1 << UNIFORM_MISC_TEXTURE_SAMPLER) |
        (1 << UNIFORM_MISC_AVERAGE_LUM_SAMPLER);
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].attribute_mask = (1 << ATTRIBUTE_POSITION);
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].Initialize("gl3_HDR_lum_history_storage.vert", "gl3_HDR_lum_history_storage.frag", "");
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].name = "HDR luminance history shader";
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].type = SRE_SHADER_MASK_HDR;
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].uniform_mask = (1 << UNIFORM_MISC_TEXTURE_SAMPLER) |
        (1 << UNIFORM_MISC_LUMINANCE_HISTORY_SLOT);
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].attribute_mask = (1 << ATTRIBUTE_POSITION);
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].Initialize("gl3_HDR_lum_history_comparison.vert",
        "gl3_HDR_lum_history_comparison.frag", "");
#endif
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

static void sreInitializeMultiPassLightingShaders() {
    // New style shader loading for lighting shaders.
    for (int i = 0; i < NU_LIGHTING_PASS_SHADERS; i++) {
#ifdef NO_SHADOW_MAP
        if (i >= 12 && i <= 18)
            continue;
#endif
        lighting_pass_shader[i].Initialize(
            lighting_pass_shader_info[i].name,
            SRE_SHADER_MASK_LIGHTING_MULTI_PASS,
            lighting_pass_shader_info[i].uniform_mask,
            lighting_pass_shader_info[i].attribute_mask,
            "gl3_lighting_pass.vert", "gl3_lighting_pass.frag",
            lighting_pass_shader_prologue[i]);
    }
}

static void sreInitializeSinglePassLightingShaders() {
    for (int i = 0; i < NU_SINGLE_PASS_SHADERS; i++) {
        single_pass_shader[i].Initialize(
            single_pass_shader_info[i].name,
            SRE_SHADER_MASK_LIGHTING_SINGLE_PASS,
            single_pass_shader_info[i].uniform_mask,
            single_pass_shader_info[i].attribute_mask,
            "gl3_lighting_pass.vert", "gl3_lighting_pass.frag",
            single_pass_shader_prologue[i]);
    }
}

// This is functions called by sreInitialize(). Depending on the demand-loading
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

void sreValidateShadowMapShaders() {
   misc_shader[SRE_MISC_SHADER_SHADOW_MAP].Validate();
   misc_shader[SRE_MISC_SHADER_SHADOW_MAP_TRANSPARENT].Validate();
   misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP].Validate();
   misc_shader[SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT].Validate();

}

void sreValidateHDRShaders() {
    misc_shader[SRE_MISC_SHADER_HDR_LOG_LUMINANCE].Validate();
    misc_shader[SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE].Validate();
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE].Validate();
    misc_shader[SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON].Validate();
    HDR_tone_map_shader[sre_internal_HDR_tone_mapping_shader].Validate();
}