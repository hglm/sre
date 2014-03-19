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

#include "sre.h"
#include "sreBackend.h"

class GameApplication : public sreBulletPhysicsApplication {
    virtual void StepBeforeRender(double demo_time);
    virtual void StepBeforePhysics(double demo_time);
};

static sreScene *scene;
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
static sreTexture *stripes_texture;

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
    stripes_texture = sreCreateStripesTexture(TEXTURE_TYPE_LINEAR,
        256, 256, 32, Color(0, 0.5f, 0.8f), Color(0.9f, 0.9f, 1.0f));
}

static void AddPlayer(float x, float y, float z) {
    // Add player sphere of size 3 as scene object 0.
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->SetTexture(stripes_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS);
    Color c;
    c.r = 0.00;
    c.g = 0.75;
    c.b = 1.0;
    scene->SetDiffuseReflectionColor(c);
    scene->SetMass(2.0f);
    int i = scene->AddObject(globe_model, x, y, z, 0, 0, 0, 3.0);
    scene->SetMass(1.0f);
}

static void AddGrassGround() {
    // Add the ground as scene object 1.
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->SetTexture(ground_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_NO_PHYSICS);
    scene->AddObject(ground_model, - 500, - 500, 0, 0, 0, 0, 1);
}

static void AddWaterGround() {
    // Add the ground as scene object 1.
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->SetTexture(water_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_NO_PHYSICS);
    scene->AddObject(ground_model, - 500, - 500, 4, 0, 0, 0, 1);
}

static void AddStars() {
#if defined(OPENGL) && !defined(NO_LARGE_TEXTURES)
    // Starry background.
    scene->SetEmissionColor(Color(1.0f, 1.0f, 1.0f));
    scene->SetEmissionMap(stars_texture);
    scene->SetFlags(SRE_OBJECT_USE_EMISSION_MAP | SRE_OBJECT_EMISSION_ONLY |
        SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_INFINITE_DISTANCE | SRE_OBJECT_NO_PHYSICS);
    scene->AddObject(globe_model, 0, 0, 0, 0, 0, 0, SRE_DEFAULT_FAR_PLANE_DISTANCE * 90);
    scene->SetEmissionColor(Color(0, 0, 0));
#endif
}

static int AddTargetObject(float x, float y, float z) {
    // Add target sphere of size 5.
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->SetTexture(beachball_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS);
    Color c;
    c.r = 0.00;
    c.g = 0.75;
    c.b = 1.0;
    scene->SetDiffuseReflectionColor(c);
    int i = scene->AddObject(globe_model, x, y, z, 0, 0, 0, 5.0);
    nu_target_objects++;
    return i;
}

static int AddLightSourceTargetObject(float x, float y, float z) {
    // Add target sphere of size 5.
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->SetEmissionMap(beachball_texture);
    scene->SetFlags(SRE_OBJECT_USE_EMISSION_MAP | SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_DYNAMIC_POSITION);
    int i = scene->AddObject(globe_model, x, y, z, 0, 0, 0, 5.0);
    int l = scene->AddPointSourceLight(SRE_LIGHT_DYNAMIC_POSITION, Point3D(x, y, z), 50.0, Color(1.0, 1.0, 1.0));
    scene->AttachLight(i, l, Vector3D(0, 0, 0), Vector3D(0, 0, 0));
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
    scene->SetDiffuseReflectionColor(red);
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
    Color red = Color(1.0f, 0.2f, 0.2f);
    scene->SetDiffuseReflectionColor(red);
    i = scene->AddObject(ramp_towards_back_30x100x30_model, - 50, 20, 0, 0, 0, 0, 1);
    // Add block after ramp.
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, - 50, 120, 0, 0, 0, 0, 1);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Add further ramps.
    scene->SetDiffuseReflectionColor(red);
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
    scene->SetDiffuseReflectionColor(red);
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
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, 80, 120, 30, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, 80, 120, 0, 0, 0, 0, 1);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Add long ramp.
    scene->SetDiffuseReflectionColor(Color(1.0, 0.2, 0.2));
    i = scene->AddObject(ramp_towards_back_30x100x30_model, 80, 150, 60, 0, 0, 0, 1);
    // Add block.
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, 80, 250, 60, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, 80, 250, 30, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, 80, 250, 0, 0, 0, 0, 1);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Add bridge.
    scene->SetDiffuseReflectionColor(Color(0.2, 0.2, 0.8));
    i = scene->AddObject(block_200x30x10_model, - 120, 250, 80, 0, 0, 0, 1);
    // Add block.
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_30x30x30_model, - 150, 250, 60, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, - 150, 250, 30, 0, 0, 0, 1);
    i = scene->AddObject(block_30x30x30_model, - 150, 250, 0, 0, 0, 0, 1);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Add ramp.
    scene->SetDiffuseReflectionColor(Color(1.0, 0.2, 0.2));
    i = scene->AddObject(ramp_towards_front_30x100x30_model, - 150, 150, 90, 0, 0, 0, 1);
    // Add final block.
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
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
    scene->SetDiffuseReflectionColor(red);
    i = scene->AddObject(ramp_towards_back_30x100x30_model, - 50, 20, 0, 0, 0, 0, 1);
    // Add block after ramp.
    scene->SetTexture(marble_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    i = scene->AddObject(block_30x30x30_model, - 50, 120, 0, 0, 0, 0, 1);
    // Add steep ramp
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    scene->SetDiffuseReflectionColor(red);
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
    Color red = Color(1.0f, 0.2f, 0.2f);
    scene->SetDiffuseReflectionColor(red);
    int i = scene->AddObject(block_30x100x10_model, 0, 0, 90, 0, 0, 0, 1);
    // Create enclosure.
    i = scene->AddObject(block_30x2x5_model, 0, 98, 100, 0, 0, 0, 1);
    i = scene->AddObject(block_2x96x5_model, 0, 2, 100, 0, 0, 0, 1);
    i = scene->AddObject(block_2x96x5_model, 28, 2, 100, 0, 0, 0, 1);
    // Pillar (blocks)
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
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
    scene->SetMass(5.0f);
    for (int x = 0; x < 9; x++)
        AddTargetObject(- 45 + x * 11, 119, 35);
    scene->SetMass(1.0f);
    AddStars();
    // Add ramp.
    int i;
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    Color red = Color(1.0f, 0.2f, 0.2f);
    scene->SetDiffuseReflectionColor(red);
    i = scene->AddObject(ramp_towards_back_100x100x30_model, - 50, 20, 0, 0, 0, 0, 1);
    // Add block after ramp.
    i = scene->AddObject(block_100x30x30_model, - 50, 120, 0, 0, 0, 0, 1);
    // Directional light
    scene->AddDirectionalLight(0, Vector3D(- 0.2, - 0.3, - 1.0), Color(0.8, 0.8, 0.8));
}

void GameApplication::StepBeforeRender(double demo_time) {
    switch (level) {
    case 1 :
    case 2 :
    case 3 :
    case 4 :
    case 5 : 
        // All target objects need to hit the ground.
        if (timeout == DBL_MAX) {
            int count = 0;
            for (int i = 2; i < 2 + nu_target_objects; i++)
                if (scene->object[i]->position.z < 6.0)
                    count++;
            if (count == nu_target_objects) {
                timeout = demo_time + 3;
                success = true;
            }
        }
        break;
    }
    if (demo_time >= timeout)
        stop_signal = SRE_APPLICATION_STOP_SIGNAL_CUSTOM;
}

void GameApplication::StepBeforePhysics(double demo_time) {
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
    sreBackendStandardTextOverlay();
}

static void RunGame(GameApplication *app) {
    CreateObjectsAndTextures();
    // Upload all models beforehand.
    scene->MarkAllModelsReferenced();
    scene->UploadModels();
    sreSetDrawTextOverlayFunc(GameDrawTextOverlay);
    scene->SetAmbientColor(Color(0.2f, 0.2f, 0.2f));
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
        timeout = DBL_MAX;
        app->view->SetViewAngles(Vector3D(0, 0, 0));
        sreRunApplication(app);
        scene->ClearObjectsAndLights();
        if (app->stop_signal & SRE_APPLICATION_STOP_SIGNAL_QUIT)
            break;
        if (success)
            level++;
    }
}

int main(int argc, char **argv) {
    GameApplication *app = new GameApplication;
    sreInitializeApplication(app, &argc, &argv);
    scene = app->scene;

    app->SetFlags((app->GetFlags() | SRE_APPLICATION_FLAG_UPLOAD_NO_MODELS)
        & (~SRE_APPLICATION_FLAG_JUMP_ALLOWED));
    app->view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
    app->view->SetMovementMode(SRE_MOVEMENT_MODE_STANDARD);
    // Provide double the horizontal impulse (higher mass).
    app->horizontal_acceleration = 200.0f;

    RunGame(app);

    sreFinalizeApplication(app);
    exit(0);
}
