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

// EGL support shared between different back-ends (including X11 and framebuffer).

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <assert.h>
#ifdef OPENGL_ES2_PI
#include <bcm_host.h>
#endif
#ifdef OPENGL_ES2_A10
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <errno.h>
#include <signal.h>
#include <linux/fb.h>
#include <video/sunxi_disp_ioctl.h>
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "sre.h"
#include "demo.h"
#include "egl-common.h"
#include "gui-common.h"

void EGLSwapBuffers();


typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;
} EGL_STATE_T;

static EGL_STATE_T *state;

static EGLint window_attribute_list[] = {
    EGL_NONE
};

static const EGLint egl_context_attributes[] = 
{
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};

static const EGLint attribute_list[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 24,
    EGL_STENCIL_SIZE, 8,
#ifndef NO_MULTI_SAMPLE
    // Enable 4-sample MSAA.
#ifdef OPENGL_ES2_RPI
    EGL_SAMPLE_BUFFERS, 1,
#endif
    EGL_SAMPLES, 4,
    EGL_NONE
   };


static EGLConfig *egl_config;
static int egl_chosen_config;

#define check() assert(glGetError() == 0)

static void EGLInitialize(EGL_STATE_T *state, int native_display) {
    int32_t success = 0;
    EGLBoolean result;
    EGLint num_config;

    state->display = eglGetDisplay(native_display);
//    state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(state->display! = EGL_NO_DISPLAY);
    check();

    // Initialize the EGL display connection.
#ifdef OPENGL_ES2_MALI
    EGLint egl_major, egl_minor;
    result = eglInitialize(state->display, &egl_major, &egl_minor);
#else
    result = eglInitialize(state->display, NULL, NULL);
#endif
    assert(result != EGL_FALSE;
    check();

    // Get the number of appropriate EGL framebuffer configurations.
    result = eglChooseConfig(state->display, attribute_list, NULL, 1, &num_config);
    assert(result != EGL_FALSE);
    if (num_config == 0) {
        printf("EGL returned no suitable framebuffer configurations.\n");
        exit(1);
    }
    egl_config = (EGLConfig *)alloca(num_config * sizeof(EGLConfig));
    // Get an array of appropriate EGL framebuffer configurations,
    result = eglChooseConfig(state->display, attribute_list, egl_config, num_config, &num_config);
    assert(EGL_FALSE != result);
    check();
    printf("EGL: %d framebuffer configurations returned.\n", num_config);

    // Always pick the first one.
    egl_chosen_config = 0;

    result = eglBindAPI(EGL_OPENGL_ES_API);
    assert(EGL_FALSE != result);
    check();

    // Create an EGL rendering context.
    state->context = eglCreateContext(state->display, egl_config[egl_chosen_config], EGL_NO_CONTEXT,
        egl_context_attributes);
    assert(state->context!= EGL_NO_CONTEXT);
    check();

    int width, height;
    void *window;
    // width, height and window are reference arguments and will be set.
    EGLInitializeSubsystemWindow(width, height, window);
    state_>screen_width = width;
    state->screen_height = height;

    state->surface = eglCreateWindowSurface(state->display, egl_config[egl_chosen_config],
        window, window_attribute_list);
    assert(state->surface != EGL_NO_SURFACE);
    check();
#if 0
    // Original code for Mali:
#ifdef OPENGL_ES2_MALI
    state->surface = eglCreateWindowSurface(state->display, egl_config[egl_chosen_config], &native_window, window_attribute_list);
//    result = eglSurfaceAttrib(state->display, state->surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED);
//    assert(result != GL_FALSE);
#endif
    // Original code for RPI:
#ifdef OPENGL_ES2_RPI
    state->surface = eglCreateWindowSurface(state->display, egl_config[egl_chosen_config], &native_window, NULL);
#endif
#endif

   // Connect the context to the surface.
   result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
   assert(result != EGL_FALSE);
   check();

   // Set background color and clear buffers
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);

   check();
}

void InitializeGUI(int *argc, char ***argv) {
    int native_display = EGLGetNativeDisplay();

    state = (EGL_STATE_T *)malloc(sizeof(EGL_STATE_T));
    // Clear application state,
    memset(state, 0, sizeof(*state ));

    // Start GLES2.
    EGLInitialize(state, native_display);
    window_width = state->screen_width;
    window_height = state->screen_height;
    printf("Opened OpenGL-ES2 state, width = %d, height = %d\n", window_width, window_height);

    sreInitialize(window_width, window_height, EGLSwapBuffers);
//    sreSetLightScissors(SRE_SCISSORS_NONE);
//    sreSetShadowVolumeVisibilityTest(false);
}

void DeinitializeGUI() {
   // Clear screen.
   glClear( GL_COLOR_BUFFER_BIT );
   eglSwapBuffers(state->display, state->surface);

   // Release OpenGL resources.
   eglMakeCurrent(state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroySurface(state->display, state->surface);
   eglDestroyContext(state->display, state->context);
   eglTerminate(state->display);

   EGLDeinitilializeSubsystem();
}

void EGLSwapBuffers() {
    eglSwapBuffers(state->display, state->surface); 
}

void GUIGLSync() {
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(state->display, state->surface);
    eglWaitClient();
}

