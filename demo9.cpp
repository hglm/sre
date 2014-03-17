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

// demo9.cpp -- simple scene with large transparent halo billboards
// Optimized for OpenGL-ES2.0

#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "sre.h"
#include "sreBackend.h"
#include "demo.h"

// Enabling shadows degrades performance significantly on OpenGL ES 2.0
// devices.
#ifndef OPENGL_ES2
#define SHADOWS
#endif

void Demo9CreateScene(sreScene *scene, sreView *view) {
    // Add player sphere as scene object 0.
    sreModel *sphere_model = sreCreateSphereModel(scene, 0);
#ifndef SHADOWS
    // Reduce the level detail.
    scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 1, 1.0, 0);
#else
    // Reduce the number of triangles in the models also when shadows
    // are enabled.
    scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 0, - 1, 2.0, 0);
#endif
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS |
        SRE_OBJECT_USE_TEXTURE);
    scene->SetTexture(sreCreateStripesTexture(TEXTURE_TYPE_LINEAR,
        256, 256, 32, Color(0, 0.5f, 0.8f), Color(0.9f, 0.9f, 1.0f)));
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->AddObject(sphere_model, Point3D(0, - 40.0f, 3.0f), Vector3D(0, 0, 0), 3.0f);

    // Add ground
#ifndef SHADOWS
    sreModel *checkerboard_model = sreCreateCheckerboardModel(scene, 4, 50,
        Color(0.5, 0.1, 0.1), Color(0.1, 0.1, 0.5));
    // Because there is only a directional light, and the surface is flat,
    // lighting (without specular effects) can be emulated with emission only.
    scene->SetFlags(SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_NO_BACKFACE_CULLING |
         SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_EMISSION_ONLY |
         SRE_OBJECT_EMISSION_ADD_DIFFUSE_REFLECTION_COLOR |
         SRE_OBJECT_NOT_OCCLUDING);
#else
    sreModel *checkerboard_model = sreCreateCheckerboardModel(scene, 4, 50,
        Color(1.0, 0.2, 0.2), Color(0.2, 0.2, 1.0));
    scene->SetFlags(SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_NO_BACKFACE_CULLING |
         SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_NOT_OCCLUDING);
#endif
    scene->SetEmissionColor(Color(0, 0, 0));
    for (int x = - 4; x <= 4; x++)
        for(int y = - 4; y <= 4; y++)
            scene->AddObject(checkerboard_model, - 100.0 + x * 200.0, y * 200.0, 0, 0, 0, 0, 1);

    // Halo test
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_LIGHT_HALO |
        SRE_OBJECT_NO_PHYSICS);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
        float x = j * 20.0f - 100.0f + (i & 1) * 5.0f;
        float y = 50.0f + 20.0f * i;
        float z = 10.0f;
        // Create a seperate billboard model for every billboard (halo).
        // Otherwise, the same vertex buffers would be repeatedly changed within the
        // rendering of a single frame.
        sreModel *billboard_object = sreCreateBillboardModel(scene, true);
        Color color;
        do {
            color.SetRandom();
        } while (color.SRGBIntensity() < 0.5f);
        scene->SetEmissionColor(color);
        scene->SetBillboardSize(20.0f, 20.0f);
        scene->SetHaloSize(1.0f);
        scene->AddObject(billboard_object, x, y, z, 0, 0, 0, 1.0f);
        }
    }

    // Add lightsource
    scene->SetAmbientColor(Color(0.2, 0.2, 0.2));
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_INFINITE_DISTANCE);
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    scene->AddObject(sphere_model, 50000.0, - 60000.0, 50000.0, 0, 0, 0, 1000.0);
    scene->AddDirectionalLight(0, Vector3D(- 0.5, 0.6, - 0.5),
       Color(0.5, 0.5, 0.5));

}

void Demo9Step(sreScene *scene, double demo_time) {
}
