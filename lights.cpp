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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>

#include "win32_compat.h"
#include "sre.h"
#include "sre_internal.h"
#include "sre_bounds.h"

/*
   Point source lights with a linear attenuation range have the following parameters:
   - Position (stored in vector.xyz)
   - Linear attenuation range (stored in attenuation.x).
   - Color (stored in color)
   Bounding volumes:
   - Sphere
   - AABB (for stationary lights)
   - Worst case sphere (for variable lights)

   Spot lights have the following parameters:
   - Position (stored in vector.xyz)
   - Axis/direction vector (stored in spotlight.xyz)
   - Spotlight exponent (stored in spotlight.w)
   - Linear attenuation range (stored in attenuation.x).
   - Color (stored in color)
   Bounding volumes:
   - Sphere
   - Cylinder
   - SphericalSector
   - AABB (for stationary lights)
   - Worst case sphere (for variable lights)

   Beam lights have the following parameter:
   - Position (stored in vector.xyz)
   - Axis/direction vector (stored in spotlight.xyz)
   - Axis cut-off distance (stored in attenuation.y)
   - Axis linear attenuation range (stored in attenuation.x)
   - Beam radius (stored in spotlight.w)
   - Radial linear attenuation range (stored in attenuation.z)
   - Color (stored in color)
   Bounding volumes:
   - Sphere (point, beam and spot light)
   - Cylinder (beam lights and spot lights)
   - Spherical sector (spot lights)
   - AABB (for stationary lights)
   - Worst case sphere (for variable lights)

   In the shaders, attenuation.y is assigned a value of 1.0 for spot and 2.0 for beam lights.
   For beam lights, attenuation.z is assigned the cut-off distance from attenuation.y in the
   Light structure, and attenuation.w is assigned the radial linear attenuation range from
   attenuation.z.

   A directional light has a dynamic shadow volume if the direction changes.
   A point source light or spot light has a dynamic shadow volume if the position changes.
   A beam light has a dynamic shadow volume if the direction changes.
*/

Light::Light() {
    most_recent_shadow_volume_change = 0;
    changing_every_frame = false;
}

// Calculate the bounding cylinder and spherical_sector radius.

#if 0

// This code has been replaced by a more direct analytical method.

void Light::CalculateSpotLightCylinderRadius() {
    if (type & SRE_LIGHT_SPOT) {
        // Calculate the maximum cylinder radius where attenuation * pow(maxf(Dot(-R, L), 0.0), p)
        // is greater than some small value.
        float exponent = spotlight.w;
        // Instead of the costly sweep method, use a more exact and fast geometrical
        // calculation.
        CalculateBoundingCylinder(l->spherical_sector, l->cylinder);
#if 0
        float linear_range = attenuation.x;
        float r_max_threshold = 0;
        if (type & SRE_LIGHT_DYNAMIC_SPOT_EXPONENT)
            // Upper bound for the cylinder radius will be calculated. Exponent of
            // zero means no angle attenuation (180 degree angular size with full intensity).
            exponent = 0; 
        // Decrease distance along axis from with (small) exponential step. Make sure d = linear_range
        // is included.
        for (float d = linear_range;; d *= 0.9f) {
            // Let distance go down to 0.001f and make sure the distance of 0.001f is
            // actually used.
            d = maxf(d, 0.001f);
            // Assume light direction (1, 0, 0).
            // The intensity at distance r from Q is equal to distance attenuation times angle
            // attenuation, which is
            //     (1.0 - clamp(sqrt(r^2 + d^2) / linear_range, 0.0, 1.0)) *
            //         pow(max(- Dot(Vector3D(1, 0, 0), normalized light direction), 0), exponent), 
            // Assume the maximum field of view of the spotlight is 90 degrees.
            float max_r = d;
            // Do 100 steps from full radius to on cylinder axis.
            for (float r = max_r;; r -= max_r / 100.0f) {
                // Go down to radius of zero and make sure radius of zero is included.
                r = maxf(r, 0);
                // Distance attenuation is linear. For a cylinder, the distance from the endpoint
                // (light position) is sqrtf(radius ^ 2 + distance ^ 2).
                float dist_att = maxf(minf(sqrtf(r * r + d * d) / linear_range, 1.0f), 0);
                // The angle attenuation depends on the angle (dot product) between the
                // spotlight direction and the normalized light direction at the position, and the
                // spotlight exponent.
                float angle_att = powf(maxf(0, Dot(Vector3D(1.0f, 0, 0), Vector3D(d, r, 0).Normalize())),
                     exponent);
                float intensity = dist_att * angle_att;
                if (intensity >= 0.01f && r > r_max_threshold)
                    r_max_threshold = r;
                if (r <= 0)
                    break;
            }
            if (d <= 0.001f)
                break;
        }
        cylinder.radius = r_max_threshold;
#endif
        if (sre_internal_debug_message_level >= 2)
            printf("Cylinder bounding radius of spotlight: %f\n", cylinder.radius);
        // The highest intensities for a given angle between the spotlight axis and the normalized
        // light direction will be at a position very close to to the light position. Therefore,
        // to determine the maximum spherical sector bounding half angle, we can calculate, at some
        // very small distance (or zero distance) from the light source, the angle for which the
        // used spot exponent produces intensity that is greater than some small value:
        //     angle_att = pow(cos_angle, exponent) >= 0.01
        // Taking logarithm with base cos_angle on both sides yields
        //     exponent = log_with_base(cos_angle, 0.01)
        //     exponent = log(0.01) / log(cos_angle)
        //     log(cos_angle) = log(0.01) / exponent
        //     cos_angle = exp(log(0.01) / exponent)
        spherical_sector.cos_half_angular_size = expf(logf(0.01f) / exponent);
        spherical_sector.sin_half_angular_size =
            sinf(acosf(spherical_sector.cos_half_angular_size));
        if (sre_internal_debug_message_level >= 2)
            printf("Spherical sector half angle for spotlight: %d degrees\n",
                (int)(acosf(spherical_sector.cos_half_angular_size) * 180.0f / M_PI));
        return;
    }
}

#endif

void sreScene::CheckLightCapacity() {
    if (nu_lights == max_scene_lights) {
        if (sre_internal_debug_message_level >= 1)
            printf("Maximum number of  lights reached -- doubling capacity to %d.\n",
                max_scene_lights * 2);
        Light **new_global_light = new Light *[max_scene_lights * 2];
        memcpy(new_global_light, global_light, sizeof(Light *) * max_scene_lights);
        delete [] global_light;
        global_light = new_global_light;
        max_scene_lights *= 2;
    }
}

void sreScene::RegisterLight(Light *l) {
    global_light[nu_lights] = l;
    l->id = nu_lights;
    nu_lights++;
}

int sreScene::AddDirectionalLight(int type, Vector3D direction, Color color) {
    CheckLightCapacity();
    Light *l = new Light;
    l->type = type | SRE_LIGHT_DIRECTIONAL;
    l->shader_light_type = SRE_SHADER_LIGHT_TYPE_DIRECTIONAL;
    if (l->type & SRE_LIGHT_DYNAMIC_DIRECTION)
        // If the direction changes, the shadow volume for an object changes.
        l->type |= SRE_LIGHT_DYNAMIC_SHADOW_VOLUME;
    l->vector = Vector4D(- direction, 0.0);
    l->color = color;
    RegisterLight(l);
    return nu_lights - 1;
}

int sreScene::AddPointSourceLight(int type, Point3D position, float linear_range, Color color) {
    CheckLightCapacity();
    Light *l = new Light;
    // Linear attenuation is forced, even though some of the shaders support the classical
    // type of attenuation.
    l->type = type | SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_LINEAR_ATTENUATION_RANGE;
    l->shader_light_type = SRE_SHADER_LIGHT_TYPE_POINT_SOURCE_LINEAR_ATTENUATION;
    if (l->type & (SRE_LIGHT_DYNAMIC_ATTENUATION | SRE_LIGHT_DYNAMIC_POSITION))
        // If the attenuation changes, the light volume changes size and the geometrical
        // shadow volume for an object changes (smaller pyramid).
        // When the position changes, the light volume changes position and an object's
        // shadow volume will change.
        l->type |= SRE_LIGHT_DYNAMIC_LIGHT_VOLUME | SRE_LIGHT_DYNAMIC_SHADOW_VOLUME;
        // When there are any worst case sphere bounds, these can be defined
        // and the SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE flag will be set.
    l->vector.Set(position.x, position.y, position.z, 1.0f);
    l->attenuation.Set(linear_range, 0, 0);
    l->color = color;
    l->CalculateBoundingVolumes();
    RegisterLight(l);
    return nu_lights - 1;
}

int sreScene::AddSpotLight(int type, Point3D position, Vector3D direction, float exponent, float linear_range,
Color color) {
    CheckLightCapacity();
    Light *l = new Light;
    l->type = type | SRE_LIGHT_SPOT | SRE_LIGHT_LINEAR_ATTENUATION_RANGE;
    l->shader_light_type = SRE_SHADER_LIGHT_TYPE_SPOT_OR_BEAM;
    if (l->type & (SRE_LIGHT_DYNAMIC_ATTENUATION | SRE_LIGHT_DYNAMIC_DIRECTION |
    SRE_LIGHT_DYNAMIC_SPOT_EXPONENT | SRE_LIGHT_DYNAMIC_POSITION)) {
        // If the attenuation changes, the light volume changes size and the shadow
        // volume changes shape (shorter cylinder).
        // If the direction changes, the light volume changes shape (rotation) and
        // the geometrical shadow volume for an object also changes.
        // If the spot exponent changes, only the light volume changes. The shadow volume
        // of an object does not change.
        // A changing position will affect both the light volume and the shadow volume 
        // of an object.
        l->type |= SRE_LIGHT_DYNAMIC_LIGHT_VOLUME;
        if (l->type & (SRE_LIGHT_DYNAMIC_ATTENUATION | SRE_LIGHT_DYNAMIC_DIRECTION |
        SRE_LIGHT_DYNAMIC_POSITION))
            l->type |= SRE_LIGHT_DYNAMIC_SHADOW_VOLUME;
        if ((l->type & (SRE_LIGHT_DYNAMIC_ATTENUATION | SRE_LIGHT_DYNAMIC_SPOT_EXPONENT |
        SRE_LIGHT_DYNAMIC_DIRECTION | SRE_LIGHT_DYNAMIC_POSITION)) == SRE_LIGHT_DYNAMIC_DIRECTION)
            // If just the direction or spot exponent changes, a rought bounding sphere
            // can be defined already.
            l->type |= SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE;
    }
    l->vector.Set(position.x, position.y, position.z, 1.0f);
    l->attenuation.Set(linear_range, 0, 0);
    l->color = color;
    direction.Normalize();
    l->spotlight = Vector4D(direction, exponent);
    l->CalculateBoundingVolumes();
    RegisterLight(l);
    return nu_lights - 1;
}

int sreScene::AddBeamLight(int type, Point3D position, Vector3D direction, float beam_radius, float
radial_linear_range, float cutoff_distance, float linear_range, Color color) {
    CheckLightCapacity();
    Light *l = new Light;
    l->type = type | SRE_LIGHT_BEAM | SRE_LIGHT_LINEAR_ATTENUATION_RANGE;
    l->shader_light_type = SRE_SHADER_LIGHT_TYPE_SPOT_OR_BEAM;
    if (l->type & (SRE_LIGHT_DYNAMIC_ATTENUATION | SRE_LIGHT_DYNAMIC_DIRECTION |
    SRE_LIGHT_DYNAMIC_POSITION)) {
        // If the attenuation changes, the light volume changes (shorter or longer cylinder)
        // and the geometrical shadow volume for an object changes (shorter or longer cylinder).
        // If the direction changes, the light volume and shadow volume change.
        // When the position changes, the light volume changes position and the
        // shadow volume may change (shorter or longer cylinder).
        l->type |= SRE_LIGHT_DYNAMIC_SHADOW_VOLUME | SRE_LIGHT_DYNAMIC_LIGHT_VOLUME;
        if ((l->type & (SRE_LIGHT_DYNAMIC_ATTENUATION |
        SRE_LIGHT_DYNAMIC_DIRECTION | SRE_LIGHT_DYNAMIC_POSITION)) == SRE_LIGHT_DYNAMIC_DIRECTION)
            // If just the direction changes, a rough bounding sphere can be defined.
            l->type |= SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE;
    }
    l->vector.Set(position.x, position.y, position.z, 1.0f);
    l->attenuation.Set(linear_range, cutoff_distance, radial_linear_range);
    l->color = color;
    direction.Normalize();
    l->spotlight = Vector4D(direction, beam_radius);
    l->CalculateBoundingVolumes();
    RegisterLight(l);
    return nu_lights - 1;
}

// Calculate bounding volumes.

void Light::CalculateBoundingVolumes() {
    // For non-directional lights, always calculate a bounding sphere.
    // For variable lights, we also calculate worst case sphere bounds.
    // For spot and beam lights, calculate a bounding cylinder.
    // For spot lights only, calculate a bounding spherical sector.
    if (type & SRE_LIGHT_POINT_SOURCE) {
        sphere.center = vector.GetPoint3D();
        sphere.radius = attenuation.x;
        worst_case_sphere.center = vector.GetPoint3D();
        // No mechanism to set special worst case bounds for a point light
        // has been implemented yet.
        worst_case_sphere.radius = attenuation.x;
    }
    else if (type & (SRE_LIGHT_BEAM | SRE_LIGHT_SPOT)) {
        // Calculate a bounding cylinder for spot and beam lights.
        float length;
        if (type & SRE_LIGHT_BEAM) {
            // Mininum of the linear range and cut-off distance.
            length = minf(attenuation.x, attenuation.y);
            // Radius of beam light is stored in spotlight.w;
            // radial attenuation range is attenuation.z. Take the minimum.
            cylinder.radius = minf(spotlight.w, attenuation.z);
            cylinder.length = length;
            cylinder.axis = spotlight.GetVector3D();
            cylinder.center = vector.GetPoint3D() + 0.5f * length * cylinder.axis;
        }
        else {
            // Spot light.
            // Define a spherical sector bounding volume for spot lights.
            // This generally provides a tighter bound than a cylinder.
            spherical_sector.center = vector.GetPoint3D();
            spherical_sector.axis = spotlight.GetVector3D();
            // The highest intensities for a given angle between the spotlight axis and the normalized
            // light direction will be at a position very close to to the light position. Therefore,
            // to determine the maximum spherical sector bounding half angle, we can calculate, at some
            // very small distance (or zero distance) from the light source, the angle for which the
            // used spot exponent produces intensity that is greater than some small value:
            //     angle_att = pow(cos_angle, exponent) >= 0.01
            // Taking logarithm with base cos_angle on both sides yields
            //     exponent = log_with_base(cos_angle, 0.01)
            //     exponent = log(0.01) / log(cos_angle)
            //     log(cos_angle) = log(0.01) / exponent
            //     cos_angle = exp(log(0.01) / exponent)
            float exponent = spotlight.w;
            spherical_sector.cos_half_angular_size = expf(logf(0.01f) / exponent);
            spherical_sector.sin_half_angular_size =
                sinf(acosf(spherical_sector.cos_half_angular_size));
            if (sre_internal_debug_message_level >= 2)
                printf("Spherical sector half angle for spotlight: %d degrees\n",
                    (int)(acosf(spherical_sector.cos_half_angular_size) * 180.0f / M_PI));
            spherical_sector.radius = attenuation.x;
            // Calculate the bouding cylinder based on the spherical sector.
            CalculateBoundingCylinder(spherical_sector, cylinder);
        }
        // Calculate the cylinder axis coefficients, which are used when an AABB is tested for
        // intersection against the light cylinder.
        cylinder.CalculateAxisCoefficients();
        // Calculate optimized and worst-case bounding spheres for spot and beam lights.
        if (type & SRE_LIGHT_SPOT) {
            // Set optimized and worst-case bounding spheres.
            // The precalculated worst case bounds only apply to DYNAMIC_DIRECTION and/or
            // DYNAMIC_SPOT_EXPONENT. It will be a sphere with the center at the light
            // position. It can be optimized with subsequent use of SetSpotLightWorstCaseBounds().
            worst_case_sphere.center = vector.GetPoint3D();
            worst_case_sphere.radius = attenuation.x;
            // However, the current sphere bounds can be set to something better.
            CalculateBoundingSphere(spherical_sector, sphere);
            printf("Spot bounding sphere radius = %lf\n", sphere.radius);
        }
        else {
            // Set optimized and worst-case bounding spheres for a beam light,
            // based on its cylinder.
            // The furthest distance from the light position is on the edge
            // (not the endpoint) of the cylinder cap at the end of the range.
            // Set the worst case sphere radius (which only applies to DYNAMIC_DIRECTION).
            // It can be optimized with subsequent use of SetBeamLightWorstCaseBounds().
            worst_case_sphere.center = vector.GetPoint3D();
            worst_case_sphere.radius = sqrtf(sqrf(cylinder.length) + sqrf(cylinder.radius));
            // The current sphere bounds can be optimized.
            CalculateBoundingSphere(cylinder, sphere);
        }
    }
    // Set the AABB for static lights for octree construction. For dynamic lights, only
    // calculate the AABB now if the worst case bounds flag was already set.
    if ((type & (SRE_LIGHT_DYNAMIC_LIGHT_VOLUME | SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE)) ==
    SRE_LIGHT_DYNAMIC_LIGHT_VOLUME)
        return;
    CalculateWorstCaseLightVolumeAABB(AABB);
}

// Calculate an AABB using current light parameters.

void Light::CalculateLightVolumeAABB(sreBoundingVolumeAABB& AABB_out) {
    if (type & SRE_LIGHT_SPOT) {
        // Use the spherical sector of the spot light to calculate the AABB.
        CalculateAABB(spherical_sector, AABB_out);
    }
    else if (type & SRE_LIGHT_BEAM) {
        CalculateAABB(cylinder, AABB_out);
    }
    else {
        // Point source light. Use the bounding sphere's AABB.
        CalculateAABB(sphere, AABB_out);
    }
}

// Calculate an AABB using the light's worst case bounding volume.

void Light::CalculateWorstCaseLightVolumeAABB(sreBoundingVolumeAABB& AABB_out) {
    if (type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE) {
        // Variable light with worst case bounds (sphere).
        // Use the worst-case bounding sphere's AABB.
        CalculateAABB(worst_case_sphere, AABB_out);
    }
    else
        // Calculate the light's AABB based on current light parameters.
        CalculateLightVolumeAABB(AABB_out);
}

void sreScene::SetLightWorstCaseBounds(int i, const sreBoundingVolumeSphere& sphere) {
    Light *l = global_light[i];
    l->worst_case_sphere = sphere;
    l->type |= SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE;
}

// Set a worst case bounding volume for a point light with variable range and position. Since
// both parameters are defined in terms of a sphere, the resulting worst case volume will
// also be a sphere.

void sreScene::SetPointLightWorstCaseBounds(int i, float max_range,
const sreBoundingVolumeSphere& position_sphere) {
    Light *l = global_light[i];
    l->worst_case_sphere = l->sphere;
    l->worst_case_sphere.radius += max_range;
    l->type |= SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE;
    // The AABB will be based on the worst-case sphere.
    l->CalculateWorstCaseLightVolumeAABB(l->AABB);
}

// Set a worst case bounding volume (which may be just a sphere, ideally it would
// be more optimized) for a spot light with variable direction, spot light exponent, range or
// position; max_direction_angle is in radians, position sphere represents the space within
// which the light's center position may be moved (for a fixed position, use a sphere centered
// at the position with radius of zero).

void sreScene::SetSpotLightWorstCaseBounds(int i, float max_direction_angle, float min_exponent,
float max_range, const sreBoundingVolumeSphere& position_sphere) {
    Light *l = global_light[i];
    // Calculate the worst case spherical sector.
    float exponent_cos_max_half_angle = expf(logf(0.01f) / min_exponent);
    float max_half_angle = clampf(max_direction_angle + acosf(exponent_cos_max_half_angle), 0,
        M_PI);
    sreBoundingVolumeSphericalSector worst_case_sector;
    worst_case_sector.center = l->spherical_sector.center;
    worst_case_sector.radius = max_range;
    worst_case_sector.cos_half_angular_size = cosf(max_half_angle);
    worst_case_sector.sin_half_angular_size = sinf(max_half_angle);
    // The worst-case sector is not yet actually used itself.
    // Use the combined sector bounding volume to calculate the worst case bounding sphere.
    CalculateBoundingSphere(worst_case_sector, l->worst_case_sphere);
    // Finally extend the sphere by the positional range.
    l->worst_case_sphere.radius += position_sphere.radius;
    l->type |= SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE;
    // The AABB will be based on the worst-case sphere.
    l->CalculateWorstCaseLightVolumeAABB(l->AABB);
}

// Set a worst case bounding volume (which may be just a sphere, ideally it would
// be more optimized) for a beam light with variable direction, range or position; the
// max_direction_angle in radians is relative to the direction it was created with and the
// position sphere represents the space within which the light's center (source) position
// may be moved.

void sreScene::SetBeamLightWorstCaseBounds(int i, float max_direction_angle, float max_range,
float max_beam_radius, const sreBoundingVolumeSphere& position_sphere) {
    Light *l = global_light[i];
    // Varying the axis of the bounding cylinder will create a spherical cap on the exterior end;
    // However bounding volume will not be a true spherical sector.
    // We can however combine all the possible cylinder bounding spheres.
    sreBoundingVolumeCylinder max_cylinder;
    max_cylinder = l->cylinder;
    max_cylinder.length = max_range;
    max_cylinder.radius = max_beam_radius;
    // Calculate the bounding sphere of one max-sized cylinder.
    sreBoundingVolumeSphere sphere;
    CalculateBoundingSphere(max_cylinder, sphere);
    // Due to the symmetrical properties, the center of the cylinder's bounding sphere will
    // itself move within the surface of a spherical cap that has a radius equal to the
    // cylinder's bounding sphere radius minus one radius of the hypothetical cylinder cap
    // at the light position, which is half the cylinder length. We can calculate the
    // bounding sphere of the spherical cap surface within which the cylinder's bounding
    // sphere moves, and then add the bounding sphere radius of a single cylinder to obtain
    // an overall bounding sphere.
    float center_sphere_radius = sinf(max_direction_angle) * 0.5f * max_cylinder.length;
    // The center sphere's center will be at the cylinder's bounding sphere center
    // displaced towards the light position by the height of the surface of the spherical cap.
    l->worst_case_sphere.center = max_cylinder.center -
        cosf(max_direction_angle) * 0.5f * max_cylinder.length * max_cylinder.axis;
    // Add the cylinder bounding sphere radius to the center positions bounding sphere radius.
    l->worst_case_sphere.radius = center_sphere_radius + sphere.radius;
    // Finally extend the sphere by the positional range of the light position.
    l->worst_case_sphere.radius += position_sphere.radius;
    l->type |= SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE;
    // The AABB will be based on the worst-case sphere.
    l->CalculateWorstCaseLightVolumeAABB(l->AABB);
}

void sreScene::ChangeDirectionalLightDirection(int i, Vector3D direction) const {
    Light *l = global_light[i];
    l->vector = Vector4D(- direction, 0);
    if (l->most_recent_shadow_volume_change == sre_internal_current_frame - 1)
        l->changing_every_frame = true;
    // Before to setting changing_every_frame to false, have to check that
    // the light wasn't changed already this frame.
     else if (l->most_recent_shadow_volume_change != sre_internal_current_frame)
        l->changing_every_frame = false;
    l->most_recent_shadow_volume_change = sre_internal_current_frame;
}

void sreScene::ChangeLightPosition(int i, Point3D position) const {
    Light *l = global_light[i];
    if (l->vector.GetPoint3D() == position)
        // Position didn't actually change.
        return;
    Vector3D translation = position - l->vector.GetPoint3D();
    l->vector = Vector4D(position, l->vector.w);
    // Any kind of spherical bounding volume will move proportionally.
    l->sphere.center += translation;
    if (l->type & SRE_LIGHT_SPOT)
        l->spherical_sector.center += translation;
    if (l->type & (SRE_LIGHT_BEAM | SRE_LIGHT_SPOT))
        l->cylinder.center += translation;
    if (l->type & SRE_LIGHT_BEAM)
        // For beam lights, a position change doesn't change the shadow volume shape.
        return;
    if (l->most_recent_shadow_volume_change == sre_internal_current_frame - 1) {
        l->changing_every_frame = true;
//        printf("Light %d is changing every frame.\n", i);
    }
    else if (l->most_recent_shadow_volume_change != sre_internal_current_frame)
        l->changing_every_frame = false;
    l->most_recent_shadow_volume_change = sre_internal_current_frame;
}

void sreScene::ChangeLightColor(int i, Color color) const {
    global_light[i]->color = color;
    // Ideally, color should affect the light volume size.
}

void sreScene::ChangeSpotLightDirection(int i, Vector3D direction) const {
    Light *l = global_light[i];
    l->spotlight = Vector4D(direction, l->spotlight.w);
    // Note that the bounding sphere will be affected too.
    if (l->type & SRE_LIGHT_SPOT) {
        l->spherical_sector.axis = direction;
        CalculateBoundingSphere(l->spherical_sector, l->sphere);
    }
    if (l->type & (SRE_LIGHT_BEAM | SRE_LIGHT_SPOT)) {
        l->cylinder.axis = direction;
        l->cylinder.center = l->vector.GetPoint3D() + direction * l->cylinder.length * 0.5f;
        l->cylinder.CalculateAxisCoefficients();
        CalculateBoundingSphere(l->cylinder, l->sphere);
    }
    if (l->type & SRE_LIGHT_BEAM) {
        // For beam lights, a direction change changes the shadow volumes.
        if (l->most_recent_shadow_volume_change == sre_internal_current_frame - 1) {
            l->changing_every_frame = true;
//            printf("Beam light %i is changing every frame.\n", i);
        }
        // Before to setting changing_every_frame to false, have to check that
        // the light wasn't changed already this frame.
        else if (l->most_recent_shadow_volume_change != sre_internal_current_frame)
            l->changing_every_frame = false;
        l->most_recent_shadow_volume_change = sre_internal_current_frame;
    }
    // Note: For spot lights, the shape of the GPU shadow volumes is not affected. They
    // depend only on the position of the light.
}

void sreScene::ChangePointSourceLightAttenuation(int i, float range) const {
    // It is assumed that SRE_LIGHT_DYNAMIC_ATTENUATION is set.
    Light *l = global_light[i];
    l->attenuation.Set(range, 0, 0);
    l->sphere.radius = range;
}

void sreScene::ChangeSpotLightAttenuationAndExponent(int i, float range, float exponent) const {
    // It is assumed that SRE_LIGHT_DYNAMIC_ATTENUATION or SRE_LIGHT_DYNAMIC_SPOT_EXPONENT
    // is set when appropriate.
    Light *l = global_light[i];
    l->attenuation.Set(range, 0, 0);
    l->spotlight.w = exponent;
    // Spherical sector has to be recalculated.
    l->spherical_sector.radius = l->attenuation.x;
    l->spherical_sector.cos_half_angular_size = expf(logf(0.01f) / exponent);
    l->spherical_sector.sin_half_angular_size = sinf(acosf(l->spherical_sector.cos_half_angular_size));
    l->cylinder.length = range;
    // The bounding sphere (based on the spherical sector) has to be recalculated.
    CalculateBoundingSphere(l->spherical_sector, l->sphere);
    // The bounding cylinder, although the primary bounding volume, has to be updated too.
    CalculateBoundingCylinder(l->spherical_sector, l->cylinder);
}

void sreScene::ChangeBeamLightAttenuation(int i, float beam_radius, float
radial_linear_range, float cutoff_distance, float linear_range) const {
    // It is assumed that SRE_LIGHT_DYNAMIC_ATTENUATION is set.
    Light *l = global_light[i];
    l->attenuation.Set(linear_range, cutoff_distance, radial_linear_range);
    l->spotlight.w = beam_radius;
    // Update bounding volumes. It is normally assumed that beam_radius is
    // always at least as small as radial_linear_range, and cutoff_distance
    // is at least as small as as linear_range.
    l->cylinder.length = minf(l->attenuation.x, l->attenuation.y);
    l->cylinder.radius = minf(beam_radius, radial_linear_range);
    // Recalculate the bounding sphere based on the cylinder.
    CalculateBoundingSphere(l->cylinder, l->sphere);
}

static sreView *internal_view;

// Light sorting is used with single-pass rendering, but only the most prominent light
// is actually used (the single pass shader uses just one light).

static int CompareLights(const void *e1, const void *e2) {
    int i1 = *(int *)e1;
    int i2 = *(int *)e2;
    if (sre_internal_scene->global_light[i1]->type & SRE_LIGHT_DIRECTIONAL)
        if (sre_internal_scene->global_light[i2]->type & SRE_LIGHT_DIRECTIONAL) {
            // Both lights are directional; impose an order based on intensity.
            float intensity1, intensity2;
            if (sre_internal_HDR_enabled) {
                intensity1 = sre_internal_scene->global_light[i1]->color.LinearIntensity();
                intensity2 = sre_internal_scene->global_light[i2]->color.LinearIntensity();
            }
            else {
                intensity1 = sre_internal_scene->global_light[i1]->color.SRGBIntensity();
                intensity2 = sre_internal_scene->global_light[i2]->color.SRGBIntensity();
            }
            if (intensity1 > intensity2)
                return - 1;
            if (intensity2 > intensity1)
                return 1;
            return 0;
        }
        else
            // Light i1 is directional, i2 is not; give precedence to the directional light.
            return - 1;
    else
        if (sre_internal_scene->global_light[i2]->type & SRE_LIGHT_DIRECTIONAL)
            // Light i2 is directional, i1 is not.
            return 1;
    // Both are non-directional lights.
    // Compare the distance to the point of interest. Use the viewpoint, or when object
    // following view mode is used, prefer to use the distance to the followed object.
    Point3D point_of_interest;
    if (internal_view->GetViewMode() == SRE_VIEW_MODE_FOLLOW_OBJECT)
        point_of_interest = sre_internal_scene->sceneobject[
            internal_view->GetFollowedObject()]->position;
    else
        point_of_interest =  sre_internal_viewpoint;
    float distsq1 = SquaredMag((sre_internal_scene->global_light[i1]->vector).GetPoint3D()
        - point_of_interest);
    float distsq2 = SquaredMag((sre_internal_scene->global_light[i2]->vector).GetPoint3D()
        - point_of_interest);
    if ((sre_internal_scene->global_light[i1]->type & (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_LINEAR_ATTENUATION_RANGE))
    == (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_LINEAR_ATTENUATION_RANGE)) {
        if ((sre_internal_scene->global_light[i2]->type & (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_LINEAR_ATTENUATION_RANGE))
        == (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_LINEAR_ATTENUATION_RANGE)) {
            // Both are point source lights with linear attenuation; calculate the intensity at the point
            // of interest.
            float att1 = clampf((sre_internal_scene->global_light[i1]->attenuation.x - sqrtf(distsq1))
                / sre_internal_scene->global_light[i1]->attenuation.x, 0, 1.0f);
            float att2 = clampf((sre_internal_scene->global_light[i2]->attenuation.x - sqrtf(distsq2))
                / sre_internal_scene->global_light[i1]->attenuation.x, 0, 1.0f);
            Color c1 = att1 * sre_internal_scene->global_light[i1]->color;
            Color c2 = att2 * sre_internal_scene->global_light[i2]->color;
            float intensity1, intensity2;
            if (sre_internal_HDR_enabled) {
                intensity1 = c1.LinearIntensity();
                intensity2 = c2.LinearIntensity();
            }
            else {
                intensity1 = c1.SRGBIntensity();
                intensity2 = c2.SRGBIntensity();
            }
            if (intensity1 > intensity2)
                return - 1;
            if (intensity2 > intensity1)
                return 1;
            return 0;
        }
        else {
            // Light i1 is a point source light, i2 is a beam or spot light.
            // To maintain a strict sorting order, give precedence to the point source light.
            return - 1;
        }
    }
    else if ((sre_internal_scene->global_light[i2]->type & (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_LINEAR_ATTENUATION_RANGE))
    == (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_LINEAR_ATTENUATION_RANGE))
        // Give precedence to i2 (point source light).
        return 1;
    // For other combinations of lights (spot or beam lights), calculating the intensity at the
    // point of interest requires a little more work; for now, just use the
    // distance to the point of interest.
    if (distsq1 < distsq2)
         return - 1;
    if (distsq1 > distsq2)
         return 1;
    return 0;
}

void sreScene::CalculateWholeSceneActiveLights(sreView *view, int max_lights) {
    // If we can support all lights as active lights, just copy them.
    // (currently only one light is supported for single-pass rendering).
    if (nu_lights <= max_lights) {
        for (int i = 0; i < nu_lights; i++)
            active_light[i] = i;
        nu_active_lights = nu_lights;
        return;
    }
    // There are more than max_lights lights.
    // Sort the lights on prominence.
    internal_view = view;
    if (max_lights == 1) {
        // When we need only the most prominent light, don't sort the whole
        // set.
        int light_element[2];
        light_element[0] = 0;
        for (int i = 1; i < nu_lights; i++) {
            light_element[1] = i;
            if (CompareLights(&light_element[0], &light_element[1]) > 0)
                light_element[0] = i;
        }
        active_light[0] = light_element[0];
//        printf("Active light: %d\n", active_light[0]);
    }
    else {
        int *light_element = (int *)alloca(sizeof(int) * nu_lights);
        for (int i = 0; i < nu_lights; i++)
            light_element[i] = i;
        qsort(light_element, nu_lights, sizeof(int), CompareLights);
        for (int i = 0; i < max_lights; i++)
            active_light[i] = light_element[i];
//        printf("First active light: %d\n", active_light[0]);
    }
    nu_active_lights = max_lights;
}

void sreScene::CalculateVisibleActiveLights(sreView *view, int max_lights) {
    // If we can support all lights as active lights, just copy them.
    // (currently only one light is supported for single-pass rendering).
    if (nu_visible_lights <= max_lights) {
        for (int i = 0; i < nu_visible_lights; i++)
            active_light[i] = visible_light[i];
        nu_active_lights = nu_visible_lights;
        return;
    }
    // There are more than max_lights lights.
    // Sort the lights on prominence.
    internal_view = view;
    if (max_lights == 1) {
        // When we need only the most prominent light, don't sort the whole
        // set.
        int light_element[2];
        light_element[0] = visible_light[0];
        for (int i = 1; i < nu_visible_lights; i++) {
            light_element[1] = visible_light[i];
            if (CompareLights(&light_element[0], &light_element[1]) > 0)
                light_element[0] = visible_light[i];
        }
        active_light[0] = light_element[0];
//        printf("Active light: %d\n", active_light[0]);
    }
    else {
        // Perform a full sort.
        int *light_element = (int *)alloca(sizeof(int) * nu_visible_lights);
        for (int i = 0; i < nu_visible_lights; i++)
            light_element[i] = visible_light[i];
        qsort(light_element, nu_visible_lights, sizeof(int), CompareLights);
        for (int i = 0; i < max_lights; i++)
            active_light[i] = light_element[i];
//        printf("First active light: %d\n", active_light[0]);
    }
    nu_active_lights = max_lights;
}

// The order of the bounding box vertices assigned can have implications
// later, for example when the angle between pyramid sides resulting from
// a projected bounding box may impact the accuracy of plane vector
// calculations.

static void ConstructBoundingBoxVertices(const SceneObject& so, Point3D *P, int& n) {
    P[0] = so.box.GetCorner(0.5f, 0.5f, 0.5f);
    P[1] = so.box.GetCorner(- 0.5f, 0.5f, 0.5f);
    P[2] = so.box.GetCorner(- 0.5f, - 0.5f, 0.5f);
    P[3] = so.box.GetCorner(0.5f, - 0.5f, 0.5f);
    if (so.box.PCA[2].SizeIsZero()) {
        n = 4;
        return;
    }
    P[4] = so.box.GetCorner(0.5f, 0.5f, - 0.5f);
    P[5] = so.box.GetCorner(- 0.5f, 0.5f, - 0.5f);
    P[6] = so.box.GetCorner(- 0.5f, - 0.5f, - 0.5f);
    P[7] = so.box.GetCorner(0.5f, - 0.5f, - 0.5f);
    n = 8;
    return;
}

static const int BB_plane_vertex[6][4] = {
    { 0, 3, 4, 7 },  // R positive
    { 1, 2, 5, 6 },  // R negative
    { 0, 1, 4, 5 },  // S positive
    { 3, 2, 7, 6 },  // S negative
    { 0, 1, 3, 2 },  // T positive
    { 4, 5, 7, 6 }   // T negative
}; 

static const int flat_BB_plane_nu_vertices[6] = {
    2, 2, 2, 2, 0, 0 
};

static const int BB_edge_vertex[12][2] = {
    { 1, 2 }, // Top left edge.
    { 0, 3 }, // Top right edge.
    { 1, 0 }, // Top far edge.
    { 2, 3 }, // Top near edge.
    { 5, 6 }, // Bottom left edge.
    { 4, 7 }, // Bottom right edge.
    { 5, 4 }, // Bottom far edge.
    { 6, 7 }, // Bottom near edge.
    { 6, 2 }, // Left side near edge.
    { 5, 1 }, // Left side far edge.
    { 7, 3 }, // Right side near edge.
    { 4, 0 }  // Right side far edge.
};

static const int BB_edge_plane[12][2] = {
    { 1, 4 },
    { 0, 4 },
    { 2, 4 },
    { 3, 4 },
    { 1, 5 },
    { 0, 5 },
    { 2, 5 },
    { 3, 5 },
    { 1, 3 },
    { 1, 2 },
    { 0, 3 },
    { 0, 2 }
};

static void MoveBoundingBoxVerticesInward(Point3D *P, int n_vertices, Vector4D *K, int plane, float dist) {
    int n;
    if (n_vertices == 4)
        n = flat_BB_plane_nu_vertices[plane];
    else
        n = 4;
    for (int i = 0; i < n; i++)
        P[BB_plane_vertex[plane][i]] += dist * K[plane].GetVector3D(); 
}

#if 0

BoundsCheckResult OctreeIntersectionWithLightVolume(const Octree& octree, const Light &light) {
    if (light.vector.w == 0)
        return SRE_COMPLETELY_INSIDE;
    // If the light volume is large compared to the size of the octree, do a sphere check.
    if (light.bounding_radius > octree.sphere_radius) {
        // Approximate the bounding volume of the octree with a sphere.
        Point3D oct_center;
        oct_center.Set((octree.dim_min.x + octree.dim_max.x) * 0.5, (octree.dim_min.y + octree.dim_max.y) * 0.5,
            (octree.dim_min.z + octree.dim_max.z) * 0.5);
        float distsqr = SquaredMag(oct_center - light.vector.GetPoint3D());
        if (distsqr >= (octree.sphere_radius + light.bounding_radius) * (octree.sphere_radius + light.bounding_radius))
            // The two spheres do not intersect.
            return SRE_COMPLETELY_OUTSIDE;
        if (light.type & (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM)) {
            // For spotlights, construct the plane for which the light direction is the normal, and check whether the
            // octree's bounding sphere is entirely on the negative side of the plane.
            Vector4D K = Vector4D(light.spotlight.GetVector3D(), - Dot(light.spotlight.GetVector3D(),
                light.vector.GetPoint3D()));
            if (Dot(K, oct_center) <= - octree.sphere_radius)
                return SRE_COMPLETELY_OUTSIDE;
        }
        if (distsqr < (light.bounding_radius - octree.sphere_radius) * (light.bounding_radius - octree.sphere_radius))
            // The octree is entirely inside the light volume.
            return SRE_COMPLETELY_INSIDE;
    }
    if (light.type & (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM)) {
        // For spot lights, check whether the corners of the octree are completely on the negative side of the
        // plane defined by the spot light direction.
        Vector4D K = Vector4D(light.spotlight.GetVector3D(), - Dot(light.spotlight.GetVector3D(),
            light.vector.GetPoint3D()));
        if (Dot(K, Point3D(octree.dim_min.x, octree.dim_min.y, octree.dim_min.z)) <= 0
        && Dot(K, Point3D(octree.dim_max.x, octree.dim_min.y, octree.dim_min.z)) <= 0
        && Dot(K, Point3D(octree.dim_min.x, octree.dim_max.y, octree.dim_min.z)) <= 0
        && Dot(K, Point3D(octree.dim_max.x, octree.dim_max.y, octree.dim_min.z)) <= 0
        && Dot(K, Point3D(octree.dim_min.x, octree.dim_min.y, octree.dim_max.z)) <= 0
        && Dot(K, Point3D(octree.dim_max.x, octree.dim_min.y, octree.dim_max.z)) <= 0
        && Dot(K, Point3D(octree.dim_min.x, octree.dim_max.y, octree.dim_max.z)) <= 0
        && Dot(K, Point3D(octree.dim_max.x, octree.dim_max.y, octree.dim_max.z)) <= 0)
            return SRE_COMPLETELY_OUTSIDE;
        // Check the bounding cylinder of the light volume with the octree for each plane of the octree.
        Vector4D L[6];
        L[0].Set(1.0, 0, 0, - octree.dim_min.x);	// Left plane.
        L[1].Set(- 1.0, 0, 0, octree.dim_max.x); 	// Right plane.
        L[2].Set(0, 1.0, 0, - octree.dim_min.y);	// Near plane.
        L[3].Set(0, - 1.0, 0, octree.dim_max.y); 	// Far plane.
        L[4].Set(0, 0, 1.0, - octree.dim_min.z);	// Bottom plane.
        L[5].Set(0, 0, - 1.0, octree.dim_max.z); 	// Top plane.
        Point3D Q1 = light.vector.GetPoint3D();
        float range;
        if (light.type & SRE_LIGHT_SPOT)
            range = light.attenuation.x;
        else
            range = minf(light.attenuation.x, light.attenuation.y);
        Point3D Q2 = light.vector.GetPoint3D() + range * light.spotlight.GetVector3D();
        BoundsCheckResult r = BoxIntersectionWithCylinder(L, 6, Q1, Q2, light.spotlight.GetVector3D(),
            light.cylinder_radius);
        return r;
    }
    // Check bounding sphere of light volume with octree for each plane of the octree in
    // world space.
    Vector4D K;
    K.Set(1.0f, 0, 0, - octree.dim_min.x);	// Left plane.
    float dist = Dot(K, light.vector.GetPoint3D());
    if (dist <= - light.bounding_radius)
        return SRE_COMPLETELY_OUTSIDE;
    bool completely_encloses = true;
    if (dist <= light.bounding_radius)
        completely_encloses = false;
    K.Set(- 1.0f, 0, 0, octree.dim_max.x); 	// Right plane.
    dist = Dot(K, light.vector.GetPoint3D());
    if (dist <= - light.bounding_radius)
        return SRE_COMPLETELY_OUTSIDE;
    if (dist <= light.bounding_radius)
        completely_encloses = false;
    K.Set(0, 1.0f, 0, - octree.dim_min.y);	// Near plane.
    dist = Dot(K, light.vector.GetPoint3D());
    if (dist <= - light.bounding_radius)
        return SRE_COMPLETELY_OUTSIDE;
    if (dist <= light.bounding_radius)
        completely_encloses = false;
    K.Set(0, - 1.0f, 0, octree.dim_max.y); 	// Far plane.
    dist = Dot(K, light.vector.GetPoint3D());
    if (dist <= - light.bounding_radius)
        return SRE_COMPLETELY_OUTSIDE;
    if (dist <= light.bounding_radius)
        completely_encloses = false;
    K.Set(0, 0, 1.0f, - octree.dim_min.z);	// Bottom plane.
    dist = Dot(K, light.vector.GetPoint3D());
    if (dist <= - light.bounding_radius)
        return SRE_COMPLETELY_OUTSIDE;
    if (dist <= light.bounding_radius)
        completely_encloses = false;
    K.Set(0, 0, - 1.0f, octree.dim_max.z); 	// Top plane.
    dist = Dot(K, light.vector.GetPoint3D());
    if (dist <= - light.bounding_radius)
        return SRE_COMPLETELY_OUTSIDE;
    if (dist <= light.bounding_radius)
        completely_encloses = false;
    if (completely_encloses) {
//        printf("octree completely encloses = (%f, %f, %f) - (%f, %f, %f)\n", octree.dim_min.x, octree.dim_min.y,
//            octree.dim_min.z, octree.dim_max.x, octree.dim_max.y, octree.dim_max.z);
        return SRE_COMPLETELY_ENCLOSES;
    }
    // Check whether the corners of the octree are inside the light volume.
    if (SquaredMag(Point3D(octree.dim_min.x, octree.dim_min.y, octree.dim_min.z) - light.vector.GetPoint3D())
    >= light.bounding_radius * light.bounding_radius)
        return SRE_PARTIALLY_INSIDE;
    if (SquaredMag(Point3D(octree.dim_max.x, octree.dim_min.y, octree.dim_min.z) - light.vector.GetPoint3D())
    >= light.bounding_radius * light.bounding_radius)
        return SRE_PARTIALLY_INSIDE;
    if (SquaredMag(Point3D(octree.dim_min.x, octree.dim_max.y, octree.dim_min.z) - light.vector.GetPoint3D())
    >= light.bounding_radius * light.bounding_radius)
        return SRE_PARTIALLY_INSIDE;
    if (SquaredMag(Point3D(octree.dim_max.x, octree.dim_max.y, octree.dim_min.z) - light.vector.GetPoint3D())
    >= light.bounding_radius * light.bounding_radius)
        return SRE_PARTIALLY_INSIDE;
    if (SquaredMag(Point3D(octree.dim_min.x, octree.dim_min.y, octree.dim_max.z) - light.vector.GetPoint3D())
    >= light.bounding_radius * light.bounding_radius)
        return SRE_PARTIALLY_INSIDE;
    if (SquaredMag(Point3D(octree.dim_max.x, octree.dim_min.y, octree.dim_max.z) - light.vector.GetPoint3D())
    >= light.bounding_radius * light.bounding_radius)
        return SRE_PARTIALLY_INSIDE;
    if (SquaredMag(Point3D(octree.dim_min.x, octree.dim_max.y, octree.dim_max.z) - light.vector.GetPoint3D())
    >= light.bounding_radius * light.bounding_radius)
        return SRE_PARTIALLY_INSIDE;
    if (SquaredMag(Point3D(octree.dim_max.x, octree.dim_max.y, octree.dim_max.z) - light.vector.GetPoint3D())
    >= light.bounding_radius * light.bounding_radius)
        return SRE_PARTIALLY_INSIDE;
    return SRE_COMPLETELY_INSIDE;
}

#endif

// Do a intersection check of an object with a light volume and at same time calculate
// the scissors region. Returns SRE_COMPLETELY_OUTSIDE if the object is completely outside
// the light volume, SRE_PARTIALLY_INSIDE if the object intersects the light volume and the
// scissors were set, SRE_COMPLETELY_INSIDE if the object intersects the light volume and
// the scissors were not set. Calculated scissors are stored in the scisssors parameter.

BoundsCheckResult SceneObject::CalculateGeometryScissors(const Light& light, const Frustum& frustum,
sreScissors& scissors) {
    // Do a sphere check first.
    float dist_squared = SquaredMag(sphere.center - light.sphere.center);
    if (dist_squared >= sqrf(sphere.radius + light.sphere.radius))
        // The two spheres do not intersect.
        return SRE_COMPLETELY_OUTSIDE;
    if (light.sphere.radius >= sphere.radius && dist_squared <= sqrf(light.sphere.radius - sphere.radius))
        return SRE_COMPLETELY_INSIDE;
    // Initialize scissors with a negative (non-existent) region.
    scissors.InitializeWithEmptyRegion();
    // Calculate the intersection of the light's bounding sphere with the object's bounding sphere.
    if ((light.type & SRE_LIGHT_POINT_SOURCE) && (model->bounds_flags & SRE_BOUNDS_PREFER_SPHERE)) {
        if (sphere.radius >= light.sphere.radius && dist_squared <= sqrf(light.sphere.radius - sphere.radius)) {
            // The light volume is completely contained inside the object.
            // In this case, the light volume is likely to be small and the scissors region is probably already small
            // enough. So return SRE_COMPLETELY_INSIDE for performance.
            return SRE_COMPLETELY_INSIDE;
        }
        // Construct the two endpoints on the line between the object's bounding sphere center
        // and the light volume's center, which delimit the intersection.
        // First find the center of the intersection circle on the line between the object's bounding sphere center
        // and the light volume's center.
        float dist = sqrtf(dist_squared);
        float t = (dist_squared - sqrf(light.sphere.radius) + sqrf(sphere.radius)) / (2.0 * dist);
        Vector3D N = light.vector.GetPoint3D() - sphere.center;
        N /= dist;
        // Find sizes the caps which delimit the intersection volume.
        float h1 = sphere.radius - t;
        float h2 = light.sphere.radius - (dist - t);
//printf("t = %f, dist = %f, h1 = %f, h2 = %f\n", t, dist, h1, h2);
        Point3D E1, E2;
        E1 = sphere.center + (t - h2) * N;
        E2 = sphere.center + (t + h1) * N;
        float r;
        if (t - h2 <= 0)
            // The intersection plane is on the negative side of the object's center. More than half of the object is
            // illuminated.
            r = sphere.radius;
        else
            // The intersection plane is on the positive side of the object's center. Less than half of the object
            // is illuminated.
            // The extent r of the caps in the tangent plane needs to be calculated. It is equal to the radius
            // of the intersection circle in that plane.
            r = sqrtf((- dist + light.sphere.radius - sphere.radius) * (- dist - light.sphere.radius +
                sphere.radius) * (- dist + light.sphere.radius + sphere.radius) * (dist + light.sphere.radius +
                sphere.radius)) / (2.0 * dist);
        // Construct the box with that encloses the intersection with a width and height of
        // two times the radius of the intersection circle.
        Vector3D up;
        if (fabsf(N.x) < 0.01 && fabsf(N.z) < 0.01) {
            if (N.y > 0)
                up = Vector3D(0, 0, - 1.0);
            else
                up = Vector3D(0, 0, 1.0);
        }
        else
            up = Vector3D(0, 1.0, 0);
        // Calculate tangent planes.
        Vector3D N2 = Cross(up, N);
        N2.Normalize();
        Vector3D N3 = Cross(N, N2);
        Point3D B[8];
        B[0] = E1 + r * N2 + r * N3;
        B[1] = E1 - r * N2 + r * N3;
        B[2] = E1 + r * N2 - r * N3;
        B[3] = E1 - r * N2 - r * N3;
        B[4] = E2 + r * N2 + r * N3;
        B[5] = E2 - r * N2 + r * N3;
        B[6] = E2 + r * N2 - r * N3;
        B[7] = E2 - r * N2 - r * N3;
// printf("Distance(E1, E2) = %f, height = %f\n", Magnitude(E1 - E2), height);
        bool result = scissors.UpdateWithWorldSpaceBoundingBox(B, 8, frustum);
        if (!result)
            return SRE_COMPLETELY_OUTSIDE;
        return SRE_PARTIALLY_INSIDE;
    }
    if (light.type & SRE_LIGHT_POINT_SOURCE) {
        // Model has SRE_BOUNDS_PREFER_BOX or SRE_BOUNDS_PREFER_LINE_SEGMENT.
        float dist[6];
        int n_planes;
        // Check for intersection of the bounding box with the light sphere and store the distances.
        dist[0] = Dot(box.plane[0], light.sphere.center);
        if (dist[0] <= - light.sphere.radius)
            return SRE_COMPLETELY_OUTSIDE;
        dist[1] = Dot(box.plane[1], light.sphere.center);
        if (dist[1] <= - light.sphere.radius)
            return SRE_COMPLETELY_OUTSIDE;
        dist[2] = Dot(box.plane[2], light.sphere.center);
        if (dist[2] <= - light.sphere.radius)
            return SRE_COMPLETELY_OUTSIDE;
        dist[3] = Dot(box.plane[3], light.sphere.center);
        if (dist[3] <= - light.sphere.radius)
            return SRE_COMPLETELY_OUTSIDE;
        dist[4] = Dot(box.plane[4], light.sphere.center);
        if (dist[4] <= - light.sphere.radius)
            return SRE_COMPLETELY_OUTSIDE;
        dist[5] = Dot(box.plane[5], light.sphere.center);
        if (dist[5] <= - light.sphere.radius)
            return SRE_COMPLETELY_OUTSIDE;
        if (!box.PCA[2].SizeIsZero())
            n_planes = 6;
        else
            n_planes = 4;
        Point3D P[8];
        int n_vertices;
        bool changed = false;
        for (int i = 0; i < n_planes; i += 2) {
            float dim = box.plane[i].w + box.plane[i + 1].w;
            if (dist[i] < - light.sphere.radius + dim) {
                // The light volume sphere enchroaches into the R/S/T_positive plane but does not completely
                // overlap the object in this dimension.
                // Move the vertices associated with the opposite plane inward by - light_radius + dim - dist.
                if (!changed)
                    ConstructBoundingBoxVertices(*this, P, n_vertices);
                MoveBoundingBoxVerticesInward(P, n_vertices, box.plane, i + 1, - light.sphere.radius + dim - dist[i]);
                changed = true;
//                printf("BB vertices moved inward by %f for plane %d\n", -light.bounding_radius + dim - dist[i], i + 1);
            }
            if (dist[i + 1] < - light.sphere.radius + dim) {
                if (!changed)
                    ConstructBoundingBoxVertices(*this, P, n_vertices);
                MoveBoundingBoxVerticesInward(P, n_vertices, box.plane, i, - light.sphere.radius + dim - dist[i + 1]);
                changed = true;
//                printf("BB vertices moved inward by %f for plane %d\n", - light.sphere.radius + dim - dist[i + 1], i);
            }
        }
        if (!changed) {
            return SRE_COMPLETELY_INSIDE;
        }
        bool result = scissors.UpdateWithWorldSpaceBoundingBox(P, n_vertices, frustum);
        if (!result)
            return SRE_COMPLETELY_OUTSIDE;
        return SRE_PARTIALLY_INSIDE;
    }
    if (light.type & (SRE_LIGHT_SPOT | SRE_LIGHT_BEAM)) {
        // For spot and beam lights the cylinder bounding volume can be used.
        // Check intersection between the object's bounding sphere and the cylinder.
        // This test is not very expensive.
        BoundsCheckResult r = QueryIntersection(sphere, light.cylinder);
        // Since only the object's bounding sphere is used to calculate the geometry
        // scissors, if the sphere is completely inside the light volume no useful
        // geometry scissors can be calculated. If the object is completely outside
        // the light cylinder, it can be skipped completely. We only continue when
        // the bounding sphere is partially inside the light cylinder. 
        if (r != SRE_PARTIALLY_INSIDE)
            return r;
        if (model->bounds_flags & (SRE_BOUNDS_PREFER_BOX | SRE_BOUNDS_PREFER_BOX_LINE_SEGMENT)) {
            // Check the bounding box of the object against the light volume cylinder.
            if (!Intersects(box, light.cylinder))
                return SRE_COMPLETELY_OUTSIDE;
            if (model->bounds_flags & SRE_BOUNDS_PREFER_BOX_LINE_SEGMENT)
                // When one dimension is much larger than the others, the bounding sphere
                // that is used for the scissors calculation is not likely to produce
                // results, so don't use geometry scissors in this case.
                return SRE_COMPLETELY_INSIDE;
        }
        // To determine the intersection of the light volume with the object for the scissors
        // calculation, treat the cylindrical light volume as a box and use the object's
        // bounding sphere. This is likely to provide the best results for objects well
        // bounded by a sphere.
        Vector3D N = light.spotlight.GetVector3D();
        Vector3D up;
        if (fabsf(N.x) < 0.01f && fabsf(N.z) < 0.01f) {
            if (N.y > 0)
                up = Vector3D(0, 0, - 1.0f);
            else
                up = Vector3D(0, 0, 1.0f);
        }
        else
            up = Vector3D(0, 1.0f, 0);
        // Calculate tangent planes.
        Vector3D N2 = Cross(up, N);
        N2.Normalize();
        Vector3D N3 = Cross(N, N2);
        // Start with the bounding box of the light volume cylinder.
        Point3D B[8];
        Point3D E1 = light.vector.GetPoint3D();
        Point3D E2 = E1 + light.cylinder.length * light.cylinder.axis;
        // The order of box vertices must correspond to the one expected by
        // MoveBoundingBoxVerticesInward().
        B[0] = E2 + light.cylinder.radius * N2 + light.cylinder.radius * N3;
        B[1] = E1 + light.cylinder.radius * N2 + light.cylinder.radius * N3;
        B[2] = E1 - light.cylinder.radius * N2 + light.cylinder.radius * N3;
        B[3] = E2 - light.cylinder.radius * N2 + light.cylinder.radius * N3;
        B[4] = E2 + light.cylinder.radius * N2 - light.cylinder.radius * N3;
        B[5] = E1 + light.cylinder.radius * N2 - light.cylinder.radius * N3;
        B[6] = E1 - light.cylinder.radius * N2 - light.cylinder.radius * N3;
        B[7] = E2 - light.cylinder.radius * N2 - light.cylinder.radius * N3;
        // Construct plane vectors where the normal points inward towards the
        // center of light's bounding box. Also calculate the signed distance to
        // the object's bounding sphere.
        Vector4D M[6];
        Vector3D normal = light.cylinder.axis;
        M[0] = Vector4D(- normal, - Dot(- normal, E2));
        M[1] = Vector4D(normal, - Dot(normal, light.vector.GetPoint3D()));
        M[2] = Vector4D(- N2, - Dot(- N2, B[0]));
        M[3] = Vector4D(N2, - Dot(N2, B[2]));
        M[4] = Vector4D(- N3, - Dot(- N3, B[0]));
        M[5] = Vector4D(N3, - Dot(N3, B[4]));
        // For each box plane, move it
        float dist[6];
        for (int i = 0; i < 6; i++)
            dist[i] = Dot(M[i], sphere.center);
        for (int i = 0; i < 6; i += 2) {
            float dim = M[i].w + M[i + 1].w;
            if (dist[i] < - sphere.radius + dim) {
                // The object's sphere enchroaches into the R/S/T-positive plane but does not completely
                // overlap the box in this dimension.
                // Move the vertices associated with the opposite plane inward by
                // - sphere.radius + dim - dist so that the box represents the intersection
                // of the light volume box and the object's bounding sphere with respect to the
                // plane.
                MoveBoundingBoxVerticesInward(B, 8, M, i + 1, - sphere.radius + dim - dist[i]);
            }
            if (dist[i + 1] < - light.sphere.radius + dim) {
                // Move the first plane if the object's sphere encroaches the second plane.
                MoveBoundingBoxVerticesInward(B, 8, M, i, - sphere.radius + dim - dist[i + 1]);
            }
        }
        bool result = scissors.UpdateWithWorldSpaceBoundingBox(B, 8, frustum);
        if (!result)
            return SRE_COMPLETELY_OUTSIDE;
//        printf("Geometry scissors set for spotlight\n");
        return SRE_PARTIALLY_INSIDE;
    }
    // This should be unreachable.
    return SRE_COMPLETELY_INSIDE;
}

// Calculate an object's shadow volume pyramid for a point source light or spot light, based
// on the oriented bounding box of the object.
// Returns the bounding volume type (SRE_BOUNDING_VOLUME_PYRAMID if a shadow volume
// could be calculated, SRE_BOUNDING_VOLUME_EMPTY if the shadow volume is empty (which
// can happen with a completely flat object that is oriented parallel to the light direction),
// or SRE_BOUNDING_VOLUME_EVERYWHERE if no shadow volume could be calculated).

sreBoundingVolumeType SceneObject::CalculateShadowVolumePyramid(const Light& light, Point3D *Q,
int& n_convex_hull) const {
        bool P_included[8];
        if (box.PCA[2].SizeIsZero()) {
            // Flat plane object.
            // Have to check for light source lying in the flat plane.
            if (Dot(box.plane[4], light.vector) == 0) {
//                printf("Shadow volume is empty.\n");
                return SRE_BOUNDING_VOLUME_EMPTY;
            }
            for (int i = 0; i < 4; i++)
                P_included[i] = true;
        }
        else {
            // Calculate the dot product of all planes with the light source.
            float dot_L[6];
            for (int i = 0; i < 6; i++)
                dot_L[i] = Dot(box.plane[i], light.vector);
            for (int i = 0; i < 8; i++)
                P_included[i] = false;
            // Traverse all planes.
            bool empty = true;
            for (int i = 0; i < 6; i++)
                if (dot_L[i] < 0) {
                    // The plane is part of the convex hull.
                    P_included[BB_plane_vertex[i][0]] = true;
                    P_included[BB_plane_vertex[i][1]] = true;
                    P_included[BB_plane_vertex[i][2]] = true;
                    P_included[BB_plane_vertex[i][3]] = true;
                    empty = false;
                }
           if (empty) {
               // The light source is within the bounding box.
//               for (int i = 0; i < n; i++)
//                   printf("Dot %d = %f, L[i] = (%f, %f, %f, %f)\n", i, dot_L[i], L[i].x, L[i].y, L[i].z, L[i].w);
               return SRE_BOUNDING_VOLUME_EVERYWHERE;
           }
        }
        Point3D P[8];
        int n_vertices;
        ConstructBoundingBoxVertices(*this, P, n_vertices);
        // Construct a pyramid with its base far enough to be outside the influence of the light.
        // First define the normal direction from the light source to the center of the bounding box.
        Vector3D V = box.center - light.vector.GetPoint3D();
        Vector3D N = V;
        N.Normalize();
        // Set the apex of the pyramid to the light position.
        Q[0] = light.vector.GetPoint3D();
        bool tried_alternative = false;
again :
        // Define the plane of the base of the pyramid as the plane with the previously calculated
        // normal displaced from the light source by the light volume radius.
        Vector4D K = Vector4D(N, - Dot(N, light.vector.GetPoint3D() + N * light.sphere.radius));
        n_convex_hull = 1;
        for (int i = 0; i < n_vertices; i++) {
            if (P_included[i]) {
                // For each bounding box vertex that is part of the convex hull, project it
                // on the base of the pyramid.
                Vector3D W = P[i] - light.vector.GetPoint3D();
                float dot = Dot(K, W);
                if (dot <= 0) {
                    // The object is close to the light source and one of its bounding box corners
                    // that is part of the convex hull is at an angle of 90 degrees or more to the
                    // normal direction. Chose a different normal direction.
                    if (tried_alternative) {
                        // We already tried the alternative normal direction. Give up.
//                        printf("Alternative normal direction failed.\n");
                        return SRE_BOUNDING_VOLUME_EVERYWHERE;
                    }
                    // Try the normal direction of the smallest dimension of the object.
                    Vector3D N_previous = N;
                    if (box.PCA[2].SizeIsZero())
                        N = box.T_normal;
                    else
                        N = box.PCA[2].vector * box.PCA[2].scale_factor;
                    if (Dot(N, N_previous) < 0)
                        N = -N;
                    tried_alternative = true;
                    goto again;
                }
                float t = - Dot(K, light.vector.GetPoint3D()) / dot;
                Q[n_convex_hull] = light.vector.GetPoint3D() + t * W;
                n_convex_hull++;
            }
        }
//        if (tried_alternative)
//            printf("Alternative normal direction for pyramid base succesfully processed.\n");
//        printf("n_convex_hull = %d\n", n_convex_hull);
//        for (int i = 0; i < n_convex_hull; i++)
//            printf("(%f, %f, %f) ", Q[i].x, Q[i].y, Q[i].z);
//        printf("\n");
        return SRE_BOUNDING_VOLUME_PYRAMID;
}

// Calculate an object's shadow volume pyramid cone for a point source light or spot light, based
// on the oriented bounding box of the object.
// Returns the bounding volume type (SRE_BOUNDING_VOLUME_PYRAMID_CONE if a shadow volume
// could be calculated, SRE_BOUNDING_VOLUME_EMPTY if the shadow volume is empty (which
// can happen with a completely flat object that is oriented parallel to the light direction),
// or SRE_BOUNDING_VOLUME_EVERYWHERE if no shadow volume could be calculated).

sreBoundingVolumeType SceneObject::CalculatePointSourceOrSpotShadowVolume(
const Light& light, Point3D *Q, int& n_convex_hull, Vector3D& axis, float& radius,
float& cos_half_angular_size) const {
        bool P_included[8];
        if (box.PCA[2].SizeIsZero()) {
            // Flat plane object.
            // Have to check for light source lying in the flat plane.
            if (Dot(box.plane[4], light.vector) == 0) {
//                printf("Shadow volume is empty.\n");
                return SRE_BOUNDING_VOLUME_EMPTY;
            }
            for (int i = 0; i < 4; i++)
                P_included[i] = true;
        }
        else {
            // Calculate the dot product of all planes with the light source.
            float dot_L[6];
            for (int i = 0; i < 6; i++)
                dot_L[i] = Dot(box.plane[i], light.vector);
            for (int i = 0; i < 8; i++)
                P_included[i] = false;
            // Traverse all planes.
            bool empty = true;
            for (int i = 0; i < 6; i++)
                if (dot_L[i] < 0) {
                    // The plane is part of the convex hull.
                    P_included[BB_plane_vertex[i][0]] = true;
                    P_included[BB_plane_vertex[i][1]] = true;
                    P_included[BB_plane_vertex[i][2]] = true;
                    P_included[BB_plane_vertex[i][3]] = true;
                    empty = false;
                }
           if (empty) {
               // The light source is within the bounding box.
//               for (int i = 0; i < n; i++)
//                   printf("Dot %d = %f, L[i] = (%f, %f, %f, %f)\n", i, dot_L[i], L[i].x, L[i].y, L[i].z, L[i].w);
               return SRE_BOUNDING_VOLUME_EVERYWHERE;
           }
        }
        Point3D P[8];
        int n_vertices;
        ConstructBoundingBoxVertices(*this, P, n_vertices);
        // Construct a pyramid cone with the axis equal to the direction from the light source to the
        // center of the bounding box, and the axis length equal to the light volume radius.
        Vector3D V = box.center - light.vector.GetPoint3D();
        Vector3D N = V;
        N.Normalize();
        axis = N;
        radius = light.sphere.radius;
        Vector3D unnormalized_axis = axis * radius;
        // Set the apex of the pyramid to the light position.
        Q[0] = light.vector.GetPoint3D();
        n_convex_hull = 1;
        float min_cos_angle = 1.0f;
        for (int i = 0; i < n_vertices; i++) {
            if (P_included[i]) {
                // For each bounding box vertex that is part of the convex hull, calculate the edge from the
                // apex and make its length equal to the light sphere radius.
                Vector3D E = P[i] - light.vector.GetPoint3D();
                Vector3D E_normalized = E;
                E_normalized.Normalize();
                float cos_angle = Dot(axis, E_normalized);
                min_cos_angle = minf(min_cos_angle, cos_angle);
                if (min_cos_angle <= 0)
                    break;
                Q[n_convex_hull] = light.vector.GetPoint3D() + E_normalized * radius;
                n_convex_hull++;
            }
        }
        if (min_cos_angle > 0) {
            // The maximum angle between the axis and any edge is smaller than 90 degrees,
            // which means the pyramid cone can be used.
            cos_half_angular_size = min_cos_angle;
            return SRE_BOUNDING_VOLUME_PYRAMID_CONE;
        }
        // When the maximum angle is 90 degrees or greater, we have to find better shadow volume bounds.
        // such as a spherical sector.
        cos_half_angular_size = min_cos_angle;
        return SRE_BOUNDING_VOLUME_SPHERICAL_SECTOR;
}

// Create an object's geometrical shadow volume for a directional light. A half-cylinder
// (cylinder that is open-ended on one end) is created, based on the object's bounding sphere.
// Always returns the bounding volume type SRE_BOUNDING_VOLUME_HALF_CYLINDER.

sreBoundingVolumeType SceneObject::CalculateShadowVolumeHalfCylinderForDirectionalLight(
const Light &light, Point3D &E, float& cylinder_radius, Vector3D& cylinder_axis) const {
    // Calculate the endpoint. It is situated on the bounding sphere of the object in
    // the direction of where the light is (which is the inverse of the direction of the
    // light).
    E = sphere.center + sphere.radius * light.vector.GetVector3D();
    // The axis points in the direction of the light.
    cylinder_axis = - light.vector.GetVector3D();
    // Simply use the bounding sphere radius for the cylinder's radius.
    cylinder_radius = sphere.radius;
//    return SRE_BOUNDING_VOLUME_EVERYWHERE;
    return SRE_BOUNDING_VOLUME_HALF_CYLINDER;
}

// Create an object's geometrical shadow volume for a beam light. A cylinder is created,
// based on the object's bounding sphere. Normally returns the bounding volume type
// SRE_BOUNDING_VOLUME_CYLINDER; returns SRE_BOUNDING_VOLUME_EMPTY when the object is
// outside the light volume.

sreBoundingVolumeType sreObject::CalculateShadowVolumeCylinderForBeamLight(
const Light& light, Point3D& center, float& length, Vector3D& cylinder_axis,
float& cylinder_radius) const {
    // Define the plane through the beam light position, with normal in the beam light direction.
    // The axis from beam light's cylinder bounding volume is used, which should be the same as the
    // vector defined in light.spotlight.
    Vector4D K = Vector4D(light.cylinder.axis, - Dot(light.cylinder.axis, light.vector.GetPoint3D()));
    // Calculate the distance from the plane of the object's bounding sphere center.
    float dist = Dot(K, sphere.center);
    // Check whether the object's bounding sphere is in fact inside the light volume cylinder.
    if (dist <= - sphere.radius || dist >= light.cylinder.length + sphere.radius)
        return SRE_BOUNDING_VOLUME_EMPTY;
    // The first endpoint of the shadow volume cylinder will be equal to sphere.center moved one
    // sphere.radius in the direction of the light position, clipped to be within the light volume.
    float dist_endpoint1 = maxf(dist - sphere.radius, 0);
    // The second endpoint will be equal to sphere.center moved in the light direction to the
    // light cylinder's end of range. In terms of distance from the light position, this is simply
    // the length of the light's cylinder.
    float dist_endpoint2 = light.cylinder.length;
    // Set the shadow volume cylinder data.
    center = light.vector.GetPoint3D() + (dist_endpoint1 + dist_endpoint2) * 0.5f * light.cylinder.axis;
    length = dist_endpoint2 - dist_endpoint1;
    cylinder_axis = light.cylinder.axis;
    cylinder_radius = sphere.radius;
    // Note: The beam light volume has the special property of being delimited by the cylinder radius.
    // This could be taken advantage of by creating a smaller geometrical shadow volume if the object
    // is partly outside the beam. The fact that the shadow volume drawn by the GPU is based on the full
    // silhouette should not affect correctness. The smaller geometrical shadow volume would help
    // limit any geometry scissors while increasing the likelyhood of the shadow volume being
    // outside the frustum in an intersection test. However, it is not easy to express this
    // shadow volume in terms a cylinder (it comes down to circle-circle intersection tests).
    return SRE_BOUNDING_VOLUME_CYLINDER;
}

// Calculate the scissors (depth bounds only) of the geometrical shadow volume
// of the object. Returns true if the scissors region is not empty, false otherwise.
// The scissors region is not clipped to screen dimensions; it may be larger.
//
// Because geometry scissors are not applied to directional lights, it is assumed
// that that the shadow volume is of the SRE_BOUNDING_VOLUME_PYRAMID type which is used
// for point and spot lights. However, for beam lights, a potential cylinder-shader
// shadow volume would have to converted to a box.

bool SceneObject::CalculateShadowVolumeScissors(const Light& light, const Frustum& frustum,
const ShadowVolume& sv, sreScissors& shadow_volume_scissors) const {
    shadow_volume_scissors.InitializeWithEmptyRegion();
    return shadow_volume_scissors.UpdateWithWorldSpaceBoundingPyramid(sv.pyramid->vertex,
        sv.pyramid->nu_vertices, frustum);
}

// Temporary shadow volume structures used when a geometrical shadow volume has
// to be calculated, but not stored, on the fly.

static ShadowVolume sre_internal_sv_pyramid, sre_internal_sv_half_cylinder,
    sre_internal_sv_cylinder, sre_internal_sv_pyramid_cone, sre_internal_sv_spherical_sector;

void sreInitializeInternalShadowVolume() {
    sre_internal_sv_pyramid.type = SRE_BOUNDING_VOLUME_PYRAMID;
    sre_internal_sv_pyramid.pyramid = new sreBoundingVolumePyramid;
    // Maximum of 12 vertices.
    sre_internal_sv_pyramid.pyramid->AllocateStorage(12);
    sre_internal_sv_pyramid_cone.type = SRE_BOUNDING_VOLUME_PYRAMID_CONE;
    sre_internal_sv_pyramid_cone.pyramid_cone = new sreBoundingVolumePyramidCone;
    // Maximum of 12 vertices.
    sre_internal_sv_pyramid_cone.pyramid_cone->AllocateStorage(12);
    sre_internal_sv_spherical_sector.type = SRE_BOUNDING_VOLUME_SPHERICAL_SECTOR;
    sre_internal_sv_spherical_sector.spherical_sector = new sreBoundingVolumeSphericalSector;
    sre_internal_sv_half_cylinder.type = SRE_BOUNDING_VOLUME_HALF_CYLINDER;
    sre_internal_sv_half_cylinder.half_cylinder = new sreBoundingVolumeHalfCylinder;
    sre_internal_sv_cylinder.type = SRE_BOUNDING_VOLUME_CYLINDER;
    sre_internal_sv_cylinder.cylinder = new sreBoundingVolumeCylinder;
}

// Calculates or looks up the shadow volume for an object with respect to the
// given light and assigns it to sv. Sets a SRE_BOUNDING_VOLUME_PYRAMID_CONE,
// SRE_BOUNDING_VOLUME_HALF_CYLINDER or SRE_BOUNDING_VOLUME_CYLINDER if a shadow
// volume could be calculated, SRE_BOUNDING_VOLUME_EMPTY if it is empty, and
// SRE_BOUNDING_VOLUME_EVERYWHERE if no shadow volume could be calculated.
//
// The shadow volume is for temporary use and may only be valid until the next
// shadow volume is calculate using this function. It should not be freed.

void SceneObject::CalculateTemporaryShadowVolume(const Light& light, ShadowVolume **sv) const {
    // If the light does not produce changing shadow volumes out of itself,
    // and the object does not move, look up the precalculated shadow volume
    // in the object's static shadow volume list.
    if (!(light.type & SRE_LIGHT_DYNAMIC_SHADOW_VOLUME) && !(flags & SRE_OBJECT_DYNAMIC_POSITION)) {
        // Note: For directional lights this could work for moving objects as well by
        // translating the shadow volume. Since the object's bounding sphere is used to create
        // a directional light shadow volume, any rotation of the object would not affect its shape.
        *sv = LookupShadowVolume(sre_internal_current_light_index);
        if (*sv != NULL)
            return;
    }
    if (light.type & SRE_LIGHT_DIRECTIONAL) {
        *sv = &sre_internal_sv_half_cylinder;
        // Calculate the object's shadow volume for the directional light.
        // This is a very quick and simple calculation using the bounding sphere of the object.
        sreBoundingVolumeType t = CalculateShadowVolumeHalfCylinderForDirectionalLight(light,
             (*sv)->half_cylinder->endpoint, (*sv)->half_cylinder->radius, (*sv)->half_cylinder->axis);
        (*sv)->type = t;
        (*sv)->is_complete = true;
        return;
    }
    if (!(light.type & (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_SPOT))) {
        // Beam light. A cylinder-shaped bounding volume is assigned.
        *sv = &sre_internal_sv_cylinder;
        // This is a fairly simple calculation using the bounding sphere of the object.
        sreBoundingVolumeType t = CalculateShadowVolumeCylinderForBeamLight(light,
             (*sv)->cylinder->center, (*sv)->cylinder->length, (*sv)->cylinder->axis,
             (*sv)->cylinder->radius);
        (*sv)->type = t;
        (*sv)->is_complete = true;
        // Note: No neeed to calculate axis coefficients, since we never test intersection
        // of an AABB against a shadow volume (generally only a test of the shadow volume
        // against the convex hull of the frustum is performed).
        return;
    }
    // Point source or spot light.
#if 0
    *sv = &sre_internal_sv_pyramid;
    // Calculating a point light shadow volume requires a little work.
    sreBoundingVolumeType t = CalculateShadowVolumePyramid(light, (*sv)->pyramid->vertex,
        (*sv)->pyramid->nu_vertices);
    (*sv)->type = t;
    if (t == SRE_BOUNDING_VOLUME_PYRAMID)
        // Note: If an actual pyramid was calculated, it is not "completed" yet (plane
        // information), because it requires some relatively slow math operations.
        // CompleteParameters() is called when the plane data is required (for example
        // some intersection tests).
        (*sv)->is_complete = false;
    else
        (*sv)->is_complete = true;
#else
    *sv = &sre_internal_sv_pyramid_cone;
    // Calculating a point light shadow volume requires a little work.
    sreBoundingVolumeType t = CalculatePointSourceOrSpotShadowVolume(light, (*sv)->pyramid->vertex,
        (*sv)->pyramid->nu_vertices, (*sv)->pyramid_cone->axis, (*sv)->pyramid_cone->radius,
        (*sv)->pyramid_cone->cos_half_angular_size);
    (*sv)->type = t; // Normally SRE_BOUNDING_VOLUME_PYRAMID_CONE.
    (*sv)->is_complete = true;
    if (t == SRE_BOUNDING_VOLUME_SPHERICAL_SECTOR) {
        ShadowVolume *sv2;
        sv2 = &sre_internal_sv_spherical_sector;
        sv2->spherical_sector->radius = (*sv)->pyramid_cone->radius;
        sv2->spherical_sector->axis = (*sv)->pyramid_cone->axis;
        sv2->spherical_sector->cos_half_angular_size = (*sv)->pyramid_cone->cos_half_angular_size;
        // The following is very slow.
        sv2->spherical_sector->sin_half_angular_size = sinf(acosf((*sv)->pyramid_cone->cos_half_angular_size));
        *sv = sv2;
    }
#endif
    return;
}


// #define STATIC_OBJECT_DETERMINATION_LOG

// Calculate a list of all static object that intersect a light volume. Objects that
// don't receive light, but can cast shadows are also included.

void sreScene::DetermineStaticLightVolumeIntersectingObjects(const FastOctree& fast_oct,
int array_index, const Light& light, int &nu_intersecting_objects,
int *intersecting_object) const {
    int node_index = fast_oct.array[array_index];
    BoundsCheckResult r;
    if (light.type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE)
        // If a worst-case light volume is defined for an otherwise variable light,
        // use the (worst-case) sphere bounds.
        r = QueryIntersection(fast_oct.node_bounds[node_index], light.sphere);
    else
        r = QueryIntersection(fast_oct.node_bounds[node_index], light);
    if (r == SRE_COMPLETELY_OUTSIDE) {
#ifdef STATIC_OBJECT_DETERMINATION_LOG
        printf("Octree %d is completely outside the light volume.\n", node_index);
#endif
        return;
    }
    int nu_octants = fast_oct.array[array_index + 1] & 0xFF;
    int nu_entities = fast_oct.array[array_index + 2];
#ifdef STATIC_OBJECT_DETERMINATION_LOG
    printf("Octree %d (%d entities) intersects the light volume.\n", node_index,
        nu_entities);
    printf("Octree extents (%f, %f, %f), (%f, %f, % f)",
        fast_oct.node_bounds[node_index].AABB.dim_min.x, fast_oct.node_bounds[node_index].AABB.dim_min.y,
        fast_oct.node_bounds[node_index].AABB.dim_min.z, fast_oct.node_bounds[node_index].AABB.dim_max.x,
        fast_oct.node_bounds[node_index].AABB.dim_max.y, fast_oct.node_bounds[node_index].AABB.dim_max.z);
    if (nu_octants > 0) {
        Point3D middle_point;
        // Get the array index where the data for the first defined octant is stored.
        int octant_array_index = fast_oct.array[array_index + 3 + nu_entities];
        int octant_node_index = fast_oct.array[octant_array_index];
        // Get the bounds of the first octant.
        sreBoundingVolumeAABB octant_AABB;
        octant_AABB = fast_oct.node_bounds[octant_node_index].AABB;
        // Either the min or max of each coordinate of the octant bounds will represent the middle
        // point, depending on which octant is the first one. The right value will be in between
        // the node bounds.
        float epsilon = 0.01f * (octant_AABB.dim_max.x - octant_AABB.dim_min.x);
        middle_point.Set(
            octant_AABB.dim_min.x > fast_oct.node_bounds[node_index].AABB.dim_min.x + epsilon ?
            octant_AABB.dim_min.x : octant_AABB.dim_max.x,
            octant_AABB.dim_min.y > fast_oct.node_bounds[node_index].AABB.dim_min.y + epsilon ?
            octant_AABB.dim_min.y : octant_AABB.dim_max.y,
            octant_AABB.dim_min.z > fast_oct.node_bounds[node_index].AABB.dim_min.z + epsilon ?
            octant_AABB.dim_min.z : octant_AABB.dim_max.z);
        printf(", middle point (%f, %f, %f)\n", middle_point.x, middle_point.y, middle_point.z);
    }
    else
        printf("\n");
    if (r == SRE_COMPLETELY_INSIDE)
        printf("Note: Octree is completely inside the light volume.\n");
#endif
    array_index += 3;
    for (int i = 0; i < nu_entities; i++) {
        SceneEntityType type;
        int index;
        fast_oct.GetEntity(array_index + i, type, index);
        if (type == SRE_ENTITY_OBJECT) {
            if ((sceneobject[index]->flags & (SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_CAST_SHADOWS))
            == SRE_OBJECT_EMISSION_ONLY)
                // Skip emission-only objects that do not cast shadows.
                continue;
            // If the octree is completely inside the light volume, we don't need to check
            // whether the object is inside the light volume.
            if (r != SRE_COMPLETELY_INSIDE) {
                 // If a worst-case light volume is defined for an otherwise variable light,
                 // use the (worst-case) sphere bounds.
                 if (light.type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE) {
                     if (!Intersects(*sceneobject[index], light.worst_case_sphere))
                         // The object is outside the worst-case light volume;
                         continue;
                 }
                 else if (!Intersects(*sceneobject[index], light))
                     // The object is outside the light volume;
                     continue;
#ifdef STATIC_OBJECT_DETERMINATION_LOG
                printf("Adding object %d (intersection test).\n", sceneobject[index]->id);
#endif
            }
#ifdef STATIC_OBJECT_DETERMINATION_LOG
            else
                printf("Adding object %d (whole octree is inside).\n", sceneobject[index]->id);
#endif
            intersecting_object[nu_intersecting_objects] = index;
            nu_intersecting_objects++;
        }
    }
    array_index += nu_entities;
    for (int i = 0; i < nu_octants; i++)
        DetermineStaticLightVolumeIntersectingObjects(fast_oct, fast_oct.array[array_index + i], light,
            nu_intersecting_objects, intersecting_object);
}

// Calculate static object lists for the light. For local lights, both shadow casters and
// objects within the light volume are determined (with seperation of objects that are
// completely as opposed to partially inside the light volume). For directional lights,
// a list of objects within the light volume wouldn't make much sense, but we can precalculate
// the light volume half cylinder with every object.

void sreScene::CalculateStaticLightObjectLists() {
    printf("Calculating static shadow volumes and static object lists for lights.\n");
    // Keep track of the number of static lights for which an object
    // is partially inside the light volume (this will be used to allocate
    // geometry scissors cache slots).
    int *object_partially_inside_light_volume_count = new int[nu_objects];
    memset(object_partially_inside_light_volume_count, 0, sizeof(int) * nu_objects);
    int *intersecting_object = new int[nu_objects];
    BoundsCheckResult *intersection_test_result = new BoundsCheckResult[nu_objects];
    for (int i = 0; i < nu_lights; i++) {
        int nu_intersecting_objects = 0;
        // Determine objects that intersect the volume. This uses bounding volume checks;
        // more accurate tests may be tried later.
        // We are only interested in local lights (for dynamic position lights,
        // the object lists will be initialized to a size of zero).
        if ((!(global_light[i]->type & SRE_LIGHT_DYNAMIC_SHADOW_VOLUME) ||
        (global_light[i]->type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE)) ||
        (!(global_light[i]->type & (SRE_LIGHT_DYNAMIC_LIGHT_VOLUME | SRE_LIGHT_DIRECTIONAL)) ||
        (global_light[i]->type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE)))
            DetermineStaticLightVolumeIntersectingObjects(fast_octree_static, 0, *global_light[i],
                nu_intersecting_objects, intersecting_object);
//        printf("%d light volume intersecting objects.\n", nu_intersecting_objects); //DEBUG
        // Create a list of potential shadow casters within the light volume (non-directional lights).
        // Calculate static shadow volumes for static objects when the light has a non-changing shadow
        // volumes for a static object, including directional lights. For a directional light and
        // large scene, the resource requirements are not extreme (directional light shadow volume
        // half-cylinders are quickly calculated and do not require much memory). Lots of local
        // lights that affect significant numbers of objects are potentially more expensive.
        // We also generate a static objects list and shadow caster list for local lights that are
        // variable but have a worst-case bounding sphere.
        if (!(global_light[i]->type & SRE_LIGHT_DYNAMIC_SHADOW_VOLUME) ||
        (global_light[i]->type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE)) {
            nu_shadow_caster_objects = 0;
            for (int k = 0; k < nu_intersecting_objects; k++) {
                int j = intersecting_object[k];
                SceneObject *so = sceneobject[j];
                // Dynamic objects shouldn't be encountered, but check anyway.
                if (so->flags & SRE_OBJECT_DYNAMIC_POSITION)
                    continue;
                // Only need to include shadow casters.
                if (!(so->flags & SRE_OBJECT_CAST_SHADOWS))
                    continue;
                // If the object is attached to the current light, don't cast shadows for this object.
                if (so->attached_light == i)
                    continue;
                // For all lights except directional lights, add the object to the list
                // of shadow casters.
                if (!(global_light[i]->type & SRE_LIGHT_DIRECTIONAL)) {
                    // Add the object to the list of shadow casters for the light.
                    // For lights with SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE set, these are only
                    // potential shadow casters.
                    shadow_caster_object[nu_shadow_caster_objects] = j;
                    nu_shadow_caster_objects++;
                }
                // When the light's shadow volumes are static for a static object,
                // calculate the shadow volume and add it to the object's list of shadow volumes.
                // SRE_LIGHT_DYNAMIC_SHADOW_VOLUME is expected to have been set appropriately
                // when the light was added to the scene, depending on light type, and on whether
                // the position, direction, range etc was marked as dynamic or not.
                if (global_light[i]->type & SRE_LIGHT_DYNAMIC_SHADOW_VOLUME)
                    continue;
                if (global_light[i]->type & (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_SPOT)) {
#if 0
                    // Point and spot light create pyramid-shaped shadow volumes.
                    Point3D Q[12];
                    int n_convex_hull;
                    // Calculate the shadow volume pyramid for the object.
                    sreBoundingVolumeType t = so->CalculateShadowVolumePyramid(*global_light[i],
                        Q, n_convex_hull);
                    ShadowVolume *sv = new ShadowVolume;
                    if (t == SRE_BOUNDING_VOLUME_EMPTY)
                        sv->SetEmpty();
                    else
                    if (t == SRE_BOUNDING_VOLUME_EVERYWHERE)
                        sv->SetEverywhere();
                    else
                        sv->SetPyramid(Q, n_convex_hull);
#else
                    // Point and spot light create pyramid cone-shaped shadow volumes.
                    Point3D Q[12];
                    int n_convex_hull;
                    Vector3D axis;
                    float radius;
                    float cos_half_angular_size;
                    // Calculate the shadow volume pyramid cone for the object.
                    sreBoundingVolumeType t = so->CalculatePointSourceOrSpotShadowVolume(*global_light[i],
                        Q, n_convex_hull, axis, radius, cos_half_angular_size);
                    ShadowVolume *sv = new ShadowVolume;
                    if (t == SRE_BOUNDING_VOLUME_EMPTY)
                        sv->SetEmpty();
                    else if (t == SRE_BOUNDING_VOLUME_EVERYWHERE)
                        sv->SetEverywhere();
                    else if (t == SRE_BOUNDING_VOLUME_PYRAMID_CONE)
                        sv->SetPyramidCone(Q, n_convex_hull, axis, radius, cos_half_angular_size);
                    else
                        sv->SetSphericalSector(axis, radius, cos_half_angular_size);
#endif
                    sv->light = i;
                    so->AddShadowVolume(sv);
                }
                else if (global_light[i]->type & SRE_LIGHT_DIRECTIONAL) {
                    // Directional lights create half cylinder (cylinder with no top)
                    // -shaped shadow volumes (based on the object's bounding sphere).
                    float cylinder_radius;
                    Vector3D cylinder_axis;
                    Point3D E;
                    so->CalculateShadowVolumeHalfCylinderForDirectionalLight(
                        *global_light[i], E, cylinder_radius, cylinder_axis);
                    ShadowVolume *sv = new ShadowVolume;
                    sv->SetHalfCylinder(E, cylinder_radius, cylinder_axis);
                    sv->light = i;
                    so->AddShadowVolume(sv);
                }
                else if (global_light[i]->type & SRE_LIGHT_BEAM) {
                    // Beam lights. The shadow volume will be a regular cylinder
                    // (based on the object's bounding sphere).
                    Point3D center;
                    float length;
                    Vector3D cylinder_axis;
                    float cylinder_radius;
                    so->CalculateShadowVolumeCylinderForBeamLight(
                        *global_light[i], center, length, cylinder_axis, cylinder_radius);
                    ShadowVolume *sv = new ShadowVolume;
                    sv->SetCylinder(center, length, cylinder_axis, cylinder_radius);
                    sv->light = i;
                    so->AddShadowVolume(sv);
                }
            }
            global_light[i]->nu_shadow_caster_objects = nu_shadow_caster_objects;
            if (!(global_light[i]->type & SRE_LIGHT_DIRECTIONAL)) {
                // Copy the shadow caster list into the light's structure.
                if (nu_shadow_caster_objects > 0) {
                    global_light[i]->shadow_caster_object = new int[nu_shadow_caster_objects];
                    memcpy(global_light[i]->shadow_caster_object, shadow_caster_object,
                        nu_shadow_caster_objects * sizeof(int));
                }
                if (sre_internal_debug_message_level >= 2)
                    printf("Light %d: %d shadow casters within light volume.\n", i,
                        nu_shadow_caster_objects);
                // Set the flag indicating there is a list containing all static shadow casters
                // for the light.
                global_light[i]->type |= SRE_LIGHT_STATIC_SHADOW_CASTER_LIST;
           }
        }
        else
            global_light[i]->nu_shadow_caster_objects = 0;
        // Calculate static object list for the light. Only applied to static lights
        // (not directional) that have a fixed light volume. However, for stationary lights
        // or lights that can only move in a fixed range that have an established worst case
        // light volume bounding sphere (SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE), such as
        // spot light that can only change direction, or a point light that can
        // change attenuation (range) up to a known limit), and a limited position movement
        // range, we can also calculate a list.
        if (!(global_light[i]->type & (SRE_LIGHT_DYNAMIC_LIGHT_VOLUME | SRE_LIGHT_DIRECTIONAL)) ||
        (global_light[i]->type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE)) {
            nu_visible_objects = 0;
            // First the objects that are partially inside the light volume.
            // Note: More accurate intersection tests, can be used, since this
            // function is not very time-sensitive. Every vertex of the object can
            // be checked (after bounding volume tests suggest intersection).
            for (int k = 0; k < nu_intersecting_objects; k++) {
                int j = intersecting_object[k];
                SceneObject *so = sceneobject[j];
                if (so->flags & SRE_OBJECT_DYNAMIC_POSITION)
                    continue;
                // Emission-only objects are not affected by the light.
                if (so->flags & SRE_OBJECT_EMISSION_ONLY)
                    continue;
                // Use the full intersection test that tests every vertex. Unless the scene is
                // very large with a large number of lights, this should be acceptable for an
                // preprocessing function. The extra accuracy should result in some rendering
                // performance gains.
#ifdef STATIC_OBJECT_DETERMINATION_LOG
                printf("Object LOD levels: %d, vertices in level 0: %d\n",
                     so->model->nu_lod_levels, so->model->lod_model[0]->nu_vertices);
#endif
                BoundsCheckResult r;
                if (global_light[i]->type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE) {
                    // Use the light's worst-case light volume.
                    r = QueryIntersectionFull(*so, *global_light[i], true);
                    // If we only have worst case bounds, treat all objects that intersect with
                    // them as partially inside.
                    if (r == SRE_COMPLETELY_INSIDE)
                        r = SRE_PARTIALLY_INSIDE;
                }
                else
                    r = QueryIntersectionFull(*so, *global_light[i]);
                // Store the intersection test result for later use below (note: indexed
                // with index into intersecting object array, not global object index/id).
                intersection_test_result[k] = r;
                if (sre_internal_debug_message_level >= 2)
                    if (r == SRE_PARTIALLY_INSIDE &&
                    QueryIntersection(*so, *global_light[i]) == SRE_COMPLETELY_INSIDE)
                        printf("Object bounding volumes completely inside light volume, "
                            "but at least one LOD model vertex is actually outside the light volume.\n");
                // Only store objects partially inside the light volume for now.
                if (r != SRE_PARTIALLY_INSIDE)
                    continue;
                visible_object[nu_visible_objects] = j;
                nu_visible_objects++;
                // Count the number of lights for which an object is partially inside the light
                // volume.
                object_partially_inside_light_volume_count[j]++;
            }
            global_light[i]->nu_visible_objects_partially_inside = nu_visible_objects;
            // Secondly the objects that are completely inside the light volume.
            for (int k = 0; k < nu_intersecting_objects; k++) {
                int j = intersecting_object[k];
                SceneObject *so = sceneobject[j];
                if (so->flags & SRE_OBJECT_DYNAMIC_POSITION)
                    continue;
                if (so->flags & SRE_OBJECT_EMISSION_ONLY)
                    continue;
                if (global_light[i]->type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE)
                    // If we only have worst case bounds, no object is completely inside (all
                    // objects were already stored as partially inside).
                    continue;
                // Since precisely the same objects were considered in the earlier (partially inside)
                // loop, the intersection test result should be valid.
                BoundsCheckResult r = intersection_test_result[k];
                // Only store objects completely inside the light volume.
                if (sre_internal_debug_message_level >= 2)
                    if (r == SRE_COMPLETELY_INSIDE &&
                    QueryIntersection(*so, *global_light[i]) == SRE_PARTIALLY_INSIDE)
                        printf("Object bounding volumes partially inside light volume, "
                            "but every LOD model vertex is actually inside the light volume.\n");
                if (r != SRE_COMPLETELY_INSIDE)
                    continue;
                visible_object[nu_visible_objects] = j;
                nu_visible_objects++;
            }
            global_light[i]->type |= SRE_LIGHT_STATIC_OBJECTS_LIST;
            global_light[i]->nu_visible_objects = nu_visible_objects;
            if (nu_visible_objects > 0) {
                global_light[i]->visible_object = new int[nu_visible_objects];
                memcpy(global_light[i]->visible_object, visible_object, nu_visible_objects * sizeof(int));
            }
            if (sre_internal_debug_message_level >= 2)
                printf("Light %d: %d objects within light volume, %d partially inside.\n", i,
                    nu_visible_objects, global_light[i]->nu_visible_objects_partially_inside);
        }
        else
            global_light[i]->nu_visible_objects = 0;
    }
    delete [] intersecting_object;
    delete [] intersection_test_result;
    // Create a geometry scissors cache for each static object that is partially inside
    // the light volume of one or more static lights. For other objects, the geometry
    // scissors cache will never be used.
    for (int i = 0; i < nu_objects; i++)
        if (object_partially_inside_light_volume_count[i] > 0)
            sceneobject[i]->geometry_scissors_cache =
                new sreScissors[object_partially_inside_light_volume_count[i]];
    delete [] object_partially_inside_light_volume_count;
}

