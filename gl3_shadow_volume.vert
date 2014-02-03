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

// Shadow volume shader for use with stencil shadows.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.

uniform mat4 MVP;
uniform vec4 light_pos_model_space_in;
attribute vec4 position_in;

void main() {
	// Vertices extruded to infinity have w = 0.
//	if (position_in.w == 0.0) {
//		vec4 position_extruded = vec4(
//			light_pos_model_space_in.w * position_in.x - light_pos_model_space_in.x,
//			light_pos_model_space_in.w * position_in.y - light_pos_model_space_in.y,
//			light_pos_model_space_in.w * position_in.z - light_pos_model_space_in.z,
//			0.0);
//		gl_Position = MVP * position_extruded;
//        }
//        else
//		gl_Position = MVP * position_in;
	// Optimized version without branching.
	//
        // Front cap (silhouette) vertices are regular vertices with a w component of 1.0.
        // Any extruded dark cap vertices have a w component (position_in.w) of 0.
        // For directional lights and beam lights, light_pos_model_space_in.w will be 0,
	// otherwise it will be 1.0.
	//
        // As a result, for directional lights all extruded vertices will be equal
        // to (- light_pos_model_space_in) with a w component of 0, which is the direction of the
        // light extruded to infinity.
	//
	// For beam lights, all extruded vertices will be equal to (- light_pos_model_space_in)
	// which is actually equal to the beam light direction extruded to infinity. In the shader
	// set up, light_pos_model_space_in must be set to the negative beam light direction with
	// a w component of 0.
	//
        // For point and spot lights, extruded vertices will be equal to
        // (position_in - light_pos_model_space) with a w component of 0, which is the vector from
        // the light position to the corresponding silhouette vertex extruded to infinity.
        // For pixels beyond the range of the light, the stencil buffer will be marked as shadow
	// when they intersect the extruded shadow volume, but this doesn't affect rendering since
        // these pixels don't receive light anyway. Scissors regions can limit the amount of
	// unneeded stencil buffer writes.
	//
	// The main advantage of extruded vertices is that for the dominant depth-pass shadow
	// volumes, only sides will need to be rendered (which uses only the silhouette vertices).
	// The potentially much larger front or dark cap (which includes all light facing
	// triangles in the object, on average half of all vertices) can be skipped most of
	// the time.

        vec4 position = (light_pos_model_space_in.w * (1.0 - position_in.w) + position_in.w) *
		position_in - (1.0 - position_in.w) * light_pos_model_space_in;
        position.w = position_in.w;
	gl_Position = MVP * position;
}
