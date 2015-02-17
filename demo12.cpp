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

static sreTexture *CreateGratingTexture() {
    unsigned int color_pixel = 0xFFFFFFFF;
    unsigned int transparent_pixel = 0x00FFFFFF;
    sreTexture *tex = new sreTexture(64, 64);
    tex->format = TEXTURE_FORMAT_RAW_RGBA8;
    // First clear the texture with transparent pixels.
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++)
            tex->SetPixel(x, y, transparent_pixel);
    // Set the opaque pixels (grating pattern).
    for (int y = 0; y < 64; y += 8)
        for (int x = 0; x < 64; x += 8) {
             for (int i = x; i <= x + 7; i++) {
                 tex->SetPixel(i, y, color_pixel);
                 tex->SetPixel(i, y + 7, color_pixel);
             }
             for (int i = y + 1; i <= y + 6; i++) {
                 tex->SetPixel(x, i, color_pixel);
                 tex->SetPixel(x + 7 , i, color_pixel);
             }
	}
    tex->UploadGL(SRE_TEXTURE_TYPE_FLAG_WRAP_REPEAT);
    return tex;
}

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

void Demo12CreateScene(sreScene *scene, sreView *view) {
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
    scene->AddObject(sphere_model, Point3D(0, - 10.0f, 3.0f), Vector3D(0, 0, 0), 3.0f);

    // Add ground
    sreModel *checkerboard_model = sreCreateCheckerboardModel(scene, 4, 12.5f,
        Color(0.5f, 0.2f, 0.2f), Color(0.2f, 0.2f, 1.0f));
    scene->SetFlags(SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_NO_BACKFACE_CULLING |
         SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_NOT_OCCLUDING);
    scene->SetEmissionColor(Color(0, 0, 0));
    for (int x = - 16; x <= 16; x++)
        for(int y = - 16; y <= 16; y++)
            scene->AddObject(checkerboard_model, - 100.0 + x * 50.0f, y * 50.0f, 0, 0, 0, 0, 1);

    sreTexture *grating_tex = CreateGratingTexture();
    y_plane_rect_model = sreCreateCenteredYPlaneRectangleModel(scene, 1.0f, 1.0f);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_TRANSPARENT_TEXTURE |
        SRE_OBJECT_NO_BACKFACE_CULLING);
    scene->SetTexture(grating_tex);

    // Create single fence.
    scene->SetDiffuseReflectionColor(Color(0.8f, 0.8f, 1.0f));
    scene->AddObject(y_plane_rect_model, Point3D(0, 0, 20.0f), Vector3D(0, 0, 0), 40.0f);

    // Create cage.
    x_plane_rect_model = sreCreateCenteredXPlaneRectangleModel(scene, 1.0f, 1.0f);
    z_plane_rect_model = sreCreateCenteredZPlaneRectangleModel(scene, 1.0f, 1.0f);
    scene->SetDiffuseReflectionColor(Color(0.3f, 0.9f, 0.3f));
    // Small cage.
    AddCage(scene, Point3D(50.0f, 50.0f, 0.0f), 40.0f);

    scene->SetDiffuseReflectionColor(Color(0.8f, 0.8f, 0.2f));
    // Big cage.
    AddCage(scene, Point3D(- 200.0f, - 200.0f, 0.0f), 400.0f);
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));

    // Add lightsource
    scene->SetAmbientColor(Color(0.1f, 0.1f, 0.1f));
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_INFINITE_DISTANCE);
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    scene->AddObject(sphere_model, 50000.0, - 60000.0, 50000.0, 0, 0, 0, 1000.0);
    scene->AddDirectionalLight(0, Vector3D(- 0.5, 0.6, - 0.5).Normalize(),
       Color(1.0f, 1.0f, 1.0f));

    // Make the maximum shadow map region for directional lights larger so that all shadows are visible.
    sreSetShadowMapRegion(Vector3D(- 200.0, -200.0, -300.0) * 2.0f, Vector3D(200.0, 200.0, 300.0) * 2.0f);
}

void Demo12Step(sreScene *scene, double demo_time) {
}
