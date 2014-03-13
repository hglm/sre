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

// demo2.cpp -- large landscape with cylinder and lights, and model loaded from .OBJ file
// in the center. Universal loading using assimp currently disabled.
// Also includes static particle system.

#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "sre.h"
#include "sreRandom.h"

#include "demo.h"

static sreRNG *rng;
static sreModel *particle_system_500_model = NULL;

#define PARTICLE_SYSTEM_RADIUS_XY 15.0f
#define PARTICLE_SYSTEM_HEIGHT 25.0f
#define PARTICLE_SIZE 0.5f

static void AddParticleSystem500(sreScene *scene, const Point3D& position, Color color) {
    if (particle_system_500_model == NULL)
        return;
    // Create a particle system in the shape of a dome (half of a sphere).
    Vector3D *particles = new Vector3D[500];
    for (int i = 0; i < 500; i++) {
        Vector3D displacement;
        displacement.x = rng->RandomFloat(2.0f) - 1.0f;
        displacement.y = rng->RandomFloat(2.0f) - 1.0f;
        displacement.z = 1.0f - pow(rng->RandomFloat(1.0f), 0.5f);
        displacement.Normalize();
        displacement.x *= PARTICLE_SYSTEM_RADIUS_XY;
        displacement.y *= PARTICLE_SYSTEM_RADIUS_XY;
        displacement.z *= PARTICLE_SYSTEM_HEIGHT;
        particles[i] = displacement;
    }
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_EMISSION_ONLY |
        SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_LIGHT_HALO |
        SRE_OBJECT_PARTICLE_SYSTEM);
    scene->SetEmissionColor(color);
    scene->SetBillboardSize(PARTICLE_SIZE, PARTICLE_SIZE);
    scene->SetHaloSize(1.0f);
    // Given the max distance of a particle to the system center, the corners of the billboard
    // (with dimensions 1 x 1, oriented towards the viewpoint) are at a further max distance of
    // sqrt(0.5^2 + 0.5^2). This defines the worst case bounding sphere of the particle system.
    float max_distance = PARTICLE_SYSTEM_RADIUS_XY > PARTICLE_SYSTEM_HEIGHT ?
        PARTICLE_SYSTEM_RADIUS_XY : PARTICLE_SYSTEM_HEIGHT;
    float bounding_sphere_radius = max_distance + sqrtf(2.0f * PARTICLE_SIZE);
    int ps_id = scene->AddParticleSystem(particle_system_500_model, 500, position,
         bounding_sphere_radius, particles);
}

Color light_color_array[4] = {
    Color(1.0, 1.0, 0.4),
    Color(1.0, 0.4, 0.4),
    Color(1.0, 1.0, 1.0),
    Color(1.0, 0.7, 0.4)
};

Color light_object_color_array[4] = {
    Color(1.0, 1.0, 0.7),
    Color(1.0, 0.7, 0.7),
    Color(1.0, 1.0, 1.0),
    Color(1.0, 0.85, 0.7)
};

void Demo2CreateScene(sreScene *scene, sreView *view) {
    rng = sreGetDefaultRNG();

    // Add player sphere as scene object 0.
    sreModel *sphere_model = sreCreateSphereModel(scene, 0);
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS |
        SRE_OBJECT_USE_TEXTURE);
    scene->SetTexture(sreCreateStripesTexture(TEXTURE_TYPE_LINEAR,
        256, 256, 32, Color(0, 0.5f, 0.8f), Color(0.9f, 0.9f, 1.0f)));
    scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
    scene->AddObject(sphere_model, Point3D(0, - 40.0f, 3.0f), Vector3D(0, 0, 0), 3.0f);

    // Add ground
    sreModel *checkerboard_model = sreCreateCheckerboardModel(scene, 20, 10,
        Color(0.5, 0.1, 0.1), Color(0.1, 0.1, 0.5));
    scene->SetFlags(SRE_OBJECT_MULTI_COLOR | SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_NO_PHYSICS);
    for (int x = - 16; x <= 16; x++)
        for(int y = - 16; y <= 16; y++)
            scene->AddObject(checkerboard_model, - 100.0 + x * 200.0, y * 200.0, 0, 0, 0, 0, 1);

#if 0
    sreModel *assimp_model = new sreModel;
    ReadFileAssimp(assimp_model, "collada/ant/AntBeast.dae", "collada/ant/");
//    ReadFileAssimp(assimp_model, "elephant.obj", "");
//    Color *color = new Color;
//    color->r = 1.0; color->g = 0.5, color->b = 0.5;
//    scene->SetColor(color);
    scene->SetTexture(NULL); // Force use of mesh-specific textures.
    scene->SetNormalMap(NULL);
    scene->SetSpecularityMap(NULL);
    scene->SetFlags(assimp_model->flags);
//    scene->SetFlags(0 /* OBJECT_FLAG_IS_LIGHTSOURCE | SRE_OBJECT_USE_TEXTURE */);
    scene->SetMaterial(1.0, 1.0);
    // Place the object on the ground at a reasonable scale. 
    model= scene->AddObject(assimp_model, 0, 40, 6.7, 0, 0, 0, 20 / assimp_model->sphere_radius);
    scene->sceneobject[model]->render_shadows = true;
#endif

    sreModel *elephant_model = sreReadModelFromFile(scene, "elephant.obj", SRE_MODEL_FILE_TYPE_OBJ, 0);
    elephant_model->SetLODModelFlags(SRE_LOD_MODEL_NOT_CLOSED);
    Color c = Color(1.0f, 0.5f, 0.5f);
    scene->SetDiffuseReflectionColor(c);
//    scene->SetFlags(SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_CAST_SHADOWS);
    // Place the object on the ground at a reasonable scale. 
//    id = scene->AddObject(elephant_model, 0, 40, 0, M_PI / 2, 0, 0, 20 / object->sphere_radius);

    // Create a circle of elephants.
    // Always use the object shadow cache for shadow volumes because there are 12 different
    // transformations for the model.
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_USE_OBJECT_SHADOW_CACHE);
    for (int i = 0; i < 12; i++) {
        scene->AddObject(elephant_model, cosf(i / 12.0 * 2.0f * M_PI) * 45.0f,
            100.0f + sinf(i / 12.0 * 2.0f * M_PI) * 45.0,
            0, M_PI / 2.0f, i / 12.0 * 2.0f * M_PI, 0, 10.0f / elephant_model->sphere.radius);
    }

    // Add movable sphere.
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetDiffuseReflectionColor(Color(0.75f, 0.75f, 1.0f));
    scene->SetMass(0.3);
    int soi = scene->AddObject(sphere_model, 40.0f, 30.0f, 5.0f, 0, 0, 0, 5.0f);
    scene->SetMass(0);

    // Add particle system with small light halos.
    particle_system_500_model = sreCreateParticleSystemModel(scene, 500, true);
    AddParticleSystem500(scene, Point3D(- 50.0f, 10.0f, 0), Color(1.0f, 1.0f, 0));
    scene->SetEmissionColor(Color(0, 0, 0));

    // Add cylinders in concentric circles.
    sreModel *cylinder_model = sreCreateCylinderModel(scene, 15.0, true, false); // Without bottom.
    cylinder_model->SetLODModelFlags(SRE_LOD_MODEL_OPEN_SIDE_HIDDEN_FROM_LIGHT);
    for (int i = 0; i < 30; i++) {
       Color color = Color(
           (float)rand() / RAND_MAX * 0.8 + 0.2,
           (float)rand() / RAND_MAX * 0.8 + 0.2,
           (float)rand() / RAND_MAX * 0.8 + 0.2
           );
        for (int j = 0; j < 50; j++) {
            float x = cosf((j / 50.0 + (i & 1) * 0.5) * 2.0 * M_PI) * (i * 100.0 + 200.0);
            float y = 100.0 + sinf((j / 50.0 + (i & 1) * 0.5) * 2.0 * M_PI) * (i * 100.0 + 200.0);
            scene->SetDiffuseReflectionColor(color);
            scene->SetEmissionColor(Color(0, 0, 0));
            scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
            // Because the cylinder is thin, we don't need the highest level of detail.
            scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 0, 2.0f);
            scene->AddObject(cylinder_model, x, y, 0, 0, 0, 0, 2.0f);
            scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 0, 1.0f);
            if ((float)rand() / RAND_MAX < 0.05) {
                int k = rand() & 3;
                Color light_color = light_color_array[k];
                scene->SetEmissionColor(light_object_color_array[k]);
                scene->SetFlags(SRE_OBJECT_EMISSION_ONLY);
                scene->AddObject(sphere_model, x, y, 35.0, 0, 0, 0, 5.0);
                scene->AddPointSourceLight(0, Point3D(x, y, 35.0), 100.0, light_color);
            }
        }
    }
    // Add central cylinder.
    scene->SetEmissionColor(Color(0, 0, 0));
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS);
    scene->SetDiffuseReflectionColor(Color(0.2, 1.0, 0.2));
    scene->AddObject(cylinder_model, 0, 100.0, 0, 0, 0, 0, 2.0);
    // The light is represented by a sphere (emission only, not a shadow caster).
    scene->SetEmissionColor(light_object_color_array[0]);
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY);
    scene->AddObject(sphere_model, 0, 100.0, 38.0, 0, 0, 0, 8.0);
    // Add point light at the location of the sphere (light will not be blocked).
    scene->AddPointSourceLight(0, Point3D(0, 100.0, 38.0), 100.0, light_color_array[0]);
    scene->SetEmissionColor(Color(0, 0, 0));

    // Add movable ellipsoid.
    sreModel *ellipsoid_model = sreCreateEllipsoidModel(scene, 0.8, 0.6);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetDiffuseReflectionColor(Color(0.5, 0.7, 0.4));
    scene->SetMass(0.8);
    scene->AddObject(ellipsoid_model, 20, 30, 3.0, 0, 0, 0, 5.0);
    scene->SetMass(0);

    // Add movable capsule.
    sreModel *capsule_model = sreCreateCapsuleModel(scene, 1.0, 2.0, 1.0, 1.0);
    scene->SetFlags(SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetDiffuseReflectionColor(Color(1.0, 0.7, 0.4));
    scene->SetMass(0.8);
    scene->AddObject(capsule_model, 0, 30, 3.0, 0, 0, 0, 3.0);
    scene->SetMass(0);

    // Add directional lightsource.
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_INFINITE_DISTANCE);
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    scene->AddObject(sphere_model, 50000.0f, - 60000.0f, 50000.0f, 0, 0, 0, 1000.0f);
    scene->AddDirectionalLight(0, Vector3D(- 0.5f, 0.6f, - 0.5f).Normalize(),
        Color(0.3f, 0.3f, 0.3f));
}

void Demo2Step(sreScene *scene, double demo_time) {
}
