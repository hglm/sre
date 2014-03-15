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
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <errno.h>
#include <signal.h>
#include <linux/fb.h>
#include <video/sunxi_disp_ioctl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "sre.h"
#include "sreBackend.h"
#include "gui-common.h"
#include "egl-common.h"
#include "linux-fb-ui.h"

class sreBackendGLES2AllwinnerMaliFB : public sreBackend {
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

sreBackend *sreCreateBackendGLES2AllwinnerMaliFB() {
    sreBackend *b = new sreBackendGLES2AllwinnerMaliFB;
    b->name = "OpenGL-ES2.0 Allwinner Mali-400 Framebuffer";
    return b;
}

#ifdef OPENGL_ES2_MALI
struct mali_native_window native_window = {
	.width = 640,
	.height = 480,
};
#endif

static int fd_disp, fd_fb[2];
static __disp_layer_info_t saved_layer_info[2];

#ifdef OPENGL_ES2_A10_SCALE
static void A10EnableScaling() {
        int ret;
        int layer_handle[2];
        __disp_layer_info_t layer_info[2];
        unsigned int args[4];
	int screen = 0;
	// Get the layer handle for both framebuffers associated with the screen.
	for (int i = 0; i < 2; i++) {
	        if (screen == 0) {
        	        ret = ioctl(fd_fb[i], FBIOGET_LAYER_HDL_0, args);
		}
        	else
                	ret = ioctl(fd_fb[i], FBIOGET_LAYER_HDL_1, args);
	        if (ret < 0) {
        	        fprintf(stderr, "Error: ioctl(FBIOGET_LAYER_HDL_%d) failed: %s\n", screen, strerror(- ret));
                	exit(ret);
	        }
	        layer_handle[i] = args[0];
	}

	for (int i = 0; i < 2; i++) {
	        args[0] = screen;
        	args[1] = layer_handle[i];
	        args[2] = (unsigned int)&layer_info[i];
	        ret = ioctl(fd_disp, DISP_CMD_LAYER_GET_PARA, args);
	        if (ret < 0) {
	                fprintf(stderr, "Error: ioctl(DISP_CMD_LAYER_GET_PARA) failed: %s\n", strerror(- ret));
	                exit(ret);
	        }
		saved_layer_info[i] = layer_info[i];
	        layer_info[i].mode = DISP_LAYER_WORK_MODE_SCALER;
		layer_info[i].src_win.x = 0;
		layer_info[i].src_win.y = 0;
	        layer_info[i].src_win.width = state->screen_width / 2;
	        layer_info[i].src_win.height = state->screen_height / 2;
		layer_info[i].scn_win.x = 0;
		layer_info[i].scn_win.y = 0;
	        layer_info[i].scn_win.width = state->screen_width;
	        layer_info[i].scn_win.height = state->screen_height;
	        args[0] = screen;
	        args[1] = layer_handle[i];
	        args[2] = (unsigned int)&layer_info[i];
	        ret = ioctl(fd_disp, DISP_CMD_LAYER_SET_PARA, args);
	        if (ret < 0) {
	                fprintf(stderr, "Error: ioctl(DISP_CMD_LAYER_SET_PARA) failed: %s\n", strerror(- ret));
	                exit(ret);
	        }
	}
}
#endif


void *EGLGetNativeDisplay() {
    return (void *)EGL_DEFAULT_DISPLAY;
}

void EGLInitializeSubsystemWindow(int requested_width, int requested_height, int &width, int &height, void *&window) {
    int tmp;
    int ret;
    unsigned int args[4];
    fd_disp = open("/dev/disp", O_RDWR);
    if (fd_disp == -1) {
        fprintf(stderr, "Error: Failed to open /dev/disp: %s\n",
            strerror(errno));
            exit(1);
    }

    tmp = SUNXI_DISP_VERSION;
    ret = ioctl(fd_disp, DISP_CMD_VERSION, &tmp);
    if (ret == -1) {
        printf("Warning: kernel sunxi disp driver does not support "
               "versioning.\n");
    } else if (ret < 0) {
        fprintf(stderr, "Error: ioctl(VERSION) failed: %s\n",
                strerror(-ret));
        exit(1);;
    } else
        printf("sunxi disp kernel module version is %d.%d\n",
                ret >> 16, ret & 0xFFFF);
    args[0] = 0;	// Screen 0.
    ret = ioctl(fd_disp, DISP_CMD_SCN_GET_WIDTH, args);
    if (ret < 0) {
        fprintf(stderr, "Error: ioctl(SCN_GET_WIDTH) failed: %s\n", strerror(-ret));
        exit(1);
    }
    width = ret;

    args[0] = 0;
    ret = ioctl(fd_disp, DISP_CMD_SCN_GET_HEIGHT, args);
    if (ret < 0) {
        fprintf(stderr, "Error: ioctl(SCN_GET_HEIGHT) failed: %s\n", strerror(-ret));
        exit(1);
    }
    height = ret;

    LinuxFBSetConsoleGraphics();

#ifdef OPENGL_ES2_A10_SCALE
    for (int i = 0; i < 2; i++) {
	char s[16];
        sprintf(s, "/dev/fb%d", i);
        fd_fb[i] = open(s, O_RDWR);
        if (fd_fb[i] == -1) {
            fprintf(stderr, "Error: Failed to open /dev/fb%d: %s\n", i, strerror(errno));
            exit(errno);
        }
    }
    A10EnableScaling();
    state->screen_width /= 2;
    state->screen_height /= 2;
    native_window.width = state->screen_width;
    native_window.height = state->screen_height;
#endif

    LinuxFBInitializeUI(width, height);

    native_window.width = width;
    native_window.height = height;
    window = &native_window;
    // width and hight have been set to the full screen framebuffer.
    return;
}

void EGLDeinitializeSubsystem() {
#ifdef OPENGL_ES2_A10_SCALE
        int ret;
        int layer_handle[2];
        unsigned int args[4];
        int screen = 0;
	// Get the layer handle for both framebuffers associated with the screen.
	for (int i = 0; i < 2; i++) {
	        if (screen == 0) {
        	        ret = ioctl(fd_fb[i], FBIOGET_LAYER_HDL_0, args);
		}
        	else
                	ret = ioctl(fd_fb[i], FBIOGET_LAYER_HDL_1, args);
	        if (ret < 0) {
        	        fprintf(stderr, "Error: ioctl(FBIOGET_LAYER_HDL_%d) failed: %s\n", screen, strerror(- ret));
                	exit(ret);
	        }
	        layer_handle[i] = args[0];
	}
	for (int i = 0; i < 2; i++) {
	        args[0] = screen;
        	args[1] = layer_handle[i];
	        args[2] = (unsigned int)&saved_layer_info[i];
	        ret = ioctl(fd_disp, DISP_CMD_LAYER_SET_PARA, args);
	        if (ret < 0) {
	                fprintf(stderr, "Error: ioctl(DISP_CMD_LAYER_SET_PARA) failed: %s\n", strerror(- ret));
	                exit(ret);
	        }
	}
#endif

    LinuxFBRestoreConsoleState();
}

// Back-end class implementation.

void sreBackendGLES2AllwinnerMaliFB::Initialize(int *argc, char ***argv, int requested_width, int requested_height,
int& actual_width, int& actual_height) {
    EGLInitialize(argc, argv, requested_width, requested_height, actual_width, actual_height);
}

void sreBackendGLES2AllwinnerMaliFB::Finalize() {
    EGLFinalize();
}

void sreBackendGLES2AllwinnerMaliFB::GLSwapBuffers() {
    EGLSwapBuffers();
}

void sreBackendGLES2AllwinnerMaliFB::GLSync() {
    EGLSync();
}

double sreBackendGLES2AllwinnerMaliFB::GetCurrentTime() {
    return LinuxFBGetCurrentTime();
}

void sreBackendGLES2AllwinnerMaliFB::ProcessGUIEvents() {
    LinuxFBProcessGUIEvents();
}

void sreBackendGLES2AllwinnerMaliFB::ToggleFullScreenMode(int& width, int& height, bool pan_with_mouse) {
}

void sreBackendGLES2AllwinnerMaliFB::HideCursor() {
}

void sreBackendGLES2AllwinnerMaliFB::RestoreCursor() {
}

void sreBackendGLES2AllwinnerMaliFB::WarpCursor(int x, int y) {
    LinuxFBWarpCursor(x, y);
}
