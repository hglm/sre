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

// Halo shader in screen space. The model object is a billboard in world space, to be rendered
// with blending.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.

#ifdef GL_ES
// We need to set float precision to highp because otherwise the float
// varying is not linked correctly.
precision highp float;
#define MEDIUMP mediump
#else
#define MEDIUMP
#endif
uniform vec3 base_color_in;
varying vec2 screen_position_var;
varying vec2 screen_position_center;
varying float scale_factor;

// An improvement would be to define a few attenuation parameters for the
// halo, which can be set to configurations similar to a solid circle,
// linear fade or somewhat segmented (good looking) fade.

// #define SOLID_CIRCLE
// #define LINEAR_FADE
#define SEGMENTED_FADE

// At least on a Mali-400, using discard is a little slower.
// #define DISCARD

void main() {
        MEDIUMP float dist;
	MEDIUMP float dx = screen_position_var.x - screen_position_center.x;
	MEDIUMP float dy = screen_position_var.y - screen_position_center.y;
	// Calculate screen distance to center in coordinates normalized to half
        // the screen height.
        dist = sqrt(dx * dx + dy * dy);
        // Apply the precalculated scale factor so that a dist value of 1.0
        // corresponds to the distance from the center to the bottom (or top) of
        // the billboard, additionally corrected for the halo size relative to
        // the billboard.
	dist *= scale_factor;
#ifdef DISCARD
        // Outside the influence of the halo, we can discard the fragment.
        if (dist > 1.0)
            discard;
#endif
        MEDIUMP float att;

#ifdef SOLID_CIRCLE
#ifdef DISCARD
	att = 1.0;
#else
	att = max(sign(1.0 - dist), 0.0);
#endif
#endif

#ifdef LINEAR_FADE
#ifdef DISCARD
	att = 1.0 - dist;
#else
        att = max(1.0 - dist, 0.0);
#endif
#endif

#ifdef SEGMENTED_FADE
	att = 1.0;
        // Linear fade from 1.0 at dist = 0.125 to 0.75 at dist = 0.25
	// First scale and clamp distance within segment to [0, 1],
        // then multiply by the intensity drop within the segment.
	att -= clamp((dist - 0.125) * (1.0 / 0.125), 0.0, 1.0) * 0.25;
	// Linear fade from 0.75 at dist = 0.25 to 0 (black) at dist = 1.0.
	att -= clamp((dist - 0.25) * (1.0 / 0.75), 0.0, 1.0) * 0.75;
#endif

	gl_FragColor = vec4(base_color_in, 1.0) * att;
}
