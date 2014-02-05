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

// demo10.cpp -- simple scene with movable geometric objects
// Optimized for OpenGL-ES2.0

#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "sre.h"
#include "demo.h"

// Enabling shadows degrades performance significantly on OpenGL ES 2.0
// devices.
#ifndef OPENGL_ES2
#define SHADOWS
#endif

void Demo10CreateScene() {
    // Add player sphere as scene object 0.
    sreModel *sphere_model = sreCreateSphereModel(scene, 0);
#ifndef SHADOWS
    // Reduce the level detail.
    scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 1, 1.0);
#else
    // Reduce the number of triangles in the models also when shadows
    // are enabled.
    scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 0, 2.0);
#endif
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS);
    Color c;
    c.r = 0;
    c.g = 0.75;
    c.b = 1.0;
    scene->SetColor(c);
    scene->AddObject(sphere_model, 0, - 40, 3.0, 0, 0, 0, 3.0);

    // Add ground
#ifndef SHADOWS
    sreModel *checkerboard_model = sreCreateCheckerboardModel(scene, 4, 50,
        Color(0.5, 0.1, 0.1), Color(0.1, 0.1, 0.5));
    // Because there is only a directional light, and the surface is flat,
    // lighting (without specular effects) can be emulated with emission only.
    scene->SetFlags(SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_NO_BACKFACE_CULLING |
         SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_EMISSION_ONLY |
         SRE_OBJECT_EMISSION_ADD_DIFFUSE_REFLECTION_COLOR);
#else
    sreModel *checkerboard_model = sreCreateCheckerboardModel(scene, 4, 50,
        Color(1.0, 0.2, 0.2), Color(0.2, 0.2, 1.0));
    scene->SetFlags(SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_NO_BACKFACE_CULLING |
         SRE_OBJECT_NO_PHYSICS);
#endif
    scene->SetEmissionColor(Color(0, 0, 0));
    for (int x = - 4; x <= 4; x++)
        for(int y = - 4; y <= 4; y++)
            scene->AddObject(checkerboard_model, - 100.0 + x * 200.0, y * 200.0, 0, 0, 0, 0, 1);

    // Add sphere
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetDiffuseReflectionColor(Color(0.75, 0.75, 1.0));
    scene->SetMass(0.3);
    int soi = scene->AddObject(sphere_model, 40, 30, 7.0, 0, 0, 0, 7.0);
    scene->SetMass(0);

    // Add movable ellipsoids.
    sreModel *ellipsoid_model = sreCreateEllipsoidModel(scene, 0.8, 0.6);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetMass(0.8);
    for (int i = 0; i < 8; i++) {
        Color color;
        color.SetRandom();
        scene->SetDiffuseReflectionColor(color);
        scene->AddObject(ellipsoid_model, 20, 30 + 40 * i, 0.6 * 7.0,
            0, 0, 0, 7.0);
    }
    scene->SetMass(0);

    // Add movable capsules.
    sreModel *capsule_model = sreCreateCapsuleModel(scene, 1.0, 2.0, 1.0, 1.0);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetDiffuseReflectionColor(Color(1.0, 0.7, 0.4));
    scene->SetMass(0.8);
    for (int i = 0; i < 8; i++) {
        Color color;
        color.SetRandom();
        scene->SetDiffuseReflectionColor(color);
        scene->AddObject(capsule_model, 0 - 25 * i, 30, 4.0, 0, 0, 0, 4.0);
    }
    scene->SetMass(0);

    // Add lightsource
    scene->SetAmbientColor(Color(0.2, 0.2, 0.2));
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_INFINITE_DISTANCE);
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    scene->AddObject(sphere_model, 50000.0, - 60000.0, 50000.0, 0, 0, 0, 1000.0);
    scene->AddDirectionalLight(0, Vector3D(- 0.5, 0.6, - 0.5),
       Color(0.5, 0.5, 0.5));
}

void Demo10Render() {
    scene->Render(view);
}

void Demo10TimeIteration(double time_previous, double time_current) {
}
