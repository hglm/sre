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

// Particle system shader in screen space. The model objects are billboards in
// world space, to be rendered with blending.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.

uniform mat4 view_projection_matrix;
attribute vec3 position_in;	// Billboard vertices in world space.
#ifdef HALO
attribute vec3 normal_in;	// Billboard center in world space for halo.
varying vec2 screen_position_var;
varying vec2 screen_position_center;
#else
attribute vec2 texcoord_in;
uniform bool use_emission_map_in;
// 3D transformation matrix to apply to the texcoords.
uniform mat3 uv_transform_in;
varying vec2 texcoord_var;
#endif

void main() {
	vec4 screen_position = view_projection_matrix * vec4(position_in, 1.0);
#ifdef HALO
        screen_position_center = (view_projection_matrix * vec4(normal_in, 1.0)).xy;
	screen_position_var = screen_position.xy;
#else
	// Default texcoords are in range [0, 1.0].
        // With the transformation matrix, a smaller part of the texture can be selected
        // (zoomed) for use with the object, and other effects are possible as well (including
        // mirroring the x or y-axis).
        texcoord_var = (uv_transform_in * vec3(texcoord_in, 1.0)).xy;
#endif
	gl_Position = screen_position;
}

