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
uniform bool multi_color_in;
#endif
#ifdef TEXTURE_OPTION
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
#if defined(SHADOW_MAP) && !defined(SPOT_LIGHT_SHADOW_MAP)
#ifdef GL_ES
uniform mat4 shadow_map_transformation_matrix;
#else
uniform mat4x3 shadow_map_transformation_matrix;
#endif
#endif
#ifdef SHADOW_MAP
// For use with shadow map parameter precalculation.
uniform vec4 shadow_map_dimensions_in;
uniform sampler2D shadow_map_in;
uniform vec4 light_position_in;
#ifdef LINEAR_ATTENUATION_RANGE
uniform vec4 spotlight_in;
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
attribute vec3 color_in;		// For multi-color objects
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
#ifdef SHADOW_MAP
#ifdef SPOT_LIGHT_SHADOW_MAP
varying vec4 model_position_var;
#else
varying vec3 shadow_map_coord_var;
#endif
#endif
#ifdef SHADOW_MAP
varying float reprocical_shadow_map_size_var;
varying float slope_var;
varying float shadow_map_depth_precision_var;
varying vec3 shadow_map_dimensions_var;
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
        if (!multi_color_in)
		diffuse_reflection_color_var = diffuse_reflection_color_in;
	else
#endif
#if defined(MULTI_COLOR_OPTION) || defined(MULTI_COLOR_FIXED)
            diffuse_reflection_color_var = color_in;
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

#ifdef SHADOW_MAP 
#ifdef SPOT_LIGHT_SHADOW_MAP
	model_position_var = position_in;
#else
	shadow_map_coord_var = (shadow_map_transformation_matrix * position_in).xyz;
#endif
#endif

	// Precalculate shadow map parameters.
#ifdef SHADOW_MAP
	reprocical_shadow_map_size_var = 1.0 / shadow_map_dimensions_in.w;
	vec3 L_bias;
#ifdef SPOT_LIGHT_SHADOW_MAP
        // The direction of the spotlight remains the z axis even for points away from the
        // central axis.
	L_bias = - spotlight_in.xyz;
#else
	L_bias = light_position_in.xyz;
#endif
	// Calculate the slope of the triangle relative to direction of the light
	// (direction of increasing depth in shadow map).
	// Use the vertex normal.
	slope_var = tan(acos(clamp(dot(normal, L_bias), 0.001, 1.0)));
        // Limit slope to 100.0.
	slope_var = min(slope_var, 100.0);

	// Set depth buffer precision.
	if (shadow_map_dimensions_in.w >= 2047.0)
		// Float depth buffer.
		shadow_map_depth_precision_var = 0.5 / pow(2.0, 23.0);
	else
		// Half-float depth buffer.
	        shadow_map_depth_precision_var = 0.5 / pow(2.0, 11.0);
	shadow_map_dimensions_var = shadow_map_dimensions_in.xyz;
#endif // defined(SHADOW_MAP)

	gl_Position = MVP * position_in;
}

