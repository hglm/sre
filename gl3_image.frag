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

// Image shader.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.
//
// When ONE_COMPONENT is defined, it replicates the first component to the other
// RGB color components (useful for visualizing textures with one-component fragments
// such as depth textures).
//
// When TEXTURE_ARRAY is defined the input texture is a texture array, the array
// index uniform (array_in) must be set.
//
// The final color is derived after multiplying with a color uniform with alpha
// (mult_color_in) and then adding add_color_in.
//
// As an example, to visualize a depth texture with one component, ONE_COMPONENT
// would be defined, which replicated the component to all RGB components; to
// invert the intensity and use yellow as color, multi_color_in would be set to
// (-1.0, -1.0, 0.0, 0.0) and add_color_in would be (1.0, 1.0, 0.0, 1.0), resulting
// in the background (depth of 1.0) being black and yellow intensity ranging up
// to (1.0, 1.0, 0.0) according to depth.

#ifdef GL_ES
precision highp float;
#endif
#ifdef TEXTURE_ARRAY
uniform sampler2DArray texture_in;
uniform int array_in;
#else
uniform sampler2D texture_in;
#endif
uniform vec4 mult_color_in;
uniform vec4 add_color_in;
varying vec2 texcoords_var;

void main() {
#ifdef TEXTURE_ARRAY
#ifdef ONE_COMPONENT
        float i = texture(texture_in, vec3(texcoords_var, array_in)).x;
#else
	vec4 tex_color = texture(texture_in, vec3(texcoords_var, array_in));
#endif
#else
#ifdef ONE_COMPONENT
        float i = texture(texture_in, texcoords_var).x;
#else
	vec4 tex_color = texture2D(texture_in, texcoords_var);
	tex_color.a = 1.0;
#endif
#endif

	vec4 c;
#ifdef ONE_COMPONENT
        c = vec4(i, i, i, 1.0);  // Replicate.
#else
	c = tex_color;
#endif
	gl_FragColor = c * mult_color_in + add_color_in;
}
