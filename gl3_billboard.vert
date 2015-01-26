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

// Particle system shader in screen space. The model objects are billboards in
// world space, to be rendered with blending.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.

uniform mat4 view_projection_matrix;
attribute vec4 position_in;	// Billboard vertices in world space.
#ifdef HALO
attribute vec3 normal_in;	// Billboard center in world space for halo.
// Halo size is in the range [0, 1] where 1.0 means it just fades
// to black precisely at the bottom/top of the billboard.
uniform float halo_size_in;
uniform float aspect_ratio_in;
varying vec2 screen_position_var;
// Screen space center position of the halo; the z coordinate is the
// distance in world space.
varying vec2 screen_position_center;
varying float scale_factor;
#else
attribute vec2 texcoord_in;
uniform bool use_emission_map_in;
// 3D transformation matrix to apply to the texcoords.
uniform mat3 uv_transform_in;
varying vec2 texcoord_var;
#endif

void main() {
	vec4 position = view_projection_matrix * vec4(position_in.xyz, 1.0);
#ifdef HALO
        vec4 projected_center = (view_projection_matrix * vec4(normal_in, 1.0));
	screen_position_center = projected_center.xy / projected_center.w;
	screen_position_var = position.xy / projected_center.w;
	// Rather than straight normalized device coordinates (which usually
        // do not have a square aspect ratio), scale the x coordinate so that
        // a equidistant coordinate system is obtained.
	screen_position_center.x *= aspect_ratio_in;
	screen_position_var.x *= aspect_ratio_in;
        // Calculate the distance in normalized device coordinates space between
        // the billboard center and the billboard bottom (or top).
	float dist = abs(screen_position_var.y - screen_position_center.y);
        // Precalculate a scale factor for the screen space distance so that
        // it reflects the halo size. A halo size of 1.0 means the halo edge
        // reaches to the bottom of the billboard. There is not much reason
        // for a value other than 1.0. The vertical distance from the center
        // of the billboard to the bottom or top is also applied to the scale
        // factor.
	scale_factor = 1.0 / (dist * halo_size_in);
#else
	// Default texcoords are in range [0, 1.0].
        // With the transformation matrix, a smaller part of the texture can be selected
        // (zoomed) for use with the object, and other effects are possible as well (including
        // mirroring the x or y-axis).
        texcoord_var = (uv_transform_in * vec3(texcoord_in, 1.0)).xy;
#endif
	gl_Position = position;
}

