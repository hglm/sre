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

// HDR luminance history comparison shader.

#version 330
uniform sampler2DRect texture_in;	// The source HDR average/maximum/actually used average/actually used maximum
					// luminance history texture (16x1).
uniform int slot_in;			// The luminance history slot where the last average luminance was stored.
out vec2 used_luminance_history;	// The actually used average and maximum luminance.

vec4 fetch_history(int slot) {
	return texture(texture_in, vec2(0.5 + slot, 0.5)).rgba;
}

void main() {
	vec4 last_luminance_history = fetch_history(slot_in);
	vec4 oldest_luminance_history = fetch_history((slot_in + 1) & 15);
	// The r component holds the calculated average luminance of each slot.
	// The g component holds the white value used in the frame corresponding to the slot.
	// The b component holds the actually used average luminance in the frame corresponding to the slot.
	// Move the average luminance used in the previous frame into the direction of the calculated average luminance of
        // 15 frames before, with a speed of 1/16th per frame.
        // Set the actually used average luminance.
        used_luminance_history.r = last_luminance_history.b + (oldest_luminance_history.r - last_luminance_history.b) / 16.0;
	// Move the white value used in the previous frame into the direction of the calculated white value of
	// 15 frames before, with a speed of 1/16th per frame.
	// Set the actually used white value.
	used_luminance_history.g = last_luminance_history.a + (oldest_luminance_history.g - last_luminance_history.a) / 16.0;
	//DEBUG
//	used_luminance_history.r = last_luminance_history.r;
//	used_luminance_history.g = last_luminance_history.g;
}

