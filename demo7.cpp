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
#define _USE_MATH_DEFINES
#include <math.h>

#include "sre.h"
#include "demo.h"

#define HALO

#define HALO_MOVING

#ifndef OPENGL_ES2
#define HALO_LIGHT
#endif

static int lightsource_object_index[2];

void Demo7CreateScene() {
    sreModel *sphere_model = sreCreateSphereModel(scene, 0);

    // Add player sphere as scene object 0.
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS |
        SRE_OBJECT_USE_TEXTURE);
    scene->SetTexture(sreCreateStripesTexture(TEXTURE_TYPE_LINEAR,
        256, 256, 32, Color(0, 0.5f, 0.8f), Color(0.9f, 0.9f, 1.0f)));
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->AddObject(sphere_model, Point3D(0, - 40.0f, 3.0f), Vector3D(0, 0, 0), 3.0f);

#if 1
    // Add floor as object 1
    sreModel *checkerboard_model = sreCreateCheckerboardModel(scene, 20, 10,
        Color(0.5, 0.1, 0.1), Color(0.1, 0.1, 0.5));
    scene->SetFlags(SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_NO_PHYSICS);
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    int i = scene->AddObject(checkerboard_model, - 10 * 10, 0, 0, 0, 0, 0, 1);
#endif

#if 1
    // Add sphere
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    scene->SetColor(Color(0.75f, 0.75f, 1.0f));
    scene->AddObject(sphere_model, 0, 30, 10, 0, 0, 0, 5.0);
#endif

    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    sreModel *torus_model = sreCreateTorusModel(scene);
    scene->SetColor(Color(1.0f, 0.25f, 0));
    for (int y = 0; y < 9; y++)
        for (int x = 0; x < 5; x++)
            scene->AddObject(sphere_model, - 10 + x * 6.0, 40 + y * 6.0, 8.0, 0, 0, 0, 3.0);

    // Add transparent texture.
#ifndef OPENGL_ES2
    // The texture is in BPTC format.
    sreTexture *transparent_texture = new sreTexture("transparent_texture",
        TEXTURE_TYPE_WRAP_REPEAT);
    sreModel *rectangle = sreCreateRepeatingRectangleModel(scene, 20.0, 5.0);
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetTexture(transparent_texture);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_NO_BACKFACE_CULLING |
        SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_TRANSPARENT_TEXTURE);
    scene->AddObject(rectangle, - 50, 60, 0, M_PI / 4, 0, 0, 1.0);
#endif

    // Add grating
    sreModel *grating_model = sreCreateGratingModel(scene, 10, 10, 0.2, 0.9, 0.1, 0.2);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    // Add big rotated grating in background.
    scene->SetColor(Color(0.5f, 0.8f, 0.2f));
    scene->AddObject(grating_model, - 20, 100, 0, M_PI / 2, 0, 0, 4.0);
    sreModel *elongated_grating_model = sreCreateGratingModel(scene, 20, 8, 0.2, 0.9, 0.1, 0.2);
    // Create steps of elongated gratings.
    scene->SetColor(Color(1.0f, 0.5f, 0.5f));
    for (int i = 0; i < 12; i++) {
        scene->AddObject(elongated_grating_model, - 5, 0.0 + i * (0.2 + 8 * (0.9 + 0.1) + 0.2),
            1.0 + i * 5.0, 0, 0, 0, 1.0);
    }

    // Add lightsources
    // Directional light
    scene->AddDirectionalLight(0, Vector3D(0.1, 0.1, - 1.0), Color(0.4, 0.4, 0.4));
#ifdef HALO
    // Halo light.
    scene->SetFlags(
#ifdef HALO_MOVING
        SRE_OBJECT_DYNAMIC_POSITION |
#endif
        SRE_OBJECT_EMISSION_ONLY |
        SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_LIGHT_HALO | SRE_OBJECT_NO_PHYSICS);
    scene->SetEmissionColor(Color(1.0, 0.9, 0.9));
    sreModel *billboard_object = sreCreateBillboardModel(scene, true);
    scene->SetBillboardSize(8.0f, 8.0f);
    scene->SetHaloSize(1.0f);
    lightsource_object_index[0] =
        scene->AddObject(billboard_object, 20, 20, 30, 0, 0, 0, 1);
#ifdef HALO_LIGHT
    int l = scene->AddPointSourceLight(
#ifdef HALO_MOVING
        SRE_LIGHT_DYNAMIC_POSITION,
#else
        0,
#endif
        Point3D(20, 20, 30), 100.0, Color(1.0, 1.0, 1.0));
    scene->AttachLight(lightsource_object_index[0], l, Vector3D(0, 0, 0));
#endif
#endif
}


void Demo7Render() {
#ifdef HALO_MOVING
    // Note: When using double precision cos and sin functions, intractable errors occurred when
    // simultaneously using the bullet library on the Raspberry Pi platform.
    float x = 20 + 20 * cosf((float)demo_time * 2 * M_PI / 5);
    float y =  20 + 20 * sinf((float)demo_time * 2 * M_PI / 5);
    float z = 30;
// printf("%lf x = %f y = %f\n", demo7_time, x, y)
    scene->ChangePosition(lightsource_object_index[0], x, y, z);
#endif
    scene->Render(view);
}

void Demo7TimeIteration(double time_previous, double time_current) {
}

