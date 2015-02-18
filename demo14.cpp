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

// demo12.cpp -- Test non-closed objects, transparent (punch-through) textures.

#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "sre.h"
#include "sreBackend.h"
#include "demo.h"

static sreModel *x_plane_rect_model;
static sreModel *y_plane_rect_model;
static sreModel *z_plane_rect_model;

static void AddCage(sreScene *scene, const Point3D& pos, float cage_size) {
    scene->AddObject(y_plane_rect_model, pos + Vector3D(cage_size * 0.5f, 0.0f, cage_size * 0.5f),
        Vector3D(0, 0, 0), cage_size);
    scene->AddObject(y_plane_rect_model, pos + Vector3D(cage_size * 0.5f, cage_size, cage_size * 0.5f),
        Vector3D(0, 0, 0), cage_size);
    scene->AddObject(x_plane_rect_model, pos + Vector3D(0.0f, cage_size * 0.5f, cage_size * 0.5f),
        Vector3D(0, 0, 0), cage_size);
    scene->AddObject(x_plane_rect_model, pos + Vector3D(cage_size, cage_size * 0.5f, cage_size * 0.5f),
        Vector3D(0, 0, 0), cage_size);
    scene->AddObject(z_plane_rect_model, pos + Vector3D(cage_size * 0.5f, cage_size * 0.5f, cage_size),
        Vector3D(0, 0, 0), cage_size);
}

void Demo14CreateScene(sreScene *scene, sreView *view) {
    // Add player sphere as scene object 0.
    sreModel *sphere_model = sreCreateSphereModel(scene, 0);
    // Set diffuse fraction to 0.6 and two roughness values of 0.1 and 0.25 with weight 0.4 and 0.6,
    // isotropic.
    scene->SetMicrofacetParameters(0.6, 0.1, 0.4, 0.25, 0.6, false);
    scene->SetSpecularExponent(40.0f);
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS |
        SRE_OBJECT_USE_TEXTURE);
    scene->SetTexture(sreCreateStripesTexture(TEXTURE_TYPE_LINEAR,
        256, 256, 32, Color(0, 0.5f, 0.8f), Color(0.9f, 0.9f, 1.0f)));
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->SetSpecularReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->AddObject(sphere_model, Point3D(0, - 40.0f, 3.0f), Vector3D(0, 0, 0), 3.0f);

    // Add ground
    sreModel *checkerboard_model = sreCreateCheckerboardModel(scene, 4, 12.5f,
        Color(0.5f, 0.2f, 0.2f), Color(0.2f, 0.2f, 1.0f));
    scene->SetFlags(SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_NO_BACKFACE_CULLING |
         SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_NOT_OCCLUDING);
    scene->SetEmissionColor(Color(0, 0, 0));
    for (int x = - 16; x <= 16; x++)
        for(int y = - 16; y <= 16; y++)
            scene->AddObject(checkerboard_model, - 100.0 + x * 50.0f, y * 50.0f, 0, 0, 0, 0, 1);

    scene->SetFlags(SRE_OBJECT_NO_BACKFACE_CULLING);

    // Create cage.
    x_plane_rect_model = sreCreateCenteredXPlaneRectangleModel(scene, 1.0f, 1.0f);
    y_plane_rect_model = sreCreateCenteredYPlaneRectangleModel(scene, 1.0f, 1.0f);
    z_plane_rect_model = sreCreateCenteredZPlaneRectangleModel(scene, 1.0f, 1.0f);
    scene->SetDiffuseReflectionColor(Color(0.3f, 0.9f, 0.3f));
    AddCage(scene, Point3D(- 50.0f, - 50.0f, 0.0f), 100.0f);

    // Add lightsource
    scene->SetAmbientColor(Color(0.1f, 0.1f, 0.1f));
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY);
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    scene->AddObject(sphere_model, Point3D(0.0f, 0.0f, 20.0f), Vector3D(0.0f, 0.0f, 0.0f), 5.0f);
    scene->AddPointSourceLight(0, Point3D(0.0f, 0.0f, 20.0f), 10000.0f, Color(1.0f, 1.0f, 1.0f));

    // Make the maximum shadow map region for directional lights larger so that all shadows are visible.
    sreSetShadowMapRegion(Vector3D(- 200.0, -200.0, -300.0) * 2.0f, Vector3D(200.0, 200.0, 300.0) * 2.0f);
}

void Demo14Step(sreScene *scene, double demo_time) {
}
