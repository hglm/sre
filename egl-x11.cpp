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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include "sre.h"
#include "sreBackend.h"
#include "gui-common.h"
#include "x11-common.h"
#include "egl-common.h"

class sreBackendGLES2X11 : public sreBackend {
public :
    virtual void Initialize(int *argc, char ***argv, int window_width, int window_height,
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

sreBackend *sreCreateBackendGLES2X11() {
    sreBackend *b = new sreBackendGLES2X11;
    b->name = "OpenGL-ES2.0 X11";
    return b;
}


// Functions called from common functions in egl-common.c.

void *EGLGetNativeDisplay() {
    return X11GetDisplay();
}

void EGLInitializeSubsystemWindow(int requested_width, int requested_height,
int& width, int &height, void *&window) {
    // Use configured window size.
    width = requested_width;
    height = requested_height;

    X11CreateWindow(width, height, NULL, "SRE OpenGL-ES2.0 X11 demo");

    window = (void *)X11GetWindow();
}

void EGLDeinitializeSubsystem() {
    X11DestroyWindow();
    X11CloseDisplay();
}

// Back-end class implementation.

void sreBackendGLES2X11::Initialize(int *argc, char ***argv, int requested_width, int requested_height,
int& actual_width, int& actual_height) {
    EGLInitialize(argc, argv, requested_width, requested_height, actual_width, actual_height);
}

void sreBackendGLES2X11::Finalize() {
    EGLFinalize();
}

void sreBackendGLES2X11::GLSwapBuffers() {
    EGLSwapBuffers();
}

void sreBackendGLES2X11::GLSync() {
    EGLSync();
}

double sreBackendGLES2X11::GetCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

void sreBackendGLES2X11::ProcessGUIEvents() {
    X11ProcessGUIEvents();
}

void sreBackendGLES2X11::ToggleFullScreenMode(int& width, int& height, bool pan_with_mouse) {
    X11ToggleFullScreenMode(width, height, pan_with_mouse);
}

void sreBackendGLES2X11::HideCursor() {
    X11HideCursor();
}

void sreBackendGLES2X11::RestoreCursor() {
    X11RestoreCursor();
}

void sreBackendGLES2X11::WarpCursor(int x, int y) {
    X11WarpCursor(x, y);
}

