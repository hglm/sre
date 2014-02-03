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

#version 150
#ifdef GL_ES
precision highp float;
#endif
#ifdef TEXTURE_ARRAY
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
uniform uint string_in[MAX_TEXT_LENGTH / 4];

// Character per row in the font texture must be a power of two.
// These constants can be included as a prologue when compiling the shader.
// #define FONT_TEXTURE_COLUMNS 16u
// #define FONT_TEXTURE_ROWS 16u

// Ideally the number of characters rows and column should be configurable.
#define FONT_CHAR_SIZE vec2(1.0 / float(FONT_TEXTURE_COLUMNS), 1.0 / float(FONT_TEXTURE_ROWS))

void main() {
	// Round down the character position into the text image to get the character index.
	float x_floor = trunc(text_image_position_var.x); 
	// Convert to integer.
	uint char_index = uint(x_floor);
	// Because text_image_position_var is in character units, the relative texcoords
        // coordinates are obtained with a simple subtraction.
	vec2 texcoords_within_char = text_image_position_var - vec2(x_floor, 0.0);
	// We now have the the relative texcoords within the character image normalized to the
	// range [0.0 - 1.0].
        // Get the ASCII character byte from the packed int array.
 	uint ch = (string_in[char_index >> 2u] >> ((char_index & 3u) << 3u)) & uint(0xFF);
	// Calculate the row and column of the character in the font texture.
	uint font_column = ch & (FONT_TEXTURE_COLUMNS - 1u);
	uint font_row = ch / FONT_TEXTURE_COLUMNS;
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
#ifdef TEXTURE_ARRAY
#ifdef ONE_COMPONENT
        float i = texture(texture_in, vec3(texcoords, array_in)).x;
#else
	vec4 tex_color = texture(texture_in, vec3(texcoords, array_in));
#endif
#else
#ifdef ONE_COMPONENT
        float i = texture(texture_in, texcoords).x;
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
