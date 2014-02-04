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
#include "gui-common.h"
#include "x11-common.h"
#include "egl-common.h"

void *EGLGetNativeDisplay() {
    return X11GetDisplay();
}

void EGLInitializeSubsystemWindow(int& width, int &height, void *&window) {
    // Use configured window size.
    width = window_width;
    height = window_height;
    
    X11CreateWindow(width, height, NULL, "SRE OpenGL-ES2.0 X11 demo");

    window = (void *)X11GetWindow();
}

void EGLDeinitializeSubsystem() {
    X11DestroyWindow();
    X11CloseDisplay();
}

const char *GUIGetBackendName() {
     return "OpenGL-ES2.0 X11";
};
