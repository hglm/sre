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

// HDR average/maximum luminance calculating shader.
// Takes a texture that is a multiple of 4x4 blocks and calculates the average.

uniform sampler2DRect texture_in;	// The source HDR average/maximum log luminance texture (red/green).
varying vec2 texcoords;
out vec2 average_luminance;

void main() {
	const float epsilon = 0.001;
	float sum = 0.0;
	float max_log_lum = - 10E20;	// Negative infinity.
	vec2 abs_texcoords = texcoords * textureSize(texture_in).x - vec2(0.5, 0.5); 
	vec2 tex;
	tex = texture(texture_in, abs_texcoords + vec2(- 1.5, - 1.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(- 0.5, - 1.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(0.5, - 1.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(1.5, - 1.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
	tex = texture(texture_in, abs_texcoords + vec2(- 1.5, - 0.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(- 0.5, - 0.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(0.5, - 0.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(1.5, - 0.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
	tex = texture(texture_in, abs_texcoords + vec2(- 1.5, 0.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(- 0.5, 0.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(0.5, 0.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(1.5, 0.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
	tex = texture(texture_in, abs_texcoords + vec2(- 1.5, 1.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(- 0.5, 1.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(0.5, 1.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
        tex = texture(texture_in, abs_texcoords + vec2(1.5, 1.5)).rg;
	sum += tex.r;
	max_log_lum = max(max_log_lum, tex.g);
	if (textureSize(texture_in).x == 4) {
		// Adjust by the same epsilon as used when the log luminance value was calculated.
		average_luminance.r = exp(sum / 16.0) - epsilon;
		average_luminance.g = exp(max_log_lum) - epsilon;
//		average_luminance.g = 0.2;
	}
	else {
		average_luminance.r = sum / 16.0;
		average_luminance.g = max_log_lum;
	}
}

