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
#include "demo.h"

static int lightsource_object_index[2];

#if defined(OPENGL)
#define HALO_MOVING
#endif
#define HALO_LIGHT

void Demo5CreateScene(sreScene *scene, sreView *view) {
    // Set diffuse fraction to 0.6 and two roughness values of 0.1 and 0.25 with weight 0.4 and 0.6,
    // isotropic.
    scene->SetMicrofacetParameters(0.6, 0.1, 0.4, 0.25, 0.6, false);

    sreTexture *tex = new sreTexture("beachball", TEXTURE_TYPE_NORMAL);

    sreModel *sphere_model = sreCreateSphereModel(scene, 0);
    // Add player sphere as scene object 0
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS | 
        SRE_OBJECT_USE_TEXTURE);
    scene->SetTexture(sreCreateStripesTexture(TEXTURE_TYPE_LINEAR,
        256, 256, 32, Color(1.0f, 1.0f, 0), Color(0.6f, 0.6f, 0)));
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->SetSpecularReflectionColor(Color(0.9f, 0.9f, 0.6f));
    scene->AddObject(sphere_model, - 15.0f, - 100.0f, 3.0f, 0, 0, 0, 3.0f);
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->SetSpecularReflectionColor(Color(1.0f, 1.0f, 1.0f));
    int i;

    sreModel *ground_model = sreCreateRepeatingRectangleModel(scene, 1000.0, 20.0);
    sreTexture *ground_texture = new sreTexture("MossAndGrass4", TEXTURE_TYPE_WRAP_REPEAT);
    scene->SetTexture(ground_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_NO_PHYSICS);
    scene->AddObject(ground_model, - 500, - 500, 0, 0, 0, 0, 1);

#if defined(OPENGL) && !defined(NO_LARGE_TEXTURES)
    // Starry background.
    sreTexture *stars_texture = new sreTexture("yale8", TEXTURE_TYPE_NORMAL);
    scene->SetEmissionColor(Color(1.0f, 1.0f, 1.0f));
    scene->SetEmissionMap(stars_texture);
    scene->SetFlags(SRE_OBJECT_USE_EMISSION_MAP | SRE_OBJECT_NO_BACKFACE_CULLING |
        SRE_OBJECT_INFINITE_DISTANCE | SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_EMISSION_ONLY);
    i = scene->AddObject(sphere_model, 0, 0, 0, 0, 0, 0, SRE_DEFAULT_FAR_PLANE_DISTANCE * 90.0f);
    scene->SetEmissionColor(Color(0, 0, 0));
#endif

#if 1
    // Torus landscape.
    sreModel *torus_model = sreCreateTorusModel(scene);
    sreTexture *donut_normalmap[3];
    donut_normalmap[0] = new sreTexture("normal_map_bump_pattern", TEXTURE_TYPE_NORMAL_MAP);
    scene->SetFlags(SRE_OBJECT_USE_NORMAL_MAP /* | SRE_OBJECT_DYNAMIC_POSITION */ | SRE_OBJECT_CAST_SHADOWS);
    // Set diffuse fraction to 0.6 and roughness values of 0.1 and 0.15, anisotropic.
    scene->SetMicrofacetParameters(0.6, 0.1, 1.0, 0.15, 1.0, true);
    // The torus model is detailed and has plenty of vertices, scale the LOD thresholds for
    // better performance, and use level 1 for physics.
    scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 0, - 1, 2.0, 1);
    Color c;
    for (int x = 0; x < 10; x++)
        for (int y = 0; y < 10; y++) {
            if (rand() % 3 == 0)
                continue;
            float wx = x * ((TORUS_RADIUS + TORUS_RADIUS2) * 2);
            float wy = y * ((TORUS_RADIUS + TORUS_RADIUS2) * 2);
            float wz = 2;
            int h = rand() % 4 + 1;
            for (int z = 0; z < h; z++) {
                c.r = (float)rand() / RAND_MAX;
                c.g = (float)rand() / RAND_MAX;
                c.b = (float)rand() / RAND_MAX;
                scene->SetColor(c);
		scene->SetNormalMap(donut_normalmap[rand() & 0]);
                i = scene->AddObject(torus_model, wx, wy, wz + z * TORUS_RADIUS2 * 2, 0, 0, 0, 1);
                // Because the object are movable and can change orientation, use the so shadow cache.
//                scene->sceneobject[i]->use_so_shadow_cache = true;
            }
        }
    scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 0, - 1, 1.0, 0);
    // Add two "copper" torusses, one anisotropic, the other isotropic.
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    scene->SetDiffuseReflectionColor(Color(0.8, 0.6, 0.1));
    scene->SetSpecularReflectionColor(Color(0.9, 0.9, 0.5));
    scene->SetMicrofacetParameters(0.4, 0.15, 1.0, 1.0, 0.0, false);
    scene->AddObject(torus_model, - 15.0 , - 5.0, 1.0, 0, 0, 0, 1.0);
    scene->SetMicrofacetParameters(0.4, 0.1, 1.0, 0.2, 1.0, true);
    scene->AddObject(torus_model, - 30.0 , - 5.0, 1.0, 0, 0, 0, 0.5);
    scene->SetMicrofacetParameters(0.6, 0.1, 0.4, 0.25, 0.6, false);
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetSpecularReflectionColor(Color(1.0, 1.0, 1.0));
#endif

#if 0
    // Tower of textures toruses.
    x = - 4 * (TORUS_RADIUS + TORUS_RADIUS2) * 2 - 40;
    y = 4 * (TORUS_RADIUS + TORUS_RADIUS2) * 2;
    float size = 3;
    float z = TORUS_RADIUS2 * size;
    sreTexture *wall_texture = new sreTexture("tijolo", TEXTURE_TYPE_NORMAL);
    scene->SetTexture(wall_texture);
    sreTexture *wall_normals = new sreTexture("tijolo_normal_map", TEXTURE_TYPE_NORMAL_MAP);
    // Because we have 20 different-size instantiations of the same object, for directional lights use the
    // scene object shadow volume cache (normally only for point lights) instead of the model object shadow
    // volume cache (which doesn't have room for 20 entries for the same object).
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
        SRE_OBJECT_USE_OBJECT_SHADOW_CACHE);
    scene->SetNormalMap(wall_normals);
    for (int i = 0; i < 20; i++) {
        Color *color = new Color;
        color->r = (float)rand() / RAND_MAX;
        color->g = (float)rand() / RAND_MAX;
        color->b = (float)rand() / RAND_MAX;
        scene->SetColor(color);
        int j = scene->AddObject(torus_model, x, y, z, 0, 0, 0, size);
        scene->sceneobject[j]->destructable = false;
        scene->sceneobject[j]->render_shadows = true;
        scene->sceneobject[j]->use_so_shadow_cache = true;
        z += TORUS_RADIUS2 * (size + size * 0.8);
        size *= 0.8;
    }
#endif

#if 1
    // Add a beachball that can be pushed
    scene->SetTexture(tex);
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_CAST_SHADOWS);
    c.r = 0.75;
    c.g = 0.75;
    c.b = 1.0;
    scene->SetColor(c);
    int sphere_object_index = scene->AddObject(sphere_model, -30, 15, 5.00, 0, 0, 0, 5.0);
#endif

#if 0
    // Add block objects.
    sreModel *unit_block_model = sreCreateUnitBlockObject();
    scene->SetTexture(wall_texture);
    scene->SetNormalMap(wall_normals);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP);
    i = scene->AddObject(unit_block_model, - 80, 10, 0, 0, 0, 0, 20);
    scene->sceneobject[i]->target_collision_reaction = COLLISION_REACTION_SLIDE;
    scene->sceneobject[i]->render_shadows = true;
    scene->SetTexture(wall_texture);
    scene->SetNormalMap(wall_normals);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP);
    i = scene->AddObject(unit_block_model, - 80, 120, 0, 0, 0, 0, 30);
    scene->sceneobject[i]->target_collision_reaction = COLLISION_REACTION_BUMP_REFLECTIVE;
    scene->sceneobject[i]->render_shadows = true;
#endif
#if 0
    i = scene->AddObject(block_model, - 20, 150, 0, 0, 0, 0, 30);
    scene->sceneobject[i]->target_collision_reaction = COLLISION_REACTION_BUMP_REFLECTIVE; /*COLLISION_REACTION_SLIDE; */
    scene->sceneobject[i]->render_shadows = true;
#endif

    // Add ramps.
    sreModel *ramp_towards_back_object = sreCreateRampModel(scene, 30, 100, 10, RAMP_TOWARDS_BACK);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    Color red;
    red.r = 1.0;
    red.g = 0.2;
    red.b = 0.2;
    scene->SetColor(red);
    i = scene->AddObject(ramp_towards_back_object, - 50, 20, 0, 0, 0, 0, 1);
    sreModel *ramp_towards_back_object2 = sreCreateRampModel(scene, 30, 50, 20, RAMP_TOWARDS_BACK);
    i = scene->AddObject(ramp_towards_back_object2, - 50, 120, 10, 0, 0, 0, 1);
    // Add block after ramp.
    sreModel *block_model = sreCreateBlockModel(scene, 30, 20, 30, 0);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    i = scene->AddObject(block_model, - 50, 170, 0, 0, 0, 0, 1);
    // Add another ramp.
    sreModel *ramp_towards_left_object = sreCreateRampModel(scene, 100, 20, 30, RAMP_TOWARDS_LEFT);
    scene->SetColor(red);
    i = scene->AddObject(ramp_towards_left_object, - 20, 170, 0, 0, 0, 0, 1);

#if defined(OPENGL)
#if 0
    // Add a lot of towers at great distance.
    sreModel *tower_model = sreCreateBlockModel(scene, 50, 50, 500);
    for (int i = 0; i < 100; i++) {
        c.r = (float)rand() / RAND_MAX;
        c.g = (float)rand() / RAND_MAX;
        c.b = (float)rand() / RAND_MAX;
        scene->SetColor(c);
        float x = (float)rand() * 10000 / RAND_MAX - 5000;
        float y = (float)rand() * 10000 / RAND_MAX - 5000;
        int j = scene->AddObject(tower_model, x, y, 0, 0, 0, 0, 1);
    }
#endif
#endif

    // Directional light
    scene->AddDirectionalLight(0, Vector3D(- 0.2, - 0.1, - 1.0), Color(0.3, 0.3, 0.3));
    // Add halo objects
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_EMISSION_ONLY |
        SRE_OBJECT_LIGHT_HALO | SRE_OBJECT_NO_PHYSICS);
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    sreModel *billboard_object1 = sreCreateBillboardModel(scene, true);
    scene->SetBillboardSize(8.0, 8.0);
    float halo_size_full_fit = 1.0f;
    scene->SetHaloSize(halo_size_full_fit);
    i = scene->AddObject(billboard_object1, 0, 20, 30, 0, 0, 0, 1);
    lightsource_object_index[0] = i;
    sreModel *billboard_object2 = sreCreateBillboardModel(scene, true);
    scene->SetEmissionColor(Color(1.0, 0.2, 0.2));
    scene->SetBillboardSize(6.0, 6.0);
    scene->SetHaloSize(halo_size_full_fit);
    i = scene->AddObject(billboard_object2, 40, 80, 20, 0, 0, 0, 1);
    lightsource_object_index[1] = i;

    // Halo lights above the scene.
#ifdef HALO_LIGHT
#ifdef HALO_MOVING
    int flags = SRE_LIGHT_DYNAMIC_POSITION;
#else
    int flags = 0;
#endif
    int l1 = scene->AddPointSourceLight(flags, Point3D(0, 20, 30), 50, Color(1.0, 1.0, 1.0));
    int l2 = scene->AddPointSourceLight(flags, Point3D(40, 80, 20), 50, Color(1.0, 0.4, 0.4));
    scene->AttachLight(lightsource_object_index[0], l1, Vector3D(0, 0, 0), Vector3D(0, 0, 0));
    scene->AttachLight(lightsource_object_index[1], l2, Vector3D(0, 0, 0), Vector3D(0, 0, 0));
#endif
#if defined(OPENGL)
    // Create a line of small yellow lights at the back of the scene. Performance test
    // for scissors optimization in multi-pass rendering.
    scene->SetEmissionColor(Color(1.0, 1.0, 0));
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_LIGHT_HALO |
        SRE_OBJECT_NO_PHYSICS);
    for (int i = 0; i < 10; i++) {
        float x = i * 30 - 100;
        float y = 200;
        float z = 10;
        // Create a seperate billboard model for every billboard (halo).
        // Otherwise, the same vertex buffers would be repeatedly changed within the
        // rendering of a single frame.
        sreModel *billboard_object = sreCreateBillboardModel(scene, true);
        scene->SetBillboardSize(3.0, 3.0);
        scene->SetHaloSize(halo_size_full_fit);
        scene->AddObject(billboard_object, x, y, z, 0, 0, 0, 1);
        scene->AddPointSourceLight(0, Point3D(x, y, z), 15.0, Color(1.0, 1.0, 0));
    }
#endif
}


void Demo5Step(sreScene *scene, double demo_time) {
#ifdef HALO_MOVING
    float x = 20 * cosf((float)demo_time * 2 * M_PI / 5);
    float y = 20 + 20 * sinf((float)demo_time * 2 * M_PI / 5);
    float z = 30;
    scene->ChangePosition(lightsource_object_index[0], x, y, z);
    x = 40 + 10 * cosf((float)demo_time * 2 * M_PI / 3 + M_PI / 2);
    y = 80 + 10 * sinf((float)demo_time * 2 * M_PI / 3 + M_PI / 2);
    z = 20;
    scene->ChangePosition(lightsource_object_index[1], x, y, z);
#endif
}

void Demo6Step(sreScene *scene, double demo_time) {
    Demo5Step(scene, demo_time);
    Point3D viewpoint = Point3D(200 * cos(demo_time / 20.0f * 2.0f * M_PI
        + 0.5f * M_PI),
        140.0f + 200.0f * sin(demo_time / 20 * 2 * M_PI + 1.5 * M_PI), 40.0f);
    Point3D lookat = Point3D(100.0f, 100.0f, 0);
    Vector3D upvector = Vector3D(0, 0, 1.0f);
    sre_internal_application->view->SetViewModeLookAt(viewpoint, lookat, upvector);
}

