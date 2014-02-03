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

// Halo particle system shader in screen space. The model objects are billboards in
// world space, to be rendered with blending.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.

#ifdef GL_ES
precision highp float;
#endif
uniform vec3 base_color_in;
uniform float aspect_ratio_in;
uniform float halo_size_in;
varying vec4 screen_position_var;
varying vec4 screen_position_center;

void main() {
        float dist;
	float dx = screen_position_var.x - screen_position_center.x;
	float dy = screen_position_var.y - screen_position_center.y;
//        dist = (dx * dx * aspect_ratio_in * aspect_ratio_in + dy * dy) / (halo_size_in * halo_size_in);
        dist = sqrt(dx * dx * aspect_ratio_in * aspect_ratio_in + dy * dy) / halo_size_in;
        float att;
        att = 1.0;
        if (dist > 0.5)
            att = 1.0 - 0.5 * (dist - 0.5);
        if (dist > 1.0)
            att = 0.75 - 0.25 * (dist - 1.0);
        if (att < 0.0)
            att = 0.0;
	gl_FragColor = vec4(base_color_in, 1.0) * att;
}

