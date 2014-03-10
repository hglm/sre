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

// demo4b.cpp -- Earth relief mesh demo (rotating earth).

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "sre.h"
#include "demo.h"

// Scale (half radius) of the earth (must match demo4.cpp).
#define X_SCALE 10000.0f
#define EARTH_RADIUS (X_SCALE * 0.5f)
// Default viewing distance from the surface for rotating earth in earth radii.
#define EARTH_VIEW_DISTANCE 2.6f
// Angle of the viewpoint relative to the sun in the direction of the equator.
#define SUN_VIEWPOINT_ANGLE 10.0f
// Number seconds for a complete rotation (day).
#define DAY_INTERVAL 20.0f
// Number of seconds of each rotation segment.
#define ROTATION_SEGMENT_TIME 30.0f
// Number of seconds for each flyover segment.
#define FLYOVER_TIME 30.0f

// Speed-up factor (for testing).
#define SPEEDUP 0.5
// Starting segment. Segments 0 - 13 are rotating earth, 14 - 20 are flyovers.
#define STARTING_SEGMENT (7 * 2)
// #define STARTING_SEGMENT 0
#define ROTATION_SEGMENT_ZOOM_LEVELS 2

// #define SKIP_FLYOVERS

// #define DEBUG

#define USE_CIRCLE_ROUTES

// Number of seconds to pause with a black screen before each circle route segment.
#define BREAK_TIME 1.0

static float view_distance;
static float view_distance_start;
static float view_distance_target;
static double view_distance_start_time;
static double view_distance_target_time;
static float view_angle;
static float view_angle_start;
static float view_angle_target;
static double view_angle_start_time = DBL_MAX;
static double view_angle_target_time = DBL_MAX;
// Because the spacecraft and player "sphere" are disabled, the sun
// is object 0.
static int sun_object_id = 0;
static int directional_light_index = 0;

// Latitudes viewed during rotating earth segments.

static float latitude[7] = {
    0, 23.438f, -23.438f, -46.0f, 46.0f, 90.0f, - 90.0f
};

// Routes followed during fly-overs.

class FlyoverRoute {
public :
    float starting_longitude;
    float ending_longitude;
    float latitude;
    float height;
    float sun_longitude;
    float sun_latitude;
};

#define NU_FLYOVERS 7

static FlyoverRoute route[NU_FLYOVERS] = {
    // Follow equator over Africa.
    { 0.0f, 50.0f, 0.0f, 200.0f, 50.0f, 0.0f },
    // Follow equator over Indonesia.
    { 85.0f, 150.0f, 0.0f, 200.0f, 120.0f, 0.0f },
    // Follow equator over South America.
    { - 95.0f, - 40.0f, 0.0f, 200.0f, - 65.0f, 0.0f },
    // Follow latitude 30 degrees from west of the Nile over mountainous Asia to the
    // Pacific Ocean. In two parts.
    { 10.0f, 80.0f, 30.0f, 200.0f, 85.0f, 0.0f },
    { 80.0f, 145.0f, 30.0f, 200.0f, 85.0f, 0.0f },
    // Europe/Alps/black sea.
    { -20.0f, 50.0f, 46.0f, 200.0f, 20.0f, 0.0f },
    // US west to east.
    { -135.0f, -65.0f, 40.0f, 200.0f, -100.0f, 0.0f }
};

class GreatCircleRouteSpec {
public :
    double long0, lat0;
    double long1, lat1;
    float height;
    float sun_longitude;
    float sun_latitude;
    float duration;
};

class GreatCircleRoute : public GreatCircleRouteSpec {
private :
    double long_diff;
    double azimuth[2];
    double central_angle;
    double nodal_azimuth;
    double nodal_angle[2];
    double nodal_longitude;

public :
    void CalculateGreatCircle();
    void CalculateIntermediatePoint(double t, double& longitude,
        double& latitude, double& azimuth);
};

#define NU_GREAT_CIRCLE_ROUTES 16

static GreatCircleRouteSpec circle_route_spec[NU_GREAT_CIRCLE_ROUTES] = {
    // Traverse the US from West to East.
    { - 135.0, 37.0, -60.0, 45.0, 200.0f, - 100.0f, 30.0f, 30.0f },
    // Follow equator over Africa.
    { - 5.0, 0.0, 50.0, 0.0, 200.0f, 50.0f, 0.0f, 30.0f },
    // South to North over Africa.
    { 15.0, - 50.0, 45.0, 25.0, 200.0f, 30.0f, 0.0f, 40.0f },
    // South to North over Europe.
    { 10.0, 32.0, 15.0, 80.0, 200.0f, 30.0f, 40.0f, 25.0f },
    // Traverse Eurasia at latitude around 50 degrees.
    { -20.0, 48.0, 90.0, 47.0, 200.0f, 25.0f, 40.0f, 30.0f },
    { 70.0, 50.0, 165.0, 53.0, 200.0f, 115.0f, 40.0f, 25.0f },
    // From Southeastern Europe over Central Asia to Western China.
    { 10.0, 40.0, 110.0, 25.0, 250.0f, 55.0f, 30.0f, 35.0f },
    // Follow latitude around 30 degrees from west of the Nile over mountainous Asia to the
    // Pacific Ocean.
    { 10.0, 25.0, 80.0, 29.0, 250.0f, 50.0f, 10.0f, 30.0f },
    { 72.0, 25.0, 145.0, 32.0, 250.0f, 110.0f, 10.0f, 30.0f },
    // Follow equator over South America.
    { - 95.0, 0.0, - 40.0, 0.0, 200.0f, - 65.0f, - 15.0f, 30.0f },
    // Follow the Andes from Mexico and end at the South Pole.
    { - 75.0, 20.0, -40.0, -89.0, 200.0f, - 70.0f, - 30.0f, 50.0f },
    // To the North Pole from Southern US.
    { - 90.0, 20.0, - 90.0, 90.0, 200.0f, -80.0f, 45.0f, 35.0f },
    // Rocky Mountains from Mexico to Alaska.
    { - 110.0, 27.0, - 154.0, 68.0, 200.0f, - 160.0f, 40.0f, 30.0f },
    // Traverse Indonesia/Australasia, crossing the equator slightly.
    { 85.0, 5.0, 150.0, - 10.0, 200.0f, 120.0f, 0.0f, 30.0f },
    // Traverse Oceania.
    { 100.0, - 15.0, 180.0, - 40.0, 200.0f, 135.0f, -30.0f, 35.0f },
};

static GreatCircleRoute circle_route[NU_GREAT_CIRCLE_ROUTES];

void GreatCircleRoute::CalculateGreatCircle() {
    long_diff = (long1 - long0) * M_PI / 180.0;
    azimuth[0] = atan2(sin(long_diff), cos(lat0 * M_PI / 180.0) *
        tan(lat1 * M_PI / 180.0) - sin(lat0 * M_PI / 180.0) * cos(long_diff));
    central_angle = acos(sin(lat0 * M_PI / 180.0) * sin(lat1 * M_PI / 180.0) +
        cos(lat0 * M_PI / 180.0) * cos(lat1 * M_PI / 180.0)
        * cos(long_diff));
    nodal_azimuth = atan2(sin(azimuth[0]) * cos(lat0 * M_PI / 180.0),
        sqrt(pow(cos(azimuth[0]), 2.0) + pow(sin(azimuth[0]), 2.0) *
        pow(sin(lat0 * M_PI / 180.0), 2.0)));
    if (lat0 == 0 && azimuth[0] == 0.5 * M_PI)
        nodal_angle[0] = 0;
    else
        nodal_angle[0] = atan2(tan(lat0 * M_PI / 180.0), cos(azimuth[0]));
    nodal_angle[1] = nodal_angle[0] + central_angle;
    double lon_A_P0 = atan2(sin(nodal_azimuth) * sin(nodal_angle[0]), cos(nodal_angle[0]));
    nodal_longitude = long0 * M_PI / 180.0 - lon_A_P0;
}

void GreatCircleRoute::CalculateIntermediatePoint(double t, double& longitude,
double& latitude, double& azimuth) {
    double ang = nodal_angle[0] + t * (nodal_angle[1] - nodal_angle[0]); 
    latitude = atan2(cos(nodal_azimuth) * sin(ang),
        sqrt(pow(cos(ang), 2.0) + pow(sin(nodal_azimuth), 2.0) *
        pow(sin(ang), 2.0)));
    longitude = atan2(sin(nodal_azimuth) * sin(ang), cos(ang)) + nodal_longitude;
    azimuth = atan2(tan(nodal_azimuth), cos(ang));
}

void Demo4bCreateScene() {
    Demo4SetParameters(DAY_INTERVAL, false, false, false, false, 1.3f);
    Demo4CreateScene();
    // Set the view and movement mode to a static one.
    Point3D viewpoint = Point3D(0, - EARTH_RADIUS - EARTH_RADIUS * EARTH_VIEW_DISTANCE, 0);
    view->SetViewModeLookAt(viewpoint, Point3D(0, 0, 0), Point3D(0, 0, 1.0f));
    view->SetMovementMode(SRE_MOVEMENT_MODE_NONE);
    // Set the view distance to precisely the edge of the globe.
    sreSetFarPlaneDistance(Magnitude(Point3D(- EARTH_RADIUS, 0, 0) - viewpoint));
    view_angle = latitude[0];
    view_angle_target = 99999.9f;
    view_distance = EARTH_RADIUS * EARTH_VIEW_DISTANCE;
    view_distance_target = - 1.0f;

    for (int i = 0; i < NU_GREAT_CIRCLE_ROUTES; i++) {
        *(GreatCircleRouteSpec *)&circle_route[i] = circle_route_spec[i];
        circle_route[i].CalculateGreatCircle();
    }
}


static void SetViewAngle(float target_angle, double start_time,
double target_time) {
    if (view_angle_target != target_angle) {
        view_angle_start = view_angle;
        view_angle_target = target_angle;
    }
    view_angle_start_time = start_time;
    view_angle_target_time = target_time;
}

static void SetViewDistanceTarget(float target) {
    if (view_distance_target != target) {
        view_distance_start = view_distance;
        view_distance_target = target;
    }
}

void Demo4bRender() {
    double time = demo_time * SPEEDUP;
    int i = floor(time / ROTATION_SEGMENT_TIME) + STARTING_SEGMENT;
#ifdef SKIP_FLYOVERS
    if (time >= 7 * ROTATION_SEGMENT_ZOOM_LEVELS * ROTATION_SEGMENT_TIME) {
#else
    if (time >= 7 * ROTATION_SEGMENT_ZOOM_LEVELS * ROTATION_SEGMENT_TIME + NU_FLYOVERS * FLYOVER_TIME) {
#endif
        demo_stop_signalled = true;
        return;
    }
    if (i >= 7 * ROTATION_SEGMENT_ZOOM_LEVELS) {
        // Follow lines of latitude from a table, or great circles.
        sreSetFarPlaneDistance(2000.0f);
        // Select the route index.
        int j;
        if (STARTING_SEGMENT <= 7 * ROTATION_SEGMENT_ZOOM_LEVELS) {
            double flyover_segment_time = time - (7 * ROTATION_SEGMENT_ZOOM_LEVELS -
                STARTING_SEGMENT) * ROTATION_SEGMENT_TIME;
#ifdef USE_CIRCLE_ROUTES
            // Determine the segment.
            float cumulative_time = 0;
            for (j = 0; j < NU_GREAT_CIRCLE_ROUTES; j++) {
                cumulative_time += BREAK_TIME;
                cumulative_time += circle_route[j].duration;
                if (flyover_segment_time < cumulative_time)
                    break;
            }
            if (j < NU_GREAT_CIRCLE_ROUTES) {
                time = flyover_segment_time - (cumulative_time - circle_route[j].duration)
                    - BREAK_TIME;
                if (time < 0) {
                    // During the break (initial seconds of the segment), show a black screen.
                    view->SetViewModeLookAt(Point3D(0, 0, EARTH_RADIUS * 2.0f),
                        Point3D(0, 0, EARTH_RADIUS * 3.0f), Vector3D(0, 1.0f, 0));
                    scene->Render(view);
                    return;
                }
            }
            if (j >= NU_GREAT_CIRCLE_ROUTES) {
#else
            j = floor(flyover_segment_time / FLYOVER_TIME);
            time = flyover_segment_time - j * FLYOVER_TIME;
            if (j >= NU_FLYOVERS) {
#endif
                demo_stop_signalled = true;
                return;
            }
        }
        else {
            j = STARTING_SEGMENT - 7 * ROTATION_SEGMENT_ZOOM_LEVELS +
                floor(time / FLYOVER_TIME);
            time -= (j - (STARTING_SEGMENT - 7 * ROTATION_SEGMENT_ZOOM_LEVELS)) *
                FLYOVER_TIME;
        }
#ifdef USE_CIRCLE_ROUTES
        double t = time / circle_route[j].duration;
        // Using great circle routes.
        float sun_longitude = circle_route[j].sun_longitude;
        float sun_latitude = circle_route[j].sun_latitude;
        double longitude, latitude, azimuth;
        circle_route[j].CalculateIntermediatePoint(t, longitude, latitude, azimuth);
        float height = circle_route[j].height;
#else
        double t = time / FLYOVER_TIME;
        // Using longitudinal routes.
        float sun_longitude = route[j].sun_longitude;
        float sun_latitude = route[j].sun_latitude;
        // Calculate the longitude for the current position on the path.
#ifdef DEBUG
        float longitude = route[j].starting_longitude;
#else
        float longitude = (route[j].starting_longitude +
            t * (route[j].ending_longitude - route[j].starting_longitude))
            * M_PI / 180.0f;
#endif
        float latitude = route[j].latitude * M_PI / 180.0;
        float height = route[j].height;
#endif

        // Fix the sun to the specified longitude position for the route,
        // shining in the direction of the equator or higher latitude.
        Matrix3D sun_m1;
        sun_m1.AssignRotationAlongZAxis(sun_longitude * M_PI / 180.0f);
        Matrix3D sun_m2;
        sun_m2.AssignRotationAlongXAxis(- sun_latitude * M_PI / 180.0f);
        Point3D sun_pos = (sun_m1 * sun_m2) * (Point3D(1.0f, 0.0f, 0.0f) *
            EARTH_RADIUS * 1000.0f);
        scene->ChangePosition(sun_object_id, sun_pos);
        Vector3D light_dir = (- sun_pos).Normalize();
        scene->ChangeDirectionalLightDirection(directional_light_index, light_dir);

        Matrix3D m1;
        m1.AssignRotationAlongZAxis(longitude);
        Matrix3D m2;
        m2.AssignRotationAlongYAxis(- latitude);
        Point3D viewpoint = m1 * (m2 * Point3D(1.0f, 0.0f, 0.0f));
        Vector3D up_vector = viewpoint;
        up_vector.Normalize();

        // Set the view direction
        Matrix3D m3;
#ifdef DEBUG
        // Look down to the surface by 60 degrees.
        m3.AssignRotationAlongZAxis(60.0f * M_PI / 180.0);
#else
        // Look down to the surface by 30 degrees.
        m3.AssignRotationAlongZAxis(30.0f * M_PI / 180.0);
#endif

#ifdef USE_CIRCLE_ROUTES
        // Calculate the view direction by moving a very small amount along the great
        // circle.
        circle_route[j].CalculateIntermediatePoint(t + 1.0 / (FLYOVER_TIME * 20.0),
            longitude, latitude, azimuth);
        m1.AssignRotationAlongZAxis(longitude);
        m2.AssignRotationAlongYAxis(- latitude);
        Point3D target = m1 * (m2 * Point3D(1.0f, 0.0f, 0.0f));
        Vector3D view_vector = (target - viewpoint).Normalize();
        Vector3D right_vector = Cross(view_vector, up_vector).Normalize();
        Matrix3D m4;
        m4.AssignRotationAlongAxis(right_vector, - 30.0f * M_PI / 180.0f);
        view_vector = m4 * view_vector;
#else
        // View is fixed eastward.
        Vector3D view_vector = m1 * (m3 * Vector3D(0.0f, 1.0f, 0));
#endif

#ifdef DEBUG
        viewpoint *= EARTH_RADIUS + 1000.0f;
#else
        viewpoint *= EARTH_RADIUS + height;
#endif

        Point3D look_at = viewpoint + view_vector;
#ifdef DEBUG
        char *s;
        s = viewpoint.GetString();
        printf("viewpoint = %s, ", s);
        delete [] s;
        s = look_at.GetString();f
        printf("lookat = %s, ", s);
        delete [] s;
        s = up_vector.GetString();
        printf("up_vector = %s\n", s);
        delete [] s;
#endif
        view->SetViewModeLookAt(viewpoint, look_at, up_vector);
        float factor = 1.0f;
        sreSetShadowMapRegion(Point3D(- 1000.0f, - 1000.0f, - 1000.0f) * factor,
            Point3D(1000.0f, 1000.0f, 200.0f) * factor);
        scene->Render(view);
        return;
    }
    int j = i / ROTATION_SEGMENT_ZOOM_LEVELS;
    double start_time = j * ROTATION_SEGMENT_TIME * ROTATION_SEGMENT_ZOOM_LEVELS;
    SetViewAngle(latitude[j] * M_PI / 180.0f, start_time, start_time +
        ROTATION_SEGMENT_TIME / 6.0);
    if (time >= view_angle_target_time)
        view_angle = view_angle_target;
    else {
        float t = (time - view_angle_start_time) /
            (view_angle_target_time - view_angle_start_time);
        view_angle = view_angle_start +
            t * (view_angle_target - view_angle_start);
    }
    // Matrix m1 defines the position relative to the sun of the viewpoint.
    Matrix3D m1;
    m1.AssignRotationAlongZAxis(SUN_VIEWPOINT_ANGLE * M_PI / 180.0f);
    Vector3D sun_pos = scene->sceneobject[sun_object_id]->position;
    // Project sun position to the equatorial plane.
    sun_pos -= ProjectOnto(sun_pos, Vector3D(0, 0, 1.0f));
    sun_pos.Normalize();
    // Matrix m2 is the latitude focused on.
    Vector3D axis = Cross(sun_pos, Vector3D(0, 0, 1.0f));
    Matrix3D m2;
    m2.AssignRotationAlongAxis(axis, view_angle);
    j = i % ROTATION_SEGMENT_ZOOM_LEVELS;
    start_time = floor(time / ROTATION_SEGMENT_TIME) * ROTATION_SEGMENT_TIME;
    if (j == 0) {
        SetViewDistanceTarget(EARTH_RADIUS * EARTH_VIEW_DISTANCE);
        view_distance_start_time = time;
        view_distance_target_time = time;
    }
    else if (j == 1) {
        // Close up (1.0).
        SetViewDistanceTarget(EARTH_RADIUS * 1.0f);
        view_distance_start_time = start_time;
        view_distance_target_time = start_time + ROTATION_SEGMENT_TIME / 6.0;
    }
    else {
        // Close up (0.3).
        SetViewDistanceTarget(EARTH_RADIUS * 0.3f);
        view_distance_start_time = start_time;
        view_distance_target_time = start_time + ROTATION_SEGMENT_TIME / 6.0;
    }
    if (time >= view_distance_target_time)
        view_distance = view_distance_target;
    else {
        float t = (time - view_distance_start_time) /
            (view_distance_target_time - view_distance_start_time);
        view_distance = view_distance_start +
            t * (view_distance_target - view_distance_start);
    }
    Point3D view_origin = m2 * (m1 * sun_pos);
    float view_dist = EARTH_RADIUS + view_distance;
    Point3D viewpoint = view_origin * view_dist;
    Vector3D up_vector = m1 * (m2 * Vector3D(0, 0, 1.0f));
    view->SetViewModeLookAt(viewpoint, Point3D(0, 0, 0), up_vector);
    sreSetShadowMapRegion(Point3D(- EARTH_RADIUS, view_dist - EARTH_RADIUS * 0.1f,
        - EARTH_RADIUS), Point3D(EARTH_RADIUS, view_dist + EARTH_RADIUS, EARTH_RADIUS));
    scene->Render(view);
}

