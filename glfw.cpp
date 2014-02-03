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
#include "demo.h"
#include "gui-common.h"

static bool initialized = false;

static void GLFWCALL WindowResizeCallback(int width, int height) {
    if (!initialized)
        return;
    window_width = width;
    window_height = height;
    sreResize(view, window_width, window_height);
}

void GUIToggleFullScreenMode(int& window_width, int& window_height, bool pan_with_mouse) {
    // Doesn't work really well with GLFW 2.x, it does not use the proper XAtom protocol
    // method to toggle full-screen in X11. No-op.
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

void GUIWarpCursor(int x,int y) {
    glfwSetMousePos(x, y);
}

void GUIHideCursor() {
    glfwDisable(GLFW_MOUSE_CURSOR);
}

void GUIRestoreCursor() {
    glfwEnable(GLFW_MOUSE_CURSOR);
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

void ProcessGUIEvents(double dt) {
    motion_occurred = false;
    glfwPollEvents();
    if (motion_occurred)
        GUIProcessMouseMotion(motion_x, motion_y);
    GUIMovePlayer(dt);
}

void GUIGLSync() {
    glfwSwapBuffers();
    glFlush();
}

void DeinitializeGUI() {
   // Clear screen.
   glClear(GL_COLOR_BUFFER_BIT);
   glfwSwapBuffers();
   glfwTerminate();
}

void InitializeGUI(int *argc, char ***argv) {
    glfwInit();
    // Require OpenGL >= 3.0.
//    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
//    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 0);
    // Because we use glBindAttribute for compability with OpenGL ES 2.0, we do not have forward compability.
//    glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#ifndef NO_MULTI_SAMPLE
    // Enable multi-sample anti-aliasing
    glfwOpenWindowHint(GLFW_FSAA_SAMPLES, 4);
#endif
    int r;
    if (fullscreen_mode) {
        // Hack for a 1920x1080 monitor. This doesn't work properly, the returned resolution is 1920x1024.
        // There is tearing on the top part of the screen.
        window_width = 1920;
        window_height = 1080;
        r = glfwOpenWindow(window_width, window_height, 8, 8, 8, 8, 24, 8, GLFW_FULLSCREEN);
    }
    else
        r = glfwOpenWindow(window_width, window_height, 8, 8, 8, 8, 24, 8, GLFW_WINDOW);
    if (!r) {
        printf("Failed to open GLFW window.\n");
        glfwTerminate();
        exit(1);
    }
    if (fullscreen_mode) {
        glfwGetWindowSize(&window_width, &window_height);
    }
    glfwSetWindowTitle("SRE demo -- OpenGL rendering demo using GLFW");
    int stencil_bits = glfwGetWindowParam(GLFW_STENCIL_BITS);
    int depth_bits = glfwGetWindowParam(GLFW_DEPTH_BITS);
    printf("Opened GLFW context of size %d x %d with 32-bit pixels, %d-bit depthbuffer and %d-bit stencil.\n", window_width,
        window_height, depth_bits, stencil_bits);
    glfwSetWindowSizeCallback(WindowResizeCallback);
    glfwSetKeyCallback(KeyCallback);
    glfwSetMousePosCallback(MousePosCallback);
    glfwSetMouseButtonCallback(MouseButtonCallback);
    // Don't poll events during glfwSwapBuffers but require explicit calls to glfwPollEvents().
    glfwDisable(GLFW_AUTO_POLL_EVENTS);
    // Generate multiple keypress events when a key is held down.
//    glfwEnable(GLFW_KEY_REPEAT);
    if (fullscreen_mode)
        glfwSetMousePos(window_width / 2, window_height / 2);

    GLenum err = glewInit();
    if (GLEW_OK != err) {
        /* Problem: glewInit failed, something is seriously wrong. */
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        exit(1);
    }
    fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

    sreInitialize(window_width, window_height, glfwSwapBuffers);
    initialized = true;
}

double GetCurrentTime() {
    return glfwGetTime();
}

const char *GUIGetBackendName() {
   return "OpenGL GLFW";
}
