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

// PC OpenGL interface using GLFW library

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <GL/glew.h>
#ifdef __GNUC__
#include <GL/glxew.h>
#endif
#include <GL/glfw.h>

#include "sre.h"
#include "sreBackend.h"
#include "demo.h"
#include "gui-common.h"

class sreBackendGLFW : public sreBackend {
public :
    virtual void Initialize(int *argc, char ***argv, int requested_width, int requested_height,
         int& actual_width, int& actual_height);
    virtual void Finalize();
    virtual void GLSwapBuffers();
    virtual void GLSync();
    virtual double GetCurrentTime();
    virtual void ProcessGUIEvents();
    virtual void ToggleFullScreenMode(int& width, int& height, bool pan_with_mouse);
    virtual void HideCursor();
    virtual void RestoreCursor();
    virtual void WarpCursor(int x, int y);
};

sreBackend *sreCreateBackendGLGLFW() {
    sreBackend *b = new sreBackendGLFW;
    b->name = "OpenGL 3.0+ GLFW";
    return b;
}

static bool initialized = false;

static void GLFWCALL WindowResizeCallback(int width, int height) {
    if (!initialized)
        return;
    sreResize(sre_internal_application->view, width, height);
}

static const unsigned int GLFW_translation_table[] = {
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE('A', 'Z'),
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE('0', '9'),
    '=', '+',
    GLFW_KEY_KP_ADD, '+',
    '-', '-',
    GLFW_KEY_KP_SUBTRACT, '-',
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE_WITH_OFFSET(GLFW_KEY_F1, GLFW_KEY_F12, SRE_KEY_F1),
    ',', ',',      // Comma
    '.', '.',      // Period
    '[', '[',
    ']', ']',
    ' ', ' ',      // Space
     '\\', '\\',   // Backslash
    '/', '/',      // Slash
    GLFW_KEY_ESC, SRE_KEY_ESC,
    SRE_TRANSLATION_TABLE_END
};

static void GLFWCALL KeyCallback(int id, int state) {
    if (state == GLFW_PRESS) {
        unsigned int key = GUITranslateKeycode(id, GLFW_translation_table);
        if (key != 0)
            GUIKeyPressCallback(key);
    }
    else if (state == GLFW_RELEASE) {
        unsigned int key = GUITranslateKeycode(id, GLFW_translation_table);
        if (key != 0)
            GUIKeyReleaseCallback(key);
    }
}

static const unsigned int GLFW_mouse_button_translation_table[] = {
    GLFW_MOUSE_BUTTON_LEFT, SRE_MOUSE_BUTTON_LEFT,
    GLFW_MOUSE_BUTTON_RIGHT, SRE_MOUSE_BUTTON_RIGHT,
    SRE_TRANSLATION_TABLE_END
};

static void GLFWCALL MouseButtonCallback(int button, int state) {
    int bc = GUITranslateKeycode(button, GLFW_mouse_button_translation_table);
    if (state == GLFW_PRESS)
        GUIMouseButtonCallback(bc, SRE_PRESS);
    else if (state == GLFW_RELEASE)
        GUIMouseButtonCallback(bc, SRE_RELEASE);
}

static bool motion_occurred = false;
static int motion_x, motion_y;

static void GLFWCALL MousePosCallback(int x, int y) {
    motion_occurred = true;
    motion_x = x;
    motion_y = y;
}

void sreBackendGLFW::ProcessGUIEvents() {
    motion_occurred = false;
    // Keypress events and mouse button events are handled directly.
    glfwPollEvents();
    // Register mouse motion if it has occurred.
    if (motion_occurred)
        GUIProcessMouseMotion(motion_x, motion_y);
}

void sreBackendGLFW::Initialize(int *argc, char ***argv, int requested_width, int requested_height,
int& actual_width, int& actual_height, unsigned int backend_flags) {
    glfwInit();
    // Require OpenGL >= 3.0.
//    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
//    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 0);
    // Because we use glBindAttribute for compability with OpenGL ES 2.0, we do not have forward compability.
//    glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    if (backend_flags & SRE_BACKEND_INIT_FLAG_MULTI_SAMPLE)
        // Enable multi-sample anti-aliasing
        glfwOpenWindowHint(GLFW_FSAA_SAMPLES, 4);
    int r;
    if (backend_flags & SRE_BACKEND_INIT_FLAG_STENCIL_BUFFER)
        r = glfwOpenWindow(requested_width, requested_height, 8, 8, 8, 8, 24, 8, GLFW_WINDOW);
    else
        r = glfwOpenWindow(requested_width, requested_height, 8, 8, 8, 8, 24, 0, GLFW_WINDOW);
    if (!r) {
        printf("Failed to open GLFW window.\n");
        glfwTerminate();
        exit(1);
    }
    glfwGetWindowSize(&actual_width, &actual_height);
    glfwSetWindowTitle("SRE demo -- OpenGL rendering demo using GLFW");
    int stencil_bits = glfwGetWindowParam(GLFW_STENCIL_BITS);
    int depth_bits = glfwGetWindowParam(GLFW_DEPTH_BITS);
    printf("Opened GLFW context of size %d x %d with 32-bit pixels, %d-bit depthbuffer and %d-bit stencil.\n",
        actual_width, actual_height, depth_bits, stencil_bits);
    glfwSetWindowSizeCallback(WindowResizeCallback);
    glfwSetKeyCallback(KeyCallback);
    glfwSetMousePosCallback(MousePosCallback);
    glfwSetMouseButtonCallback(MouseButtonCallback);
    // Don't poll events during glfwSwapBuffers but require explicit calls to glfwPollEvents().
    glfwDisable(GLFW_AUTO_POLL_EVENTS);
    // Generate multiple keypress events when a key is held down.
//    glfwEnable(GLFW_KEY_REPEAT);
//    if (fullscreen_mode)
//        glfwSetMousePos(window_width / 2, window_height / 2);

    GLenum err = glewInit();
    if (GLEW_OK != err) {
        /* Problem: glewInit failed, something is seriously wrong. */
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        exit(1);
    }
    fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

    initialized = true;
}

void sreBackendGLFW::Finalize() {
   // Clear screen.
   glClear(GL_COLOR_BUFFER_BIT);
   glfwSwapBuffers();
   glfwTerminate();
}

void sreBackendGLFW::GLSwapBuffers() {
    glfwSwapBuffers();
}

void sreBackendGLFW::GLSync() {
    glfwSwapBuffers();
    glFlush();
}

double sreBackendGLFW::GetCurrentTime() {
    return glfwGetTime();
}

void sreBackendGLFW::ToggleFullScreenMode(int& width, int& height, bool pan_with_mouse) {
    // Doesn't work really well with GLFW 2.x, it does not use the proper XAtom protocol
    // method to toggle full-screen in X11. No-op.
}

void sreBackendGLFW::HideCursor() {
    glfwDisable(GLFW_MOUSE_CURSOR);
}

void sreBackendGLFW::RestoreCursor() {
    glfwEnable(GLFW_MOUSE_CURSOR);
}

void sreBackendGLFW::WarpCursor(int x, int y) {
    glfwSetMousePos(x, y);
}

