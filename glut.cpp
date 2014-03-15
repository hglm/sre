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

// OpenGL interface support using the GLUT library.
//
// If GLUT_FREEGLUT is defined, additional features of freeglut
// are utilized.
//
// freeglut works fine, plain GLUT doesn't easily integrate given
// the way its main event loop works, so we there's no user input.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <GL/glew.h>
#ifdef __GNUC__
#include <GL/glxew.h>
#endif
#ifdef OPENGL_FREEGLUT
#include <GL/freeglut.h>
#else
#include <GL/glut.h>
#endif

#include "sre.h"
#include "sreBackend.h"
#include "demo.h"
#include "gui-common.h"

class sreBackendGLUT : public sreBackend {
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

sreBackend *sreCreateBackendGLUT() {
    sreBackend *b = new sreBackendGLUT;
#ifdef OPENGL_FREEGLUT
    b->name = "OpenGL 3.0+ freeglut";
#else
    b->name = "OpenGL 3.0+ GLUT";
#endif
    return b;
}

static bool initialized = false;
static int glut_window;

static void WindowResizeCallback(int width, int height) {
    if (!initialized)
        return;
    sreResize(sre_internal_application->view, width, height);
}

void sreBackendGLUT::ToggleFullScreenMode(int& window_width, int& window_height, bool pan_with_mouse) {
    // May be tricky with standard glut. Only support with freeglut for now.
#ifdef OPENGL_FREEGLUT
    glutFullScreenToggle();
#endif
}

static const unsigned int GLUT_translation_table[] = {
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE('A', 'Z'),
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE_WITH_OFFSET('a', 'z', 'A'),
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE('0', '9'),
    '=', '+',
    '-', '-',
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE_WITH_OFFSET(GLUT_KEY_F1, GLUT_KEY_F12, SRE_KEY_F1),
    ',', ',',      // Comma
    '.', '.',      // Period
    '[', '[',
    ']', ']',
    ' ', ' ',      // Space
     '\\', '\\',   // Backslash
    '/', '/',      // Slash
    SRE_TRANSLATION_TABLE_END
};

static void KeyPressCallback(int keycode, int x, int y) {
    unsigned int key = GUITranslateKeycode(keycode, GLUT_translation_table);
    if (key != 0)
        GUIKeyPressCallback(key);
}

static void NonSpecialKeyPressCallback(unsigned char keycode, int x, int y) {
    KeyPressCallback(keycode, x, y);
}

static void KeyReleaseCallback(int keycode, int x, int y) {
    unsigned int key = GUITranslateKeycode(keycode, GLUT_translation_table);
    if (key != 0)
        GUIKeyReleaseCallback(key);
}

static void NonSpecialKeyReleaseCallback(unsigned char keycode, int x, int y) {
    KeyReleaseCallback(keycode, x, y);
}

void sreBackendGLUT::WarpCursor(int x,int y) {
    glutWarpPointer(x, y);
}

void sreBackendGLUT::HideCursor() {
    glutSetCursor(GLUT_CURSOR_NONE);
}

void sreBackendGLUT::RestoreCursor() {
    glutSetCursor(GLUT_CURSOR_INHERIT);
}

static const unsigned int GLUT_mouse_button_translation_table[] = {
    GLUT_LEFT_BUTTON, SRE_MOUSE_BUTTON_LEFT,
    GLUT_RIGHT_BUTTON, SRE_MOUSE_BUTTON_RIGHT,
    0
};

static bool motion_occurred = false;
static int motion_x, motion_y;

static void MouseButtonCallback(int button, int state, int x, int y) {
    // The state value provided by GLUT matches the internal definition
    // (SRE_PRESS = 0 = press, SRE_RELEASE = 1 = release).
    int sre_button;
    if (button == GLUT_LEFT_BUTTON)
        sre_button = SRE_MOUSE_BUTTON_LEFT;
    else if (button == GLUT_RIGHT_BUTTON)
        sre_button = SRE_MOUSE_BUTTON_RIGHT;
    else if (button == GLUT_MIDDLE_BUTTON)
        sre_button = SRE_MOUSE_BUTTON_MIDDLE;
    else
        return;
    GUIMouseButtonCallback(sre_button, state);
}

static void MouseMotionCallback(int x, int y) {
    motion_occurred = true;
    motion_x = x;
    motion_y = y;
}

void sreBackendGLUT::ProcessGUIEvents() {
    motion_occurred = false;
#ifdef OPENGL_FREEGLUT
    glutMainLoopEvent();
#endif
    if (motion_occurred)
        GUIProcessMouseMotion(motion_x, motion_y);
}

void sreBackendGLUT::GLSwapBuffers() {
    glutSwapBuffers();
}

void sreBackendGLUT::GLSync() {
    glutSwapBuffers();
    glFlush();
}

void sreBackendGLUT::Finalize() {
   // Clear screen.
   glClear(GL_COLOR_BUFFER_BIT);
   glutSwapBuffers();
   glutDestroyWindow(glut_window);
}

void sreBackendGLUT::Initialize(int *argc, char ***argv, int requested_width, int requested_height,
int& actual_width, int& actual_height) {
    glutInit(argc, *argv);
    glutInitWindowSize(requested_width, requested_height);
    int glut_display_mode = GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH | GLUT_STENCIL;
#ifndef NO_MULTI_SAMPLE
    glut_display_mode |= GLUT_MULTISAMPLE;
#endif
    glutInitDisplayMode(glut_display_mode);
    glut_window = glutCreateWindow("SRE demo -- OpenGL rendering demo using GLUT");
    if (glut_window == 0) {
        printf("Failed to open GLUT window.\n");
        exit(1);
    }
    int depth_bits = glutGet(GLUT_WINDOW_DEPTH_SIZE);
    int stencil_bits = glutGet(GLUT_WINDOW_STENCIL_SIZE);
    actual_width = glutGet(GLUT_WINDOW_WIDTH);
    actual_height = glutGet(GLUT_WINDOW_HEIGHT);
    printf("Opened GLUT context of size %d x %d with 32-bit pixels, %d-bit depthbuffer and %d-bit stencil.\n",
        actual_width, actual_height, depth_bits, stencil_bits);
    printf("Multi-sample level: %d.\n", glutGet(GLUT_WINDOW_NUM_SAMPLES));

    glutReshapeFunc(WindowResizeCallback);
    glutKeyboardFunc(NonSpecialKeyPressCallback);
    glutSpecialFunc(KeyPressCallback);
    glutKeyboardUpFunc(NonSpecialKeyReleaseCallback);
    glutSpecialUpFunc(KeyReleaseCallback);
    glutMouseFunc(MouseButtonCallback);
    glutPassiveMotionFunc(MouseMotionCallback);
    glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
//    if (fullscreen_mode)
//        glutWarpPointer(window_width / 2, window_height / 2);

    GLenum err = glewInit();
    if (GLEW_OK != err) {
        /* Problem: glewInit failed, something is seriously wrong. */
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        exit(1);
    }
    fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

    initialized = true;
}

double sreBackendGLUT::GetCurrentTime() {
    return (double)glutGet(GLUT_ELAPSED_TIME) * 0.001;
}
