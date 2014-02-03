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

// HDR log luminance calculating shader.

#version 330
uniform sampler2DRect texture_in;	// The source HDR framebuffer color texture.
varying vec2 texcoords;
out vec2 log_luminance;

void main() {
	const vec3 lum_factors = vec3(0.2125, 0.7152, 0.0722);
	const float epsilon = 0.001;
	int x_samples = (textureSize(texture_in).x + 255) / 256;
	int y_samples = (textureSize(texture_in).y + 255) / 256;
	// For y_samples == 2, y_sample_offset = - 0.5.
	// For y_samples == 3, y_sample_offset = - 1.0.
	// For y_samples == 4, y_sample_offset = - 1.5.
	// etc.
	float log_lum_sum = 0.0;
	float max_log_luminance = - 10E20;
	float y_sample_offset = - 0.5 * (y_samples - 1);
	vec2 abs_texcoords = texcoords * textureSize(texture_in) - vec2(0.5, 0.5);
	for (int y = 0; y < y_samples; y++) {
		float x_sample_offset = - 0.5 * (x_samples - 1);
		for (int x = 0; x < x_samples; x++) {
			vec3 c = texture(texture_in, abs_texcoords + vec2(x_sample_offset, y_sample_offset)).rgb;
			float log_lum = log(dot(c, lum_factors) + epsilon);
			log_lum_sum += log_lum;
			max_log_luminance = max(max_log_luminance, log_lum);
			x_sample_offset += 1.0;
		}
		y_sample_offset += 1.0;
	}
	// Output average log luminance in the x coordinate.
	log_luminance.r = log_lum_sum / (y_samples * x_samples);
	// Output the maximum log luminance in the y coordinate.
	log_luminance.g = max_log_luminance;
}

