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

#if defined(CUBE_MAP) || defined(PROJECTION) || defined (GL_ES)
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
#ifdef CUBE_MAP
uniform mat4x3 model_matrix;
varying vec3 position_world_var;
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
#ifdef CUBE_MAP
	position_world_var = (model_matrix * position_in).xyz;
#endif
#ifdef PROJECTION
	gl_Position = MVP * position_in;
#else
	gl_Position = vec4((MVP * position_in).xyz, 1.0);
#endif
}

