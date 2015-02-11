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

// Versatile per-pixel Phong shader supporting constant color, multi-color,
// textures, normal maps and specular maps. This version is for multi-pass rendering
// techniques such as stencil shadows. By design it only applies one light.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.

// For OpenGL-ES 2.0, allow medium precision for interpolated values that do not need
// the highest precision. These are preceded by the macro MEDIUMP, which expands to
// nothing when using OpenGL.
#ifndef GL_ES
#version 130
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
#ifdef SHADOW_MAP
// For use with shadow map parameter precalculation (directional/beam lights).
uniform vec4 shadow_map_dimensions_in;
uniform sampler2D shadow_map_in;
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
#ifdef SHADOW_MAP
invariant varying float reprocical_shadow_map_size_var;
varying float slope_var;
invariant varying float shadow_map_depth_precision_var;
invariant varying vec3 shadow_map_dimensions_var;
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
	vec3 normal = model_rotation_matrix * normalize(normal_in);
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
		tbn_matrix_var = mat3(t.x, b.x, normal.x,
			t.y, b.y, normal.y,
			t.z, b.z, normal.z);
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

#if defined(SHADOW_MAP)
	shadow_map_coord_var = (shadow_map_transformation_matrix * position_in).xyz;
	// shadow_map_coord_var maps to ([0, 1.0], [0, 1.0], [0, 1.0]) for the shadow
	// map region.
#endif
#if defined(SPOT_LIGHT_SHADOW_MAP)
	shadow_map_coord_var = shadow_map_transformation_matrix * vec4(position_world_var, 1.0);
	// shadow_map_coord_var.xyz / shadow_map_coord_var.w maps to
	// ([-1.0, 1.0], [-1.0, 1.0], [-1.0, 1.0]) for the shadow
	// map region.
#endif
	// Precalculate shadow map parameters for directional and beam lights.
#ifdef SHADOW_MAP
	reprocical_shadow_map_size_var = 1.0 / shadow_map_dimensions_in.w;
	vec3 L_bias;

      	vec3 light_parameters_position;
        light_parameters_position = vec3(
		light_parameters_in[LIGHT_POSITION_X],
		light_parameters_in[LIGHT_POSITION_Y],
		light_parameters_in[LIGHT_POSITION_Z]
		);

#ifdef DIRECTIONAL_LIGHT
	L_bias = light_parameters_position;
#else
	// For a beam light, the direction of light remains the z axis even for points away
	// from the central axis.
	vec3 light_parameters_axis_direction;
        light_parameters_axis_direction = vec3(
		light_parameters_in[LIGHT_AXIS_DIRECTION_X],
		light_parameters_in[LIGHT_AXIS_DIRECTION_Y],
		light_parameters_in[LIGHT_AXIS_DIRECTION_Z]
		);
	L_bias = - light_parameters_axis_direction;
#endif
	// Calculate the slope of the triangle relative to direction of the light
	// (direction of increasing depth in shadow map).
	// Use the vertex normal.
	slope_var = tan(acos(clamp(dot(normal, L_bias), 0.001, 1.0)));
        // Limit slope to 100.0.
	slope_var = min(slope_var, 100.0);

#ifdef GL_ES
	// GLES2 currently uses only 16-bit depth buffers.
	shadow_map_depth_precision_var = 0.5 / pow(2.0, 16.0);
#else
	// Set depth buffer precision.
	if (shadow_map_dimensions_in.w >= 2047.0)
		// Float depth buffer.
		shadow_map_depth_precision_var = 0.5 / pow(2.0, 23.0);
	else
		// Half-float depth buffer.
	        shadow_map_depth_precision_var = 0.5 / pow(2.0, 11.0);
#endif
	shadow_map_dimensions_var = shadow_map_dimensions_in.xyz;
#endif // defined(SHADOW_MAP)

	gl_Position = MVP * position_in;
}

