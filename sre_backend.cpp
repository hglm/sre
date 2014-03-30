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
#include <math.h>
#include <string.h>

#include "sre.h"
#include "sreBackend.h"
#include "gui-common.h"

#ifdef INCLUDE_BACKEND_GL_X11
sreBackend *sreCreateBackendGLX11();
#endif
#ifdef INCLUDE_BACKEND_GL_GLFW
sreBackend *sreCreateBackendGLGLFW();
#endif
#ifdef INCLUDE_BACKEND_GLES2_X11
sreBackend *sreCreateBackendGLES2X11();
#endif
#ifdef INCLUDE_BACKEND_GLES2_ALLWINNER_MALI_FB
sreBackend *sreCreateBackendGLES2AllwinnerMaliFB();
#endif
#ifdef INCLUDE_BACKEND_GLES2_RPI_FB
sreBackend *sreCreateBackendGLES2RPIFB();
#endif

sreBackend *sre_internal_backend = NULL;
sreApplication *sre_internal_application;

// Command-line options.
static bool benchmark_mode = false;
static bool multiple_lights = false;
static bool multipass_rendering = false;
static bool fullscreen_mode = false;
static int shadows = SRE_SHADOWS_NONE;
static bool preprocess = false;
static int debug_level = 0;
static bool demand_load_shaders = false;

void sreSelectBackend(int backend) {
    if (backend == SRE_BACKEND_DEFAULT)
        backend = DEFAULT_BACKEND;
    const char *gl_platform_str;
    if (sreGetOpenGLPlatform() == SRE_OPENGL_PLATFORM_GLES2)
        gl_platform_str = "OpenGL-ES 2.0";
    else
        gl_platform_str = "OpenGL 3.0+";
    // OpenGL-ES 2.0 and OpenGL 3.0+ platforms are never mixed.
    switch (backend) {
#ifdef INCLUDE_BACKEND_GL_X11
    case SRE_BACKEND_GL_X11 :
        sre_internal_backend = sreCreateBackendGLX11();
        break;
#endif
#ifdef INCLUDE_BACKEND_GL_GLFW
    case SRE_BACKEND_GL_GLFW :
        sre_internal_backend = sreCreateBackendGLGLFW();
        break;
#endif
#ifdef INCLUDE_BACKEND_GLES2_X11
    case SRE_BACKEND_GLES2_X11 :
        sre_internal_backend = sreCreateBackendGLES2X11();
        break;
#endif
#ifdef INCLUDE_BACKEND_GLES2_ALLWINNER_MALI_FB
    case SRE_BACKEND_GLES2_ALLWINNER_MALI_FB :
        sre_internal_backend = sreCreateBackendGLES2AllwinnerMaliFB();
        break;
#endif
#ifdef INCLUDE_BACKEND_GLES2_RPI_FB
    case SRE_BACKEND_GLES2_RPI_FB :
        sre_internal_backend = sreCreateBackendGLES2RPIFB();
        break;
#endif
    default :
        sreFatalError("sreSelectBackend: Invalid back-end or back-end not supported.");
        break;
    }
    sre_internal_backend->index = backend;
    sreMessage(SRE_MESSAGE_INFO, "GL Platform: %s, Back-end selected: %s",
        gl_platform_str, sre_internal_backend->name);
}

void sreBackendGLSwapBuffers() {
    sre_internal_backend->GLSwapBuffers();
}

static void sreBackendSetOptionDefaults() {
    if (sreGetOpenGLPlatform() == SRE_OPENGL_PLATFORM_GLES2)
        shadows = SRE_SHADOWS_NONE;
    else
        shadows = SRE_SHADOWS_SHADOW_VOLUMES;
    if (sreGetOpenGLPlatform() == SRE_OPENGL_PLATFORM_GLES2) {
        multiple_lights = false;
        multipass_rendering = false;
    }
    else {
        multiple_lights = true;
        multipass_rendering = true;
    }
}

static void sreBackendProcessOptions(int *argcp, char ***argvp) {
    int argi = 1;
    int argc = *argcp;
    char **argv = *argvp;
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
    if (argi > 1)
        memcpy(argv + 1, argv + 1 + argi - 1, argi - 1);
    *argcp = argc - (argi - 1);
}

static double fps_table[10];
static int nu_fps = 0;
static double current_averaged_fps = 0;

void sreBackendStandardTextOverlay() {
    char s[32];
    sprintf(s, "FPS: %.2lf", current_averaged_fps);
    // Set standard parameters (blending).
    sreSetImageBlendingMode(SRE_IMAGE_BLEND_ADDITIVE);
    // Force rebinding of the current font.
    sreSetFont(NULL);
    Vector2D font_size = Vector2D(0.02, 0.04);
    sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size);
    if (sre_internal_application->flags & SRE_APPLICATION_FLAG_DISPLAY_FPS)
        sreDrawText(s, 0.01, 0);
    if (sre_internal_backend->GetCurrentTime() >= sre_internal_application->text_message_time + sre_internal_application->text_message_timeout)
        GUITextMessageTimeoutCallback();
    font_size = Vector2D(0.012, 0.04);
    sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size);
    for (int i = 0; i < sre_internal_application->nu_text_message_lines; i++)
        if (sre_internal_application->text_message[i][0] != '\0')
            sreDrawText(sre_internal_application->text_message[i], 0.01, 0.04 + 0.04 * i);
}

static void sreBackendInitialize(sreApplication *app, int *argc, char ***argv) {
   // Some SRE parameters need to be set before the initialization function is called.
    sreSetDebugMessageLevel(debug_level);
    if (demand_load_shaders)
        sreSetDemandLoadShaders(true);

    // Initialize GUI and SRE library.
    int actual_width, actual_height;
    sre_internal_backend->Initialize(argc, argv, WINDOW_WIDTH, WINDOW_HEIGHT, actual_width, actual_height);
    app->window_width = actual_width;
    app->window_height = actual_height;
    sreInitialize(actual_width, actual_height, sreBackendGLSwapBuffers);

    sreSetDrawTextOverlayFunc(sreBackendStandardTextOverlay);
    sreSetShadowsMethod(shadows);
    if (multipass_rendering)
        sreEnableMultiPassRendering();
    else
        sreDisableMultiPassRendering();
    sreSetLightScissors(SRE_SCISSORS_GEOMETRY);
    if (multipass_rendering && !multiple_lights)
        sreSetMultiPassMaxActiveLights(1);

    // Enable mouse panning by default for framebuffer back-ends.
    if (sre_internal_backend->index == SRE_BACKEND_GLES2_ALLWINNER_MALI_FB ||
    sre_internal_backend->index == SRE_BACKEND_GLES2_RPI_FB) {
        sre_internal_application->SetFlags(sre_internal_application->GetFlags() |
            SRE_APPLICATION_FLAG_PAN_WITH_MOUSE);
        // The Linux console mouse interface has relatively low sensitivity.
        // Also reverse the y movement.
        sre_internal_application->mouse_sensitivity = Vector2D(2.0f, - 2.0f);
    }

}

static const char *shadow_str[3] = {
    "No shadows", "Stencil shadow volumes", "Shadow mapping" };

static void PrintConfigurationInfo() {
    sreMessage(SRE_MESSAGE_INFO, "Back-end: %s", sre_internal_backend->name);
    sreMessage(SRE_MESSAGE_INFO, "Default shadow settings: %s", shadow_str[shadows]);
    sreMessage(SRE_MESSAGE_INFO, "Default number of lights: %s",
        multiple_lights == true ? "Unlimited" : "Single");
    sreMessage(SRE_MESSAGE_INFO, "Rendering method: %s",
        multipass_rendering == true ? "Multi-pass" : "Single-pass");
    if (debug_level > 0)
        sreMessage(SRE_MESSAGE_INFO, "SRE library debug message level = %d.", debug_level);
    if (benchmark_mode)
        sreMessage(SRE_MESSAGE_INFO, "Benchmark mode enabled.");
}

void sreInitializeApplication(sreApplication *app, int *argc, char ***argv) {
    sreMessage(SRE_MESSAGE_INFO, "Initializing back-end.");
    sre_internal_application = app;
    if (sre_internal_backend == NULL)
        sreSelectBackend(SRE_BACKEND_DEFAULT);
    sreBackendSetOptionDefaults();
    sreBackendProcessOptions(argc, argv);
    if (preprocess)
        app->SetFlags(app->GetFlags() | SRE_APPLICATION_FLAG_PREPROCESS);
    sreBackendInitialize(app, argc, argv);
    PrintConfigurationInfo();

    sreMessage(SRE_MESSAGE_INFO, "Initializing scene.");
    // Create a scene with initial default maximums of 1024 objects, 256 models and 128 lights.
    // Dynamic reallocation in libsre should ensure that actual numbers are practically
    // unlimited (except for main memory and GPU memory restrictions).
    app->scene = new sreScene(1024, 256, 128);

    // Create a view. Must be called after initialization.
    app->view = new sreView;
    app->view->SetViewModeLookAt(Point3D(0, 0, 50.0f), Point3D(0, 50.0f, 0),
        Vector3D(0, 1.0f, - 1.0f).Normalize());
    app->view->SetMovementMode(SRE_MOVEMENT_MODE_STANDARD);
}

sreApplication::sreApplication() {
    flags = SRE_APPLICATION_FLAG_DISPLAY_FPS | SRE_APPLICATION_FLAG_JUMP_ALLOWED;
    input_acceleration = 0;
    horizontal_acceleration = 100.0f;
    max_horizontal_velocity = 100.0f;
    gravity_position = Point3D(0, 0, 0);
    hovering_height_acceleration = 100.0f;
    jump_requested = false;
    stop_signal = 0;
    control_object = 0;
    mouse_sensitivity = Vector2D(1.0f, 1.0f);

    text_message_time = 0;
    text_message_timeout = 3.0;
    nu_text_message_lines = 2;
    text_message[0] = "";
    text_message[1] = "";
}

unsigned int sreApplication::GetFlags() {
    return flags;
}

void sreApplication::SetFlags(unsigned int _flags) {
    flags = _flags;
}

void sreMainLoop(sreApplication *app) {
    sreMessage(SRE_MESSAGE_INFO, "Starting main rendering loop.");
    double time_physics_previous = sre_internal_backend->GetCurrentTime();
    double time_physics_current = time_physics_previous;
    double end_time = sre_internal_backend->GetCurrentTime();
    app->start_time = end_time;
    double previous_time;
    int nu_frames = 0;
    app->stop_signal = 0;
    for (;;) {
        if (app->stop_signal != 0) {
            break;
        }
        app->StepBeforeRender(end_time - app->start_time);
        app->scene->Render(app->view);

        app->StepBeforePhysics(end_time - app->start_time);
        time_physics_current = sre_internal_backend->GetCurrentTime();
        if (!(app->flags & SRE_APPLICATION_FLAG_NO_PHYSICS))
           app->DoPhysics(time_physics_previous, time_physics_current);
        time_physics_previous = time_physics_current;
        nu_frames++;
        previous_time = end_time;
        end_time = sre_internal_backend->GetCurrentTime();
        sre_internal_backend->ProcessGUIEvents();
        app->ApplyControlObjectInputs(end_time - previous_time);
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
//            sreMessage(SRE_MESSAGE_INFO, "current fps: %.2lf\n", current_averaged_fps);
        }
        if (benchmark_mode && end_time - app->start_time > 20.0)
            break;
    }    
}

void sreRunApplication(sreApplication *app) {
    unsigned int prepare_flags = 0;
    if (app->flags & SRE_APPLICATION_FLAG_PREPROCESS)
        prepare_flags |= SRE_PREPARE_PREPROCESS;
    if (app->flags & SRE_APPLICATION_FLAG_UPLOAD_NO_MODELS)
        prepare_flags |= SRE_PREPARE_UPLOAD_NO_MODELS;
    if (app->flags & SRE_APPLICATION_FLAG_UPLOAD_ALL_MODELS)
        prepare_flags |= SRE_PREPARE_UPLOAD_ALL_MODELS;
    if (app->flags & SRE_APPLICATION_FLAG_REUSE_OCTREES)
        prepare_flags |= SRE_PREPARE_REUSE_OCTREES;
    app->scene->PrepareForRendering(prepare_flags);
    if (!(app->flags & SRE_APPLICATION_FLAG_NO_PHYSICS))
        app->InitializePhysics();
    sreMainLoop(app);
    if (benchmark_mode) {
       double fps = (double)sreGetCurrentFrame() /
           (sre_internal_backend->GetCurrentTime() - app->start_time);
       sreMessage(SRE_MESSAGE_INFO, "Benchmark result: %.3lf fps", fps);
    }
    if (!(app->flags & SRE_APPLICATION_FLAG_NO_PHYSICS))
        app->DestroyPhysics();
    if (!(app->flags & SRE_APPLICATION_FLAG_REUSE_OCTREES))
        app->scene->ClearOctrees();
}

void sreFinalizeApplication(sreApplication *app) {
    sreMessage(SRE_MESSAGE_INFO, "Deleting scene and closing back-end window.");
    delete app->scene;
    delete app;
    sre_internal_backend->Finalize();
}

void sreNoPhysicsApplication::DoPhysics(double previous_time, double current_time) {
}

void sreNoPhysicsApplication::InitializePhysics() {
}

void sreNoPhysicsApplication::StepBeforePhysics(double demo_time) {
}

static Vector3D input_velocity;
static Vector3D player_velocity;

void sreGenericPhysicsApplication::DoPhysics(double previous_time, double current_time) {
    float dtime = current_time - previous_time;
    // Move the player.
    if (input_velocity != Vector3D(0, 0, 0)) {
        player_velocity += input_velocity;
        input_velocity.Set(0, 0, 0);
    }
    if (player_velocity != Vector3D(0, 0, 0)) {
        scene->ChangePosition(control_object, scene->object[control_object]->position.x
            + player_velocity.x * dtime,
            scene->object[control_object]->position.y + player_velocity.y * dtime,
            scene->object[control_object]->position.z + player_velocity.z * dtime);
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

void sreGenericPhysicsApplication::InitializePhysics() {
    input_velocity = Vector3D(0, 0, 0);
    player_velocity = Vector3D(0, 0, 0);
}

// sreBulletPhysicsApplication::InitializePhysics and DoPhysics are provided in
// bullet.cpp.
