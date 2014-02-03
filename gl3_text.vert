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

// Text shader.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.
//
// position is between (0.0) (top-left corner of the screen) and (1.0, 1.0)
// (bottom-right corner).

attribute vec2 position_in;	// In screen space.
attribute vec2 texcoord_in;	// Offsets into character set texture.
varying vec2 texcoord_var;

void main() {
        texcoord_var = texcoord_in;
	// Map position from [0, 1] x [0, 1] to [- 1, 1] x [- 1, 1] and
	// invert the y coordinate.
	vec2 position_clip_space = position_in * vec2(2.0, - 2.0) + vec2(- 1.0, 1.0);
	gl_Position = vec4(position_clip_space, 0.0, 1.0);
}

