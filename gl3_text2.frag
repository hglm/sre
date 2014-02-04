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

// Text shader #2. Don't pass texcoords as attributes but calculate them in the fragment shader
// based on position, which allows the use of a single quad. The text string is uploaded as a
// uniform array.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.

#ifndef GL_ES
#version 150
// We recognize the three levels of integer precision form the GLSL used in OpenGL-ES 2.0,
// but all are defined as 32-bit integers in OpenGL GLSL.
// In OpenGL-ES 2.0, lowp has the range (- 2^8, 2^8), mediump (- 2^10, 2^10),
// highp (- 2^16, 2^16) but precision may be lower (lower order bits not defined).
#define _uintlowp uint
#define _uintmediump uint
#define _uinthighp uint
#else
// Note: On the Mali-400 platform, highp float is not supported in the fragment shader.
// (treated as mediump).
precision highp float; 
// OpenGL ES 2.0 does not have any unsigned integer type.
#define _uintlowp lowp int
#define _uintmediump mediump int
#define _uinthighp highp int
#endif
// When compiling for OpenGL-ES 2.0, texture arrays are not supported
// and the respective shader is identical to the regular texture image
// shader.
#if defined(TEXTURE_ARRAY) && !defined(GL_ES)
uniform sampler2DArray texture_in;
uniform int array_in;
#else
uniform sampler2D texture_in;
#endif
uniform vec4 mult_color_in;
uniform vec4 add_color_in;
// The only input is the offset in character units into the text image (relative
// to the top-left corner of the text image).
varying vec2 text_image_position_var;

// The string buffer size should match the settings in the library. Generally,
// when characters are packed into ints (basically raw string data), the array
// size should be 64 (max length 256 characters).
// With OpenGL-ES2, only two characters are packed into each int.
#ifdef GL_ES
uniform _uintlowp string_in[MAX_TEXT_LENGTH];
#else
uniform uint string_in[MAX_TEXT_LENGTH / 4];
#endif

// Character per row in the font texture must be a power of two.
// These constants can be included as a prologue when compiling the shader.
// #define FONT_TEXTURE_COLUMNS 16u
// #define FONT_TEXTURE_ROWS 16u

// Ideally the number of characters rows and column should be configurable.
#define FONT_CHAR_SIZE vec2(1.0 / float(FONT_TEXTURE_COLUMNS), 1.0 / float(FONT_TEXTURE_ROWS))

void main() {
	// Round down the character position into the text image to get the character index.
	float x_floor = floor(text_image_position_var.x); 
	// Convert to integer.
#ifdef GL_ES
	_uintlowp char_index = int(x_floor);
#else
	_uintlowp char_index = uint(x_floor);
#endif
	// Because text_image_position_var is in character units, the relative texcoords
        // coordinates are obtained with a simple subtraction.
	vec2 texcoords_within_char = text_image_position_var - vec2(x_floor, 0.0);
	// We now have the the relative texcoords within the character image normalized to the
	// range [0.0 - 1.0].
        // Get the ASCII character byte from the packed int array.
#ifdef GL_ES
	// Because OpenGL-ES 2.0 GLSL does not have unsigned integers, integers have only
	// about 16 or 17 bits or even 14 bits of maximum precision, and there are no shift
        // operators, we need an int for each character.
	_uintlowp ch = string_in[char_index];
#else
 	_uintlowp ch = (string_in[char_index >> 2u] >> ((char_index & 3u) << 3u)) &
		uint(0xFF);
#endif
	// Calculate the row and column of the character in the font texture.
#ifdef GL_ES
	_uintlowp font_row = ch / FONT_TEXTURE_COLUMNS;
	_uintlowp font_column = ch - (font_row * FONT_TEXTURE_COLUMNS);
#else
	_uintlowp font_column = ch & (FONT_TEXTURE_COLUMNS - 1u);
	_uintlowp font_row = ch / FONT_TEXTURE_COLUMNS;
#endif
	float column_f = float(font_column);
	float row_f = float(font_row);
	// Calculate the font texture coordinates of the top left corner of the character,
	// and add the offset into the character image.
	vec2 texcoords_in_char_units = vec2(column_f, row_f) + texcoords_within_char;
        // Convert to texture coordinates.
	vec2 texcoords = texcoords_in_char_units * FONT_CHAR_SIZE;

	// The following section is copied from the image shader. A one-component texture
	// is efficient for fonts, while a texture array might be useful for fast multiple
	// font support.
#if defined(TEXTURE_ARRAY) && !defined(GL_ES)
#ifdef ONE_COMPONENT
        float i = texture(texture_in, vec3(texcoords, array_in)).x;
#else
	vec4 tex_color = texture(texture_in, vec3(texcoords, array_in));
#endif
#else
#ifdef ONE_COMPONENT
        float i = texture2D(texture_in, texcoords).x;
#else
	vec4 tex_color = texture2D(texture_in, texcoords);
	tex_color.a = 1.0;
#endif
#endif

	vec4 c;
#ifdef ONE_COMPONENT
        c = vec4(i, i, i, 1.0);  // Replicate.
#else
	c = tex_color;
#endif
	gl_FragColor = c * mult_color_in + add_color_in;
}
