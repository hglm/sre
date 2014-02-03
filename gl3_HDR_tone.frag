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
uniform sampler2DRect texture_in;	// The source HDR framebuffer color texture.
varying vec2 texcoords;
varying float average_lum;	// The average luminance (constant).
varying float white;		// The maximum luminance in the scene (constant).
varying float key_value;	// The key value luminance (constant).
const float epsilon = 0.001;
out vec4 frag_color;

#if 0
// sRGB conversion not used; the GPU converts automatically when writing into the framebuffer.
float ConvertComponentFromsRGBToLinearRGB(float cs) {
	float cl;
	if (cs <= 0.04045)
		cl = cs / 12.92;
	else
		cl = pow((cs + 0.055) / 1.055, 2.4);
	return cl;
}

vec3 ConvertFromsRGBToxyY(vec3 rgb) {
	vec3 rgb_linear;
	rgb_linear.r = ConvertComponentFromsRGBToLinearRGB(rgb.r);
	rgb_linear.g = ConvertComponentFromsRGBToLinearRGB(rgb.g);
	rgb_linear.b = ConvertComponentFromsRGBToLinearRGB(rgb.b);
	const mat3 m = mat3(
		0.4124, 0.2126, 0.0193,
		0.3576, 0.7152, 0.1192,
		0.1805, 0.0722, 0.9505);
	vec3 XYZ = m * rgb_linear;
	vec3 xyY;
	xyY.r = XYZ.r / (XYZ.r + XYZ.g + XYZ.b);
	xyY.y = XYZ.g / (XYZ.r + XYZ.g + XYZ.b);
	xyY.b = XYZ.g;
	return xyY;
}

float ConvertComponentFromLinearRGBTosRGB(float cl) {
	float cs;
	if (cl < 0.0031308) {
		cs = 12.92 * cl;
	} else {
		cs = 1.055 * pow(cl, 0.41666) - 0.055;
	}
	return cs;
}

vec3 ConvertFromxyYTosRGB(vec3 xyY) {
	const mat3 m = mat3(
		3.2406, -0.9689, 0.0557,
		-1.5372, 1.8758, -0.2040,
		-0.4986, 0.0415, 1.057);
	vec3 XYZ;
	XYZ.r = xyY.b * xyY.r / xyY.g;
	XYZ.g = xyY.b;
	XYZ.b = xyY.b * (1.0 - xyY.r - xyY.g) / xyY.g;
	vec3 rgb_linear = m * XYZ;
	vec3 srgb;
	srgb.r = ConvertComponentFromLinearRGBTosRGB(rgb_linear.r);
	srgb.g = ConvertComponentFromLinearRGBTosRGB(rgb_linear.g);
	srgb.b = ConvertComponentFromLinearRGBTosRGB(rgb_linear.b);
	return srgb;
}
#endif

vec3 ConvertFromLinearRGBToxyY(vec3 rgb_linear) {
	const mat3 m = mat3(
		0.4124, 0.2126, 0.0193,
		0.3576, 0.7152, 0.1192,
		0.1805, 0.0722, 0.9505);
	vec3 XYZ = m * rgb_linear;
	vec3 xyY;
	xyY.r = XYZ.r / (XYZ.r + XYZ.g + XYZ.b);
	xyY.y = XYZ.g / (XYZ.r + XYZ.g + XYZ.b);
	xyY.b = XYZ.g;
	return xyY;
}

vec3 ConvertFromxyYToLinearRGB(vec3 xyY) {
	const mat3 m = mat3(
		3.2406, -0.9689, 0.0557,
		-1.5372, 1.8758, -0.2040,
		-0.4986, 0.0415, 1.057);
	vec3 XYZ;
	XYZ.r = xyY.b * xyY.r / xyY.g;
	XYZ.g = xyY.b;
	XYZ.b = xyY.b * (1.0 - xyY.r - xyY.g) / xyY.g;
	vec3 rgb_linear = m * XYZ;
	return rgb_linear;
}

vec3 ToneMap(vec3 hdr_color) {
	vec3 xyY = ConvertFromLinearRGBToxyY(hdr_color);
#ifdef TONE_MAP_REINHARD
	float Lw = xyY.b;
	float L = key_value * Lw / (average_lum + epsilon);
	float Ld = L  * (1.0 + L / (white * white)) / (1.0 + L);
	xyY.b *= Ld;
#endif
#ifdef TONE_MAP_LINEAR
	// Scale such that the pixel with the maximum luminance is mapped to 1.0.
	xyY.b = xyY.b / white;
#endif
#ifdef TONE_MAP_EXPONENTIAL
	// Scale such that the pixel with the maximum luminance is mapped to 1.0.
	xyY.b = pow(xyY.b, 2.0) / pow(white, 2.0);
#endif
	return ConvertFromxyYToLinearRGB(xyY);
}

void main() {
	vec3 c = ToneMap(texture(texture_in, texcoords * vec2(textureSize(texture_in))).rgb);
	frag_color = vec4(c, 1.0);
}

