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

static dstRNG *rng;

// The number of triangles that are defined by the fluid surface on one side.
#define FLUID_GRID_SIZE 256
// The size of the fluid surface in world coordinates (square).
#define FLUID_WORLD_SIZE 120.0f

// Size (range in y) of wave impulses at the far end of the water surface.
#define WAVE_SIZE 20
// Intensity of wave disturbance.
#define WAVE_DISTURBANCE_SIZE 2.0f
// Frequency of wave impulse in 1/60ths of a second.
#define WAVE_FREQUENCY 500
// How much to fade the wave impulse as distance increases from the far end of the water surface.
#define WAVE_FADE_FACTOR 0.8f

#define USE_WATER
#ifdef USE_WATER
const float disturbance_frequency = 0.1f;
const Color liquid_specular_reflection_color = Color(1.0f, 1.0f, 1.0f);

static float disturbance_displacement_func() {
   return - 0.5f;
}
#else
const float disturbance_frequency = 0.1;
const Color liquid_specular_reflection_color = Color(0.2f, 0.2f, 0.2f);

static float disturbance_displacement_func() {
   float r = rng->RandomFloat(1.0f);
   return 0.3 + 1.5 * pow(r, 6);
}
#endif

static sreObject *fluid_scene_object;

static void CreateWave() {
    for (int x = 2; x < FLUID_GRID_SIZE - 1; x++)
        for (int y = FLUID_GRID_SIZE - 2; y >= FLUID_GRID_SIZE - 2 - WAVE_SIZE + 1; y--) {
            sreCreateModelFluidDisturbance(fluid_scene_object->model,
                x, y, WAVE_DISTURBANCE_SIZE * (rng->RandomFloat(0.2f) + 0.8f)
                * (1.0f - WAVE_FADE_FACTOR * ((FLUID_GRID_SIZE - 2) - y) / WAVE_SIZE));
	}
}

void Demo13CreateScene(sreScene *scene, sreView *view) {
    rng = dstGetDefaultRNG();

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

    // Create fluid.
    // Arguments: Grid size, size between grids
    sreModel *fluid_object = sreCreateFluidModel(scene, FLUID_GRID_SIZE, FLUID_GRID_SIZE,
        FLUID_WORLD_SIZE / FLUID_GRID_SIZE, 1.0f, 0.1f, 0.003f, FLUID_WORLD_SIZE / 10.0f);
#ifdef USE_WATER
    sreTexture *texture = new sreTexture("water1", TEXTURE_TYPE_NORMAL |
        SRE_TEXTURE_TYPE_FLAG_WRAP_REPEAT);
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    scene->SetSpecularReflectionColor(liquid_specular_reflection_color);
    scene->SetFlags(SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetTexture(texture);
    scene->SetSpecularExponent(120.0);
#else
    sreTexture *texture = new sreTexture("volcanic8", TEXTURE_TYPE_NORMAL |
        SRE_TEXTURE_TYPE_FLAG_WRAP_REPEAT);
    scene->SetFlags(SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_USE_EMISSION_MAP | SRE_OBJECT_NO_PHYSICS |
        SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_OPEN_SIDE_HIDDEN_FROM_LIGHT);
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    scene->SetEmissionMap(texture);
    scene->SetDiffuseReflectionColor(Color(0.4, 0, 0));
    scene->SetSpecularReflectionColor(Color(0.2, 0.2, 0.2));
    scene->SetSpecularExponent(4.0);
#endif
    int j = scene->AddObject(fluid_object, Point3D(- 0.5f * FLUID_WORLD_SIZE, - 0.5f * FLUID_WORLD_SIZE,
        3.0f), Vector3D(0, 0, 0), 1.0f);
    fluid_scene_object = scene->object[j];  

    // Add lightsource
    scene->SetAmbientColor(Color(0.1f, 0.1f, 0.1f));
    scene->SetFlags(SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_INFINITE_DISTANCE);
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    scene->AddObject(sphere_model, 50000.0, - 60000.0, 50000.0, 0, 0, 0, 1000.0);
    scene->AddDirectionalLight(0, Vector3D(- 0.1, - 1.0, - 0.3).Normalize(),
       Color(1.0f, 1.0f, 1.0f));
}

static double demo13_previous_time = 0.0d;
static double fluid_time = 0.0d;
static int counter = 0;

void Demo13Step(sreScene *scene, double demo_time) {
    double elapsed_time = demo_time - demo13_previous_time;
    demo13_previous_time = demo_time;
    fluid_time += elapsed_time;
    while (fluid_time >= (float)1 / 60.0f) {
        fluid_time -= (float)1 / 60.0f;
        // On average every 60th of a second, create a disturbance.
        if (rng->RandomFloat(1.0f) < disturbance_frequency)
            sreCreateModelFluidDisturbance(fluid_scene_object->model,
                rng->RandomInt(FLUID_GRID_SIZE - 1) + 1, rng->RandomInt(FLUID_GRID_SIZE - 1) + 1,
                disturbance_displacement_func());
        counter++;
        if (counter == WAVE_FREQUENCY) {
            CreateWave();
            counter = 0;
        }
        sreEvaluateModelFluid(fluid_scene_object->model);
    }
}

