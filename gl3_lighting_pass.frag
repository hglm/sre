/*

Copyright (c) 2015 Harm Hanemaaijer <fgenfb@yahoo.com>

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
// On architectures that support high precision floats in the fragment shader,
// colors might be more accurate when the default float precision is high.
// precision highp float;
precision mediump float;
#define MEDIUMP mediump
#define LOWP lowp
#else
#define MEDIUMP
#define LOWP
// Set GLSL version to 3.30 for OpenGL, 4.0 when cube maps are enabled.
#ifdef SHADOW_CUBE_MAP
#version 400
#else
#version 330
#endif
#endif
#ifdef TEXTURE_MAP_OPTION
uniform bool use_texture_map_in;
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
#if defined(POINT_SOURCE_LIGHT) || defined(SPOT_LIGHT) || defined(BEAM_LIGHT) || defined(GENERAL_LOCAL_LIGHT)
#define LOCAL_LIGHT
#endif
#ifdef LIGHT_PARAMETERS
uniform float light_parameters_in[NU_LIGHT_PARAMETERS_MAX];
//uniform vec4 light_position_in;
#ifndef DIRECTIONAL_LIGHT
//uniform vec4 light_att_in;
#endif
//uniform vec3 light_color_in;
#if defined(GENERAL_LOCAL_LIGHT) || defined(SPOT_LIGHT) || defined(BEAM_LIGHT)
//uniform vec4 spotlight_in;
#endif
// Object light parameters.
uniform vec3 specular_reflection_color_in;
#ifdef MICROFACET
uniform float diffuse_fraction_in;
uniform vec2 roughness_in;
uniform vec2 roughness_weights_in;
uniform bool anisotropic_in;
#else
uniform float specular_exponent_in;
#endif
#endif // defined(LIGHT_PARAMETERS)
#ifdef EMISSION_COLOR_IN
uniform vec3 emission_color_in;
#endif
#ifdef SPECULARITY_MAP_OPTION
uniform bool use_specular_map_in;
#endif
#ifdef EMISSION_MAP_OPTION
uniform bool use_emission_map_in;
#endif
#ifdef TEXTURE_MAP_SAMPLER
uniform sampler2D texture_map_in;
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
#if defined(SHADOW_MAP) || defined(SPOT_LIGHT_SHADOW_MAP)
#if defined(USE_SHADOW_SAMPLER)
uniform sampler2DShadow shadow_map_in;
#else
uniform sampler2D shadow_map_in;
#endif
#endif
#ifdef SHADOW_CUBE_MAP
#ifdef USE_SHADOW_SAMPLER
uniform samplerCubeShadow cube_shadow_map_in;
#else
uniform samplerCube cube_shadow_map_in;
#endif
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
varying vec3 shadow_map_coord_var;
#endif
#ifdef SPOT_LIGHT_SHADOW_MAP
varying vec4 shadow_map_coord_var;
#endif
#if defined(SHADOW_MAP) || defined(SPOT_LIGHT_SHADOW_MAP)
varying float slope_var;
#endif
#if defined(SHADOW_MAP) || defined(SPOT_LIGHT_SHADOW_MAP) || defined(SHADOW_CUBE_MAP)
uniform float shadow_map_parameters_in[NU_SHADOW_MAP_PARAMETERS_MAX];
#endif
#ifdef NORMAL_MAP_TANGENT_SPACE_VECTORS
varying vec3 V_tangent_var;
varying vec3 L_tangent_var;
#endif

#ifdef GL_ES
#define TEXTURE_CUBE_FUNC textureCube
#define TEXTURE_PROJ_FUNC texture2DProj
#else
// With recent versions of GLSL, textureCube is not allowed.
#define TEXTURE_CUBE_FUNC texture
#define TEXTURE_PROJ_FUNC textureProj
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

#if (defined(SHADOW_MAP) || defined(SPOT_LIGHT_SHADOW_MAP)) && !defined(USE_SHADOW_SAMPLER)

#ifndef GL_ES

float SampleShadowPoisson(sampler2D shadow_map, vec3 coords, float bias, float factor) {
	const vec2 poisson_disk[4] = vec2[](
  		vec2(-0.94201624, -0.39906216),
		vec2(0.94558609, -0.76890725),
		vec2(-0.094184101, -0.92938870),
		vec2(0.34495938, 0.29387760)
		);
	float light_factor = 1.0;
	for (int i = 0; i < 4; i++)
		if (texture2D(shadow_map, coords.xy + poisson_disk[i] * factor).z
		< coords.z + bias)
			light_factor -= 0.25;
	return light_factor;
}

#endif

float SampleShadowSimple(sampler2D shadow_map, vec3 coords, float bias) {
	// When rendering closed models in the shadow map, only back faces are rendered
	// so the bias has to be added to the calculated z coordinates to prevent light
        // "bleeding" from around the back faces.
	//
	// For non-closed models, light-facing triangles are also rendered so the bias
	// has to be negative to prevent incorrect self-shadows on the side facing the light.
	// This should be implemented by already adding a bias to the depth stored
	// in the shadow map when rendering a non-closed model. This bias should be somewhat
        // greater than the bias calculated when drawing the same object in the lighting pass
	// to prevent self-shadowing.
	if (texture2D(shadow_map, coords.xy).z < coords.z + bias)
		return 0.0;
	else
		return 1.0;
}

#endif

#if defined(SHADOW_MAP) && !defined(USE_SHADOW_SAMPLER)

float CalculateShadow() {
	// Scale the Poisson factor according to the resolution of the shadow map.
        // Note: 1.0 / shadow_map_parameters_in[SHADOW_MAP_SIZE] could be added as a uniform.
	float poisson_factor = 1.3 / shadow_map_parameters_in[SHADOW_MAP_SIZE];

        // z / xy in shadow map in real world coordinates at the fragment position, in terms
        // of shadow map u,v and z coordinates (normalized to [0, 1]).
	float shadow_map_world_depth_range;
	float shadow_map_world_width;
//	shadow_map_world_width = d_light_direction;
	// Directional light. The dimensions of the shadow map were precalculated as uniforms.
	shadow_map_world_depth_range = shadow_map_parameters_in[SHADOW_MAP_DIMENSIONS_Z];
	shadow_map_world_width = shadow_map_parameters_in[SHADOW_MAP_DIMENSIONS_X];

	float slope = slope_var;
        // Set bias corresponding to the world space depth difference of adjacent pixels
	// in shadow map.
	float bias = 0.0001 * slope * shadow_map_world_width *
            shadow_map_parameters_in[SHADOW_MAP_SIZE] / 2048.0;
	// Normalize bias from world space coordinates to depth buffer coordinates.
	bias *= 1.0 / shadow_map_world_depth_range;
	// Example bias calculation for directional light, slope = 1.0:
	// ..

	// Add one unit of shadow map depth precision (0.00024 for 16-bit depth buffer).
	bias += shadow_map_parameters_in[SHADOW_MAP_DEPTH_PRECISION];

	// Produce slightly soft shadows with the Poisson disk.
#ifndef GL_ES
	return SampleShadowPoisson(shadow_map_in, shadow_map_coord_var, bias,
		poisson_factor);
#else
	return SampleShadowSimple(shadow_map_in, shadow_map_coord_var, bias);
#endif
}

#endif

#if defined(SPOT_LIGHT_SHADOW_MAP) && !defined(USE_SHADOW_SAMPLER)

float CalculateShadow(vec3 shadow_map_coords, float bias) {
	// Scale the Poisson factor according to the resolution of the shadow map.
	float poisson_factor = 1.3 / shadow_map_parameters_in[SHADOW_MAP_SIZE];

	// Produce slightly soft shadows with the Poisson disk.
#ifndef GL_ES
	return SampleShadowPoisson(shadow_map_in, shadow_map_coords, bias,
		poisson_factor);
#else
	return SampleShadowSimple(shadow_map_in, shadow_map_coords, bias);
#endif
}

#endif

// When USE_REFLECTION_VECTOR is defined, the reflect() function is used for specular
// light calculation instead of calculating the halfway vector. The use of the reflection
// vector produces better, more varied specular highlights.
#if (!defined(GL_ES) || defined(USE_REFLECTION_VECTOR_GLES2)) && !defined(MICROFACET)
#define USE_REFLECTION_VECTOR
#endif

#if !defined(GL_ES) || defined(USE_REFLECTION_VECTOR)
// When using the reflection vector, normalizing all tangent space vectors seems to
// be significantly faster for some reason.
#define NORMALIZE_TANGENT_SPACE_VECTORS
#endif

#if !defined(GL_ES) || defined(USE_REFLECTION_VECTOR)
// When using the halway vector with OpenGL ES 2.0 (with no normalization of light vectors for
// normal mapping), skipping the normalization of the interpolated normal for regular shading
// improves performance, but with noticeably more irregular specular highlights.
#define NORMALIZE_INTERPOLATED_NORMAL
#endif

void main() {
#ifdef LIGHT_PARAMETERS
#if 1
	// Decode the light parameter variables.
	vec4 light_parameters_position;
        light_parameters_position.xyz = vec3(
		light_parameters_in[LIGHT_POSITION_X],
		light_parameters_in[LIGHT_POSITION_Y],
		light_parameters_in[LIGHT_POSITION_Z]
		);
	vec3 light_parameters_color;
        light_parameters_color = vec3(
		light_parameters_in[LIGHT_COLOR_R],
		light_parameters_in[LIGHT_COLOR_G],
		light_parameters_in[LIGHT_COLOR_B]
		);
	// The following light types are possible:
	// DIRECTIONAL_LIGHT
	// POINT_SOURCE_LIGHT
	// SPOT_LIGHT
	// BEAM_LIGHT
	// GENERAL_LOCAL_LIGHT (point, spot or beam)
	// Shaders (such as multi-pass light shader 0) currently also exist that support all
	// kinds of light).
#ifdef DIRECTIONAL_LIGHT
	light_parameters_position.w = 0.0;
#ifdef ENABLE_DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR
	float light_parameters_spill_over_factor =
            light_parameters_in[DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR];
#endif
#else
	light_parameters_position.w = 1.0;
	float light_parameters_linear_attenuation_range =
		light_parameters_in[LIGHT_LINEAR_ATTENUATION_RANGE];
#if defined(SPOT_LIGHT) || defined(BEAM_LIGHT) || defined(GENERAL_LOCAL_LIGHT)
	vec3 light_parameters_axis_direction;
        light_parameters_axis_direction = vec3(
		light_parameters_in[LIGHT_AXIS_DIRECTION_X],
		light_parameters_in[LIGHT_AXIS_DIRECTION_Y],
		light_parameters_in[LIGHT_AXIS_DIRECTION_Z]
		);
#endif
#ifdef SPOT_LIGHT
	float light_parameters_spotlight_exponent = light_parameters_in[SPOT_LIGHT_EXPONENT];
#elif defined(BEAM_LIGHT)
	float light_parameters_beam_axis_cut_off_distance =
		light_parameters_in[BEAM_LIGHT_AXIS_CUT_OFF_DISTANCE];
	float light_parameters_beam_radius = light_parameters_in[BEAM_LIGHT_RADIUS];
	float light_parameters_beam_radial_linear_attenuation_range =
		light_parameters_in[BEAM_LIGHT_RADIAL_LINEAR_ATTENUATION_RANGE];
#elif defined(POINT_SOURCE_LIGHT)
	// No extra parameters for point source light.
#else
	// GENERAL_LOCAL_LIGHT
	// Local light that can be any of three types.
	float light_parameters_type;
	// Type values:
	// 1.0 Point source
	// 2.0 Spot
	// 3.0 Beam
	light_parameters_type = light_parameters_in[LOCAL_LIGHT_TYPE];
	float light_parameters_spotlight_exponent = light_parameters_in[LOCAL_LIGHT_SPOT_EXPONENT];
	float light_parameters_beam_axis_cut_off_distance =
		light_parameters_in[LOCAL_LIGHT_BEAM_AXIS_CUT_OFF_DISTANCE];
	float light_parameters_beam_radius = light_parameters_in[LOCAL_LIGHT_BEAM_RADIUS];
	float light_parameters_beam_radial_linear_attenuation_range =
		light_parameters_in[LOCAL_LIGHT_BEAM_RADIAL_LINEAR_ATTENUATION_RANGE];
#endif
#endif // !defined(DIRECTIONAL_LIGHT)

#else // if !1
	// Set new light parameters from old uniforms for compatibility.
	vec4 light_parameters_position = light_position_in;
	vec3 light_parameters_color = light_color_in;
#ifdef DIRECTIONAL_LIGHT
	light_parameters_position.w = 0.0;
#ifdef ENABLE_DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR
	LOWP light_parameters_spill_over_factor = 0.1;
#endif
#else
	light_parameters_position.w = 1.0;
	float light_parameters_type;
#ifdef POINT_SOURCE_LIGHT
	light_parameters_type = 1.0;
#elif defined(SPOT_LIGHT)
	light_parameters_type = 2.0;
#elif defined(BEAM_LIGHT)
	light_parameters_type = 3.0;
#else
	// GENERAL_LOCAL_LIGHT
	// Local light that can be any of three types.
	// Type values:
	// 1.0 Point source
	// 2.0 Spot
	// 3.0 Beam
	light_parameters_type = float(light_att_in.y > 0.0) + float(light_att_in.y > 1.5);
#endif
	float light_parameters_linear_attenuation_range = light_att_in.x;
#if !defined(POINT_SOURCE_LIGHT)
	vec3 light_parameters_axis_direction = spotlight_in.xyz;
#if !defined(BEAM_LIGHT)
        float light_parameters_spotlight_exponent = spotlight_in.w;
#endif
#if !defined(SPOT_LIGHT)
	float light_parameters_beam_axis_cut_off_distance = light_att_in.z;
	float light_parameters_beam_radius = spotlight_in.w;
	float light_parameters_beam_radial_linear_attenuation_range = light_att_in.w;
#endif
#endif // !defined(POINT_SOURCE_LIGHT)
#endif // !defined(DIRECTIONAL_LIGHT)
#endif
#endif // defined(LIGHT_PARAMETERS)

        LOWP vec3 diffuse_reflection_color;
	LOWP vec3 diffuse_base_color;
#if defined(MULTI_COLOR_OPTION) || defined(MULTI_COLOR_FIXED)
	diffuse_reflection_color = diffuse_reflection_color_var;
#else
	diffuse_reflection_color = diffuse_reflection_color_in;
#endif

#if defined(TEXTURE_MAP_OPTION) || defined(TEXTURE_MAP_FIXED)
	LOWP vec4 tex_color = vec4(0, 0, 0, 1.0);
#endif
#ifdef TEXTURE_MAP_OPTION
	if (use_texture_map_in) {
#endif
#if defined(TEXTURE_MAP_OPTION) || defined(TEXTURE_MAP_FIXED)
		tex_color = texture2D(texture_map_in, texcoord_var);
		diffuse_base_color = diffuse_reflection_color * tex_color.rgb;
#endif
#ifdef TEXTURE_MAP_OPTION
        }
	else
#endif
#ifndef TEXTURE_MAP_FIXED
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
        LOWP vec4 emission_map_color;
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
        float dot_sun_angle = dot(light_parameters_position.xyz, earth_normal);
        float atmospheric_illumination;
	// Calculate atmospheric illumination between 0 and 1.0
        if (dot_sun_angle > 0.3)
		// Atmospheric illumimnation sqrt(dot_sun_angle) when the Sun is higher
		// in the sky (down to dot_sun_angle = 0.3).
	        atmospheric_illumination = sqrt(dot_sun_angle);
	else if (dot_sun_angle >= 0.0)
		// Atmospheric illumination from 0.2 (sunset) to sqrt(0.3) at
		// dot_sun_angle = 0.3.
		atmospheric_illumination = 0.2 + dot_sun_angle * (sqrt(0.3) - 0.2) / 0.3;
	else if (dot_sun_angle >= - 0.2)
		// Illumination between 0 and 0.2 when the sun is below the horizon.
		atmospheric_illumination = dot_sun_angle + 0.2;
	else
		atmospheric_illumination = 0.0;
	// Scale to between 0 and 0.4 (real atmospheric illumination lighting factor).
	// The adjusted one (with sqrt curve) is actually applied; the unadjusted one
	// is used for other checks.
	float adjusted_atmospheric_illumination = sqrt(atmospheric_illumination) * 0.4;
	atmospheric_illumination *= 0.4;
	// Lights go on just before sunset (atmospheric_illumination = 0.08).
	if (atmospheric_illumination < 0.10) {
		// Add the night light emission map. When there is still some light
		// (as determined by the sun angle, ideally it would depend on the
		// terrain and true light intensity), reduce the night light intensity.
		float night_light_factor = 0.4 + (0.10 - atmospheric_illumination) *
			0.6 / 0.10;
		c = texture2D(emission_map_in, texcoord_var).rgb * night_light_factor;
	}
        else
		c = vec3(0, 0, 0);
#endif

#ifndef NO_SMOOTH_SHADING
	// Non-ambient lighting pass, or single pass rendering.

	// Calculate the inverse view direction
        MEDIUMP vec3 V = normalize(viewpoint_in - position_world_var);
	// Calculate light direction of point source light or directional light
	MEDIUMP vec3 L;
#ifdef DIRECTIONAL_LIGHT
	L = light_parameters_position.xyz;
#endif
#if defined(POINT_SOURCE_LIGHT) || defined(SPOT_LIGHT)
	L = normalize(light_parameters_position.xyz - position_world_var);
#endif
#ifdef BEAM_LIGHT
	L = - light_parameters_axis_direction;
#endif
#ifdef USE_REFLECTION_VECTOR
        MEDIUMP vec3 R;
#else	
	MEDIUMP vec3 H;
#endif

	// Declaring and calculating dot products early and seperately within all if/then
	// clauses can improve performance. 
	LOWP float NdotL;
#ifdef USE_REFLECTION_VECTOR
	LOWP float RdotV;
#else
	LOWP float NdotH;
#endif

	// The normal for lighting calculations, affected by normal maps.
	MEDIUMP vec3 normal;
	// The surface (triangle) normal. This is equivalent to the normal_var except when
	// the normal of a double-sided triangle is inverted.
	MEDIUMP vec3 surface_normal;
	// Save normalized light direction in case it is needed later, before light vectors
	// are converted to tangent space when a normal map is used.
        MEDIUMP vec3 V_orig = V;
	MEDIUMP vec3 L_orig = L;
#ifdef NORMAL_MAP_OPTION
	if (use_normal_map_in) {
#endif

#if defined(NORMAL_MAP_OPTION) || defined(NORMAL_MAP_FIXED)
		// When the z component is non-zero, it is assumed to be the z component of
		// the normal vector, and the components are scaled from [0, 1] to [-1, 1].
		// When the z component is zero (which happens with two-component signed
		// or unsigned formats like RGTC2), calculate the component from the x and y
		// components.
		MEDIUMP vec3 n = texture2D(normal_map_in, texcoord_var).rgb;
#ifdef GL_ES
		// Under OpenGL ES 2.0, normal maps have three components so there is no need
		// to calculate z.
		// Note: In the future, OpenGL ES 3.0+ might be supported and support
		// two-component textures using formats like SIGNED_RG11_EAC.
		// Move range from [0,1] to [-1, 1].
		n.xy = n.xy * 2.0 - vec2(1.0, 1.0);
                // Calculating z with accuracy is important for correct highlights.
		// Normalizing n might be an alternative?
		normal = vec3(n.x, n.y, sqrt(1.0 - n.x * n.x - n.y * n.y));;
#else
		if (n.z != 0.0)
			// Move range from [0,1] to  [-1, 1].
			n.xy = n.xy * 2.0 - vec2(1.0, 1.0);
		// Calculate z from x and y based on the fact that the magnitude of
		// a normal vector is 1.0.
		normal = vec3(n.x, n.y, sqrt(1.0 - n.x * n.x - n.y * n.y));
#endif
		// Invert normal for back faces for correct lighting of faces that
                // can be looked at from both sides.
		normal *= float(gl_FrontFacing) * 2.0 - 1.0;

		// Light calculations will be performed in tangent space.

		// Convert vectors for the light calculation into tangent space.
		// Avoid expensive normalization when using OpenGL ES 2.0.
		// For the L vector (towards the light source), proper normalization
		// is virtually required when USE_REFLECTION_VECTOR is defined because
		// otherwise exaggerated highlights appear and performance is worse on
		// OpenGL ES 2.0 (maybe because of the power function involved in the
		// specular light calculation).
#ifdef NORMAL_MAP_TANGENT_SPACE_VECTORS
		L = normalize(L_tangent_var);
		V = normalize(V_tangent_var);
#else
		L = tbn_matrix_var * L;
#if defined(NORMALIZE_TANGENT_SPACE_VECTORS) || defined(USE_REFLECTION_VECTOR)
		L = normalize(L);
#endif
#if defined(MICROFACET) || defined(USE_REFLECTION_VECTOR)
		V = tbn_matrix_var * V;
#ifdef NORMALIZE_TANGENT_SPACE_VECTORS
		V = normalize(V);
#endif
#endif
#endif // !defined(NORMAL_MAP_TANGENT_SPACE_VECTORS)

#ifdef USE_REFLECTION_VECTOR
		// Calculate the tangent space reflection vector.
        	R = - reflect(L, normal);
		// R is already in tangent space
#else
		// Calculate the tangent space halfway vector.
		H = normalize(V + L);
#endif

		NdotL = dot(normal, L);
#ifdef USE_REFLECTION_VECTOR
		RdotV = max(dot(R, V), 0.0);
#else
		NdotH = max(dot(normal, H), 0.0);
#endif
#endif	// defined(NORMAL_MAP_OPTION) || defined(NORMAL_MAP_FIXED)

#ifdef NORMAL_MAP_OPTION
	}
        else
#endif
#ifndef NORMAL_MAP_FIXED
	{
		// Calculate normal, NdotL and NdotV/NdotH (no normal mapping).
		normal = normal_var;
		// Invert normal for back faces for correct lighting of faces that
                // can be looked at from both sides.
		normal *= float(gl_FrontFacing) * 2.0 - 1.0;
#ifdef NORMALIZE_INTERPOLATED_NORMAL
		// Normalize the normal vector.
		normal = normalize(normal);
#endif
#ifdef USE_REFLECTION_VECTOR
		// Calculate the reflection vector.
        	R = - reflect(L, normal);
#else
		// Calculate the halfway vector.
		H = normalize(V + L);
#endif
		NdotL = dot(normal, L);
#ifdef USE_REFLECTION_VECTOR
		RdotV = max(dot(R, V), 0.0);
#else
		NdotH = max(dot(normal, H), 0.0);
#endif

	}
#endif

	// Calculate light attenuation.
	LOWP float light_att = 1.0;
#ifdef DIRECTIONAL_LIGHT
#else
	// Calculate light attenuation of a local light.
	// The code currently assumes that the light is either:
	// - A point source light (POINT_SOURCE_LIGHT is defined).
	// - A spot light (SPOT_LIGHT is defined).
	// - A beam light (BEAM_LIGHT is defined).
	// - A beam or spot light (none of the above is defined).
	// The case of the light being either a point source, spot or beam light is
	// not supported.
	MEDIUMP float d_light_direction;
       	MEDIUMP float dist = distance(position_world_var, light_parameters_position.xyz);
	light_att = clamp((light_parameters_linear_attenuation_range - dist) /
		light_parameters_linear_attenuation_range, 0.0, 1.0);
#ifndef POINT_SOURCE_LIGHT
	// Spot or beam light with a linear attenuation range.
	// Construct the plane going through the light position perpendicular to the light
	// direction.
	MEDIUMP vec4 plane = vec4(light_parameters_axis_direction.xyz, - dot(
		light_parameters_axis_direction.xyz, light_parameters_position.xyz));
	// Calculate the distance of the world position to the plane.
	d_light_direction = dot(plane, vec4(position_world_var, 1.0));
#ifndef SPOT_LIGHT
#ifndef BEAM_LIGHT
	if (light_parameters_type > 2.5) {
#endif
		// Beam light.
		if (d_light_direction < 0.0 || d_light_direction >=
		light_parameters_beam_axis_cut_off_distance)
			light_att = 0.0;
		else {
			// Calculate the distance of the world position to the axis.
			MEDIUMP float dot_proj = dot(position_world_var -
				light_parameters_position.xyz,
				light_parameters_axis_direction);
			MEDIUMP float d_light_direction_axis = sqrt(dist * dist -
				dot_proj * dot_proj);
			// Apply the radial linear attenuation range.
			light_att *= clamp((light_parameters_beam_radial_linear_attenuation_range -
				d_light_direction_axis)	/
				light_parameters_beam_radial_linear_attenuation_range, 0.0, 1.0);
			// Apply the radial cut-off distance.
			if (d_light_direction_axis >= light_parameters_beam_radius)
				light_att = 0.0;
		}
#ifndef BEAM_LIGHT
	}
#endif
#endif	// !defined(SPOT_LIGHT)
#if !defined(BEAM_LIGHT) && !defined(SPOT_LIGHT)
	else
#endif
#if !defined(BEAM_LIGHT)
		// Spot light.
		light_att *= pow(max(- dot(light_parameters_axis_direction.xyz, L_orig), 0.0),
			light_parameters_spotlight_exponent);
#endif
#endif	// !defined(POINT_SOURCE_LIGHT)
	// Unused code for classical light attenuation of a local light.
//	light_att = 1.0 / (light_att_in.x + light_att_in.y * dist +
//		light_att_in.z * dist * dist);
#endif	// !defined(DIRECTIONAL_LIGHT)

	float NdotL_diffuse;
#ifdef ENABLE_DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR
	// Adjust NdotL so that [0, 1.0] maps to [DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR, 1.0]
	// and [-1.0, 0] maps to [0, DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR].
	float negative_NdotL = min(NdotL, 0.0);
	NdotL_diffuse = NdotL * (1.0 - light_parameters_spill_over_factor) +
		light_parameters__spill_over_factor;
	NdotL_diffuse = max(NdotL_diffuse, light_parameters_spill_over_factor);
	NdotL_diffuse += negative_NdotL * light_parameters_spill_over_factor;
#else
	NdotL_diffuse = NdotL;
#endif

#if !defined(SINGLE_PASS) && !defined(EARTH_SHADER)
	c = vec3(0.0, 0.0, 0.0);
#endif

#if defined(SHADOW_MAP) || defined(SPOT_LIGHT_SHADOW_MAP) || defined(SHADOW_CUBE_MAP)
	float shadow_light_factor;
#ifdef SHADOW_MAP
	// Directional or beam light, orthogonal transformation shadow map.
#ifdef USE_SHADOW_SAMPLER
	vec3 coords = shadow_map_coord_var;
        float bias = 0.00005 * slope_var;
        coords.z += bias;
	shadow_light_factor = texture(shadow_map_in, coords);
#else
	bool out_of_shadow_bounds = any(lessThan(shadow_map_coord_var.xyz, vec3(0.0, 0.0, 0.0))) ||
            any(greaterThan(shadow_map_coord_var.xyz, vec3(1.0, 1.0, 1.0)));
        // The following variable holds the amount of shadow received by the fragment ([0, 1.0]).
        shadow_light_factor = 1.0;
	if (!out_of_shadow_bounds)
		shadow_light_factor = CalculateShadow();
#endif
#endif

#ifdef SPOT_LIGHT_SHADOW_MAP
	float bias = 0.00005 * slope_var * shadow_map_parameters_in[SHADOW_MAP_SIZE] / 2048.0;
#ifdef USE_SHADOW_SAMPLER
	vec4 coords = shadow_map_coord_var;
        coords.z += bias * coords.w;
	shadow_light_factor = TEXTURE_PROJ_FUNC(shadow_map_in, coords);
        light_att *= shadow_light_factor;
#else
	// Spotlight shadow map. Only one shadow map is used.
	vec4 P_trans = shadow_map_coord_var;
	vec3 P_proj = P_trans.xyz / P_trans.w;
	bool out_of_spotlight_bounds = any(lessThan(P_proj.xyz, vec3(0.0, 0.0, 0.0))) ||
		any(greaterThan(P_proj.xyz, vec3(1.0, 1.0, 1.0)));
#if 1
	// Spotlight shadow map stores projected z coordinate.
	// If the fragment is outside of the spot light volume, just discard it.
	if (out_of_spotlight_bounds)
		discard;
	shadow_light_factor = CalculateShadow(P_proj, bias);
#else
	// Spotlight shadow map stores scaled distance from the spotlight.
	if (out_of_spotlight_bounds)
		discard;
	vec3 space_vector_from_light = position_world_var - light_parameters_position.xyz;
	float shadow_radial_dist = texture2D(shadow_map_in, P_proj.xy).z;
       // Calculate the radial distance of the world position from the light position,
        // and scale it so that the range [0, 1] corresponds to the values stored in the
        // shadow map segments.
	float radial_dist = length(space_vector_from_light) * segment_distance_scaling_in;
	float bias = 0.001 * slope_var;
        bias = clamp(bias, 0.0005, 0.002);
	if (shadow_radial_dist < radial_dist - bias)
		discard;
#endif
#endif
#endif

#if defined(SHADOW_CUBE_MAP)
	vec3 space_vector_from_light = position_world_var - light_parameters_position.xyz;
#ifdef CUBE_MAP_STORES_DISTANCE
	// Point source light shadow cube map (six sides).
	// Use an optimized method using built-in cube map texture look up functions.

        // Calculate the radial distance of the world position from the light position,
        // and scale it so that the range [0, 1] corresponds to the values stored in the
        // shadow map segments.
	float radial_dist = length(space_vector_from_light) *
		shadow_map_parameters_in[SHADOW_MAP_SEGMENT_DISTANCE_SCALING];
	// For cube map, it is difficult to incorporate the slope of the surface into a
	// bias value calculation.
	const float bias = 0.0005;
#ifdef USE_SHADOW_SAMPLER
	// Use cube shadow texture lookup with texture compare mode.
	shadow_light_factor = TEXTURE_CUBE_FUNC(cube_shadow_map_in,
		vec4(space_vector_from_light, radial_dist - bias));
#else
	// Look up radial distance where shadow begins from the cube map.
	float shadow_radial_dist = TEXTURE_CUBE_FUNC(cube_shadow_map_in, space_vector_from_light).z;
	// Set shadow light factor to 0.0 if shadow, 1.0 if no shadow.
	shadow_light_factor = float(shadow_radial_dist >= radial_dist - bias);
#endif

#else
	// Cube map stores depth values.
	// Need to derive the depth value for comparison from the world coordinates.
	vec3 abs_vec = abs(space_vector_from_light);
        float local_z_comp = max(abs_vec.x, max(abs_vec.y, abs_vec.z));
        // Calculate (f + n) / (f - n) - 2 * f * n / (local_z_comp * (f - n));
	float norm_z_comp = shadow_map_parameters_in[SHADOW_MAP_F_N_COEFFICIENT_1] -
            shadow_map_parameters_in[SHADOW_MAP_F_N_COEFFICIENT_2] / local_z_comp;
	float depth = (norm_z_comp + 1.0) * 0.5;
	const float bias = 0.00005;
	// Since only back faces have been written to the shadow map, the bias value has to be
        // substracted from the depth comparison value.
	depth -= bias;
#ifdef USE_SHADOW_SAMPLER
	shadow_light_factor = TEXTURE_CUBE_FUNC(cube_shadow_map_in, vec4(space_vector_from_light,
		depth));
#else
	float shadow_map_depth = TEXTURE_CUBE_FUNC(cube_shadow_map_in, space_vector_from_light);
	shadow_light_factor = float(shadow_map_depth >= depth);
#endif
#endif	// !defined(CUBE_MAP_STORES_DISTANCE)
#endif	// defined(SHADOW_CUBE_MAP)

	// Apply the shadow light factor.
#ifndef EARTH_SHADER
	// For the earth shader, there is atmospheric illumination, so don't discard the fragment.
	// For all other cases of no-cube shadow map, discard the fragment if it is fully in shadow.
	if (shadow_light_factor == 0.0)
		discard;
#endif
        light_att *= shadow_light_factor;
#endif

	// Special handling for earth shader lighting.
	LOWP vec3 light_color = light_parameters_color;
	// For the sun, light_position_in.w is zero (directional light).
        float light_is_sun = 1.0 - light_parameters_position.w;
#ifdef EARTH_SHADER
	// Make the light color yellow/red when the sun sets.
	// Assumes the components of the base sun light color are equal (white).
	if (light_parameters_position.w == 0.0 && atmospheric_illumination >= 0.06) {
		// Sun is just below the horizon or higher.
		if (atmospheric_illumination < 0.10) {
			// Red (1to white (below the horizon, moving towards faint white light
			// of dusk).
			float component = 0.4 + ((0.08 - atmospheric_illumination) / 0.02) * 0.6;
			light_color = vec3(light_color.r, light_color.g * component,
				light_color.b * component);
		}
		else if (atmospheric_illumination < 0.10) {
			// Yellow to red (setting Sun).
			float component = ((atmospheric_illumination - 0.08) / 0.02) * 0.6 + 0.4;
			light_color = vec3(light_color.r, light_color.g * component, light_color.b * 0.4);
		}
		else if (atmospheric_illumination < 0.15) {
			// White to yellow.
			float component = 1.0 - ((0.15 - atmospheric_illumination) / 0.05) * 0.8;
			light_color = vec3(light_color.r, light_color.g, light_color.b * component);
		}
	}
	// Apply the atmospheric illumination (basically ambient light), for the sun only.
	// The adjusted atmospheric illumination is used (sqrt-like curve).
	c += light_is_sun * light_parameters_color * diffuse_base_color *
		adjusted_atmospheric_illumination;
#endif

#ifdef EARTH_SHADER
	// If the sun is below the horizon, avoid diffuse and specular reflection.
	// Do allow other lights to contribute even on the night side.
	if (atmospheric_illumination >= 0.08 * light_is_sun && NdotL > 0.0) {
		// Atmospheric illumination is a value between 0.08 and 0.4, and roughly
		// corresponds with the height of the Sun above the horizon.
		// When the sun is lower above the horizon, diffuse and specular reflection
		///decrease due to extinction of the direct sunlight.
		// However, do not apply this extinction factor for other
		// lights (light_position_in.w == 1.0).
		float sun_light_factor = (atmospheric_illumination - 0.08) / 0.32;
		// Apply a square root-based correction.
		sun_light_factor = sqrt(sun_light_factor);
		light_att *= max(sun_light_factor, light_parameters_position.w);
#else
	if (NdotL_diffuse > 0.0) {
#endif
#ifdef MICROFACET
		c += diffuse_fraction_in * light_att * light_color * diffuse_base_color *
			NdotL_diffuse;
#else
		c += light_att * light_color * diffuse_base_color * NdotL_diffuse;
#endif
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
#ifdef USE_REFLECTION_VECTOR
		specular_component = specular_reflection_color_in * pow(RdotV, specular_exponent_in);
#else
		specular_component = specular_reflection_color_in * pow(NdotH, specular_exponent_in);
#endif
		specular_component *= step(0.0, NdotL);
#endif
		// The specular color contribution should not depend on the color of the
		// light, but only its intrinsic intensity.
		const vec3 Crgb = vec3(
		    0.212655, // Red factor
		    0.715158, // Green factor
		    0.072187  // Blue factor
		    );
		float light_intensity = dot(light_parameters_color, Crgb);
		c += light_att * light_intensity * specular_map_color * specular_component;
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
#ifdef TEXTURE_MAP_ALPHA
		gl_FragColor = vec4(c, tex_color.a);
#else
		gl_FragColor = vec4(c, 1.0);
#endif
#else
	// Not single pass.
#ifdef TEXTURE_MAP_ALPHA
	// In a multi-pass lighting pass, allow for transparent textures with "punchthrough" (on or off) alpha.
	// This requires the use of glAphaFunc() and glEnable(GL_ALPHA_TEST).
	gl_FragColor = vec4(c, tex_color.a);
#else
        gl_FragColor = vec4(c, 1.0);
#endif
#endif
}

