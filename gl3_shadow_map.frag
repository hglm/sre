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

// Shadow map fragment shader.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.

#ifndef GL_ES
#version 330
#endif

#ifndef CUBE_MAP
uniform vec4 shadow_map_dimensions_in;
#endif
#ifdef TEXTURE_ALPHA
uniform sampler2D texture_in;
varying vec2 texcoord_var;
#endif
#ifdef CUBE_MAP
uniform vec3 light_position_in;
uniform float segment_distance_scaling_in;
varying vec3 position_world_var;
#endif
#if defined(ADD_BIAS) && !defined(CUBE_MAP)
#ifdef PROJECTION
varying float reprocical_shadow_map_size_var;
varying float slope_var;
varying float shadow_map_depth_precision_var;
#else
varying float bias_var;
#endif
#endif

void main() {
#ifdef TEXTURE_ALPHA
	float alpha = texture2D(texture_in, texcoord_var).a;
        if (alpha <= 0.1)
		discard;
#endif

#if defined(ADD_BIAS) && !defined(CUBE_MAP)
	// Only apply bias for front (light)-facing triangles of non-closed objects. Since
	// only back (non-light)-facing triangles of closed objects are drawn, all front
	// facing triangles encountered are part of a non-closed object.
	float bias = float(gl_FrontFacing);
#ifdef PROJECTION
	// Calculate bias.
	float shadow_map_world_depth_range;
	float shadow_map_world_width;
	// Calculate real-world dimensions of the shadow map for the fragment position.
	shadow_map_world_depth_range = shadow_map_dimensions_in.z;
	// The shadow map width in world space units at the z of the world space position is
	// equal to the distance to the light in the direction of the light.
	shadow_map_world_width = gl_FragCoord.z * shadow_map_world_depth_range;
	// Apply bias to all triangles of non-closed-objects.
        // Set bias corresponding to the world space depth difference of adjacent pixels
	// in shadow map.
	bias *= slope_var * shadow_map_world_width * reprocical_shadow_map_size_var;
	// Normalize bias from world space coordinates to depth buffer coordinates.
	bias *= 1.0 / shadow_map_world_depth_range;
	// Add a little extra so that the bias is greater that the bias used when drawing
        // the triangle in the lighting pass.
	bias += shadow_map_depth_precision_var * 2.0;
#else
	// Directional light. Bias is passed as varying.
	bias *= bias_var;
#endif
#endif

#ifdef CUBE_MAP
	gl_FragDepth = distance(position_world_var, light_position_in) *
		segment_distance_scaling_in;
#else
#ifdef ADD_BIAS
	gl_FragDepth = gl_FragCoord.z + bias;
#else
#endif
	gl_FragDepth = gl_FragCoord.z;
#endif
}

