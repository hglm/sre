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
// The functions EGLGetNativeDisplay(), EGLInitializeSubsystemWindow() and
// EGLDeinitializeSubsystem() are called and must be provided by the EGL back-end-specific
// implementation. Different EGL back-ends cannot easily be mixed.

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
#include "sreBackend.h"
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

static const EGLint attribute_list_base[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_NATIVE_RENDERABLE, EGL_TRUE,
    EGL_NONE,
};

static const EGLint attribute_list_stencil_buffer[] = {
    EGL_DEPTH_SIZE, 24,
    EGL_STENCIL_SIZE, 8,
    EGL_NONE,
};

static const EGLint attribute_list_no_stencil_buffer[] = {
    EGL_DEPTH_SIZE, 24,
    EGL_STENCIL_SIZE, 0,
    EGL_NONE,
};

static const EGLint attribute_list_multi_sample[] =  {
    // Enable 4-sample MSAA.
#ifdef OPENGL_ES2_PLATFORM_RPI
    EGL_SAMPLE_BUFFERS, 1,
#endif
    EGL_SAMPLES, 4,
    EGL_NONE
};

#define MAX_ATTRIBUTES_SIZE ((sizeof(attribute_list_base) + \
    sizeof(attribute_list_stencil_buffer) + \
    sizeof(attribute_list_multi_sample)) / sizeof(EGLint))

static void AddAttributes(EGLint *attributes, const EGLint *extra_attributes) {
    int i = 0;
    while (attributes[i] != EGL_NONE)
        i++;
    int j = 0;
    while (extra_attributes[j] != EGL_NONE) {
        attributes[i] = extra_attributes[j];
        i++;
        j++;
    }
    attributes[i] = EGL_NONE;
}

static EGLConfig *egl_config;
static int egl_chosen_config;

#define check() assert(glGetError() == 0)

// Open a window with the requested size; the actual size may be different
// (e.g. a full-screen framebuffer).

static void EGLOpenWindow(EGL_STATE_T *state, EGLNativeDisplayType native_display,
int requested_width, int requested_height, unsigned int backend_flags) {
    // First initialize the native window.
    int width, height;
    void *window;
    // width, height and window are reference arguments and will be set.
    EGLInitializeSubsystemWindow(requested_width, requested_height, width,
        height, window);
    state->screen_width = width;
    state->screen_height = height;

    EGLBoolean result;
    EGLint num_config;

    state->display = eglGetDisplay(native_display);
//    state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(state->display != EGL_NO_DISPLAY);
    check();

    // Initialize the EGL display connection.
    EGLint egl_major, egl_minor;
    if (sre_internal_backend->index == SRE_BACKEND_GLES2_RPI_FB)
        result = eglInitialize(state->display, NULL, NULL);
    else
        result = eglInitialize(state->display, &egl_major, &egl_minor);
    assert(result != EGL_FALSE);
    check();

    // Arrange attribute list.
    EGLint attribute_list[MAX_ATTRIBUTES_SIZE];
    attribute_list[0] = EGL_NONE;
    AddAttributes(attribute_list, attribute_list_base);
    if (backend_flags & SRE_BACKEND_FLAG_STENCIL_BUFFER)
        AddAttributes(attribute_list, attribute_list_stencil_buffer);
    else
        AddAttributes(attribute_list, attribute_list_no_stencil_buffer);
    if (backend_flags & SRE_BACKEND_FLAG_MULTI_SAMPLE)
        AddAttributes(attribute_list, attribute_list_multi_sample);

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
    assert(result != EGL_FALSE);
    check();
    printf("EGL: %d framebuffer configurations returned.\n", num_config);

    // Always pick the first one.
    egl_chosen_config = 0;

    result = eglBindAPI(EGL_OPENGL_ES_API);
    assert(result != EGL_FALSE);
    check();

    // Create an EGL rendering context.
    state->context = eglCreateContext(state->display,
        egl_config[egl_chosen_config], EGL_NO_CONTEXT,
        egl_context_attributes);
    assert(state->context != EGL_NO_CONTEXT);
    check();

    state->surface = eglCreateWindowSurface(state->display,
        egl_config[egl_chosen_config],
        (EGLNativeWindowType)window, window_attribute_list);
    assert(state->surface != EGL_NO_SURFACE);
    check();

    // Connect the context to the surface.
    result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
    assert(result != EGL_FALSE);
    check();

    // Set background color and clear buffers
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    check();
}

void EGLInitialize(int *argc, char ***argv, int requested_width, int requested_height,
int& actual_width, int& actual_height, unsigned int backend_flags) {
    EGLNativeDisplayType native_display = (EGLNativeDisplayType)EGLGetNativeDisplay();

    state = (EGL_STATE_T *)malloc(sizeof(EGL_STATE_T));
    // Clear application state,
    memset(state, 0, sizeof(*state ));

    // Start GLES2.
    EGLOpenWindow(state, native_display, requested_width, requested_height, backend_flags);
    actual_width = state->screen_width;
    actual_height = state->screen_height;
    sreMessage(SRE_MESSAGE_INFO, "Opened OpenGL-ES2 state, width = %d, height = %d",
        actual_width, actual_height);
}

void EGLFinalize() {
   // Clear screen.
   glClear( GL_COLOR_BUFFER_BIT );
   eglSwapBuffers(state->display, state->surface);

   // Release OpenGL resources.
   eglMakeCurrent(state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroySurface(state->display, state->surface);
   eglDestroyContext(state->display, state->context);
   eglTerminate(state->display);

   EGLDeinitializeSubsystem();
}

void EGLSwapBuffers() {
    eglSwapBuffers(state->display, state->surface); 
}

void EGLSync() {
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(state->display, state->surface);
    eglWaitClient();
}

