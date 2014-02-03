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

// HDR average luminance storage shader.

#version 330
uniform sampler2DRect texture_in;	// The source HDR calculated average/maximum luminance texture (1x1).
uniform sampler2DRect average_lum_in;	// The actually used average/maximum luminance from the previous frame (1x1).
varying vec2 texcoords;
out vec4 luminance_history;	// The 16x1 output texture that stores the calculated average luminance (x),
				// the maximum luminance (y), the average luminance actually used in the previous frame (z),
				// and the maximum average luminance actually used in the previous frame (a).

void main() {
	luminance_history.rg = texture(texture_in, vec2(0.5, 0.5)).rg;
	luminance_history.ba = texture(average_lum_in, vec2(0.5, 0.5)).rg;
}

