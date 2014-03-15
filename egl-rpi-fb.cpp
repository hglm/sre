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
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <assert.h>

#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "sre.h"
#include "sreBackend.h"
#include "gui-common.h"
#include "egl-common.h"
#include "linux-fb-ui.h"

class sreBackendGLES2RPIFB : public sreBackend {
public :
    virtual void Initialize(int *argc, char ***argv, int requested_width,
        int requested_height, int& actual_width, int& actual_height);
    virtual void Finalize();
    virtual void GLSwapBuffers();
    virtual void GLSync();
    virtual double GetCurrentTime();
    virtual void ProcessGUIEvents();
    virtual void ToggleFullScreenMode(int& width, int& height,
        bool pan_with_mouse);
    virtual void HideCursor();
    virtual void RestoreCursor();
    virtual void WarpCursor(int x, int y);
};

sreBackend *sreCreateBackendGLES2RPIFB() {
    sreBackend *b = new sreBackendGLES2RPIFB;
    b->name = "OpenGL-ES2.0 Raspberry Pi Framebuffer";
    return b;
}

void *EGLGetNativeDisplay() {
    return (void *)EGL_DEFAULT_DISPLAY;
}

#define check() assert(glGetError() == 0)

void EGLInitializeSubsystemWindow(int requested_width, int requested_height,
int& width, int& height, void *&window) {
    bcm_host_init();

    int success = graphics_get_display_size(0 /* LCD */,
        (uint32_t *)&width, (uint32_t *)&height);
    assert(success >= 0);

    static EGL_DISPMANX_WINDOW_T native_window;
    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = width;
    dst_rect.height = height;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = width << 16;
    src_rect.height = height << 16;

    dispman_display = vc_dispmanx_display_open(0 /* LCD */);
    dispman_update = vc_dispmanx_update_start(0);

    VC_DISPMANX_ALPHA_T alpha;
    alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
    alpha.opacity = 0xFF;
    alpha.mask = DISPMANX_NO_HANDLE;
    dispman_element = vc_dispmanx_element_add(dispman_update, dispman_display,
        0/*layer*/, &dst_rect, 0/*src*/,
        &src_rect, DISPMANX_PROTECTION_NONE, &alpha /*alpha*/, 0/*clamp*/,
        (DISPMANX_TRANSFORM_T)0/*transform*/);

//    nativewindow.element = dispman_element;
//    nativewindow.width = width;
//    nativewindow.height = height;

    vc_dispmanx_update_submit_sync(dispman_update);
    check();

    LinuxFBInitializeUI(width, height);
}

void EGLDeinitializeSubsystem() {
    LinuxFBRestoreConsoleState();
}

// Back-end class implementation.

void sreBackendGLES2RPIFB::Initialize(int *argc, char ***argv,
int requested_width, int requested_height,
int& actual_width, int& actual_height) {
    EGLInitialize(argc, argv, requested_width, requested_height,
        actual_width, actual_height);
}

void sreBackendGLES2RPIFB::Finalize() {
    EGLFinalize();
}

void sreBackendGLES2RPIFB::GLSwapBuffers() {
    EGLSwapBuffers();
}

void sreBackendGLES2RPIFB::GLSync() {
    EGLSync();
}

double sreBackendGLES2RPIFB::GetCurrentTime() {
    return LinuxFBGetCurrentTime();
}

void sreBackendGLES2RPIFB::ProcessGUIEvents() {
    LinuxFBProcessGUIEvents();
}

void sreBackendGLES2RPIFB::ToggleFullScreenMode(int& width, int& height,
bool pan_with_mouse) {
}

void sreBackendGLES2RPIFB::HideCursor() {
}

void sreBackendGLES2RPIFB::RestoreCursor() {
}

void sreBackendGLES2RPIFB::WarpCursor(int x, int y) {
    LinuxFBWarpCursor(x, y);
}

