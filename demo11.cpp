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

// demo11.cpp -- test multi-color object instancing.

#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "sre.h"
#include "demo.h"

void Demo11CreateScene() {
    // Add player sphere as scene object 0.
    sreModel *sphere_model = sreCreateSphereModel(scene, 0);

    scene->SetMass(1.0f);
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS |
        SRE_OBJECT_USE_TEXTURE);
    scene->SetTexture(sreCreateStripesTexture(TEXTURE_TYPE_LINEAR,
        256, 256, 32, Color(0, 0.5f, 0.8f), Color(0.9f, 0.9f, 1.0f)));
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->AddObject(sphere_model, Point3D(0, - 40.0f, 3.0f), Vector3D(0, 0, 0), 3.0f);
    scene->SetMass(0);

    // Add ground
    sreModel *checkerboard_model = sreCreateCheckerboardModel(scene, 4, 50,
        Color(1.0, 0.2, 0.2), Color(0.2, 0.2, 1.0));
    scene->SetFlags(SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_NO_BACKFACE_CULLING |
         SRE_OBJECT_NO_PHYSICS);
    scene->SetEmissionColor(Color(0, 0, 0));
    for (int x = - 4; x <= 4; x++)
        for(int y = - 4; y <= 4; y++)
            scene->AddObject(checkerboard_model, - 100.0 + x * 200.0, y * 200.0, 0, 0, 0, 0, 1);

#if 0
    // Create a sphere modified (instanced) for multi-color.
    sreModel *multi_color_sphere = sreCreateNewMultiColorModel(scene, sphere_model,
        SRE_MULTI_COLOR_FLAG_SHARE_RESOURCES | SRE_MULTI_COLOR_FLAG_NEW_RANDOM, 0, NULL);

    // Add it to the scene.
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_MULTI_COLOR);
    scene->SetDiffuseReflectionColor(Color(0.75, 0.75, 1.0));
    scene->SetMass(0.3);
    int soi = scene->AddObject(multi_color_sphere, 40, 30, 7.0, 0, 0, 0, 7.0);
    scene->SetMass(0);
#endif

    // Read model file (high-poly model).
    sreModel *venus_model = sreReadModelFromFile(scene, "venusm.obj", SRE_MODEL_FILE_TYPE_OBJ, 0);
    // We don't want to use all those triangles for collisions, reduce to convex hull.
    venus_model->collision_shape_static = SRE_COLLISION_SHAPE_CONVEX_HULL;
    sreBoundingVolumeAABB AABB;
    venus_model->GetMaxExtents(&AABB, NULL);
    // We rotate the model 90 degrees along x, so that the min x coordinate will be the
    // min z coordinate (relative to the ground at z == 0).
    float venus_model_scale = 20.0f / (AABB.dim_max.x - AABB.dim_min.x);
    // Calculate z so that the model is based on the ground.
    float z = - AABB.dim_min.y * venus_model_scale;
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 0.25f));
    const int nu_venus_models = 30;
    const float circle_radius = 150.0f;
    // Create a circle.
    for (int i = 0; i < nu_venus_models; i++) {
        const Color color[3] = {
            Color(1.0f, 1.0f, 0.5f), Color(1.0f, 1.0f, 1.0f), Color(0.85f, 1.0f, 0.85f)
            };
        scene->SetDiffuseReflectionColor(color[i % 3]);
        float x = cosf(i * 2.0f * M_PI / nu_venus_models) * circle_radius;
        float y = sinf(i * 2.0f * M_PI / nu_venus_models) * circle_radius;
        // Rotate along y so that models all face inward.
        float y_rot = i * 2.0f * M_PI / nu_venus_models - M_PI * 0.5f;
        scene->AddObject(venus_model, x, y, z,
             // Rotate along y, and rotate 90 degrees along model's x axis.
            M_PI * 0.5f, y_rot, 0,
            venus_model_scale);
    }

    // Add central cylinder (length = 30) with light source.
    // Cylinder is scaled by two (length 60, radius 2).
    // Location is off-center to vary the light angle on the models.
    Point3D cylinder_pos = Point3D(0, 80.0f, 0);
    sreModel *cylinder_model = sreCreateCylinderModel(scene, 30.0f, true, false); // Without bottom.
    scene->SetEmissionColor(Color(0, 0, 0));
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    scene->SetDiffuseReflectionColor(Color(0.2f, 1.0f, 0.2f));
    scene->AddObject(cylinder_model, cylinder_pos, Vector3D(0, 0, 0), 2.0f);
    scene->SetEmissionColor(Color(1.0f, 1.0f, 1.0f));
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY);
    scene->AddObject(sphere_model, cylinder_pos + Vector3D(0, 0.0, 68.0f), Vector3D(0, 0, 0), 8.0);
    scene->AddPointSourceLight(0, cylinder_pos + Vector3D(0, 0, 68.0f), 300.0f,
        Color(1.0f, 1.0f, 1.0f));
    scene->SetEmissionColor(Color(0, 0, 0));

#if 0
    // Add lightsource
    scene->SetAmbientColor(Color(0.2, 0.2, 0.2));
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_INFINITE_DISTANCE);
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    scene->AddObject(sphere_model, 50000.0, - 60000.0, 50000.0, 0, 0, 0, 1000.0);
    scene->AddDirectionalLight(0, Vector3D(- 0.5, 0.6, - 0.5),
       Color(0.5, 0.5, 0.5));
#endif
}

void Demo11Render() {
    scene->Render(view);
}

void Demo11TimeIteration(double time_previous, double time_current) {
}
