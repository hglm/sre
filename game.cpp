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
#include <float.h>

#include "win32_compat.h"
#include "sre.h"
#include "demo.h"

static int level = 1;
static double timeout;
static bool success;
static int nu_target_objects;

static sreModel *globe_model;
static sreModel *ground_model;
static sreModel *ramp_towards_back_30x100x30_model;
static sreModel *ramp_towards_left_100x30x30_model;
static sreModel *ramp_towards_right_100x30x30_model;
static sreModel *ramp_towards_front_30x100x30_model;
static sreModel *ramp_towards_back_100x100x30_model;
static sreModel *ramp_towards_back_30x50x30_model;
static sreModel *block_30x30x30_model;
static sreModel *block_200x30x10_model;
static sreModel *block_100x30x30_model;
static sreModel *block_30x100x10_model;
static sreModel *block_30x2x5_model;
static sreModel *block_2x96x5_model;
static sreTexture *beachball_texture;
static sreTexture *ground_texture;
static sreTexture *water_texture;
static sreTexture *stars_texture;
static sreTexture *marble_texture;

static void CreateObjectsAndTextures() {
    globe_model = sreCreateSphereModel(scene, 0);
    ground_model = sreCreateRepeatingRectangleModel(scene, 1000, 20);
    ramp_towards_back_30x100x30_model = sreCreateRampModel(scene, 30, 100, 30, RAMP_TOWARDS_BACK);
    ramp_towards_right_100x30x30_model = sreCreateRampModel(scene, 100, 30, 30, RAMP_TOWARDS_RIGHT);
    ramp_towards_left_100x30x30_model = sreCreateRampModel(scene, 100, 30, 30, RAMP_TOWARDS_LEFT);
    ramp_towards_front_30x100x30_model = sreCreateRampModel(scene, 30, 100, 30, RAMP_TOWARDS_FRONT);
    ramp_towards_back_100x100x30_model = sreCreateRampModel(scene, 100, 100, 30, RAMP_TOWARDS_BACK);
    ramp_towards_back_30x50x30_model = sreCreateRampModel(scene, 30, 50, 30, RAMP_TOWARDS_BACK);
    block_30x30x30_model = sreCreateBlockModel(scene, 30, 30, 30, 0);
    block_200x30x10_model = sreCreateBlockModel(scene, 200, 30, 10, 0);
    block_100x30x30_model = sreCreateBlockModel(scene, 100, 30, 30, 0);
    block_30x100x10_model = sreCreateBlockModel(scene, 30, 100, 10, 0);
    block_30x2x5_model = sreCreateBlockModel(scene, 30, 2, 5, 0);
    block_2x96x5_model = sreCreateBlockModel(scene, 2, 96, 5, 0);

    beachball_texture = new sreTexture("beachball", TEXTURE_TYPE_NORMAL);
    ground_texture = new sreTexture("MossAndGrass4", TEXTURE_TYPE_WRAP_REPEAT);
    water_texture = new sreTexture("water1", TEXTURE_TYPE_WRAP_REPEAT);
#if defined(OPENGL) && !defined(NO_LARGE_TEXTURES)
    stars_texture = new sreTexture("yale8", TEXTURE_TYPE_NORMAL);
#endif
    marble_texture = new sreTexture("Marble9", TEXTURE_TYPE_NORMAL);
}

static void AddPlayer(float x, float y, float z) {
    // Add player sphere of size 3 as scene object 0.
    scene->SetTexture(beachball_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS);
    Color c;
    c.r = 0.00;
    c.g = 0.75;
    c.b = 1.0;
    scene->SetColor(c);
    int i = scene->AddObject(globe_model, x, y, z, 0, 0, 0, 3.0);
}

static void AddGrassGround() {
    // Add the ground as scene object 1.
    scene->SetTexture(ground_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_NO_PHYSICS);
    scene->AddObject(ground_model, - 500, - 500, 0, 0, 0, 0, 1);
}

static void AddWaterGround() {
    // Add the ground as scene object 1.
    scene->SetTexture(water_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_NO_PHYSICS);
    scene->AddObject(ground_model, - 500, - 500, 4, 0, 0, 0, 1);
}

static void AddStars() {
#if defined(OPENGL) && !defined(NO_LARGE_TEXTURES)
    // Starry background.
    scene->SetEmissionMap(stars_texture);
    scene->SetFlags(SRE_OBJECT_USE_EMISSION_MAP | SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_NO_BACKFACE_CULLING |
        SRE_OBJECT_INFINITE_DISTANCE | SRE_OBJECT_NO_PHYSICS);
    scene->AddObject(globe_model, 0, 0, 0, 0, 0, 0, SRE_DEFAULT_FAR_PLANE_DISTANCE * 90);
#endif
}

static int AddTargetObject(float x, float y, float z) {
    // Add target sphere of size 5.
    scene->SetTexture(beachball_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS);
    Color c;
    c.r = 0.00;
    c.g = 0.75;
    c.b = 1.0;
    scene->SetColor(c);
    int i = scene->AddObject(globe_model, x, y, z, 0, 0, 0, 5.0);
    nu_target_objects++;
    return i;
}

static int AddLightSourceTargetObject(float x, float y, float z) {
    // Add target sphere of size 5.
    scene->SetEmissionMap(beachball_texture);
    scene->SetFlags(SRE_OBJECT_USE_EMISSION_MAP | SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_DYNAMIC_POSITION);
    int i = scene->AddObject(globe_model, x, y, z, 0, 0, 0, 5.0);
    int l = scene->AddPointSourceLight(SRE_LIGHT_DYNAMIC_POSITION, Point3D(x, y, z), 50.0, Color(1.0, 1.0, 1.0));
    scene->AttachLight(i, l, Vector3D(0, 0, 0));
    nu_target_objects++;
    return i;
}

void LevelOneCreateScene() {
    // The player and the ground must be the first two objects.
    AddPlayer(0, 0, 3.0);
    AddGrassGround();
    // Add target object.
    AddTargetObject(- 35, 135, 35);
    AddStars();
    // Add ramp.
    int i;
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    Color red;
    red.r = 1.0;
    red.g = 0.2;
    red.b = 0.2;
    scene->SetColor(red);
    i = scene->AddObject(ramp_towards_back_30x100x30_model, - 50, 20, 0, 0, 0, 0, 1);
    // Add block after ramp.
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, - 50, 120, 0, 0, 0, 0, 1);
    // Directional light
    scene->AddDirectionalLight(0, Vector3D(0.2, 0.3, - 1.0), Color(0.8, 0.8, 0.8));
}

void LevelTwoCreateScene() {
    // The player and the ground must be the first two objects.
    AddPlayer(0, 0, 3.0);
    AddGrassGround();
    // Add target objects.
    AddTargetObject(- 45, 125, 35);
    AddTargetObject(- 25, 125, 35);
    AddTargetObject(- 45, 145, 35);
    AddTargetObject(- 25, 145, 35);
    AddStars();
    // Add ramp.
    int i;
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    Color red;
    red.r = 1.0;
    red.g = 0.2;
    red.b = 0.2;
    scene->SetColor(red);
    i = scene->AddObject(ramp_towards_back_30x100x30_model, - 50, 20, 0, 0, 0, 0, 1);
    // Add block after ramp.
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, - 50, 120, 0, 0, 0, 0, 1);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Add further ramps.
    i = scene->AddObject(ramp_towards_front_30x100x30_model, - 50, 150, 0, 0, 0, 0, 1);
    i = scene->AddObject(ramp_towards_right_100x30x30_model, - 150, 120, 0, 0, 0, 0, 1);
    i = scene->AddObject(ramp_towards_left_100x30x30_model, - 20, 120, 0, 0, 0, 0, 1);
    // Directional light
    scene->AddDirectionalLight(0, Vector3D(0.2, 0.3, - 0.5), Color(0.8, 0.8, 0.8));
}

void LevelThreeCreateScene() {
    // The player and the ground must be the first two objects.
    AddPlayer(0, 0, 3.0);
    AddGrassGround();
    // Add target object.
    AddTargetObject(- 135, 135, 125);
    AddStars();
    // Add ramp.
    int i;
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    Color red;
    red.r = 1.0;
    red.g = 0.2;
    red.b = 0.2;
    scene->SetColor(red);
    i = scene->AddObject(ramp_towards_back_30x100x30_model, - 50, 20, 0, 0, 0, 0, 1);
    // Add block after ramp.
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, - 50, 120, 0, 0, 0, 0, 1);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Add another ramp.
    scene->SetDiffuseReflectionColor(Color(1.0, 0.2, 0.2));
    i = scene->AddObject(ramp_towards_right_100x30x30_model, - 20, 120, 30, 0, 0, 0, 1);
    // Add block.
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, 80, 120, 30, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, 80, 120, 0, 0, 0, 0, 1);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Add long ramp.
    i = scene->AddObject(ramp_towards_back_30x100x30_model, 80, 150, 60, 0, 0, 0, 1);
    // Add block.
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, 80, 250, 60, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, 80, 250, 30, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, 80, 250, 0, 0, 0, 0, 1);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Add bridge.
    i = scene->AddObject(block_200x30x10_model, - 120, 250, 80, 0, 0, 0, 1);
    // Add block.
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, - 150, 250, 60, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, - 150, 250, 30, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, - 150, 250, 0, 0, 0, 0, 1);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Add ramp.
    i = scene->AddObject(ramp_towards_front_30x100x30_model, - 150, 150, 90, 0, 0, 0, 1);
    // Add final block.
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, - 150, 120, 90, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, - 150, 120, 60, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, - 150, 120, 30, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, - 150, 120, 0, 0, 0, 0, 1);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Directional light
    scene->AddDirectionalLight(0, Vector3D(0.2, - 0.1, - 1.0), Color(0.8, 0.8, 0.8));
}

void LevelFourCreateScene() {
    // The player and the ground must be the first two objects.
    AddPlayer(0, 0, 3.0);
    AddWaterGround();
    // Add target object.
    AddTargetObject(- 35, 135, 35);
    AddTargetObject( -35, 215, 65);
    AddStars();
    // Add ramp.
    int i;
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    Color red;
    red.r = 1.0;
    red.g = 0.2;
    red.b = 0.2;
    scene->SetColor(red);
    i = scene->AddObject(ramp_towards_back_30x100x30_model, - 50, 20, 0, 0, 0, 0, 1);
    // Add block after ramp.
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, - 50, 120, 0, 0, 0, 0, 1);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Add steep ramp
    i = scene->AddObject(ramp_towards_back_30x50x30_model, - 50, 150, 30, 0, 0, 0, 1);
    // Add blocks after ramp.
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, - 50, 200, 30, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, - 50, 200, 0, 0, 0, 0, 1);
    // Directional light
    scene->AddDirectionalLight(0, Vector3D(- 0.5, - 0.3, - 1.0), Color(0.8, 0.8, 0.8));
}

void LevelFiveCreateScene() {
    // The player and the ground must be the first two objects.
    AddPlayer(15, 8, 103.0);
    AddWaterGround();
    // Add target object that is a point lightsource.
    AddLightSourceTargetObject(15, 92, 105);
    AddStars();
    // Add platform
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    Color red;
    red.r = 1.0;
    red.g = 0.2;
    red.b = 0.2;
    scene->SetColor(red);
    int i = scene->AddObject(block_30x100x10_model, 0, 0, 90, 0, 0, 0, 1);
    // Create enclosue.
    i = scene->AddObject(block_30x2x5_model, 0, 98, 100, 0, 0, 0, 1);
    i = scene->AddObject(block_2x96x5_model, 0, 2, 100, 0, 0, 0, 1);
    i = scene->AddObject(block_2x96x5_model, 28, 2, 100, 0, 0, 0, 1);
    // Pillar (blocks)
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, 0, 35, 0, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, 0, 35, 30, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, 0, 35, 60, 0, 0, 0, 1);
}

void EndLevelCreateScene() {
    // The player and the ground must be the first two objects.
    AddPlayer(0, 0, 3.0);
    AddGrassGround();
    // Add target objects.
    for (int x = 0; x < 9; x++)
        AddTargetObject(- 45 + x * 11, 119, 35);
    AddStars();
    // Add ramp.
    int i;
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    Color red;
    red.r = 1.0;
    red.g = 0.2;
    red.b = 0.2;
    scene->SetColor(red);
    i = scene->AddObject(ramp_towards_back_100x100x30_model, - 50, 20, 0, 0, 0, 0, 1);
    // Add block after ramp.
    i = scene->AddObject(block_100x30x30_model, - 50, 120, 0, 0, 0, 0, 1);
    // Directional light
    scene->AddDirectionalLight(0, Vector3D(- 0.2, - 0.3, - 1.0), Color(0.8, 0.8, 0.8));
}


void GameRender() {
    scene->Render(view);
}

void GameTimeIteration(double time_previous, double time_current) {
    switch (level) {
    case 1 :
    case 2 :
    case 3 :
    case 4 :
    case 5 : 
        // All target objects need to hit the ground.
        if (timeout == POSITIVE_INFINITY_DOUBLE) {
            int count = 0;
            for (int i = 2; i < 2 + nu_target_objects; i++)
                if (scene->sceneobject[i]->position.z < 6.0)
                    count++;
            if (count == nu_target_objects) {
                timeout = demo_time + 3;
                success = true;
            }
        }
        break;
    }
    if (demo_time >= timeout)
        demo_stop_signalled = true;
}

void GameDrawTextOverlay() {
    char s[32];
    sprintf(s, "Level %d", level);
    sreSetFont(NULL); // Set default font.
    sreSetTextParameters(SRE_IMAGE_SET_COLORS, NULL, NULL); // Set default colors.
    Vector2D font_size = Vector2D(0.02f, 0.04f);
    sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size);
    sreDrawText(s, 0, 0.96);
    if (success)
        sreDrawText("Success!", 0.42, 0.48);
    DemoTextOverlay();
}

void RunGame() {
    CreateObjectsAndTextures();
    RenderFunc = GameRender;
    TimeIterationFunc = GameTimeIteration;
    sreSetDrawTextOverlayFunc(GameDrawTextOverlay);
    jump_allowed = false;
    for (;;) {
        nu_target_objects = 0;
        switch (level) {
        case 1 : 
            LevelOneCreateScene();
            break;
        case 2 :
            LevelTwoCreateScene();
            break;
        case 3 :
            LevelThreeCreateScene();
            break;
        case 4 :
            LevelFourCreateScene();
            break;
        case 5 :
            LevelFiveCreateScene();
            break;
        case 6 : 
            EndLevelCreateScene();
            break;
        default :
            level = 1;
            LevelOneCreateScene();
            break;
        }
        success = false;
        timeout = POSITIVE_INFINITY_DOUBLE;
        view->SetViewAngles(Vector3D(0, 0, 0));
        RunDemo();
        if (success)
            level++;
        BulletDestroy();
        scene->Clear();
    }
}

