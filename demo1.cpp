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
#include <math.h>

#include "sre.h"
#include "demo.h"

#ifndef OPENGL_ES2
#define LOTS_OF_SPOTLIGHTS
#endif

static dstRNG *rng;

#define FLUID_SIZE 64
#define USE_WATER
#ifdef USE_WATER
const float disturbance_frequency = 0.01;
const Color liquid_specular_reflection_color = Color(1.0, 1.0, 1.0);

static float disturbance_displacement_func() {
   return - 0.5;
}
#else
const float disturbance_frequency = 0.1;
const Color liquid_specular_reflection_color = Color(0.2, 0.2, 0.2);

static float disturbance_displacement_func() {
   float r = rng->RandomFloat(1.0f);
   return 0.3 + 1.5 * pow(r, 6);
}
#endif

static sreObject *fluid_scene_object;
static int light_object[13 * 28];

void Demo1CreateScene(sreScene *scene, sreView *view) {
    rng = dstGetDefaultRNG();

    sreModel *sphere_model = sreCreateSphereModel(scene, 0);
    // Add player sphere as scene object 0.
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS |
        SRE_OBJECT_USE_TEXTURE);
    scene->SetTexture(sreCreateStripesTexture(TEXTURE_TYPE_LINEAR,
        256, 256, 32, Color(0, 0.5f, 0.8f), Color(0.9f, 0.9f, 1.0f)));
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->AddObject(sphere_model, Point3D(- 20.0f, - 40.0f, 3.0f), Vector3D(0, 0, 0), 3.0f);
#if 0
    // Ground floor is object 1.
    sreModel *checkerboard_model = sreCreateCheckerboardObject(32, 10,
        Color(0.5, 0.1, 0.1), Color(0.1, 0.1, 0.5));
    scene->SetFlags(SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_NO_PHYSICS);
    i = scene->AddObject(checkerboard_model, - 16 * 10, - 16 * 10, 0, 0, 0, 0, 1);
    scene->object[i]->target_collision_reaction = COLLISION_REACTION_BUMP_REFLECTIVE;
#else
    sreModel *ground_model = sreCreateRepeatingRectangleModel(scene, 320, 10);
    sreTexture *ground_texture = new sreTexture("StonesAndBricks5",
        TEXTURE_TYPE_NORMAL | SRE_TEXTURE_TYPE_FLAG_WRAP_REPEAT);
    scene->SetTexture(ground_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_NO_PHYSICS);
    scene->AddObject(ground_model, - 16 * 10, - 16 * 10, 0, 0, 0, 0, 1.0);
#endif
    // Create block objects.
    sreModel *block_model = sreCreateBlockModel(scene, 1.0, 1.0, 1.0, 0);
    // Blocks with no bottom are only safe for shadow volumes if they are on the ground (since we never look up from below).
    sreModel *block_model_no_bottom = sreCreateBlockModel(scene, 1.0, 1.0, 1.0, SRE_BLOCK_NO_BOTTOM);
    sreModel *block_model_no_top = sreCreateBlockModel(scene, 1.0, 1.0, 1.0, SRE_BLOCK_NO_TOP);
    sreModel *block_model_no_bottom_no_top = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_BOTTOM | SRE_BLOCK_NO_TOP);
    sreModel *block_model_no_bottom_no_top_no_right = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_BOTTOM | SRE_BLOCK_NO_TOP | SRE_BLOCK_NO_RIGHT);
    sreModel *block_model_no_bottom_no_top_no_left = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_BOTTOM | SRE_BLOCK_NO_TOP | SRE_BLOCK_NO_LEFT);
    sreModel *block_model_no_bottom_no_right = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_BOTTOM | SRE_BLOCK_NO_RIGHT);
    sreModel *block_model_no_bottom_no_left = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_BOTTOM | SRE_BLOCK_NO_LEFT);
    sreModel *block_model_no_left_no_right = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_RIGHT | SRE_BLOCK_NO_LEFT);
    sreModel *block_model_no_bottom_no_left_no_right = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_BOTTOM | SRE_BLOCK_NO_RIGHT | SRE_BLOCK_NO_LEFT);
    sreModel *block_model_no_top_no_left = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_TOP | SRE_BLOCK_NO_LEFT);
    sreModel *block_model_no_top_no_right = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_TOP | SRE_BLOCK_NO_RIGHT);

    sreModel *block_model_no_bottom_no_right_no_back = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_BOTTOM | SRE_BLOCK_NO_RIGHT | SRE_BLOCK_NO_BACK);
    sreModel *block_model_no_bottom_no_left_no_back = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_BOTTOM | SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_BACK);
    sreModel *block_model_no_bottom_no_right_no_front = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_BOTTOM | SRE_BLOCK_NO_RIGHT | SRE_BLOCK_NO_FRONT);
    sreModel *block_model_no_bottom_no_left_no_front = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_BOTTOM | SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_FRONT);
    sreModel *block_model_no_bottom_no_front_no_back = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_BOTTOM | SRE_BLOCK_NO_FRONT | SRE_BLOCK_NO_BACK);
    sreModel *block_model_no_front = sreCreateBlockModel(scene, 1.0, 1.0, 1.0,
        SRE_BLOCK_NO_FRONT);

    sreTexture *wall_texture = new sreTexture("tijolo", TEXTURE_TYPE_NORMAL);
    scene->SetTexture(wall_texture);
    sreTexture *wall_normals = new sreTexture("tijolo_normal_map", TEXTURE_TYPE_NORMAL_MAP);
    scene->SetNormalMap(wall_normals);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP | SRE_OBJECT_CAST_SHADOWS |
        SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_LIGHT);

#define COMPOUND_OPEN_WALL_OBJECT

#ifdef COMPOUND_OPEN_WALL_OBJECT
    // Create open wall.
    sreModel *open_wall = sreCreateCompoundModel(scene, true, true, SRE_LOD_MODEL_CONTAINS_HOLES |
        SRE_LOD_MODEL_NOT_CLOSED);
    // Bottom left corner.
    sreAddToCompoundModel(open_wall, block_model_no_bottom_no_top_no_right, Point3D(0, 0, 0),
        Vector3D(0, 0, 0), 5.0);
    // Top left corner.
    sreAddToCompoundModel(open_wall, block_model_no_bottom_no_right, Point3D(0, 0, 4 * 5.0), Vector3D(0, 0, 0), 5.0);
    // Bottom and top bars.
    for (int i = 0; i < 18; i++) {
        sreAddToCompoundModel(open_wall, block_model_no_bottom_no_left_no_right,
            Point3D(i * 5 + 5.0, 0, 0), Vector3D(0, 0, 0), 5.0);
        sreAddToCompoundModel(open_wall, block_model_no_left_no_right, Point3D(i * 5 + 5.0, 0, 4 * 5.0),
            Vector3D(0, 0, 0), 5.0);
    }
    // Bottom right corner.
    sreAddToCompoundModel(open_wall, block_model_no_bottom_no_top_no_left, Point3D(19 * 5.0, 0, 0),
        Vector3D(0, 0, 0), 5.0);
    // Top right corner.
    sreAddToCompoundModel(open_wall, block_model_no_bottom_no_left, Point3D(19 * 5.0, 0, 4 * 5.0),
        Vector3D(0, 0, 0), 5.0);
    // Interior left and right pillars.
    for (int i = 0; i < 3; i++) {
        sreAddToCompoundModel(open_wall, block_model_no_bottom_no_top, Point3D(0, 0, 5.0 + i * 5.0), Vector3D(0, 0, 0),
            5.0);
        sreAddToCompoundModel(open_wall, block_model_no_bottom_no_top, Point3D(19.0 * 5.0, 0, 5.0 + i * 5.0),
            Vector3D(0, 0, 0), 5.0);
    }
    sreFinalizeCompoundModel(scene, open_wall);

    scene->AddObject(open_wall, 0, 10.0, 0, 0, 0, 0, 1.0);
#else
    // Create open wall uses individual block objects. For objects on the ground, the bottom is ommitted.
    // Bottom left corner.
    scene->AddObject(block_model_no_bottom, Point3D(0, 0, 0), Vector3D(0, 0, 0), 5.0);
    // Top left corner.
    scene->AddObject(block_model, Point3D(0, 0, 4 * 5.0), Vector3D(0, 0, 0), 5.0);
    // Bottom bar.
    for (int i = 0; i < 18; i++)
        scene->AddObject(block_model_no_bottom, Point3D(i * 5 + 5.0, 0, 0),
            Vector3D(0, 0, 0), 5.0);
    // Top bar.
    for (int i = 0; i < 18; i++)
        scene->AddObject(block_model, Point3D(i * 5 + 5.0, 0, 4 * 5.0),
            Vector3D(0, 0, 0), 5.0);
    // Bottom right corner.
    scene->AddObject(block_model_no_bottom, Point3D(19 * 5.0, 0, 0),
        Vector3D(0, 0, 0), 5.0);
    // Top right corner.
    scene->AddObject(block_model, Point3D(19 * 5.0, 0, 4 * 5.0),
        Vector3D(0, 0, 0), 5.0);
    // Interior left and right pillars.
    for (int i = 0; i < 3; i++) {
        scene->AddObject(block_model, Point3D(0, 0, 5.0 + i * 5.0), Vector3D(0, 0, 0),
            5.0);
        scene->AddObject(block_model, Point3D(19.0 * 5.0, 0, 5.0 + i * 5.0),
            Vector3D(0, 0, 0), 5.0);
    }
#endif
    // Create pillars on the edges.
    sreModel *pillar = sreCreateCompoundModel(scene, true, true, 0);
    sreAddToCompoundModel(pillar, block_model_no_top, Point3D(0, 0, 0), Vector3D(0, 0, 0), 10.0);
    for (int i = 0; i < 3; i++)
        sreAddToCompoundModel(pillar, block_model_no_bottom_no_top, Point3D(0, 0, i * 10.0 + 10.0),
            Vector3D(0, 0, 0), 10.0);
    sreAddToCompoundModel(pillar, block_model_no_bottom, Point3D(0, 0, 40.0), Vector3D(0, 0, 0), 10.0);
    sreFinalizeCompoundModel(scene, pillar);
    for (int i = 0; i < 32; i += 4) {
        scene->AddObject(pillar, i * 10 - 16 * 10, - 16 * 10, 0, 0, 0, 0, 1.0);
        scene->AddObject(pillar, i * 10 - 16 * 10, 15 * 10, 0, 0, 0, 0, 1.0);
        scene->AddObject(pillar, - 16 * 10, i * 10 - 16 * 10, 0, 0, 0, 0, 1.0);
        scene->AddObject(pillar, 15 * 10, i * 10 - 16 * 10, 0, 0, 0, 0, 1.0);
    }
    // Create pond boundary.
    sreTexture *marble_texture = new sreTexture("Marble9", TEXTURE_TYPE_NORMAL);
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS |
        SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_LIGHT | SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_VIEW);
    // The pond model is technically not closed but it causes no problems with stencil shadows
    // because it is never lit from below (indicated with SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_LIGHT).
    sreModel *pond = sreCreateCompoundModel(scene, true, true, SRE_LOD_MODEL_NOT_CLOSED |
        SRE_LOD_MODEL_CONTAINS_HOLES);
    sreAddToCompoundModel(pond, block_model_no_bottom_no_right_no_back,
        Point3D(0 * 5.0f - 50.0f, 10.0f, 0), Vector3D(0, 0, 0), 5.0f);
    sreAddToCompoundModel(pond, block_model_no_bottom_no_left_no_back,
        Point3D(7 * 5.0f - 50.0f, 10.0f, 0), Vector3D(0, 0, 0), 5.0f);
    sreAddToCompoundModel(pond, block_model_no_bottom_no_right_no_front,
        Point3D(0 * 5.0f - 50.0f, 10.0f + 7 * 5.0f, 0), Vector3D(0, 0, 0), 5.0f);
    sreAddToCompoundModel(pond, block_model_no_bottom_no_left_no_front,
        Point3D(7 * 5.0f - 50.0f, 10.0f + 7 * 5.0f, 0), Vector3D(0, 0, 0), 5.0f);
    for (int i = 1; i < 7; i++) {
            sreAddToCompoundModel(pond, block_model_no_bottom_no_left_no_right,
                Point3D(i * 5.0f - 50.0f, 10.0f, 0), Vector3D(0, 0, 0), 5.0f);
            sreAddToCompoundModel(pond, block_model_no_bottom_no_left_no_right,
                Point3D(i * 5.0f - 50.0f, 10.0f + 7 * 5.0f, 0), Vector3D(0, 0, 0), 5.0f);
    }
    for (int i = 1; i < 7; i++) {
            sreAddToCompoundModel(pond, block_model_no_bottom_no_front_no_back, Point3D(0 * 5.0f - 50.0f,
                10.0f + i * 5.0f, 0), Vector3D(0, 0, 0), 5.0f);
            sreAddToCompoundModel(pond, block_model_no_bottom_no_front_no_back, Point3D(7 * 5.0f - 50.0f,
                10.0f + i * 5.0f, 0), Vector3D(0, 0, 0), 5.0f);
    }
    sreFinalizeCompoundModel(scene, pond);
    scene->AddObject(pond, 0, 0, 0, 0, 0, 0, 1.0f);

    // Create fluid.
#if 1
    sreModel *fluid_object = sreCreateFluidModel(scene, FLUID_SIZE, FLUID_SIZE,
        30.0f / FLUID_SIZE, 1.0f, 0.1f, 0.01f, 1.0f);
#ifdef USE_WATER
    sreTexture *texture = new sreTexture("water1", TEXTURE_TYPE_NORMAL);
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetSpecularReflectionColor(liquid_specular_reflection_color);
    scene->SetFlags(SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_LIGHT);
    scene->SetTexture(texture);
    scene->SetSpecularExponent(120.0);
#else
    sreTexture *texture = new sreTexture("volcanic8", TEXTURE_TYPE_NORMAL);
    scene->SetFlags(SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_USE_EMISSION_MAP | SRE_OBJECT_NO_PHYSICS |
        SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    scene->SetEmissionMap(texture);
    scene->SetDiffuseReflectionColor(Color(0.4, 0, 0));
    scene->SetSpecularReflectionColor(Color(0.2, 0.2, 0.2));
    scene->SetSpecularExponent(4.0);
#endif
    int j = scene->AddObject(fluid_object, Point3D(- 45.0f, 15.0f, 3.0f), Vector3D(0, 0, 0), 1.0f);
    fluid_scene_object = scene->object[j];
#endif

    // Add dim directional light source.
    scene->AddDirectionalLight(0, Vector3D(0.8, 0.6, - 0.3).Normalize(), Color(0.5, 0.5, 0.5));

    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);

    // Add spotlights.
    scene->AddSpotLight(0,
        Point3D(- 10.0, -20.0, 20.0), Vector3D(0, 0, - 1.0), 10.0f, 50.0, Color(1.0, 1.0, 0.5));
    scene->SetEmissionColor(Color(0, 0, 0));
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 0.5));
    scene->SetSpecularReflectionColor(Color(0, 0, 0));
    scene->AddObject(block_model, - 12.0, -22.0, 20.0, 0, 0, 0, 4.0);
//    scene->AddObject(block_model, - 12.0, -22.0, 0, 0, 0, 0, 4.0);

    Vector3D spot_dir = Vector3D(1.0f, - 0.8f, - 1.2f).Normalize();
    int l = scene->AddSpotLight(0,
        Point3D(- 60.0, - 60.0f, 20.0), spot_dir, 20.0f, 80.0, Color(3.0, 1.5, 1.5));
    scene->SetEmissionColor(Color(1.0, 0.7, 0.7));
    scene->SetDiffuseReflectionColor(Color(1.0, 0.5, 0.5));
    j = scene->AddObject(sphere_model, - 60.0, - 60.0f, 20.0, 0, 0, 0, 3.0);
    // This overrides the original direction of the light.
    scene->AttachLight(j, l, Vector3D(0, 0, 0), spot_dir);
    scene->SetEmissionColor(Color(0, 0, 0));

#ifdef LOTS_OF_SPOTLIGHTS
    // Add colored spotlights.
    for (int j = 0; j < 28; j++)
    for (int i = 0; i < 13; i++) {
        Color color;
        switch ((i + j) & 3) {
        case 0 :
            color.Set(2.0, 2.0, 1.0);
            break;
        case 1 :
            color.Set(2.0, 2.0, 2.0);
            break;
        case 2 :
            color.Set(2.0, 1.0, 1.0);
            break;
        case 3 :
            color.Set(1.0, 1.0, 2.0);
            break;
        }
	color *= powf(1.5f, (log(1.0f) / log(1.5f)) + rng->RandomFloat(
            (log(8.0f) / log(1.5f)) - (log(1.0f) / log(1.5f))));
        light_object[j * 13 + i] = scene->AddSpotLight(SRE_LIGHT_DYNAMIC_DIRECTION,
            Point3D(- 150.0 + j * 10.0, 20.0 + i * 10.0, 30.0), Vector3D(0.1, 0, - 1.0),
            200.0f, 50.0f, color);
    }
#endif

    // Add dynamic block objects.
    scene->SetDiffuseReflectionColor(Color(1.0, 0.4, 0.3));
    scene->SetMass(1.0);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_DYNAMIC_POSITION);
    scene->AddObject(block_model, -20.0f, -20.0f, 0, 0, 0, 0, 8.0f);
    scene->AddObject(block_model, 20.0f, - 7.0f, 0, 0, 0, 0, 5.0f);

    // Add test block with no bottom.
    scene->SetDiffuseReflectionColor(Color(0.4f, 1.0f, 0.3f));
    scene->SetMass(0.0f);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_LIGHT);
    scene->AddObject(block_model_no_bottom, Point3D(40.0f, -50.0f, 0.0f),
        Vector3D(0.0f, 0.0f, 0.0f), 10.0f);
    // Add test block with no front.
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_NO_BACKFACE_CULLING);
    // Put the object at z = 0.1f to avoid overlap with the ground.
    scene->AddObject(block_model_no_front, Point3D(15.0f, -40.0f, 0.1f),
        Vector3D(0.0f, 0.0f, 0.0f), 10.0f);
    // Add single plane objects.
    sreModel *rect_model = sreCreateCenteredYPlaneRectangleModel(scene, 20.0f, 10.0f);
    scene->AddObject(rect_model, Point3D(-5.0f, -55.0f, 0.0f),
        Vector3D(0.0f, 0.0f, M_PI), 1.0f);
    scene->AddObject(rect_model, Point3D(-30.0f, -45.0f, 0.0f),
        Vector3D(0.0f, 0.0f, 0.0f), 1.0f);
    scene->AddObject(rect_model, Point3D(-40.0f, -50.0f, 0.0f),
        Vector3D(0.0f, 0.0f, 0.0f), 1.0f);
}

static double demo1_previous_time = 0;
static double fluid_time = 0;

void Demo1Step(sreScene *scene, double demo_time) {
    double elapsed_time = demo_time - demo1_previous_time;
    demo1_previous_time = demo_time;
#ifndef OPENGL_ES2
    fluid_time += elapsed_time;
    while (fluid_time >= (float)1 /60) {
        fluid_time -= (float)1 / 60;
        // On average every hundred 60th of a second, create a disturbance.
        if (rng->RandomFloat(0.1f) < disturbance_frequency)
            sreCreateModelFluidDisturbance(fluid_scene_object->model,
                rng->RandomInt(FLUID_SIZE - 1) + 1, rng->RandomInt(FLUID_SIZE - 1) + 1,
                disturbance_displacement_func());
        sreEvaluateModelFluid(fluid_scene_object->model);
    }
#endif
#ifdef LOTS_OF_SPOTLIGHTS
    Matrix4D M_rot;
    Vector4D V = Vector4D(0.1, 0, - 1.0, 0);
    V.Normalize();
    for (int i = 0; i < 13 * 28; i++) {
        M_rot.AssignRotationAlongZAxis((fmod(demo_time * 0.5, 1.0) + i * 0.13) * 2.0 * M_PI);
        Vector3D W = (M_rot * V).GetVector3D();
        scene->ChangeSpotOrBeamLightDirection(light_object[i], W);
    }
#endif
}
