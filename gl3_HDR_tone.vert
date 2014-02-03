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

// HDR tone mapping shader.

#version 330
uniform sampler2DRect average_lum_in;	// The average/max luminance input texture (1x1).
uniform float key_value_in;
attribute vec2 position_in;
varying vec2 texcoords;
varying float average_lum;
varying float white;
varying float key_value;

float log10(float x) {
	return log(x) / log(10.0);
}

void main() {
	const float epsilon = 0.001;
	vec2 tex = texture(average_lum_in, vec2(0.5, 0.5)).rg;
	average_lum = tex.r;
        white = tex.g;
//	white = tex.g * 0.00001 + 1.0;
	if (key_value_in >= 0.0)
		key_value = key_value_in;
	else
		key_value = 1.03 - 2.0 / (2.0 + log10(average_lum + 1.0));
        texcoords = vec2((position_in.x + 1.0) * 0.5, (position_in.y + 1.0) * 0.5);
	gl_Position = vec4(position_in, - 10.0, 1.0);
}

