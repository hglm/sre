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

// demo3.cpp -- Solar system animation demo.
// Currently non-functional because jpg texture loading has been disabled.

#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "MatrixClasses.h"
#include "render.h"

// Exported variables for demo3
double demo3_start_time;
double demo3_days_per_second;
double demo3_elapsed_time;
double demo3_time;
// Other variables for demo3
static Point3D earth_position;
static sreModel *globe_model_oblate_jupiter;
static sreModel *globe_model_oblate_saturn;
static sreModel *rings_object;
static sreModel *uranus_rings_object;
static sreTexture *earth_texture;
static sreTexture *earth_bumpmap;
static sreTexture *earth_atmosphere;
static sreTexture *earth_specularity;
static sreTexture *mars_texture;
static sreTexture *mars_bumpmap;
static sreTexture *saturn_texture;
static sreTexture *rings_texture;
static sreTexture *rings_transparency;
static sreTexture *io_texture;
static sreTexture *moon_texture;
static sreTexture *moon_bumpmap;
static sreTexture *jupiter_texture;
static sreTexture *uranus_texture;
static sreTexture *uranus_rings_texture;
static sreTexture *uranus_rings_transparency;
static sreTexture *neptune_texture;
static sreTexture *mercury_texture;
static sreTexture *venus_texture;
static sreTexture *sun_texture;
static sreTexture *stars_texture;
static int sun_id, mercury_id, venus_id, earth_id, earth_atmosphere_id, moon_id, mars_id, jupiter_id,
    saturn_id, saturn_rings_id, uranus_id, uranus_rings_id, neptune_id;

#define SOLAR_SYSTEM_COMPRESSION 300
#define EARTH_ORBIT_RADIUS (149598261 / (6371.0 * SOLAR_SYSTEM_COMPRESSION))
#define SUN_COMPRESSION 10
#define MOON_ORBIT_SCALING 30
#define DEFAULT_DAYS_PER_SECOND 1.0

void Demo3CreateScene() {
    sreModel *globe_model = CreateSphereObject(0);
    globe_model_oblate_jupiter = CreateSphereObject(0.06487);
    globe_model_oblate_saturn = CreateSphereObject(0.09796);
    rings_object = CreateRingsObject(11.695, 22.036);
    uranus_rings_object = CreateRingsObject(6.57, 8.03); 
//            earth_texture = new sreTexture("earthmap1k.jpg");
//            earth_bumpmap = new sreTexture("earthbump1k.jpg");
//            earth_texture = new sreTexture("world.200404.3x5400x2700.png");
//            earth_texture = new sreTexture("EarthMap_2500x1250.jpg");
//            earth_bumpmap = new sreTexture("EarthElevation_2500x1250.jpg");
//            earth_atmosphere = new sreTexture("EarthClouds_2500x1250.jpg");
    earth_texture = new sreTexture("4_no_ice_clouds_mts_8k.jpg", TEXTURE_TYPE_NORMAL);
    earth_bumpmap = new sreTexture("EarthNormal.png", TEXTURE_TYPE_NORMAL);
    earth_atmosphere = new sreTexture("fair_clouds_8k.jpg", TEXTURE_TYPE_TRANSPARENT_EXTEND_TO_ALPHA);
    earth_specularity = new sreTexture("water_8k.png", TEXTURE_TYPE_NORMAL);
    mars_texture = new sreTexture("Mars.png", TEXTURE_TYPE_NORMAL);
    mars_bumpmap = new sreTexture("MarsNormal.png", TEXTURE_TYPE_NORMAL_MAP);
    saturn_texture = new sreTexture("saturnmap.jpg", TEXTURE_TYPE_NORMAL);
    rings_texture = new sreTexture("saturn_backscattered.png", TEXTURE_TYPE_WILL_MERGE_LATER);
    rings_transparency = new sreTexture("saturn_transparency.png", TEXTURE_TYPE_WILL_MERGE_LATER);
    rings_texture->MergeTransparencyMap(rings_transparency);
    moon_texture = new sreTexture("Moon.png", TEXTURE_TYPE_NORMAL);
    moon_bumpmap = new sreTexture("MoonNormal.png", TEXTURE_TYPE_NORMAL_MAP);
    jupiter_texture = new sreTexture("jupitermap.jpg", TEXTURE_TYPE_NORMAL);
    uranus_texture = new sreTexture("uranusmap.jpg", TEXTURE_TYPE_NORMAL);
    uranus_rings_texture = new sreTexture("uranusringcolour.jpg", TEXTURE_TYPE_WILL_MERGE_LATER);
    uranus_rings_transparency = new sreTexture("uranusringtrans.gif", TEXTURE_TYPE_WILL_MERGE_LATER);
    uranus_rings_texture->MergeTransparencyMap(uranus_rings_transparency);
    neptune_texture = new sreTexture("neptunemap.jpg", TEXTURE_TYPE_NORMAL);
    mercury_texture = new sreTexture("mercury.jpg", TEXTURE_TYPE_NORMAL);
    venus_texture = new sreTexture("venus.jpg", TEXTURE_TYPE_NORMAL);
    sun_texture = new sreTexture("sunmap.jpg", TEXTURE_TYPE_NORMAL);
    stars_texture = new sreTexture("yale8.png", TEXTURE_TYPE_NORMAL);

    // Add static objects.
    scene->SetTexture(stars_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | OBJECT_FLAG_IS_LIGHTSOURCE | SRE_OBJECT_NO_BACKFACE_CULLING |
        SRE_OBJECT_INFINITE_DISTANCE);
    scene->AddObject(globe_model, 0, 0, 0, 0, 27 * M_PI / 180, 0, FAR_CLIPPING_PLANE * 90);
    scene->SetTexture(sun_texture);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | OBJECT_FLAG_IS_LIGHTSOURCE);
    sun_id = scene->AddObject(globe_model, 0, 0, 0, 0, 0, demo3_time * 2 * M_PI / 25.05, 109 / SUN_COMPRESSION);
    // Add moving objects.
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetTexture(mercury_texture);
    scene->SetMaterial(0.8, 0.2);
    Point3D mercury_position;
    mercury_position.Set(EARTH_ORBIT_RADIUS * 0.387098 * cos(demo3_time * 2 * M_PI / 87.9691),
        EARTH_ORBIT_RADIUS * 0.387098 * sin(demo3_time * 2 * M_PI / 87.9691), 0);
    mercury_id = scene->AddObject(globe_model, mercury_position.x, mercury_position.y, mercury_position.z,
        0, 0, demo3_time * 2 * M_PI / 58.646 , 0.382);
    scene->SetTexture(venus_texture);
    scene->SetMaterial(0.8, 0.1);
    Point3D venus_position;
    venus_position.Set(EARTH_ORBIT_RADIUS * 0.723332 * cos(demo3_time * 2 * M_PI / 224.70069),
        EARTH_ORBIT_RADIUS * 0.723332 * sin(demo3_time * 2 * M_PI / 224.70069), 0);
    venus_id = scene->AddObject(globe_model, venus_position.x, venus_position.y, venus_position.z,
        177.4 * M_PI, 0, demo3_time * 2 * M_PI / 243.018, 0.949);
    earth_position.Set(EARTH_ORBIT_RADIUS * cos(demo3_time * 2 * M_PI / 365.256363),
         EARTH_ORBIT_RADIUS * sin(demo3_time * 2 * M_PI / 365.256363), 0);
    scene->SetTexture(earth_texture);
    scene->SetNormalMap(earth_bumpmap);
    scene->SetSpecularityMap(earth_specularity);
    scene->SetMaterial(0.8, 0.8);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP |
       SRE_OBJECT_USE_SPECULARITY_MAP | SRE_OBJECT_DYNAMIC_POSITION);
    earth_id = scene->AddObject(globe_model, earth_position.x, earth_position.y, earth_position.z,
        23.44 * M_PI / 180 , 0, demo3_time * 2 * M_PI, 1);
    scene->SetMaterial(0.8, 0.2);
    scene->SetTexture(moon_texture);
    scene->SetNormalMap(moon_bumpmap);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP | SRE_OBJECT_DYNAMIC_POSITION);
    Point3D moon_position;
    moon_position.Set(EARTH_ORBIT_RADIUS * 0.00257 * MOON_ORBIT_SCALING * cos(demo3_time * 2 * M_PI / 27.321582),
        EARTH_ORBIT_RADIUS * 0.00257 * MOON_ORBIT_SCALING * sin(demo3_time * 2 * M_PI / 27.321582), 0);
    moon_position += earth_position;
    moon_id = scene->AddObject(globe_model, moon_position.x, moon_position.y, moon_position.z,
        1.54 * M_PI / 180, 0, demo3_time * 2 * M_PI / 27.321582, 0.273);
    scene->SetTexture(mars_texture);
    scene->SetNormalMap(mars_bumpmap);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_NORMAL_MAP | SRE_OBJECT_DYNAMIC_POSITION);
    Point3D mars_position;
    mars_position.Set(EARTH_ORBIT_RADIUS * 1.523679 * cos(demo3_time * 2 * M_PI / 686.971),
        EARTH_ORBIT_RADIUS * 1.523679 * sin(demo3_time * 2 * M_PI / 686.971), 0);
    mars_id = scene->AddObject(globe_model, mars_position.x, mars_position.y, mars_position.z,
        25.19 * M_PI / 180, 0, demo3_time * 2 * M_PI / 1.026, 0.532);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetTexture(jupiter_texture);
    scene->SetMaterial(0.8, 0.1);
    Point3D jupiter_position;
    jupiter_position.Set(EARTH_ORBIT_RADIUS * 5.204267 * cos(demo3_time * 2 * M_PI / 4332.59),
        EARTH_ORBIT_RADIUS * 5.204267 * sin(demo3_time * 2 * M_PI / 4332.59), 0);
    jupiter_id = scene->AddObject(globe_model_oblate_jupiter, jupiter_position.x, jupiter_position.y,
        jupiter_position.z, 3.13 * M_PI / 180, 0, demo3_time * 2 * M_PI * 24.0 / 9.925, 11.209);
//    globe_model->ChangeTexture(io_texture);
//    globe_model->ChangeMaterialLightParameters(0.8, 0.5);
//    scene->AddObject(globe_model, 20, 25, 4, 0, 0, demo3_angle * M_PI / 180, 0.286);
    scene->SetTexture(saturn_texture);
    scene->SetMaterial(0.8, 0.1);
    Point3D saturn_position;
    saturn_position.Set(EARTH_ORBIT_RADIUS * 9.58201720 * cos(demo3_time * 2 * M_PI / 10759.22),
        EARTH_ORBIT_RADIUS * 9.58201720 * sin(demo3_time * 2 * M_PI / 10759.22), 0);
    saturn_id = scene->AddObject(globe_model_oblate_saturn, saturn_position.x, saturn_position.y,
        saturn_position.z, 26.73 * M_PI / 180, 0, demo3_time * 2 * M_PI * 24.0 / 10.57, 9.449);
    scene->SetTexture(uranus_texture);
    scene->SetMaterial(0.8, 0.1);
    Point3D uranus_position;
    uranus_position.Set(EARTH_ORBIT_RADIUS * 19.22944195 * cos(demo3_time * 2 * M_PI / 30799.095),
        EARTH_ORBIT_RADIUS * 19.22844195 * sin(demo3_time * 2 * M_PI / 30799.095), 0);
    uranus_id = scene->AddObject(globe_model, uranus_position.x, uranus_position.y, uranus_position.z,
        97.77 * M_PI / 180, 0, demo3_time * 2 * M_PI / 0.71833 , 4.007);
    scene->SetTexture(neptune_texture);
    Point3D neptune_position;
    neptune_position.Set(EARTH_ORBIT_RADIUS * 30.10366151 * cos(demo3_time * 2 * M_PI / 60190),
        EARTH_ORBIT_RADIUS * 30.10366151 * sin(demo3_time * 2 * M_PI / 60190), 0);
    neptune_id = scene->AddObject(globe_model, neptune_position.x, neptune_position.y, neptune_position.z,
        28.32 * M_PI / 180, 0, demo3_time * 2 * M_PI / 0.6713 , 3.883);
    scene->SetTexture(earth_atmosphere);
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_TRANSPARENT_TEXTURE | SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetMaterial(0.8, 0.2);
    earth_atmosphere_id = scene->AddObject(globe_model, earth_position.x, earth_position.y, earth_position.z,
        23.4 * M_PI / 180, 0, demo3_time * 2 * M_PI, 1.01);
    scene->SetTexture(rings_texture);
    scene->SetFlags(SRE_OBJECT_NO_BACKFACE_CULLING | SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_TRANSPARENT_TEXTURE |
       SRE_OBJECT_DYNAMIC_POSITION);
    scene->SetMaterial(0.8, 0.5);
    saturn_rings_id = scene->AddObject(rings_object, saturn_position.x, saturn_position.y, saturn_position.z,
        26.73 * M_PI / 180, 0, 0, 1.0);
    scene->SetTexture(uranus_rings_texture);
    scene->SetMaterial(0.8, 0.5);
    uranus_rings_id = scene->AddObject(uranus_rings_object, uranus_position.x, uranus_position.y,
        uranus_position.z, 97.77 * M_PI / 180, 0, 0, 1.0);
//    for (int i = 1; i < 10; i++)
//        scene->sceneobject[i]->render_shadows = true;
     scene->AddLight(SRE_LIGHT_POINT_SOURCE, 0, 0, 0, 0.8, 1.0, 1, 0, 0, 1.0, 1.0, 1.0);
     viewpoint.Set(0, - EARTH_ORBIT_RADIUS * 1.2, 0);
     demo3_days_per_second = DEFAULT_DAYS_PER_SECOND;
}

void Demo3Render() {
    // Set the positions of the moving celestial bodies.
    Point3D mercury_position;
    mercury_position.Set(EARTH_ORBIT_RADIUS * 0.387098 * cos(demo3_time * 2 * M_PI / 87.9691),
        EARTH_ORBIT_RADIUS * 0.387098 * sin(demo3_time * 2 * M_PI / 87.9691), 0);
    scene->ChangePositionAndRotation(mercury_id, mercury_position.x, mercury_position.y, mercury_position.z,
        0, 0, demo3_time * 2 * M_PI / 58.646);
    Point3D venus_position;
    venus_position.Set(EARTH_ORBIT_RADIUS * 0.723332 * cos(demo3_time * 2 * M_PI / 224.70069),
        EARTH_ORBIT_RADIUS * 0.723332 * sin(demo3_time * 2 * M_PI / 224.70069), 0);
    scene->ChangePositionAndRotation(venus_id, venus_position.x, venus_position.y, venus_position.z,
        177.4 * M_PI, 0, demo3_time * 2 * M_PI / 243.018);
    earth_position.Set(EARTH_ORBIT_RADIUS * cos(demo3_time * 2 * M_PI / 365.256363),
         EARTH_ORBIT_RADIUS * sin(demo3_time * 2 * M_PI / 365.256363), 0);
    scene->ChangePositionAndRotation(earth_id, earth_position.x, earth_position.y, earth_position.z,
        23.44 * M_PI / 180 , 0, demo3_time * 2 * M_PI);
    Point3D moon_position;
    moon_position.Set(EARTH_ORBIT_RADIUS * 0.00257 * MOON_ORBIT_SCALING * cos(demo3_time * 2 * M_PI / 27.321582),
        EARTH_ORBIT_RADIUS * 0.00257 * MOON_ORBIT_SCALING * sin(demo3_time * 2 * M_PI / 27.321582), 0);
    moon_position += earth_position;
    scene->ChangePositionAndRotation(moon_id, moon_position.x, moon_position.y, moon_position.z,
        1.54 * M_PI / 180, 0, demo3_time * 2 * M_PI / 27.321582);
    Point3D mars_position;
    mars_position.Set(EARTH_ORBIT_RADIUS * 1.523679 * cos(demo3_time * 2 * M_PI / 686.971),
        EARTH_ORBIT_RADIUS * 1.523679 * sin(demo3_time * 2 * M_PI / 686.971), 0);
    scene->ChangePositionAndRotation(mars_id, mars_position.x, mars_position.y, mars_position.z,
        25.19 * M_PI / 180, 0, demo3_time * 2 * M_PI / 1.026);
    Point3D jupiter_position;
    jupiter_position.Set(EARTH_ORBIT_RADIUS * 5.204267 * cos(demo3_time * 2 * M_PI / 4332.59),
        EARTH_ORBIT_RADIUS * 5.204267 * sin(demo3_time * 2 * M_PI / 4332.59), 0);
    scene->ChangePositionAndRotation(jupiter_id, jupiter_position.x, jupiter_position.y,
        jupiter_position.z, 3.13 * M_PI / 180, 0, demo3_time * 2 * M_PI * 24.0 / 9.925);
    Point3D saturn_position;
    saturn_position.Set(EARTH_ORBIT_RADIUS * 9.58201720 * cos(demo3_time * 2 * M_PI / 10759.22),
        EARTH_ORBIT_RADIUS * 9.58201720 * sin(demo3_time * 2 * M_PI / 10759.22), 0);
    scene->ChangePositionAndRotation(saturn_id, saturn_position.x, saturn_position.y,
        saturn_position.z, 26.73 * M_PI / 180, 0, demo3_time * 2 * M_PI * 24.0 / 10.57);
    Point3D uranus_position;
    uranus_position.Set(EARTH_ORBIT_RADIUS * 19.22944195 * cos(demo3_time * 2 * M_PI / 30799.095),
        EARTH_ORBIT_RADIUS * 19.22844195 * sin(demo3_time * 2 * M_PI / 30799.095), 0);
    scene->ChangePositionAndRotation(uranus_id, uranus_position.x, uranus_position.y, uranus_position.z,
        97.77 * M_PI / 180, 0, demo3_time * 2 * M_PI / 0.71833);
    Point3D neptune_position;
    neptune_position.Set(EARTH_ORBIT_RADIUS * 30.10366151 * cos(demo3_time * 2 * M_PI / 60190),
        EARTH_ORBIT_RADIUS * 30.10366151 * sin(demo3_time * 2 * M_PI / 60190), 0);
    scene->ChangePositionAndRotation(neptune_id, neptune_position.x, neptune_position.y, neptune_position.z,
        28.32 * M_PI / 180, 0, demo3_time * 2 * M_PI / 0.6713);
    scene->ChangePositionAndRotation(earth_atmosphere_id, earth_position.x, earth_position.y, earth_position.z,
        23.4 * M_PI / 180, 0, demo3_time * 2 * M_PI);
    scene->ChangePositionAndRotation(saturn_rings_id, saturn_position.x, saturn_position.y, saturn_position.z,
        26.73 * M_PI / 180, 0, 0);
    scene->ChangePositionAndRotation(uranus_rings_id, uranus_position.x, uranus_position.y,
        uranus_position.z, 97.77 * M_PI / 180, 0, 0);
    scene->Render();
}

void Demo3TimeIteration(double time_previous, double time_current) {
    if (recording_movie) {
        double elapsed_time = (double)1 / 30;
        demo3_time += elapsed_time * demo3_days_per_second;
    }
    else {
        double current_time = GetCurrentTime();
        demo3_time = demo3_elapsed_time + (current_time - demo3_start_time) * demo3_days_per_second;
    }
}

