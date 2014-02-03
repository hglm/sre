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

// Image vertex shader.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.
//
// Only one attibute (position) is used, with a static buffer.
//
// Used by both SRE image shaders and the text2 shader.

#version 130

// The position attribute is always equal to (0,0), (1.0, 0), (0, 1.0) or (1.0, 1.0).
attribute vec2 position_in;	// (0,0) is top-left corner of area.
// Size of the on-screen image rectangle (x, y, width and height), range [0, 1].
uniform vec4 rectangle_in;
#ifdef UV_TRANSFORM
// 3D transformation matrix to apply to the texcoords.
uniform mat3 uv_transform_in;
#endif
#ifdef TEXT_SHADER
// The on-screen font size is defined by the following uniform, which is equal to
// (1.0, 1.0) / font_size.
uniform vec2 screen_size_in_chars_in;
// For the text shader, output the relative position into the text image in character units.
varying vec2 text_image_position_var;
#else
// For the image shaders, output texcoords for the texture image.
varying vec2 texcoords_var;
#endif

void main() {
#ifndef TEXT_SHADER
#ifdef UV_TRANSFORM
	// Default texcoords are one of (0,0), (1.0, 0), (0, 1.0) and (1.0, 1.0).
        // With the transformation matrix, a smaller part of the texture can be selected
        // (zoomed), and other effects are possible as well (including inverting the x
        // or y-axis).
        texcoords_var = (uv_transform_in * vec3(position_in, 1.0)).xy;
#else
	texcoords_var = position_in;
#endif
#endif

        // Scale position to the actual size of the rectangle.
        vec2 position = position_in * rectangle_in.zw;

#ifdef TEXT_SHADER
        // Calculate the relative position into the text image in character units and pass
        // it as an output to the fragment shader.
	text_image_position_var = position * screen_size_in_chars_in;
#endif

	// Translate position to the actual rectangle location (add top-left corner
        // coordinates).
	position += rectangle_in.xy;

	// Map position from [0, 1] x [0, 1] to [- 1, 1] x [- 1, 1] and
	// invert the y coordinate.
	vec2 position_clip_space = position * vec2(2.0, - 2.0) + vec2(- 1.0, 1.0);
	gl_Position = vec4(position_clip_space, 0.0, 1.0);
}

