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

#ifdef GL_ES
precision mediump float;
#endif

#ifdef TEXTURE_ALPHA
uniform sampler2D texture_in;
varying vec2 texcoord_var;
#endif
#if defined(CUBE_MAP) && defined(CUBE_MAP_STORES_DISTANCE)
uniform vec3 light_position_in;
uniform float segment_distance_scaling_in;
varying vec3 position_world_var;
#endif
#if defined(ADD_BIAS) && !defined(CUBE_MAP) && !defined(SPOTLIGHT)
varying float bias_var;
#endif

void main() {
#ifdef TEXTURE_ALPHA
	float alpha = texture2D(texture_in, texcoord_var).a;
        if (alpha <= 0.1)
		discard;
#endif

#if defined(ADD_BIAS) && !defined(CUBE_MAP) && !defined(SPOTLIGHT)
	// Only apply bias for front (light)-facing triangles of non-closed objects.
	float bias = float(gl_FrontFacing);
	// Directional light. Bias is passed as varying.
	bias *= bias_var;
#endif

#if defined(CUBE_MAP) && defined(CUBE_MAP_STORES_DISTANCE)
	// Write the distance to the depth buffer. This is impossible in OpenGL ES 2.0.
	gl_FragDepth = distance(position_world_var, light_position_in) *
		segment_distance_scaling_in;
#else
#ifdef ADD_BIAS
	gl_FragDepth = gl_FragCoord.z + bias;
#else
	// The color value written doesn't matter. The fixed-function depth value is written as
	// a side effect.
	gl_FragColor = vec4(1.0);
#endif
#endif
}

