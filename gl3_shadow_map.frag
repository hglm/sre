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

#version 150

#ifdef TEXTURE_ALPHA
uniform sampler2D texture_in;
varying vec2 texcoord_var;
#endif
#ifdef CUBE_MAP
uniform vec3 light_position_in;
uniform float segment_distance_scaling_in;
varying vec3 position_world_var;
#endif

void main() {
#ifdef TEXTURE_ALPHA
	float alpha = texture2D(texture_in, texcoord_var).a;
        if (alpha <= 0.1)
		discard;
	else
#endif
#ifdef CUBE_MAP
	gl_FragDepth = distance(position_world_var, light_position_in) * segment_distance_scaling_in;
#else
	gl_FragDepth = gl_FragCoord.z;
#endif
}

