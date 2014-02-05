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

// Versatile per-pixel Phong shader supporting constant color, multi-color,
// textures, normal maps and specular maps. This version is for multi-pass rendering
// techniques such as stencil shadows. By design it only applies one light.
//
// It has been written to be compatible with both OpenGL 2.0+ and OpenGL ES 2.0.

#ifdef GL_ES
// On archtectures that support high precision floats in the fragment shader,
// colors might be more accurate when the default float precision is high.
// precision highp float;
precision mediump float;
#define MEDIUMP mediump
#define LOWP lowp
#else
#define MEDIUMP
#define LOWP
#endif
#ifdef SHADOW_CUBE_MAP
#version 400
#endif
#ifdef TEXTURE_OPTION
uniform bool use_texture_in;
#endif
#ifdef NORMAL_MAP_OPTION
uniform bool use_normal_map_in;
#endif
#ifdef VIEWPOINT_IN
uniform vec3 viewpoint_in;
#endif
#ifdef AMBIENT_COLOR_IN
uniform vec3 ambient_color_in;
#endif
#ifdef LIGHT_PARAMETERS
uniform vec4 light_position_in;
#ifndef DIRECTIONAL_LIGHT
uniform vec4 light_att_in;
#endif
uniform vec3 light_color_in;
uniform vec3 specular_reflection_color_in;
#ifdef MICROFACET
uniform float diffuse_fraction_in;
uniform vec2 roughness_in;
uniform vec2 roughness_weights_in;
uniform bool anisotropic_in;
#else
uniform float specular_exponent_in;
#endif
#ifdef LINEAR_ATTENUATION_RANGE
uniform vec4 spotlight_in;
#endif
#endif
#ifdef EMISSION_COLOR_IN
uniform vec3 emission_color_in;
#endif
#ifdef SPECULARITY_MAP_OPTION
uniform bool use_specular_map_in;
#endif
#ifdef EMISSION_MAP_OPTION
uniform bool use_emission_map_in;
#endif
#ifdef TEXTURE_SAMPLER
uniform sampler2D texture_in;
#endif
#ifdef NORMAL_MAP_SAMPLER
uniform sampler2D normal_map_in;
#endif
#ifdef SPECULARITY_MAP_SAMPLER
uniform sampler2D specular_map_in;
#endif
#ifdef EMISSION_MAP_SAMPLER
uniform sampler2D emission_map_in;
#endif
#ifdef SHADOW_MAP
#ifdef SPOT_LIGHT_SHADOW_MAP
uniform mat4 shadow_map_transformation_matrix;
#endif
//uniform sampler2DShadow shadow_map_in;
uniform sampler2D shadow_map_in;
#endif
#ifdef SHADOW_CUBE_MAP
uniform sampler2DArray cube_shadow_map_in;
uniform float segment_distance_scaling_in[6];
#endif
#if !defined(MULTI_COLOR_OPTION) && !defined(MULTI_COLOR_FIXED)
uniform vec3 diffuse_reflection_color_in;
#else
varying MEDIUMP vec3 diffuse_reflection_color_var;
#endif
#ifdef NORMAL_VAR
varying vec3 normal_var;
#endif
#ifdef POSITION_WORLD_VAR
varying vec3 position_world_var;
#endif
#ifdef TBN_MATRIX_VAR
varying MEDIUMP mat3 tbn_matrix_var;
#endif
#ifdef TEXCOORD_VAR
varying vec2 texcoord_var;
#endif
#ifdef MICROFACET
varying vec3 tangent_var;
#endif
#ifdef SHADOW_MAP
#ifdef SPOT_LIGHT_SHADOW_MAP
varying vec4 model_position_var;
#else
varying vec3 shadow_map_coord_var;
#endif
#endif

#ifdef MICROFACET

float sqr(float value) {
	return value * value;
}

float GeometricAttenuationFactor(float NdotH, float LdotH, float NdotV, float NdotL) {
	float term1 = 2.0 * NdotH * NdotV / LdotH;
	float term2 = 2.0 * NdotH * NdotL / LdotH;
	return min(min(1.0, term1), term2);
}

float FresnelComponent(float g, float LdotH) {
	float term1 = sqr((g - LdotH) / (g + LdotH));
	float term2 = sqr((LdotH * (g + LdotH) - 1.0) / (LdotH * (g - LdotH) + 1.0));
	return 0.5 * term1 * term2;
}

vec3 Fresnel(float LdotH) {
	vec3 specular_reflection_color;
	vec3 color;
	specular_reflection_color = clamp(specular_reflection_color_in, 0.0, 0.9999);
	float eta_r = (1.0 + sqrt(specular_reflection_color.r)) / (1.0 - sqrt(specular_reflection_color.r));
        color.r = FresnelComponent(sqrt(eta_r * eta_r - 1.0 + LdotH * LdotH), LdotH);
	float eta_g = (1.0 + sqrt(specular_reflection_color.g)) / (1.0 - sqrt(specular_reflection_color.g));
        color.g = FresnelComponent(sqrt(eta_g * eta_g - 1.0 + LdotH * LdotH), LdotH);
	float eta_b = (1.0 + sqrt(specular_reflection_color.b)) / (1.0 - sqrt(specular_reflection_color.b));
        color.b = FresnelComponent(sqrt(eta_b * eta_b - 1.0 + LdotH * LdotH), LdotH);
	return color;
}

float MicrofacetDistributionFunction(float NdotH) {
	NdotH = max(NdotH, 0.00001);
	float term1 = 1.0 / (4.0 * roughness_in.x * roughness_in.x * NdotH * NdotH * NdotH * NdotH);
	float term2 = exp((NdotH * NdotH - 1.0) / (roughness_in.x * roughness_in.x * NdotH * NdotH));
        float roughness1 = roughness_weights_in.x * term1 * term2;
        if (roughness_weights_in.y == 0.0)
		return roughness1;
	term1 = 1.0 / (4.0 * roughness_in.y * roughness_in.y * NdotH * NdotH * NdotH * NdotH);
	term2 = exp((NdotH * NdotH - 1.0) / (roughness_in.y * roughness_in.y * NdotH * NdotH));
        float roughness2 = roughness_weights_in.y * term1 * term2;
       	return roughness1 + roughness2;
}

vec3 MicrofacetTextureValue(float NdotH, float LdotH) {
	return Fresnel(LdotH) * MicrofacetDistributionFunction(NdotH) /  3.1415926535;
}

float AnisotropicMicrofacetDistributionFunction(float NdotH, float TdotP) {
	NdotH = max(NdotH, 0.00001);
	float term1 = 1.0 / (4.0 * roughness_in.x * roughness_in.x * NdotH * NdotH * NdotH * NdotH);
	float term2 = (TdotP * TdotP) / (roughness_in.x * roughness_in.x) + (1.0 - TdotP * TdotP) /
		(roughness_in.y * roughness_in.y);
	float term3 = (NdotH * NdotH - 1.0) / (NdotH * NdotH);
        float roughness1 = term1 * exp(term2 * term3);
	return roughness1;
}

vec3 AnisotropicMicrofacetTextureValue(float NdotH, float LdotH, float TdotP) {
	return Fresnel(LdotH) * AnisotropicMicrofacetDistributionFunction(NdotH, TdotP) / 3.1415926535;
}

#endif

void main() {
        LOWP vec3 diffuse_reflection_color;
	LOWP vec3 diffuse_base_color;
#if defined(MULTI_COLOR_OPTION) || defined(MULTI_COLOR_FIXED)
	diffuse_reflection_color = diffuse_reflection_color_var;
#else
	diffuse_reflection_color = diffuse_reflection_color_in;
#endif

#if defined(TEXTURE_OPTION) || defined(TEXTURE_FIXED)
	LOWP vec4 tex_color = vec4(0, 0, 0, 1.0);
#endif
#ifdef TEXTURE_OPTION
	if (use_texture_in) {
#endif
#if defined(TEXTURE_OPTION) || defined(TEXTURE_FIXED)
		tex_color = texture2D(texture_in, texcoord_var);
		diffuse_base_color = diffuse_reflection_color * tex_color.rgb;
#endif
#ifdef TEXTURE_OPTION
        }
	else
#endif
#ifndef TEXTURE_FIXED
		diffuse_base_color = diffuse_reflection_color;
#endif
	LOWP vec3 c;
#ifdef AMBIENT_COLOR_IN
	// Ambient lighting pass or single pass rendering.

	c = ambient_color_in * diffuse_base_color;
#endif
#if defined(EMISSION_COLOR_IN) && !defined(AMBIENT_COLOR_IN)
	c = vec3(0, 0, 0);
#endif
#ifdef EMISSION_COLOR_IN
#ifdef EMISSION_MAP_OPTION
        vec4 emission_map_color;
	if (use_emission_map_in) {
		emission_map_color = texture2D(emission_map_in, texcoord_var);
		c += emission_color_in * emission_map_color.rgb;
	}
        else
#endif
		c += emission_color_in;
#endif
#ifdef EARTH_SHADER
	// The center of the earth is at (0, 0, 0), so the world position approximates the spherical surface normal.
	vec3 earth_normal = normalize(position_world_var);
	// Cities lights are enabled at a certain atmospheric illumination defined by the angle between the sun and
	// the generalized surface normal.
        float dot_sun_angle = dot(light_position_in.xyz, earth_normal);
        float atmospheric_illumination;
	// Calculate atmospheric illumination between 0 and 1.0
        if (dot_sun_angle > 0.3)
	        atmospheric_illumination = sqrt(dot_sun_angle);
	else
	if (dot_sun_angle >= 0.0)
		atmospheric_illumination = 0.2 + dot_sun_angle * (sqrt(0.3) - 0.2) / 0.3;
	else
	if (dot_sun_angle >= - 0.2)
		// Illumination between 0 and 0.2 when the sun is below the horizon.
		atmospheric_illumination = pow(dot_sun_angle + 0.2, 2.0) * 0.2 / 0.04;
	else
		atmospheric_illumination = 0.0;
	// Scale to 0 and 0.4 (real lighting factor).
	atmospheric_illumination *= 0.4;
	if (atmospheric_illumination < 0.08)
		c = texture2D(emission_map_in, texcoord_var).xyz;
        else
		c = vec3(0, 0, 0);
#endif

#ifndef NO_SMOOTH_SHADING
	// Non-ambient lighting pass, or single pass rendering.

	// Calculate the inverse view direction
        vec3 V = normalize(viewpoint_in - position_world_var);
	// Calculate light direction of point source light or directional light
	vec3 L;
#ifdef DIRECTIONAL_LIGHT
        L = light_position_in.xyz;
#else
	// Calculated normalized negative light direction (for all kinds of lights, even
        // directional).
	L = normalize(light_position_in.xyz - light_position_in.w * position_world_var);
#endif
	// Calculate the half vector.
	vec3 H = normalize(V + L);

	vec3 normal;
	// Save normalized light direction in case it is needed later, before light vectors
	// are converted to tangent space when a normal map is used.
        vec3 V_orig = V;
	vec3 L_orig = L;
#ifdef NORMAL_MAP_OPTION
	if (use_normal_map_in) {
#endif
#if defined(NORMAL_MAP_OPTION) || defined(NORMAL_MAP_FIXED)
		// Convert vectors for the light calculation into tangent space.
		L = normalize(tbn_matrix_var * L);
		H = normalize(tbn_matrix_var * H);
#ifdef MICROFACET
		V = normalize(tbn_matrix_var * V);
#endif
		// Lookup normal from normal map, move range from [0,1] to  [-1, 1], normalize.
		normal = normalize(2.0 * texture2D(normal_map_in, texcoord_var).rgb - 1.0);
		// Light calculations will be performed in tangent space.
#endif
#ifdef NORMAL_MAP_OPTION
	}
        else
#endif
#ifndef NORMAL_MAP_FIXED
        // Normalize the normal vector.
		normal = normalize(normal_var);
#endif

	// Calculate light attenuation.
	LOWP float light_att = 1.0;
#ifndef DIRECTIONAL_LIGHT
#ifndef POINT_SOURCE_LIGHT
	if (light_position_in.w > 0.5) {
#endif
        	float dist = distance(position_world_var, light_position_in.xyz);
#ifdef LINEAR_ATTENUATION_RANGE
		light_att = clamp((light_att_in.x - dist) / light_att_in.x, 0.0, 1.0);
		if (light_att_in.y > 0.0) {
			// Spot or beam light with a linear attenuation range.
			if (light_att_in.y > 1.5) {
				// Beam light.
				// Construct the plane going through the light position perpendicular to the light
				// direction.
				vec4 plane = vec4(spotlight_in.xyz, - dot(spotlight_in.xyz, light_position_in.xyz));
				// Calculate the distance of the world position to the plane.
				float d = dot(plane, vec4(position_world_var, 1.0));
				if (d < 0.0 || d >= light_att_in.z)
					light_att = 0.0;
				else {
					// Calculate the distance of the world position to the axis.
					float dot_proj = dot(position_world_var - light_position_in.xyz,
						spotlight_in.xyz);
					float d_axis = sqrt(dist * dist - dot_proj * dot_proj);
                                        // Apply the radial linear attenuation range.
					light_att *= clamp((light_att_in.w - d_axis) / light_att_in.w, 0.0, 1.0);
                                        // Apply the radial cut-off distance.
					if (d_axis >= spotlight_in.w)
						light_att = 0.0;
				}
			}
			else
				// Spot light.
				light_att *= pow(max(- dot(spotlight_in.xyz, L_orig), 0.0), spotlight_in.w);
                }
#else
		light_att = 1.0 / (light_att_in.x + light_att_in.y * dist +
			light_att_in.z * dist * dist);
#endif
#ifndef POINT_SOURCE_LIGHT
	}
#endif
#endif

#if !defined(SINGLE_PASS) && !defined(EARTH_SHADER)
	c = vec3(0.0, 0.0, 0.0);
#endif

// #define PRECALCULATE_BIAS

#ifdef SHADOW_MAP
	const vec2 poisson_disk[4] = vec2[](
  		vec2(-0.94201624, -0.39906216),
		vec2(0.94558609, -0.76890725),
		vec2(-0.094184101, -0.92938870),
		vec2(0.34495938, 0.29387760)
	);
#endif
#ifdef SHADOW_MAP
#ifdef PRECALCULATE_BIAS
	// Precalculating the shadow map bias, even when the fragment is out of bounds,
        // may not be necessary, but doing so may help GPU pipelining.
	float bias = 0.01 * tan(acos(clamp(dot(normal, L), 0.0001, 1.0)));
        bias = clamp(bias, 0.002, 0.005);
#endif
        // The following variable holds the amount of shadow received by the fragment ([0, 1.0]).
        float shadow_light_factor = 0.0;
	bool out_of_bounds = false;
	vec4 shadow_map_coord;
	float poisson_factor;
#ifdef SPOT_LIGHT_SHADOW_MAP
	// For spot lights, which use a projection matrix, we have to compute the shadow map
        // transformation for each fragment to handle discontinuities in the coordinate space.
	// For beam lights, we do that as well but it is not actually necessary.
	shadow_map_coord = shadow_map_transformation_matrix * model_position_var;
	poisson_factor = 1.0 / 350.0;
	if (light_att_in.y > 1.5) {
		// Beam light. The shadow map coordinates already include the viewport transformation.
		if (shadow_map_coord.z >= 1.0) {
			// The fragment is beyond the far plane of the shadow map matrix, but we
                        // can determine whether is in shadow based on the value in the shadow map.
			if (texture2D(shadow_map_in, shadow_map_coord.xy) == 1.0)
				shadow_light_factor = 1.0;
			else
				// There is something blocking the light in this direction.
				shadow_light_factor = 0.0;
			out_of_bounds = true;
		}
		else
	        if (shadow_map_coord.z < 0.0 || shadow_map_coord.x < 0.0 ||
		shadow_map_coord.x > 1.0 || shadow_map_coord.y < 0.0 || shadow_map_coord.y > 1.0) {
			shadow_light_factor = 1.0;
			out_of_bounds = true;
		}
	}
	else
	// Spot light.
	// The near plane of the spotlight shadow map projection is at 0.001.
        if (shadow_map_coord.w >= 0.001) {
#ifdef PRECALCULATE_BIAS
		bias *= 0.02;
#endif
		shadow_map_coord.x = 0.5 * shadow_map_coord.x / shadow_map_coord.w + 0.5;
		shadow_map_coord.y = 0.5 * shadow_map_coord.y / shadow_map_coord.w + 0.5;
		shadow_map_coord.z = 0.5 * shadow_map_coord.z / shadow_map_coord.w + 0.5;
		if (shadow_map_coord.z >= 1.0) {
			// The fragment is beyond the far plane of the projection matrix, but we can determine whether
			// it is in shadow based on the value in the shadow map.
			if (texture2D(shadow_map_in, shadow_map_coord.xy) == 1.0)
				shadow_light_factor = 1.0;
			else
				// There is something blocking the light in this direction.
				shadow_light_factor = 0.0;
			out_of_bounds = true;
		}
		else
	        if (shadow_map_coord.z < 0.0 || shadow_map_coord.x < 0.0 ||
		shadow_map_coord.x > 1.0 || shadow_map_coord.y < 0.0 || shadow_map_coord.y > 1.0) {
			shadow_light_factor = 1.0;
			out_of_bounds = true;
		}
	}
	else {
		// On the opposite side of the spotlight, there is no light.
		shadow_light_factor = 0.0;
		out_of_bounds = true;
	}
#else	// Directional light.
        shadow_map_coord.xyz = shadow_map_coord_var;
	poisson_factor = 1.0 / 1400.0;
//	poisson_factor = 0;
        if (shadow_map_coord.z < 0.0 || shadow_map_coord.z > 1.0 || shadow_map_coord.x < 0.0 ||
	shadow_map_coord.x > 1.0 || shadow_map_coord.y < 0.0 || shadow_map_coord.y > 1.0) {
		shadow_light_factor = 1.0;
		out_of_bounds = true;
	}
#endif
	if (!out_of_bounds) {
#ifndef PRECALCULATE_BIAS
		float bias = 0.01 * tan(acos(clamp(dot(normal, L), 0.0001, 1.0)));
        	bias = clamp(bias, 0.002, 0.005);
#ifdef SPOT_LIGHT_SHADOW_MAP
		bias *= 0.02;
#endif
#endif
		// Produce slightly soft shadows with the Poisson disk.
		for (int i = 0; i < 4; i++)
			if (texture2D(shadow_map_in, shadow_map_coord.xy +
			poisson_disk[i] * poisson_factor).z > shadow_map_coord.z - bias)
				shadow_light_factor += 0.25;
	}
#ifndef EARTH_SHADER
	// For the earth shader, there is atmospheric illumination, so don't discard the fragment.
	if (shadow_light_factor == 0.0)
		discard;
#endif
        light_att *= shadow_light_factor;
#endif

#ifdef SHADOW_CUBE_MAP
	const vec3 face_axis[6] = vec3[](
		vec3(1.0, 0.0, 0.0),
		vec3(- 1.0, 0.0, 0.0),
		vec3(0.0, 1.0, 0.0),
		vec3(0.0, - 1.0, 0.0),
		vec3(0.0, 0.0, 1.0),
		vec3(0.0, 0.0, -1.0)
	);
	// Note: It should be possible to optimize this by reducing the amount
        // of conditionals.
	int face;
        float max_dot = - 1.0;
	vec3 stp = position_world_var.xyz - light_position_in.xyz;
	for (int i = 0; i < 6; i++) {
		float v = dot(face_axis[i], stp);
		if (v > max_dot) {
			max_dot = v;
			face = i;
		}
	}
	if (segment_distance_scaling_in[face] >= 0.0) {
		// Segment is not empty.
		float s, t;
		switch (face) {
		case 0 :
			s = 0.5 - 0.5 * stp.z / stp.x;
			t = 0.5 - 0.5 * stp.y / stp.x;
			break;
		case 1 :
			s = 0.5 - 0.5 * stp.z / stp.x;
			t = 0.5 + 0.5 * stp.y / stp.x;
			break;
		case 2 :
			s = 0.5 + 0.5 * stp.x / stp.y;
			t = 0.5 + 0.5 * stp.z / stp.y;
			break;
		case 3 :
			s = 0.5 - 0.5 * stp.x / stp.y;
			t = 0.5 + 0.5 * stp.z / stp.y;
			break;
		case 4 :
			s = 0.5 + 0.5 * stp.x / stp.z;
			t = 0.5 - 0.5 * stp.y / stp.z;
			break;
		case 5 :
			s = 0.5 + 0.5 * stp.x / stp.z;
			t = 0.5 + 0.5 * stp.y / stp.z;
			break;
		}
		float d = distance(position_world_var.xyz, light_position_in.xyz) * segment_distance_scaling_in[face];
		// If d > 1.0, the fragment is within the light volume but outside the shadow caster volume for the light.
		if (d <= 1.0) {
			float bias = 0.001 * tan(acos(clamp(dot(normal, L), 0.0, 1.0)));
		        bias = clamp(bias, 0.0, 0.002);
			float z = texture(cube_shadow_map_in, vec3(s, t, face)).z;
        	        // The distances are scaled to [0, 1.0].
			if (z < d - bias)
				discard;
		}
	}
#endif

	LOWP vec3 light_color = light_color_in;
#ifdef EARTH_SHADER
	// Make the light color yellow/red when the sun sets.
	// Assumes the components of the base sun light color are equal (white).
	if (atmospheric_illumination >= 0.06) {
		if (atmospheric_illumination < 0.10) {
			// Yellow to red.
			float component = (abs(0.08 - atmospheric_illumination) / 0.02) * 0.8 + 0.2;
			light_color = vec3(light_color.r, light_color.g * component, light_color.b * 0.2);
		}
		else
		if (atmospheric_illumination < 0.15) {
			// White to yellow.
			float component = 1.0 - ((0.15 - atmospheric_illumination) / 0.05) * 0.8;
			light_color = vec3(light_color.r, light_color.g, light_color.b * component);
		}
	}
	c += light_color_in * diffuse_base_color * atmospheric_illumination;
#endif
	LOWP float NdotL = dot(normal, L);
#ifdef EARTH_SHADER
	// If the sun is below the horizon, avoid diffuse and specular reflection.
	if (atmospheric_illumination >= 0.08 && NdotL > 0.0) {
		// Atmospheric illumination is a value between 0.08 and 0.4.
		// When the sun is lower above the horizon, diffuse and specular reflection decrease.
		light_att *= (atmospheric_illumination - 0.08) / 0.32;
#else
	if (NdotL > 0.0) {
#endif
#ifdef MICROFACET
		c += diffuse_fraction_in * light_att * light_color * diffuse_base_color * NdotL;
#else
		c += light_att * light_color * diffuse_base_color * NdotL;
#endif
		LOWP float NdotH = max(dot(normal, H), 0.0);
		LOWP vec3 specular_map_color;
#ifdef SPECULARITY_MAP_FIXED
		specular_map_color = texture2D(specular_map_in, texcoord_var).rgb;
#else
#ifdef SPECULARITY_MAP_OPTION
		if (use_specular_map_in)
			specular_map_color = texture2D(specular_map_in, texcoord_var).rgb;
		else
#endif
			specular_map_color = vec3(1.0, 1.0, 1.0);
#endif
		LOWP vec3 specular_component;
#ifdef MICROFACET
		float NdotV = max(dot(normal, V), 0.00001);
		float LdotH = max(dot(L, H), 0.00001);
		vec3 microfacet_texture_value;
		if (anisotropic_in) {
			vec3 P = normalize(H - dot(normal, H) * normal);
			float TdotP = dot(normalize(tangent_var), P);
			microfacet_texture_value = AnisotropicMicrofacetTextureValue(NdotH, LdotH, TdotP);
		}
		else
			microfacet_texture_value = MicrofacetTextureValue(NdotH, LdotH);
		specular_component = (1.0 - diffuse_fraction_in) * microfacet_texture_value *
			GeometricAttenuationFactor(NdotH, LdotH,  dot(normal, V), dot(normal, L)) / NdotV;
#else
		specular_component = specular_reflection_color_in * pow(NdotH, specular_exponent_in);
#endif
		c += light_att * light_color * specular_map_color * specular_component;
	}
#endif

#ifdef ADD_DIFFUSE_TO_EMISSION
	c += diffuse_reflection_color;
#endif

#ifdef SINGLE_PASS
	// In single pass rendering, which is also used as the final pass in multi-pass rendering,
        // allow for transparent textures in the form of emission maps.
#ifdef EMISSION_MAP_ALPHA
#ifdef EMISSION_MAP_OPTION
	if (use_emission_map_in)
#endif
#if defined(EMISSION_MAP_OPTION) || defined(EMISSION_MAP_FIXED)
		gl_FragColor = vec4(c, emission_map_color.a);
#endif
#ifdef EMISSION_MAP_OPTION
	else
#endif
#endif
#ifdef TEXTURE_ALPHA
		gl_FragColor = vec4(c, tex_color.a);
#else
		gl_FragColor = vec4(c, 1.0);
#endif
#else
	// Not single pass.
#ifdef TEXTURE_ALPHA
	// In a multi-pass lighting pass, allow for transparent textures with "punchthrough" (on or off) alpha.
	// This requires the use of glAphaFunc() and glEnable(GL_ALPHA_TEST).
	gl_FragColor = vec4(c, tex_color.a);
#else
        gl_FragColor = vec4(c, 1.0);
#endif
#endif
}

