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
#else
precision mediump float;
#endif

#ifndef CUBE_MAP
uniform vec4 shadow_map_dimensions_in;
#endif
#ifdef TEXTURE_ALPHA
uniform sampler2D texture_in;
varying vec2 texcoord_var;
#endif
#if defined(CUBE_MAP) || defined(SPOTLIGHT)
uniform vec3 light_position_in;
uniform float segment_distance_scaling_in;
varying vec3 position_world_var;
#endif
#if defined(ADD_BIAS) && !defined(CUBE_MAP) && !defined(SPOTLIGHT)
varying float bias_var;
#endif


#ifdef GL_ES

void WriteDepth(float z) {
#if defined(CUBE_MAP) || defined(SPOTLIGHT)
	// We want to write the distance, which might be impossible in OpenGL ES 2.0
	// depending on the implementation of GL_OES_depth_texture_cube_map.
	// At least make the shader depend on z to avoid compilation errors.
	gl_FragColor.z = z;
#else
	// With OpenGL ES 2.0, the depth is written using as a fixed function
	// side effect, the color doesn't matter.
	gl_FragColor = vec4(1.0);
#endif
}

#else

void WriteDepth(float z) {
	gl_FragDepth = z;
}

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

#if defined(CUBE_MAP) || defined(SPOTLIGHT)
        WriteDepth(distance(position_world_var, light_position_in) *
		segment_distance_scaling_in);
#else
#ifdef ADD_BIAS
	WriteDepth(gl_FragCoord.z + bias);
#else
	WriteDepth(gl_FragCoord.z);
#endif
#endif
}

