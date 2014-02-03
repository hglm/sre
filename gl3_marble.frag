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

#version 330 core
uniform vec4 base_color_in;
uniform bool multi_color_in;
uniform bool use_texture_in;
uniform bool use_normal_map_in;
uniform vec3 viewpoint_in;
uniform float ambient_in;
uniform int nu_lights_in;
uniform vec4 light_in[8];
uniform vec3 light_att_in[8];
uniform vec3 light_color_in[8];
uniform float scale_in;
uniform int texture3d_type_in;
uniform sampler2D normal_map_in;

in vec3 normal_var;
in vec3 position_world_var;
in vec3 position_object_var;
in vec3 base_color_var;
in mat3 tbn_matrix_var;
smooth in vec2 texcoord_var;
out vec4 frag_color;

vec4 GetMarbleColor();
vec4 GetGraniteColor();
vec4 GetWoodColor();

void main(){
	const vec3 specular_color = vec3(1.0, 1.0, 1.0);
	const float shininess = 60.0;
	const float diffuse_in = 1.0;
        const float specular_in = 0.8;

	// Calculate the inverse view direction
        vec3 V = normalize(viewpoint_in - position_world_var);

	vec3 normal = normalize(normal_var);

        vec3 base_color;
        switch (texture3d_type_in) {
        case 0 : base_color = base_color_var * GetMarbleColor().rgb; break;
        case 1 : base_color = base_color_var * GetGraniteColor().rgb; break;
        case 2 : base_color = GetWoodColor().rgb; break;
        }
	vec3 c = ambient_in * base_color;
	for (int i = 0; i < nu_lights_in; i++) {
		// Calculate light direction of the light.
		vec3 L = normalize(light_in[i].xyz - light_in[i].w * position_world_var.xyz);
		// Calculate the half vector.
		vec3 H = normalize(V + L);

		if (use_normal_map_in) {
			// Convert vectors for the light calculation into tangent space.
			L = normalize(tbn_matrix_var * L);
			H = normalize(tbn_matrix_var * H);
			// Lookup normal from normal map, move range from [0,1] to  [-1, 1], normalize.
			normal = normalize(2.0 * texture2D(normal_map_in, texcoord_var).rgb - 1.0);
			// Light calculations will be performed in tangent space.
		}

		// Calculate light attenuation
                float light_att = 1.0;
                if (light_in[i].w == 1) {
	        	float dist = distance(position_world_var.xyz, light_in[i].xyz);
			light_att = 1 / (light_att_in[i].x + light_att_in[i].y * dist +
				light_att_in[i].z * dist * dist);
		}

		float NdotL = dot(normal, L);
	        if (NdotL > 0.0) {
	 		NdotL = 0.2 + 0.8 * NdotL;
			c += light_att * light_color_in[i] * diffuse_in * NdotL * base_color;
			float NdotHV = max(dot(normal, H), 0.0);
			c += light_att * light_color_in[i] * specular_in * specular_color * pow(NdotHV, shininess);
		}
	}

	frag_color = vec4(c, 1.0);
}

float cnoise(vec3);

// Granite implementation

// This function takes a value between -1 and 1, and moves it to between 0 and 1
float unsign(float x) {
	return x * 0.5 + 0.5;
}

vec4 GetGraniteColor() {
	vec3 rp = position_object_var.xyz * scale_in * 20.0;
	float intensity = min(1.0, (unsign(cnoise(rp)) / 16.0) * 18.0);
	vec4 color = vec4(intensity, intensity, intensity, 1.0);
	return color;
}

// Marble implementation.

#define NNOISE 4

#define PI 3.141592653

#define PALE_BLUE vec4(0.25, 0.25, 0.35, 1.0)
//#define PALE_BLUE vec4(0.90, 0.90, 1.0, 1.0)
#define MEDIUM_BLUE vec4(0.10, 0.10, 0.30, 1.0)
#define DARK_BLUE vec4(0.05, 0.05, 0.26, 1.0)
#define DARKER_BLUE vec4(0.03, 0.03, 0.20, 1.0)

#define PALE_INTENSITY vec4(0.8, 0.8, 0.8, 1.0)
#define MEDIUM_INTENSITY vec4(0.4, 0.4, 0.4, 1.0)
#define DARK_INTENSITY vec4(0.2, 0.2, 0.2, 1.0)
#define DARKER_INTENSITY vec4(0.1, 0.1, 0.1, 1.0)

vec4 marble_color(float);
vec4 spline(float x, int y, vec4 z[25]);

vec4 GetMarbleColor() {
	vec3 rp = position_object_var * scale_in;
	
	// create the grayscale marbling here
	float marble = 0.0;
	float f = 1.0;
	for (int i = 0; i < NNOISE; i++) {
		marble += (cnoise(rp * f)) / f;
		f *= 2.17;
	}
	
	vec4 color;
	color = marble_color(marble);
	
        return color;
}



vec4 marble_color(float m) {
	vec4 c[25];
	
	c[0] = PALE_INTENSITY;
	c[1] = PALE_INTENSITY;
	c[2] = MEDIUM_INTENSITY;
	c[3] = MEDIUM_INTENSITY;
	c[4] = MEDIUM_INTENSITY;
	c[5] = PALE_INTENSITY;
	c[6] = PALE_INTENSITY;
	c[7] = DARK_INTENSITY;
	c[8] = DARK_INTENSITY;
	c[9] = DARKER_INTENSITY;
	c[10] = DARKER_INTENSITY;
	c[11] = PALE_INTENSITY;
	c[12] = DARKER_INTENSITY;
	
	vec4 res = spline(clamp(2.0 * m + 0.75, 0.0, 1.0), 13, c);
	
	return res;
}

// Wood impementation

vec4 GetWoodColor() {
	vec3 LightWood = vec3(0.6, 0.3, 0.1);
	vec3 DarkWood = vec3(0.4, 0.2, 0.07);
	//vec3 LightWood = vec3(0.46, 0.35, 0.19);
	//vec3 DarkWood = vec3(0.29, 0.27, 0.06);
	float RingFreq = 4.0;
	//float RingFreq = 0.30;
	float LightGrains = 1.0;
	float DarkGrains = 0.0;
	float GrainThreshold = 0.8;
	vec3 NoiseScale= vec3(0.5, 0.1, 0.1);
	float Noisiness = 3.0;
	float GrainScale = 17.0;

	vec3 rp = position_object_var * scale_in * NoiseScale;
	
	vec3 noisevec = vec3(cnoise(rp), cnoise(rp + 3.33), cnoise(rp + 7.77)) * Noisiness;
	vec3 location = rp + noisevec;
	
	float dist = sqrt(location.x * location.x + location.z * location.z);
	dist *= RingFreq;
	
	float rf = fract(dist + unsign(noisevec[0])/256.0 + unsign(noisevec[1])/32.0 + unsign(noisevec[2])/16.0) * 2.0;
	//float rf = fract(dist)*2.0;
	if (rf > 1.0) {
		rf = 2.0 - rf;
	}
	
	vec4 color = vec4(mix(LightWood, DarkWood, rf), 1.0);
	
	rf = fract((rp.x + rp.z) * GrainScale +0.5);
	noisevec[2] *= rf;
	
	if( rf < GrainThreshold) {
		color.xyz += LightWood * LightGrains * unsign(noisevec[2]);
	} else {
		color.xyz -= LightWood * DarkGrains * unsign(noisevec[2]);
	}
	return color;
}


//
// GLSL textureless classic 3D noise "cnoise",
// with an RSL-style periodic variant "pnoise".
// Author: Stefan Gustavson (stefan.gustavson@liu.se)
// Version: 2011-10-11
//
// Many thanks to Ian McEwan of Ashima Arts for the
// ideas for permutation and gradient selection.
//
// Copyright (c) 2011 Stefan Gustavson. All rights reserved.
// Distributed under the MIT license. See LICENSE file.
// https://github.com/ashima/webgl-noise
//

vec3 mod289(vec3 x)
{
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x)
{
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x)
{
  return mod289(((x*34.0)+1.0)*x);
}

vec4 taylorInvSqrt(vec4 r)
{
  return 1.79284291400159 - 0.85373472095314 * r;
}

vec3 fade(vec3 t) {
  return t*t*t*(t*(t*6.0-15.0)+10.0);
}

// Classic Perlin noise
float cnoise(vec3 P)
{
  vec3 Pi0 = floor(P); // Integer part for indexing
  vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
  Pi0 = mod289(Pi0);
  Pi1 = mod289(Pi1);
  vec3 Pf0 = fract(P); // Fractional part for interpolation
  vec3 Pf1 = Pf0 - vec3(1.0); // Fractional part - 1.0
  vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
  vec4 iy = vec4(Pi0.yy, Pi1.yy);
  vec4 iz0 = Pi0.zzzz;
  vec4 iz1 = Pi1.zzzz;

  vec4 ixy = permute(permute(ix) + iy);
  vec4 ixy0 = permute(ixy + iz0);
  vec4 ixy1 = permute(ixy + iz1);

  vec4 gx0 = ixy0 * (1.0 / 7.0);
  vec4 gy0 = fract(floor(gx0) * (1.0 / 7.0)) - 0.5;
  gx0 = fract(gx0);
  vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
  vec4 sz0 = step(gz0, vec4(0.0));
  gx0 -= sz0 * (step(0.0, gx0) - 0.5);
  gy0 -= sz0 * (step(0.0, gy0) - 0.5);

  vec4 gx1 = ixy1 * (1.0 / 7.0);
  vec4 gy1 = fract(floor(gx1) * (1.0 / 7.0)) - 0.5;
  gx1 = fract(gx1);
  vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
  vec4 sz1 = step(gz1, vec4(0.0));
  gx1 -= sz1 * (step(0.0, gx1) - 0.5);
  gy1 -= sz1 * (step(0.0, gy1) - 0.5);

  vec3 g000 = vec3(gx0.x,gy0.x,gz0.x);
  vec3 g100 = vec3(gx0.y,gy0.y,gz0.y);
  vec3 g010 = vec3(gx0.z,gy0.z,gz0.z);
  vec3 g110 = vec3(gx0.w,gy0.w,gz0.w);
  vec3 g001 = vec3(gx1.x,gy1.x,gz1.x);
  vec3 g101 = vec3(gx1.y,gy1.y,gz1.y);
  vec3 g011 = vec3(gx1.z,gy1.z,gz1.z);
  vec3 g111 = vec3(gx1.w,gy1.w,gz1.w);

  vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
  g000 *= norm0.x;
  g010 *= norm0.y;
  g100 *= norm0.z;
  g110 *= norm0.w;
  vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
  g001 *= norm1.x;
  g011 *= norm1.y;
  g101 *= norm1.z;
  g111 *= norm1.w;

  float n000 = dot(g000, Pf0);
  float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
  float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
  float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
  float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
  float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
  float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
  float n111 = dot(g111, Pf1);

  vec3 fade_xyz = fade(Pf0);
  vec4 n_z = mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
  vec2 n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
  float n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x);
  return 2.2 * n_xyz;
}

// Classic Perlin noise, periodic variant
float pnoise(vec3 P, vec3 rep)
{
  vec3 Pi0 = mod(floor(P), rep); // Integer part, modulo period
  vec3 Pi1 = mod(Pi0 + vec3(1.0), rep); // Integer part + 1, mod period
  Pi0 = mod289(Pi0);
  Pi1 = mod289(Pi1);
  vec3 Pf0 = fract(P); // Fractional part for interpolation
  vec3 Pf1 = Pf0 - vec3(1.0); // Fractional part - 1.0
  vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
  vec4 iy = vec4(Pi0.yy, Pi1.yy);
  vec4 iz0 = Pi0.zzzz;
  vec4 iz1 = Pi1.zzzz;

  vec4 ixy = permute(permute(ix) + iy);
  vec4 ixy0 = permute(ixy + iz0);
  vec4 ixy1 = permute(ixy + iz1);

  vec4 gx0 = ixy0 * (1.0 / 7.0);
  vec4 gy0 = fract(floor(gx0) * (1.0 / 7.0)) - 0.5;
  gx0 = fract(gx0);
  vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
  vec4 sz0 = step(gz0, vec4(0.0));
  gx0 -= sz0 * (step(0.0, gx0) - 0.5);
  gy0 -= sz0 * (step(0.0, gy0) - 0.5);

  vec4 gx1 = ixy1 * (1.0 / 7.0);
  vec4 gy1 = fract(floor(gx1) * (1.0 / 7.0)) - 0.5;
  gx1 = fract(gx1);
  vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
  vec4 sz1 = step(gz1, vec4(0.0));
  gx1 -= sz1 * (step(0.0, gx1) - 0.5);
  gy1 -= sz1 * (step(0.0, gy1) - 0.5);

  vec3 g000 = vec3(gx0.x,gy0.x,gz0.x);
  vec3 g100 = vec3(gx0.y,gy0.y,gz0.y);
  vec3 g010 = vec3(gx0.z,gy0.z,gz0.z);
  vec3 g110 = vec3(gx0.w,gy0.w,gz0.w);
  vec3 g001 = vec3(gx1.x,gy1.x,gz1.x);
  vec3 g101 = vec3(gx1.y,gy1.y,gz1.y);
  vec3 g011 = vec3(gx1.z,gy1.z,gz1.z);
  vec3 g111 = vec3(gx1.w,gy1.w,gz1.w);

  vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
  g000 *= norm0.x;
  g010 *= norm0.y;
  g100 *= norm0.z;
  g110 *= norm0.w;
  vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
  g001 *= norm1.x;
  g011 *= norm1.y;
  g101 *= norm1.z;
  g111 *= norm1.w;

  float n000 = dot(g000, Pf0);
  float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
  float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
  float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
  float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
  float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
  float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
  float n111 = dot(g111, Pf1);

  vec3 fade_xyz = fade(Pf0);
  vec4 n_z = mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
  vec2 n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
  float n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x);
  return 2.2 * n_xyz;
}


#define CR00 (-0.5)
#define CR01 (1.5)
#define CR02 (-1.5)
#define CR03 (0.5)
#define CR10 (1.0)
#define CR11 (-2.5)
#define CR12 (2.0)
#define CR13 (-0.5)
#define CR20 (-0.5)
#define CR21 (0.0)
#define CR22 (0.5)
#define CR23 (0.0)
#define CR30 (0.0)
#define CR31 (1.0)
#define CR32 (0.0)
#define CR33 (0.0)

vec4 spline(float x, int nknots, vec4 knots[25]) {
	int nspans = nknots - 3;
	if (nspans < 1) {
		//there must be at least one span
		return vec4(0.0);
	} else if (x < 0.0) {
			return knots[1];
	} else if (x >= 1.0) {
			return knots[nknots-2];
	} else {
		vec4 val0, val1, val2, val3;
		if (x < 1.0/float(nspans)) {
			val0 = knots[0];
			val1 = knots[1];
			val2 = knots[2];
			val3 = knots[3];
		} else if (x < 2.0/float(nspans)) {
			val0 = knots[1];
			val1 = knots[2];
			val2 = knots[3];
			val3 = knots[4];
		} else if (x < 3.0/float(nspans)) {
			val0 = knots[2];
			val1 = knots[3];
			val2 = knots[4];
			val3 = knots[5];
		} else if (x < 4.0/float(nspans)) {
			val0 = knots[3];
			val1 = knots[4];
			val2 = knots[5];
			val3 = knots[6];
		} else if (x < 5.0/float(nspans)) {
			val0 = knots[4];
			val1 = knots[5];
			val2 = knots[6];
			val3 = knots[7];
		} else if (x < 6.0/float(nspans)) {
			val0 = knots[5];
			val1 = knots[6];
			val2 = knots[7];
			val3 = knots[8];
		} else if (x < 7.0/float(nspans)) {
			val0 = knots[6];
			val1 = knots[7];
			val2 = knots[8];
			val3 = knots[9];
		} else if (x < 8.0/float(nspans)) {
			val0 = knots[7];
			val1 = knots[8];
			val2 = knots[9];
			val3 = knots[10];
		} else if (x < 9.0/float(nspans)) {
			val0 = knots[8];
			val1 = knots[9];
			val2 = knots[10];
			val3 = knots[11];
		} else if (x < 10.0/float(nspans)) {
			val0 = knots[9];
			val1 = knots[10];
			val2 = knots[11];
			val3 = knots[12];
		} else if (x < 11.0/float(nspans)) {
			val0 = knots[10];
			val1 = knots[11];
			val2 = knots[12];
			val3 = knots[13];
		} else if (x < 12.0/float(nspans)) {
			val0 = knots[11];
			val1 = knots[12];
			val2 = knots[13];
			val3 = knots[14];
		} else if (x < 13.0/float(nspans)) {
			val0 = knots[12];
			val1 = knots[13];
			val2 = knots[14];
			val3 = knots[15];
		} else if (x < 14.0/float(nspans)) {
			val0 = knots[13];
			val1 = knots[14];
			val2 = knots[15];
			val3 = knots[16];
		} else if (x < 15.0/float(nspans)) {
			val0 = knots[14];
			val1 = knots[15];
			val2 = knots[16];
			val3 = knots[17];
		} else if (x < 16.0/float(nspans)) {
			val0 = knots[15];
			val1 = knots[16];
			val2 = knots[17];
			val3 = knots[18];
		} else if (x < 17.0/float(nspans)) {
			val0 = knots[16];
			val1 = knots[17];
			val2 = knots[18];
			val3 = knots[19];
		} else if (x < 18.0/float(nspans)) {
			val0 = knots[17];
			val1 = knots[18];
			val2 = knots[19];
			val3 = knots[20];
		} else if (x < 19.0/float(nspans)) {
			val0 = knots[18];
			val1 = knots[19];
			val2 = knots[20];
			val3 = knots[21];
		} else if (x < 20.0/float(nspans)) {
			val0 = knots[19];
			val1 = knots[20];
			val2 = knots[21];
			val3 = knots[22];
		} else if (x < 21.0/float(nspans)) {
			val0 = knots[20];
			val1 = knots[21];
			val2 = knots[22];
			val3 = knots[23];
		} else {
			val0 = knots[21];
			val1 = knots[22];
			val2 = knots[23];
			val3 = knots[24];
		}
		
		float y = fract(clamp(x, 0.0, 1.0) * float(nspans));
		
		vec4 c3 = CR00*val0 + CR01*val1 + CR02*val2 + CR03*val3;
		vec4 c2 = CR10*val0 + CR11*val1 + CR12*val2 + CR13*val3;
		vec4 c1 = CR20*val0 + CR21*val1 + CR22*val2 + CR23*val3;
		vec4 c0 = CR30*val0 + CR31*val1 + CR32*val2 + CR33*val3;
		
		//return (val0 + val1 + val2 + val3)/4.0;
		return ((c3*y + c2)*y +c1)*y + c0;
		//return c1;
	}
}

