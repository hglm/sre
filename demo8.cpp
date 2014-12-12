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
#define _USE_MATH_DEFINES
#include <math.h>

#include "sre.h"
#include "sreBackend.h"
#include "sreRandom.h"

#include "demo.h"

#define ROBOTS
#define MOVE_ROBOTS

#define MAX_ROBOTS 16
// Light attenuation range of robots. Longer range means more visible
// shadows, but should mean slower FPS.
#define ROBOT_LIGHT_RANGE 35.0f
// #define ROBOT_LIGHT_RANGE 20.0f

#ifdef OPENGL
#define NU_STARS 1024
#else
#define NU_STARS 128
#endif
#define TWINKLING_STARS

class Robot {
public :
    Point3D initial_pos;
    int object_index;
    Point3D target_pos;
    int light_object_index;
};

static int nu_robots;
Robot robot[MAX_ROBOTS];

static Color robot_color[2] = {
    Color(1.0, 1.0, 1.0),
    Color(1.0, 0, 0)
};

static Color robot_light_color[2] = {
    Color(1.0, 1.0, 1.0),
    Color(1.0, 0.8, 0.8)
};

static void initialize_robots() {
    nu_robots = 8;
    for (int i = 0; i < nu_robots; i++) {
        if (nu_robots == 1)
            robot[i].initial_pos.x = 20.0 + 4.0 * 20;
        else
            robot[i].initial_pos.x = 20.0 + (float)i * 20;
        robot[i].initial_pos.y = 180.0;
        robot[i].initial_pos.z = 5.0;
        robot[i].target_pos = robot[i].initial_pos;
    }
}

static float calculate_grating_dim_x(int NU_HOLES_X, int NU_HOLES_Y, float BORDER_WIDTH, float GAP_WIDTH,
float BAR_WIDTH, float THICKNESS) {
	return BORDER_WIDTH * 2 + NU_HOLES_X * GAP_WIDTH + (NU_HOLES_X - 1) * BAR_WIDTH;
}

static float calculate_grating_dim_y(int NU_HOLES_X, int NU_HOLES_Y, float BORDER_WIDTH, float GAP_WIDTH,
float BAR_WIDTH, float THICKNESS) {
	return BORDER_WIDTH * 2 + NU_HOLES_Y * GAP_WIDTH + (NU_HOLES_Y - 1) * BAR_WIDTH;
}

static int corner_light_object_index[4];
static int corner_light_index[4];
static int beam_light;
static int star_object_index[NU_STARS];
static float star_object_billboard_size[NU_STARS];
static float star_billboard_size_start[NU_STARS];
static float star_billboard_size_target[NU_STARS];
static double star_last_twinkle_time[NU_STARS];
static float star_dec[NU_STARS];

static dstRNG *rng;

void Demo8CreateScene(sreScene *scene, sreView *view) {
    int l;
    rng = dstGetDefaultRNG();

    scene->SetAmbientColor(Color(0.15, 0.15, 0.15));
    sreModel *sphere_model = sreCreateSphereModel(scene, 0);
    // Add player sphere as scene object 0
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS |
        SRE_OBJECT_USE_TEXTURE);
    scene->SetTexture(sreCreateStripesTexture(TEXTURE_TYPE_LINEAR,
        256, 256, 32, Color(0, 0.5f, 0.8f), Color(0.9f, 0.9f, 1.0f)));
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->AddObject(sphere_model, Point3D(97.0f, - 40.0f, 3.0f), Vector3D(0, 0, 0), 3.0f);
#ifdef CONTROL_OBJECT_LIGHT
    // Add a light source at the center of the controllable sphere.
    // Because the sphere naturally rolls, it is hard to realistically
    // associate a light with it.
    l = scene->AddPointSourceLight(SRE_LIGHT_DYNAMIC_POSITION,
            Point3D(100, 50, 3.0), 20.0, // Linear range of 20.
            Color(1.0, 1.0, 1.0));
//    scene->AttachLight(0, l, Vector3D(0, 0, 0), Vector3D(0, 0, 0));
    scene->SetEmissionColor(Color(0, 0, 0));
#endif

    // Add floor.
    sreModel *checkerboard_model = sreCreateCheckerboardModel(scene, 20, 10,
        Color(0.5, 0.1, 0.1), Color(0.1, 0.1, 0.5));
    scene->SetFlags(SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_NO_PHYSICS);
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    int i = scene->AddObject(checkerboard_model, 0, 0, 0, 0, 0, 0, 1);

    // Add boundary wall with blocks.
    sreModel *block_model = sreCreateUnitBlockModel(scene);
    sreTexture *marble_texture = new sreTexture("Marble9", TEXTURE_TYPE_NORMAL);
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    // Just corners.
    for (int i = 0; i < 20; i += 19) {
            scene->AddObject(block_model, (float)i * 10.0, 0, 0, 0, 0, 0, 10.0);
            scene->AddObject(block_model, (float)i * 10.0, 190.0, 0, 0, 0, 0, 10.0);
    }
#if 0
    for (int i = 0; i < 18; i++) {
            scene->AddObject(block_model, 0, 10.0 + (float)i * 10.0, 0, 0, 0, 0, 10.0);
            scene->AddObject(block_model, 190.0, 10.0 + (float)i * 10.0, 0, 0, 0, 0, 10.0);
    }
#endif
    // Add globe lights in the corners.
    scene->SetFlags(0);
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 0.6));
    scene->SetSpecularReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetEmissionColor(Color(1.0, 1.0, 0.6));
    int j = scene->AddObject(sphere_model, 5.0, 5.0, 30.0, 0, 0, 0, 5.0);
    corner_light_object_index[0] = j;
    l = scene->AddPointSourceLight(0, Point3D(5.0, 5.0, 30.0), 50.0, Color(1.0, 1.0, 0.8));
    corner_light_index[0] = l;
    j = scene->AddObject(sphere_model, 195.0, 5.0, 30.0, 0, 0, 0, 5.0);
    corner_light_object_index[1] = j;
    l = scene->AddPointSourceLight(0, Point3D(195.0, 5.0, 30.0), 50.0, Color(1.0, 1.0, 0.8));
    corner_light_index[1] = l;
    j = scene->AddObject(sphere_model, 5.0, 195.0, 30.0, 0, 0, 0, 5.0);
    corner_light_object_index[2] = j;
    l = scene->AddPointSourceLight(0, Point3D(5.0, 195.0, 30.0), 50, Color(1.0, 1.0, 0.8));
    corner_light_index[2] = l;
    j = scene->AddObject(sphere_model, 195.0, 195.0, 30.0, 0, 0, 0, 5.0);
    corner_light_object_index[3] = j;
    l = scene->AddPointSourceLight(0, Point3D(195.0, 195.0, 30.0), 50.0, Color(1.0, 1.0, 0.8));
    corner_light_index[3] = l;
    // Restore defaults.
    scene->SetSpecularReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetEmissionColor(Color(0, 0, 0));
    // Add the pedestals for the globe lights.
    sreModel *pedestal = sreCreateBlockModel(scene, 0.5, 0.5, 15.0, 0);
    scene->SetEmissionColor(Color(0, 0.4, 0));
    // Because the point light above the pedestal gets blocked, so the sides of the pedestal are not
    // illuminated, treat the pedestal as an emission source, and don't render shadows to avoid a
    // spurious shadow at the base.
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY);
    scene->AddObject(pedestal, 4.75, 4.75, 10.0, 0, 0, 0, 1.0);
    scene->AddObject(pedestal, 194.75, 4.75, 10.0, 0, 0, 0, 1.0);
    scene->AddObject(pedestal, 4.75, 194.75, 10.0, 0, 0, 0, 1.0);
    scene->AddObject(pedestal, 194.75, 194.75, 10.0, 0, 0, 0, 1.0);

    // Add gratings
    sreModel *grating_model = sreCreateGratingModel(scene, 10, 10, 0.2, 0.9, 0.1, 0.2);
    float grating_dim_x = calculate_grating_dim_x(10, 10, 0.2, 0.9, 0.1, 0.2);
    float grating_dim_y = calculate_grating_dim_y(10, 10, 0.2, 0.9, 0.1, 0.2);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    scene->SetDiffuseReflectionColor(Color(0.5, 0.8, 0.2));
    scene->SetEmissionColor(Color(0, 0, 0));
    scene->AddObject(grating_model, 10.0, 100.0 + 0.4, 0, M_PI / 2, 0, 0, 4.0);
    scene->AddObject(grating_model, 190.0 - grating_dim_x * 4.0, 100.0 + 0.4, 0, M_PI / 2, 0, 0, 4.0);
    // Add a pedestal lights before the grating, producing nice shadows.
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY);
    scene->SetEmissionColor(Color(0.4f, 0.4f, 0.4f));
    Point3D P1 = Point3D(10.0f + grating_dim_x * 4.0f * 0.5f - 0.25f, 87.0f, 0);
    Point3D P2 = Point3D(190.0f - grating_dim_x * 4.0f * 0.5f - 0.25f, 87.0f, 0);
    // Pedestal model block is 0.5 wide. Scale by 2.5f, making the height 37.5.
    float grating_light_pedestal_scale = 2.0f;
    float grating_light_pedestal_height = 15.0f * grating_light_pedestal_scale;
    Vector3D grating_light_pedestal_position_offset =
        - Vector3D(1.0f, 1.0f, 0) * grating_light_pedestal_scale * 0.5f * 0.5f;
    scene->AddObject(pedestal, P1 + grating_light_pedestal_position_offset, Vector3D(0, 0, 0),
        grating_light_pedestal_scale);
    scene->AddObject(pedestal, P2 + grating_light_pedestal_position_offset, Vector3D(0, 0, 0),
        grating_light_pedestal_scale);
    // Add spheres representing the lights. Although bright, some light can additionally fall on them.
    // They better not cast shadows because that may block the light sources inside them.
    scene->SetFlags(0);
    scene->SetDiffuseReflectionColor(Color(0.9f, 0.9f, 0.9f));
    scene->SetEmissionColor(Color(0.9f, 0.9f, 0.9f));
    scene->AddObject(sphere_model, P1 + Vector3D(0, 0, grating_light_pedestal_height + 2.0f),
        Vector3D(0, 0, 0), 2.0f);
    scene->AddObject(sphere_model, P2 + Vector3D(0, 0, grating_light_pedestal_height + 2.0f),
        Vector3D(0, 0, 0), 2.0f);
    // Add the light sources for the pedestal lights. Range 55.
    // Although unphysical except with HDR rendering, the lights are extra bright.
    scene->AddPointSourceLight(0, P1 + Vector3D(0, 0, grating_light_pedestal_height + 2.0f),
        55.0f, Color(1.5f, 1.5f, 1.5f));
    scene->AddPointSourceLight(0, P2 + Vector3D(0, 0, grating_light_pedestal_height + 2.0f),
        55.0f, Color(1.5f, 1.5f, 1.5f));

    // Add beam light in the middle of the area.
    beam_light = scene->AddBeamLight(SRE_LIGHT_DYNAMIC_DIRECTION,
        Point3D(100.0, 100.0, 100.0), Vector3D(0, 0, - 1.0), 10.0, 10.0, 150.0, 1000.0, Color(1.0, 1.0, 1.0));

#ifdef ROBOTS
    // Add spherical robots (large ones that follow smaller ones that give light).
    initialize_robots();
    sreTexture *emission_map = new sreTexture("globe_emission_map", TEXTURE_TYPE_NORMAL);
    for (int i = 0; i < nu_robots; i++) {
        scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_USE_EMISSION_MAP);
        scene->SetEmissionColor(Color(0.5, 0.5, 0));
        scene->SetEmissionMap(emission_map);
        scene->SetDiffuseReflectionColor(robot_color[i & 1]);
        scene->SetMass(0.5);
        int object_index = scene->AddObject(sphere_model, robot[i].initial_pos.x, robot[i].initial_pos.y,
            robot[i].initial_pos.z, 0, 0, 0, 5.0);
        robot[i].object_index = object_index;
        scene->SetDiffuseReflectionColor(robot_light_color[i & 1]);
        scene->SetEmissionColor(robot_light_color[i & 1]);
        scene->SetMass(0.2);
        scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS);
        object_index = scene->AddObject(sphere_model, robot[i].initial_pos.x, robot[i].initial_pos.y,
            robot[i].initial_pos.z + 7.0, 0, 0, 0, 2.0);
        robot[i].light_object_index = object_index;
#if 1
        // Add lights attached to sphere robots.
        int l = scene->AddPointSourceLight(SRE_LIGHT_DYNAMIC_POSITION,
            Point3D(robot[i].initial_pos.x, robot[i].initial_pos.y, robot[i].initial_pos.z + 7.0),
            ROBOT_LIGHT_RANGE, Color(1.0, 1.0, 1.0));
        scene->AttachLight(object_index, l, Vector3D(0, 0, 0), Vector3D(0, 0, 0));
#endif
    }
    scene->SetMass(0);
#endif

    // Landscape "lights".
    sreModel *sphere_model_simple = sreCreateSphereModelSimple(scene, 0);
    scene->SetFlags(SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_EMISSION_ONLY);
//    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    for (int x = 0; x < 51; x++)
        for (int y = 0; y < 51; y++) {
            scene->AddObject(sphere_model_simple, (x - 25) * 400.0 - 100.0, (y - 25) * 400.0 - 100.0, 0, 0, 0, 0, 1.0);
        }
    // Landscape fields.
    sreModel *field_object = sreCreateRepeatingRectangleModel(scene, 200.0, 200.0);
//    scene->SetFlags(SRE_OBJECT_NO_PHYSICS | OBJECT_FLAG_IS_LIGHTSOURCE | OBJECT_FLAG_NO_SMOOTH_SHADING);
    scene->SetFlags(SRE_OBJECT_NO_PHYSICS);
    scene->SetDiffuseReflectionColor(Color(0, 0, 1.0));
    scene->SetEmissionColor(Color(0, 0, 0));
    for (int x = 0; x < 51; x++)
        for (int y = 0; y < 51; y++) {
            if (x != 25 || y != 25)
                scene->AddObject(field_object, (x - 25) * 400.0, (y - 25) * 400.0, 0, 0, 0, 0, 1.0);
        }
    // Landscape pedestal lights.
    sreModel *pedestal2 = sreCreateBlockModel(scene, 0.5, 0.5, 20.0, 0);
    for (int x = 0; x < 51; x++)
        for (int y = 0; y < 51; y++) {
            if (x == 25 && y == 25)
                continue;
            scene->SetFlags(SRE_OBJECT_EMISSION_ONLY);
            scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
            scene->AddObject(sphere_model, (x - 25) * 400.0 + 100.0, (y - 25) * 400.0 + 100.0, 23.0, 0, 0, 0, 3.0);
            scene->SetFlags(SRE_OBJECT_EMISSION_ONLY);
            scene->SetEmissionColor(Color(0, 0.4, 0));
            scene->AddObject(pedestal2, (x - 25) * 400 + 100.0, (y - 25) * 400.0 + 100.0, 0, 0, 0, 0, 1.0);
//            if (x >= 15 && x <= 35 && y >= 15 && y <= 35)
                scene->AddPointSourceLight(0,
                    Point3D((x - 25) * 400.0 + 100.0, (y - 25) * 400.0 + 100.0, 23.0),
                    100.0, Color(1.0, 1.0, 1.0));
        }
    // Landscape cylinders.
    sreModel *cylinder_model = sreCreateCylinderModel(scene, 8.0f, true, true);
    scene->SetEmissionColor(Color(0, 0, 0));
//    sreTexture *cylinder_texture = new sreTexture("StonesAndBricks5", TEXTURE_TYPE_NORMAL);
    scene->SetTexture(marble_texture);
    // Reduce the LOD a little. 
    scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 0, - 1, 2.0f, 1);
    for (int x = 20; x < 31; x++)
        for (int y = 20; y < 31; y++) {
            if (x == 25 && y == 25)
                continue;
            scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_USE_TEXTURE);
            scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
            scene->AddObject(cylinder_model, (x - 25) * 400.0 + 120.0, (y - 25) * 400.0 + 120.0, 10.0, M_PI / 2, 0, 0, 10.0);
    }
    scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 0, - 1, 1.0f, 0);

    // Add star halos in the distance.
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY |
        SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_LIGHT_HALO | SRE_OBJECT_NO_PHYSICS |
        SRE_OBJECT_INFINITE_DISTANCE);
//    sreModel *billboard_object = sreCreateBillboardModel(scene, true);
    for (int i = 0 ; i < NU_STARS; i++) {
        // Have a seperate billboard model for each star may be preferable.
        // Ideally, a special particle system would have to be used for which
        // the halo size would be configurable as a vertex attribute for each
        // particle (billboard).
        sreModel *billboard_object = sreCreateBillboardModel(scene, true);
        int r = rng->RandomInt(7);
        Color c;
        switch (r) {
        case 0 :
            c.r = 0.8;
            c.g = 0.4;
            c.b = 0.4;
            break;
        case 1 :
        case 2 :
            c.r = 0.8;
            c.g = 0.8;
            c.b = 0.4;
            break;
        default :
            c.r = 0.8;
            c.g = 0.8;
            c.b = 0.8;
            break;
        }
        scene->SetEmissionColor(c);
        float dist = 10000.0f; // 1000000.0f;
        float f = rng->RandomFloat(1.0f);
        float size = (32.0f + f * f * f * f * 96.0f) * (dist / 5000.0f);
        scene->SetBillboardSize(size, size);
        float halo_size_full_fit = 1.0f;
        scene->SetHaloSize(halo_size_full_fit);
        star_object_billboard_size[i] = size;
        star_last_twinkle_time[i] = 0;
        float ra = 2.0f * M_PI * rng->RandomFloat(1.0f);
        float dec = 0.5f * M_PI * (0.99f * rng->RandomFloat(1.0f) + 0.01f);
        star_dec[i] = dec;
        star_object_index[i] = scene->AddObject(billboard_object,
            100.0f + dist * cosf(dec) * cosf(ra), 100.0f + dist *
            cosf(dec) * sinf(ra), dist * sinf(dec), 0, 0, 0, 1.0f);
    }
}

static void move_towards_target(sreScene *scene, int soi, const Vector3D &target, float dt) {
        sreObject *so = scene->object[soi];
        Vector3D v;
        v = target - so->position;
        float m = Magnitude(v);
        if (m > 1.0) {
            float vel_limit;
            // If the distance is >= 20.0, given an impulse of magnitude 20.
            if (m > 20.0) {
                v *= 20.0 / m;
                vel_limit = 20.0;
            }
            else
            // If the distance is >= 5.0, given an impulse of magnitude 5.
            if (m > 5.0) {
                v *= 5.0 / m;
                vel_limit = 5.0;
            }
            else {
                // Else, give an impulse of magnitude 1.
                v *= 1 / m;
                vel_limit = 1.0;
            }
            v *= dt * so->mass / 0.5;
            Vector3D vel;
            scene->BulletGetLinearVelocity(soi, &vel);
            if (Magnitude(vel) < vel_limit)
                scene->BulletApplyCentralImpulse(soi, v);
        }
}

static double time_previous = 0;

void Demo8Step(sreScene *scene, double demo_time) {
    float dt = demo_time - time_previous;
    time_previous = demo_time;
#ifdef MOVE_ROBOTS
    for (int i = 0; i < nu_robots; i++) {
        float f = rng->RandomFloat(1.0f);
        // On average once per 20 seconds, set a new target location for the
        // big red balls (robots).
        if (f < 0.05 * dt) {
            robot[i].target_pos.x = 15.0 + 170.0 * rng->RandomFloat(1.0f);
            robot[i].target_pos.y = 15.0 + 170.0 * rng->RandomFloat(1.0f);
            robot[i].target_pos.z = 0;
        }
        move_towards_target(scene, robot[i].object_index, robot[i].target_pos, dt);
        // The small light globes follow their robot.
        move_towards_target(scene, robot[i].light_object_index, scene->object[robot[i].object_index]->position, dt);
    }
#endif
    for (int i = 0; i < 4; i++) {
        // On once per second, change the color of a corner light.
        float f = fmod(demo_time, 1.0);
        if (f < dt) {
            Color light_color, object_color;
            int c = ((int)floor(fmod(demo_time, 4.0)) + i) % 4;
            switch (c) {
            case 0 :
                light_color = Color(1.0, 1.0, 0.8);
                object_color = Color(1.0, 1.0, 0.6);
                break;
            case 1 :
                light_color = Color(1.0, 0.6, 0.6);
                object_color = Color(1.0, 0.5, 0.5);
		break;
            case 2 :
                light_color = Color(1.0, 1.0, 1.0);
                object_color = Color(1.0, 1.0, 1.0);
		break;
            case 3 :
                light_color = Color(0.8, 1.0, 1.0);
                object_color = Color(0.6, 1.0, 1.0);
		break;
            }
            scene->ChangeLightColor(corner_light_index[i], light_color);
            scene->ChangeDiffuseReflectionColor(corner_light_object_index[i], object_color);
            scene->ChangeEmissionColor(corner_light_object_index[i], object_color);
        }
    }
    // Rotate the beam light.
    Matrix4D M_rot;
    Vector4D V = Vector4D(0.2, 0, - 1.0, 0);
    V.Normalize();
    M_rot.AssignRotationAlongZAxis(fmod(demo_time * 0.5 , 1.0) * 2.0 * M_PI);
    Vector3D W = (M_rot * V).GetVector3D();
    scene->ChangeSpotOrBeamLightDirection(beam_light, W);
    // Let the stars twinkle.
#ifdef TWINKLING_STARS
    for (int i = 0; i < NU_STARS; i++) {
        float current_bsize;
        if (star_last_twinkle_time[i] != 0) {
            if (demo_time - star_last_twinkle_time[i] < 0.2) {
                // The star is moving towards a billboard size target.
                // Interpolate billboard size towards to the target size over time.
                current_bsize = (star_billboard_size_target[i] - star_billboard_size_start[i]) *
                    (demo_time - star_last_twinkle_time[i]) / 0.2 + star_billboard_size_start[i];
                scene->ChangeBillboardSize(star_object_index[i],
                    current_bsize, current_bsize);
                continue;
            }
            // The target has been reached; a new one will be set.
            current_bsize = star_billboard_size_target[i];
        }
        else
            // No target has been defined yet.
            current_bsize = star_object_billboard_size[i];
        // Set a new billboard size target.
        float f = ((0.5 * M_PI - star_dec[i]) / (0.5 * M_PI));
        f = f * f * f;
        float billboard_size;
        if ((rng->RandomBits(1)) == 0)
            billboard_size = star_object_billboard_size[i] * (1 + 0.2 * f);
        else
            billboard_size = star_object_billboard_size[i] * (1 - 0.2 * f);
        star_last_twinkle_time[i] = demo_time;
        star_billboard_size_target[i] = billboard_size;
        star_billboard_size_start[i] = current_bsize;
    }
#endif
}

