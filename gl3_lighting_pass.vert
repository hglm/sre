/*

Copyright (c) 2015 Harm Hanemaaijer <fgenfb@yahoo.com>

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

// Versatile per-pixel Phong shader supporting constant color, multi-color,
// textures, normal maps and specular maps. This version is for multi-pass rendering
// techniques such as stencil shadows. By design it only applies one light.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.

// For OpenGL-ES 2.0, allow medium precision for interpolated values that do not need
// the highest precision. These are preceded by the macro MEDIUMP, which expands to
// nothing when using OpenGL.
#ifndef GL_ES
#define MEDIUMP
#else
// When using OpenGL ES 2.0, the maximum precision in the fragment shader may be only
// "mediump", so that uniforms or varying variables must be limited in precision.
#define MEDIUMP mediump
#endif
uniform mat4 MVP;
#ifdef POSITION_WORLD_VAR
#ifdef GL_ES
uniform mat4 model_matrix;
#else
uniform mat4x3 model_matrix;
#endif
#endif
#ifdef NORMAL_VAR
uniform mat3 model_rotation_matrix;
#endif
#ifdef MULTI_COLOR_OPTION
uniform bool use_multi_color_in;
#endif
#ifdef TEXTURE_MAP_OPTION
uniform bool use_texture_in;
#endif
#ifdef NORMAL_MAP_OPTION
uniform bool use_normal_map_in;
#endif
#ifdef SPECULARITY_MAP_OPTION
uniform bool use_specular_map_in;
#endif
#ifdef EMISSION_MAP_OPTION
uniform bool use_emission_map_in;
#endif
#ifdef VIEWPOINT_IN
uniform vec3 viewpoint_in;
#endif
#ifdef MULTI_COLOR_OPTION
uniform vec3 diffuse_reflection_color_in;
#endif
#if defined(SHADOW_MAP)
#if defined(GL_ES)
uniform mat4 shadow_map_transformation_matrix;
#else
uniform mat4x3 shadow_map_transformation_matrix;
#endif
#endif
#ifdef SPOT_LIGHT_SHADOW_MAP
uniform mat4 shadow_map_transformation_matrix;
#endif
#if defined(SHADOW_MAP) || defined(SPOT_LIGHT_SHADOW_MAP) || defined(NORMAL_MAP_TANGENT_SPACE_VECTORS)
#ifdef GL_ES
uniform mediump float light_parameters_in[NU_LIGHT_PARAMETERS_MAX];
#else
uniform float light_parameters_in[NU_LIGHT_PARAMETERS_MAX];
#endif
#endif
attribute vec4 position_in;
#ifdef TEXCOORD_IN
attribute vec2 texcoord_in;
#endif
#ifdef UV_TRANSFORM
// 3D transformation matrix to apply to the texcoords.
uniform mat3 uv_transform_in;
#endif
#ifdef NORMAL_IN
attribute vec3 normal_in;
#endif
#ifdef TANGENT_IN
attribute vec4 tangent_in;
#endif
#ifdef COLOR_IN
// Vertex attribute for multi-color objects.
#ifdef COMPRESS_COLOR_ATTRIBUTE
attribute float color_in;
#else
attribute vec3 color_in;
#endif
varying MEDIUMP vec3 diffuse_reflection_color_var;
#endif
#ifdef NORMAL_VAR
varying vec3 normal_var;
#endif
#ifdef POSITION_WORLD_VAR
varying vec3 position_world_var;
#endif
#ifdef TBN_MATRIX_VAR
varying MEDIUMP mat3 tbn_matrix_var;
#endif
#ifdef TEXCOORD_VAR
varying vec2 texcoord_var;
#endif
#ifdef MICROFACET
varying vec3 tangent_var;
#endif
#if defined(SHADOW_MAP)
varying vec3 shadow_map_coord_var;
#endif
#if defined(SPOT_LIGHT_SHADOW_MAP)
varying vec4 shadow_map_coord_var;
#endif
#if defined(SHADOW_MAP) || defined(SPOT_LIGHT_SHADOW_MAP)
varying float slope_var;
#endif
#ifdef NORMAL_MAP_TANGENT_SPACE_VECTORS
varying vec3 V_tangent_var;
varying vec3 L_tangent_var;
#endif

#ifdef COMPRESS_COLOR_ATTRIBUTE

vec3 DecompressColor(float cc) {
	const float f1 = 256.0;
        const float f2 = 65536.0;
        const float f3 = 16777216.0;
	vec3 c;
	c.r = floor(cc * f1);
	cc -= c.r / f1;
	c.g = floor(cc * f2);
	cc -= c.g / f2;
	c.b = floor(cc * f3);
        c = c * (1.0 / 255.0);
        return c;
}

#endif

void main() {
#ifdef POSITION_WORLD_VAR
	// The vertex position is specified in model space. Convert to world space.
	position_world_var = (model_matrix * position_in).xyz;
#endif
#ifdef NORMAL_VAR
	// Normal given is in model space.
        // Convert normal from model space to world space.
	vec3 normal = model_rotation_matrix * normal_in;
	normal_var = normal;
#endif

#ifdef MULTI_COLOR_OPTION
        if (!use_multi_color_in)
		diffuse_reflection_color_var = diffuse_reflection_color_in;
	else
#endif
#if defined(MULTI_COLOR_OPTION) || defined(MULTI_COLOR_FIXED)
#ifdef COMPRESS_COLOR_ATTRIBUTE
		diffuse_reflection_color_var = DecompressColor(color_in);
#else
		diffuse_reflection_color_var = color_in;
#endif
#endif

#if defined(NORMAL_MAP_OPTION) || defined(NORMAL_MAP_FIXED)
	mat3 tbn_matrix;
#endif
#ifdef NORMAL_MAP_OPTION
	// Normal mapping.
	if (use_normal_map_in) {
#endif
#if defined(NORMAL_MAP_OPTION) || defined(NORMAL_MAP_FIXED)
                // Convert tangent from object space to world space.
		vec3 t = model_rotation_matrix * normalize(tangent_in.xyz);
		// Calculate the bitangent; handedness is in the w coordinate of the input tangent.
	        vec3 b = normalize(cross(normal, t)) * tangent_in.w;
		// Calculate the matrix to convert from world to tangent space.
		tbn_matrix = mat3(t.x, b.x, normal.x,
			t.y, b.y, normal.y,
			t.z, b.z, normal.z);
#ifndef NORMAL_MAP_TANGENT_SPACE_VECTORS
		tbn_matrix_var = tbn_matrix;
#endif
#endif
#ifdef NORMAL_MAP_OPTION
	}
#endif

#ifdef MICROFACET
	tangent_var = tangent_in.xyz;
#endif

#ifdef TEXCOORD_VAR
#ifdef UV_TRANSFORM
	// Default texcoords are in range [0, 1.0].
        // With the transformation matrix, a smaller part of the texture can be selected
        // (zoomed) for use with the object, and other effects are possible as well (including
        // mirroring the x or y-axis).
        texcoord_var = (uv_transform_in * vec3(texcoord_in, 1.0)).xy;
#else
	texcoord_var = texcoord_in;
#endif
#endif

#if defined(SHADOW_MAP) || defined(SPOT_LIGHT_SHADOW_MAP) || defined(NORMAL_MAP_TANGENT_SPACE_VECTORS)
	// Prepare light parameters.
      	vec3 light_parameters_position;
        light_parameters_position = vec3(
		light_parameters_in[LIGHT_POSITION_X],
		light_parameters_in[LIGHT_POSITION_Y],
		light_parameters_in[LIGHT_POSITION_Z]
		);
	vec3 light_parameters_axis_direction;
        light_parameters_axis_direction = vec3(
		light_parameters_in[LIGHT_AXIS_DIRECTION_X],
		light_parameters_in[LIGHT_AXIS_DIRECTION_Y],
		light_parameters_in[LIGHT_AXIS_DIRECTION_Z]
		);
#endif

#if defined(SHADOW_MAP) || defined(SPOT_LIGHT_SHADOW_MAP)
	// For directional, beam and spot light shadow maps, we want to calculate and interpolate
	// the slope value (the orientation of the triangle relative tot the shadow map light
	// direction).
	// First calculate L_bias, the inverted shadow map light direction.
	vec3 L_bias;
#ifdef DIRECTIONAL_LIGHT
	L_bias = light_parameters_position;
#else
	// For a beam light, the direction of light remains the z axis even for points away
	// from the central axis. For spot lights, this calculation is also correct with respect
	// to the slope value in the depth buffer.
	L_bias = - light_parameters_axis_direction;
#endif
	// Calculate the slope of the triangle relative to direction of the light
	// (direction of increasing depth in shadow map).
	// Use the vertex normal. For double-sided surfaces, the slope value will be correct
	// even if the normal faces the wrong side.
	// This equation limits slope to approximately 100.0.
	slope_var = tan(acos(clamp(dot(normal, L_bias), 0.01, 1.0)));
#endif

#ifdef NORMAL_MAP_TANGENT_SPACE_VECTORS
	// Convert light and view vectors to tangent space.
	// Calculate the light vector L.
	vec3 L;
#ifdef DIRECTIONAL_LIGHT
	L = light_parameters_position;
#endif
#if defined(POINT_SOURCE_LIGHT) || defined(SPOT_LIGHT)
	// The light vector for point and spot lights is unnormalized. It will be normalized
	// by the fragment shader.
	L = light_parameters_position.xyz - position_world_var;
#endif
#ifdef BEAM_LIGHT
	L = - light_parameters_axis_direction;
#endif
	// Calculate the unnormalized inverse view direction V.
        vec3 V = viewpoint_in - position_world_var;
	// Convert L and V to tangent space.
	L_tangent_var = tbn_matrix * L;
	V_tangent_var = tbn_matrix * V;
#endif	// defined(NORMAL_MAP_TANGENT_SPACE_VECTORS)

#if defined(SHADOW_MAP)
	shadow_map_coord_var = (shadow_map_transformation_matrix * position_in).xyz;
	// shadow_map_coord_var maps to ([0, 1.0], [0, 1.0], [0, 1.0]) for the shadow
	// map region.
#endif
#if defined(SPOT_LIGHT_SHADOW_MAP)
	shadow_map_coord_var = shadow_map_transformation_matrix * vec4(position_world_var, 1.0);
	// shadow_map_coord_var.xyz / shadow_map_coord_var.w maps to
	// ([0.0, 1.0], [0.0, 1.0], [0.0, 1.0]) for the shadow map region.
#endif

	gl_Position = MVP * position_in;
}

