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

// SRE demos -- shared main program.


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "sre.h"
#include "demo.h"
#include "gui-common.h"

sreScene *scene;
sreView *view;
bool lock_panning = false;

// Command-line options.
bool benchmark_mode = false;
static bool multiple_lights = false;
bool multipass_rendering = false;
bool fullscreen_mode = false;
int shadows = SRE_SHADOWS_NONE;
static bool preprocess = false;
static int debug_level = 0;
static bool demand_load_shaders = false;

void (*RenderFunc)();
void (*TimeIterationFunc)(double, double);
bool recording_movie = false;

// Whether to allow jumping.
bool jump_allowed = true;
// Player control variables, modified by the platform specific GUI interfaces in glfw.cpp or opengles2.cpp.
int control_object = 0;
bool jump_requested = false;
float input_acceleration = 0;
float horizontal_acceleration = 100.0;
float max_horizontal_velocity = 100.0;
bool dynamic_gravity = false;
Point3D gravity_position = Point3D(0, 0, 0);
bool no_gravity = false;
float hovering_height;
float hovering_height_acceleration = 100.0;
bool no_ground_plane = false;

// RunDemo() variables.
double demo_time;
bool demo_stop_signalled = false;
double demo_start_time;

// Text overlay variables.
double text_message_time = 0;
double text_message_timeout = 3.0;
int nu_text_message_lines = 2;
char *text_message[24] = { "", "" };

static const char *shadow_str[3] = {
    "No shadows", "Stencil shadow volumes", "Shadow mapping" };

static void PrintConfigurationInfo() {
        printf("Back-end: %s\n", GUIGetBackendName());
        printf("Default shadow settings: %s\n"
            "Default number of lights: %s\n"
            "Rendering method: %s\n", shadow_str[shadows],
            multiple_lights == true ? "Unlimited" : "Single",
            multipass_rendering == true ? "Multi-pass" : "Single-pass");
    if (debug_level > 0)
        printf("SRE library debug message level = %d.\n", debug_level);
    if (benchmark_mode)
        printf("Benchmark mode enabled.\n");
}

#ifndef USE_BULLET

// When Bullet is disabled, use a very simple physics model with only horizontal movement
// of the player and no collision detection.

Vector3D player_velocity(0, 0, 0);

void DoGenericPhysics(double time_previous, double time_current) {
    float dtime = time_current - time_previous;
    // Move the player.
    if (input_velocity != Vector3D(0, 0, 0)) {
        player_velocity += input_velocity;
        input_velocity.Set(0, 0, 0);
    }
    if (player_velocity != Vector3D(0, 0, 0)) {
        scene->ChangePosition(0, scene->sceneobject[0]->position.x + player_velocity.x * dtime,
            scene->sceneobject[0]->position.y + player_velocity.y * dtime,
            scene->sceneobject[0]->position.z + player_velocity.z * dtime);
        // Slow down the horizontal velocity.
        Vector2D v;
        v.x = player_velocity.x;
        v.y = player_velocity.y;
        Vector2D n;
        n = v;
        if (v == Vector2D(0, 0))
            n = Vector2D(0, 0);
        else
            n.Normalize();
        float new_mag = Magnitude(v) - Magnitude(v) * dtime * 1.0;
        if (new_mag < 0)
            new_mag = 0;
        v = n * new_mag;
        player_velocity.x = v.x;
        player_velocity.y = v.y;
    }
}

#endif

static double fps_table[10];
static int nu_fps = 0;
static double current_averaged_fps = 0;

void RunDemo() {
    scene->PrepareForRendering(preprocess);
#ifdef USE_BULLET
    BulletInitialize();
#endif
    PrintConfigurationInfo();
    printf("Starting rendering.\n");
    double time_physics_previous = GUIGetCurrentTime();
    double time_physics_current = time_physics_previous;
    double end_time = GUIGetCurrentTime();
    demo_start_time = end_time;
    double previous_time;
    int nu_frames = 0;
    for (;;) {
        if (demo_stop_signalled) {
            demo_stop_signalled = false;
            break;
        }
        RenderFunc();

        time_physics_current = GUIGetCurrentTime();
#ifdef USE_BULLET
        scene->DoBulletPhysics(time_physics_previous, time_physics_current);
#else
        DoGenericPhysics(time_physics_previous, time_physics_current);
#endif
        TimeIterationFunc(time_physics_previous, time_physics_current);
        time_physics_previous = time_physics_current;
        nu_frames++;
        previous_time = end_time;
        end_time = GUIGetCurrentTime();
        demo_time = end_time - demo_start_time;
        GUIProcessEvents(end_time - previous_time);
        double current_fps = 1 / (end_time - previous_time);
        if (nu_fps < 10) {
            fps_table[nu_fps] = current_fps;
            nu_fps++;
            continue;
        }
        for (int i = 0; i < 9; i++)
            fps_table[i] = fps_table[i + 1];
        fps_table[9] = current_fps;
        nu_fps++;
        if (nu_fps % 50 == 0) {
            double total = 0;
            for (int i = 0; i < 10; i++)
                total += fps_table[i];
            current_averaged_fps = total / 10;
//            printf("current fps: %.2lf\n", current_averaged_fps);
        }
        if (benchmark_mode && end_time - demo_start_time > 20.0)
            break;
    }
}

void DemoTextOverlay() {
    char s[32];
    sprintf(s, "FPS: %.2lf", current_averaged_fps);
#if 0
    // Set standard parameters (blending).
    sreSetTextParameters(0, 0, Color(1.0, 1.0, 1.0), 0);
    sreBeginText();
    sreDrawText(s, 0.01, 0, 0.02, 0.04);
    if (GUIGetCurrentTime() >= text_message_time + text_message_timeout)
        GUITextMessageTimeoutCallback();
    for (int i = 0; i < nu_text_message_lines; i++)
        if (text_message[i][0] != '\0')
            sreDrawText(text_message[i], 0.01, 0.04 + 0.04 * i, 0.012, 0.04);
    sreEndText();
#else
    // Set standard parameters (blending).
    sreSetImageBlendingMode(SRE_IMAGE_BLEND_ADDITIVE);
    // Force rebinding of the current font.
    sreSetFont(NULL);
    Vector2D font_size = Vector2D(0.02, 0.04);
    sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size);
    sreDrawText(s, 0.01, 0);
    if (GUIGetCurrentTime() >= text_message_time + text_message_timeout)
        GUITextMessageTimeoutCallback();
    font_size = Vector2D(0.012, 0.04);
    sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size);
    for (int i = 0; i < nu_text_message_lines; i++)
        if (text_message[i][0] != '\0')
            sreDrawText(text_message[i], 0.01, 0.04 + 0.04 * i);
#endif
}

int main(int argc, char **argv) {
    srand(1);

#ifdef SHADOW_VOLUMES
    shadows = SRE_SHADOWS_SHADOW_VOLUMES;
#else
#ifdef SHADOW_MAPPING
    shadows = SRE_SHADOWS_SHADOW_MAPPING;
#else
#if defined(OPENGL) && !defined(NO_SHADOWS)
    shadows = SRE_SHADOWS_SHADOW_VOLUMES;
#endif
#endif
#endif

#ifdef MULTIPLE_LIGHTS_ENABLED
    multiple_lights = true;
    multipass_rendering = true;
#else
#ifdef MULTIPLE_LIGHTS_DISABLED
    multiple_lights = false;
#else
#ifdef OPENGL
    multiple_lights = true;
    multipass_rendering = true;
#endif
#endif
#endif

    int argi = 1;
    if (argc == 1) {
        const char *text =
            "Option --shadow-volumes enables stencil shadows at start-up.\n"
#ifdef OPENGL
            "Option --shadow-mapping enables shadow mapping at start-up.\n"
#endif
            "Option --no-shadows disables shadows at start-up.\n"
            "Option --multiple-lights enables multiple lights in the scene. Implies\n"
            "--multi-pass.\n"
            "Option --single-light limits the scene to one light.\n"
            "Option --multi-pass enables multi-pass rendering (for multiple lights).\n"
            "Option --single-pass disables multi-pass rendering.\n"
            "Option --benchmark makes the application quit automatically after 20s of\n"
            "rendering, displaying the number of frames per second.\n"
            "Option --preprocess performs T-junction elimination on all static scenery at start-up.\n"
            "Option --demand-load-shaders enables demand-loading of shaders (experimental).\n"
#ifdef OPENGL_GLFW
            "Option --full-screen enables full-screen mode (GLFW only). Not recommended, \n"
            "changes video mode and is not perfect. Better to maximize the window and use\n"
            "mouse panning (press F).\n"
#endif
            "Options --debug1, --debug2 and --debug3 set the SRE libary debug message level to\n"
            "1, 2 or 3 (default 0).\n"
            "demo1 shows a scene with textured, bump-mapped blocks and fluid animation\n"
            "(on OpenGL3). A large number of colored spotlights are present with OpenGL.\n"
            "demo2 shows a large scene with numerous point lights scattered across the landscape.\n"
            "demo4 is an advanced Earth terrain demo. Only works with OpenGL\n"
            "and requires large texture data files.\n"
            "demo5 shows a torus landscape with some ramps and a ball to push and multiple lights.\n"
            "demo6 is a circumnavigating view of the demo5 scene.\n"
            "demo7 shows a simple scene with a grating to test stencil shadows.\n"
            "demo8 has a large scene with numerous point lights and moving spheres in a central field.\n"
            "demo10 is a simple scene optimized for OpenGL ES2.0 with geometric objects\n"
            "that can be moved.\n"
            "texturememorytest reports the number of textures that can be loaded until video memory is exhausted.\n"
            "texturememorytestcompressed reports the number of compressed textures that can be loaded.\n"
            "texturetest is a performance test for uncompressed textures.\n"
            "texturetestcompressed is a performance test for compressed textures.\n"
            "game is a simple game where a ball has to be pushed to the ground. This demo\n"
            "hasn't been updated for a while and may result in errors on some platforms.\n"
            "\n"
#if defined(OPENGL) || defined(OPENGL_ES2_X11)
            "Keyboard < is pan left and > is pan right, H is pan up and N is pan down.\n"
            "Keyboard / is jump.\n"
#endif
#if defined(OPENGL_ES2) && !defined(OPENGL_ES2_X11)
            "Press Ctrl-C to quit. Mouse panning is enabled by default, the left mouse button is accelerate, "
            "the right mouse button is reverse and the middle mouse button is jump.\n";
#else
            "Press keypad + and - to zoom in/out.\n"
            "Press A to accelerate, Z to decelerate, left mouse button is jump.\n"
            "Press M to toggle mouse panning.\n"
            "Press Q to quit.\n"
            "F1 brings up a menu with advanced rendering options.\n"
#endif
#if defined(X11) || defined(OPENGL_FREEGLUT)
            "Press F to toggle full-screen mode.\n"
#endif
            ;
#ifdef OPENGL_ES2
        printf("OpenGL-ES 2.0 demo ");
#endif
#ifdef OPENGL
        printf("OpenGL 3.0+ demo ");
#endif
        printf("using %s backend.\nUsage: render <options> demoname\n%s\n", GUIGetBackendName(), text);

        PrintConfigurationInfo();
        exit(0);
    }

    for (;;) {
        if (argc >= argi + 1 && strcmp(argv[argi], "--benchmark") == 0) {
            benchmark_mode = true;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--multiple-lights") == 0) {
            multiple_lights = true;
            multipass_rendering = true;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--single-light") == 0) {
            multiple_lights = false;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--no-shadows") == 0) {
            shadows = SRE_SHADOWS_NONE;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--shadow-volumes") == 0) {
            shadows = SRE_SHADOWS_SHADOW_VOLUMES;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--shadow-mapping") == 0) {
            shadows = SRE_SHADOWS_SHADOW_MAPPING;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--multi-pass") == 0) {
            multipass_rendering = true;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--single-pass") == 0) {
            multipass_rendering = false;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--full-screen") == 0) {
            fullscreen_mode = true;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--preprocess") == 0) {
            preprocess = true;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--debug1") == 0) {
            debug_level = 1;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--debug2") == 0) {
            debug_level = 2;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--debug3") == 0) {
            debug_level = 3;
            argi++;
            continue;
        }
        if (argc >= argi + 1 && strcmp(argv[argi], "--demand-load-shaders") == 0) {
            demand_load_shaders = true;
            argi++;
            continue;
        }
        break;
    }

    // Some SRE parameters need to be set before the initialization function is called.
    sreSetDebugMessageLevel(debug_level);
    if (demand_load_shaders)
        sreSetDemandLoadShaders(true);

    // Initialize GUI and SRE library.
    GUIInitialize(&argc, &argv);

    sreSetDrawTextOverlayFunc(DemoTextOverlay);
    sreSetShadowsMethod(shadows);
    if (multipass_rendering)
        sreEnableMultiPassRendering();
    else
        sreDisableMultiPassRendering();
    sreSetLightScissors(SRE_SCISSORS_GEOMETRY);
    if (multipass_rendering && !multiple_lights)
        sreSetMultiPassMaxActiveLights(1);

    // Create a scene with initial default maximums of 1024 objects, 256 models and 128 lights.
    // Dynamic reallocation in libsre should ensure that actual numbers are practically
    // unlimited (except for main memory and GPU memory restrictions).
    scene = new sreScene(1024, 256, 128);

    // Create a view. Must be called after initialization.
    view = new sreView;

    if (argc >= argi + 1) {

        if (strcmp(argv[argi], "demo1") == 0) {
            Demo1CreateScene();
            view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
            RenderFunc = Demo1Render;
            TimeIterationFunc = Demo1TimeIteration;
            RunDemo();
        }
        else if (strcmp(argv[argi], "demo2") == 0) {
            Demo2CreateScene();
            view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
            RenderFunc = Demo2Render;
            TimeIterationFunc = Demo2TimeIteration;
            RunDemo();
        }
#if 0
        else if (strcmp(argv[argi], "demo3") == 0) {
            Demo3CreateScene();
            demo3_elapsed_time = 0.75 * 365;
            demo3_time = demo3_elapsed_time;
            demo3_start_time = GUIGetCurrentTime();
            RenderFunc = Demo3Render;
            TimeIterationFunc = Demo3TimeIteration;
            RunDemo();
        }
#endif
        else if (strcmp(argv[argi], "demo4") == 0) {
            Demo4CreateScene();
//            view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
            RenderFunc = Demo4Render;
            TimeIterationFunc = Demo4TimeIteration;
            RunDemo();
        }
        else if (strcmp(argv[argi], "demo4b") == 0) {
            Demo4bCreateScene();
            RenderFunc = Demo4bRender;
            TimeIterationFunc = Demo4TimeIteration;
            RunDemo();
        }
        else if (strcmp(argv[argi], "demo5") == 0) {
            Demo5CreateScene();
            view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
            RenderFunc = Demo5Render;
            TimeIterationFunc = Demo5TimeIteration;
            RunDemo();
        }
        else if (strcmp(argv[argi], "demo6") == 0) {
            Demo5CreateScene();
            view->SetViewModeLookAt(Point3D(0, - 60.0, 40.0), Point3D(0, 140, 0), Vector3D(0, 0, 1.0));
            RenderFunc = Demo5Render;
            TimeIterationFunc = Demo6TimeIteration;
            RunDemo();
	}
        else if (strcmp(argv[argi], "demo7") == 0) {
            Demo7CreateScene();
            view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
            RenderFunc = Demo7Render;
            TimeIterationFunc = Demo7TimeIteration;
            RunDemo();
        }
        else if (strcmp(argv[argi], "demo8") == 0) {
            Demo8CreateScene();
            view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
            RenderFunc = Demo8Render;
            TimeIterationFunc = Demo8TimeIteration;
            RunDemo();
        }
        else if (strcmp(argv[argi], "demo9") == 0) {
            Demo9CreateScene();
            view->SetViewModeFollowObject(0, 40.0f, Vector3D(0, 0, 10.0f));
            RenderFunc = Demo9Render;
            TimeIterationFunc = Demo9TimeIteration;
            RunDemo();
        }
#ifdef USE_BULLET
        else if (strcmp(argv[argi], "game") == 0) {
            view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
            RunGame();
        }
#endif
        else if (strcmp(argv[argi], "texturetest") == 0) {
            TextureTestCreateScene(false);
            view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
            view->SetMovementMode(SRE_MOVEMENT_MODE_NONE);
            lock_panning = true;
            RenderFunc = TextureTestRender;
            TimeIterationFunc = TextureTestTimeIteration;
            RunDemo();
        }
        else if (strcmp(argv[argi], "texturetestcompressed") == 0) {
            TextureTestCreateScene(true);
            view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
            view->SetMovementMode(SRE_MOVEMENT_MODE_NONE);
            lock_panning = true;
            RenderFunc = TextureTestRender;
            TimeIterationFunc = TextureTestTimeIteration;
            RunDemo();
        }
        else if (strcmp(argv[argi], "texturememorytest") == 0) {
            TextureMemoryTest(false);
        }
        else if (strcmp(argv[argi], "texturememorytestcompressed") == 0) {
            TextureMemoryTest(true);
        }
        else if (strcmp(argv[argi], "demo10") == 0) {
            Demo10CreateScene();
            view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
            RenderFunc = Demo10Render;
            TimeIterationFunc = Demo10TimeIteration;
            RunDemo();
        }
        else if (strcmp(argv[argi], "demo11") == 0) {
            Demo11CreateScene();
            view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
            RenderFunc = Demo11Render;
            TimeIterationFunc = Demo11TimeIteration;
            RunDemo();
        }
        else if (strcmp(argv[argi], "textdemo") == 0) {
            TextDemoCreateScene();
            view->SetViewModeLookAt(Point3D(0, 0, 0), Point3D(0, 100.0f, 0), Vector3D(0, 0, 1.0f));
            view->SetMovementMode(SRE_MOVEMENT_MODE_NONE);
            lock_panning = true;
            RenderFunc = TextDemoRender;
            TimeIterationFunc = TextDemoTimeIteration;
            RunDemo();
        }
        else {
            printf("No recognized demo name specified.\n");
            sleep(3);
            GUIFinalize();
            exit(1);
        }
    }

    if (benchmark_mode) {
       double fps = (double)sreGetCurrentFrame() /
           (GUIGetCurrentTime() - demo_start_time);
       printf("Benchmark result: %.3lf fps\n", fps);
    }
    GUIFinalize();
    exit(0);
}

