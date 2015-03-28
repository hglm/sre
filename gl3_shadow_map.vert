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

// Shadow map vertex shader.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.

#if defined(CUBE_MAP) || defined(SPOTLIGHT) || defined(GL_ES)
uniform mat4 MVP;
#else
uniform mat4x3 MVP;
#endif
attribute vec4 position_in;
#ifdef TEXTURE_ALPHA
attribute vec2 texcoord_in;
varying vec2 texcoord_var;
#endif
#ifdef UV_TRANSFORM
// 3D transformation matrix to apply to the texcoords.
uniform mat3 uv_transform_in;
#endif
#if defined(CUBE_MAP) && defined(CUBE_MAP_STORES_DISTANCE)
#ifdef GL_ES
uniform mat4 model_matrix;
#else
uniform mat4x3 model_matrix;
#endif
varying vec3 position_world_var;
#endif
#if defined(ADD_BIAS) && !defined(CUBE_MAP) && !defined(SPOTLIGHT)
// In the case of directional and beam lights, light_position_in is in fact
// the inverted light direction, not position.
uniform vec3 light_position_in;
attribute vec3 normal_in;
uniform vec4 shadow_map_dimensions_in;
varying float bias_var;
#endif

void main() {
#ifdef TEXTURE_ALPHA
	// Optional support for "punchthrough" transparent textures. 
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

#if defined(ADD_BIAS) && !defined(CUBE_MAP) && !defined(SPOTLIGHT)
	// Caculate slope for bias adjustment for light-facing triangles of non-closed models.
	// Since light-facing triangles of closed models are not rendered, every
	// front-facing triangle encountered is part of a non-closed model.
	float slope;
	vec3 L_bias;
	// Note: light_position_in is the inverted light direction, not position.
	L_bias = light_position_in.xyz;
	// Calculate the slope of the triangle relative to direction of the light
	// (direction of increasing depth in shadow map).
	// Use the vertex normal.
	slope = tan(acos(clamp(dot(normal_in, L_bias), 0.001, 1.0)));
        // Limit slope to 100.0.
	slope = min(slope, 100.0);
	// Set depth buffer precision.
	float shadow_map_depth_precision;
#ifdef GL_ES
        shadow_map_depth_precision = 1.0 / pow(2.0, 16.0);
#else
	if (shadow_map_dimensions_in.w >= 2047.0)
		// Float depth buffer.
		shadow_map_depth_precision = 0.5 / pow(2.0, 23.0);
	else
		// Half-float depth buffer.
	        shadow_map_depth_precision = 0.5 / pow(2.0, 11.0);
#endif
	float reprocical_shadow_map_size = 1.0 / shadow_map_dimensions_in.w;
	// Directional or beam light. Calculate bias.
	// Apply bias to all triangles of non-closed-objects.
        // Set bias corresponding to the world space depth difference of adjacent pixels
	// in shadow map.
	float bias = slope * shadow_map_dimensions_in.x * reprocical_shadow_map_size;
	// Normalize bias from world space coordinates to depth buffer coordinates.
	bias *= 1.0 / shadow_map_dimensions_in.z;
	// Example bias calculation for directional light, slope = 1.0:
	// bias = 1.0 * 400.0 * (1.0 / 2048.0) / 400.0 = 1.0 / 2048.0 = 0.000488

	// When Poisson disk shadows are enabled, bias needs to be somewhat greater to reduce
        // artifacts.
	bias *= 2.5;

	// Add a little extra so that the bias is greater that the bias used when drawing
        // the triangle in the lighting pass.
	bias += shadow_map_depth_precision * 2.0;
	bias_var = bias;	
#endif

#if defined(CUBE_MAP) && defined(CUBE_MAP_STORES_DISTANCE)
	position_world_var = (model_matrix * position_in).xyz;
	gl_Position = MVP * position_in;
#else
	// With OpenGL ES 2.0, the transformed vertex position needs to generate the correct
	// depth value for the fixed-function pipeline.
	// For directional light/beam light shadow maps, the w value does not matter.
#if defined(SPOTLIGHT) || defined(CUBE_MAP)
	gl_Position = MVP * position_in;
#else
	gl_Position = vec4((MVP * position_in).xyz, 1.0);
#endif
#endif
}

