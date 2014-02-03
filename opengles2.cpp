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

// OpenGL-ES2 interface for:
// - Raspberry Pi (Broadcom SOC) (-DOPENGL_ES2_RPI)
// - Allwinner A1x/A20 ARM devices with Mali400 (-DOPENGL_ES2_A10 -DOPENGL_ES2_MALI).
// - EGL X11 (-DOPENGL_ES2_X11), tested with Allwinner A20 with Mali400.

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
#ifdef OPENGL_ES2_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "sre.h"
#ifndef OPENGL_ES2_X11
#include "MouseEventQueue.h"
#endif
#include "demo.h"
#include "x11-common.h"
#include "gui-common.h"

#ifndef OPENGL_ES2_X11
MouseEventQueue *mouse_event_queue;
#endif

#ifdef OPENGL_ES2_MALI
struct mali_native_window native_window = {
	.width = 640,
	.height = 480,
};
#endif
#endif

typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;
// OpenGL|ES objects
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

static EGLConfig *egl_config;
static int egl_chosen_config = 0;

#define check() assert(glGetError() == 0)

#ifdef OPENGL_ES2_A10

static int fd_disp, fd_fb[2];
static __disp_layer_info_t saved_layer_info[2];

static void SetTextMode() {
    int tty = open("/dev/tty0", O_RDWR); 
    ioctl(tty, KDSETMODE, KD_TEXT);
    close(tty);
}

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

static void A10RestoreGraphicsState() {
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
    SetTextMode();
}

struct sigaction signal_quit_oldact, signal_segv_oldact, signal_int_oldact;

void signal_quit(int num, siginfo_t *info, void *p) {
    A10RestoreGraphicsState();
    if (signal_quit_oldact.sa_flags & SA_SIGINFO)
        signal_quit_oldact.sa_sigaction(num, info, p);
    else
        signal_quit_oldact.sa_handler(num);
}

void signal_segv(int num, siginfo_t *info, void *p) {
    A10RestoreGraphicsState();
    if (signal_segv_oldact.sa_flags & SA_SIGINFO)
        signal_segv_oldact.sa_sigaction(num, info, p);
    else
        signal_segv_oldact.sa_handler(num);
}

void signal_int(int num, siginfo_t *info, void *p) {
    A10RestoreGraphicsState();
    if (signal_int_oldact.sa_flags & SA_SIGINFO)
        signal_int_oldact.sa_sigaction(num, info, p);
    else
        signal_int_oldact.sa_handler(num);
}
#endif

static void init_ogl(EGL_STATE_T *state) {
   int32_t success = 0;
   EGLBoolean result;
   EGLint num_config;

#ifdef OPENGL_ES2_PI
   static EGL_DISPMANX_WINDOW_T native_window;

   DISPMANX_ELEMENT_HANDLE_T dispman_element;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   DISPMANX_UPDATE_HANDLE_T dispman_update;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;
#endif

   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 24,
      EGL_STENCIL_SIZE, 8,
#ifndef NO_MULTI_SAMPLE
#ifdef OPENGL_ES2_RPI
      EGL_SAMPLE_BUFFERS, 1,	// Use MSAA
      EGL_SAMPLES, 4,
#endif
#if defined(OPENGL_ES2_MALI) || defined (OPENGL_ES2_X11)
      EGL_SAMPLES, 4,
#endif
#endif
#ifdef OPENGL_ES2_MALI
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
#else
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
#endif
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE
   };
   
#ifdef OPENGL_ES2_X11
   X11CreateWindow(window_width, window_height,
       "render OpenGL-ES2.0 X11 demo");
   state->display = eglGetDisplay((EGLNativeDisplayType) X11GetDisplay());
#else
   // get an EGL display connection
   state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#endif

   assert(state->display!=EGL_NO_DISPLAY);
   check();

   // initialize the EGL display connection
#ifdef OPENGL_ES2_MALI
    EGLint egl_major, egl_minor;
    result = eglInitialize(state->display, &egl_major, &egl_minor);
#else
    result = eglInitialize(state->display, NULL, NULL);
#endif
   assert(EGL_FALSE != result);
   check();

    // Get the number of appropriate EGL framebuffer configurations.
    result = eglChooseConfig(state->display, attribute_list, NULL, 1, &num_config);
    assert(EGL_FALSE != result);
    if (num_config == 0) {
        printf("EGL returned no suitable framebuffer configurations.\n");
        exit(1);
    }
    egl_config = (EGLConfig *)alloca(num_config * sizeof(EGLConfig));
    // Get an array of appropriate EGL frame buffer configuration
    result = eglChooseConfig(state->display, attribute_list, egl_config, num_config, &num_config);
    assert(EGL_FALSE != result);
    check();
    printf("EGL: %d framebuffer configurations returned.\n", num_config);

   // get an appropriate EGL frame buffer configuration
   result = eglBindAPI(EGL_OPENGL_ES_API);
   assert(EGL_FALSE != result);
   check();

   // create an EGL rendering context
   state->context = eglCreateContext(state->display, egl_config[egl_chosen_config], EGL_NO_CONTEXT,
       egl_context_attributes);
   assert(state->context!=EGL_NO_CONTEXT);
   check();

#ifdef OPENGL_ES2_RPI
   // create an EGL window surface
   success = graphics_get_display_size(0 /* LCD */, &state->screen_width, &state->screen_height);
   assert( success >= 0 );

   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = state->screen_width;
   dst_rect.height = state->screen_height;
      
   src_rect.x = 0;
   src_rect.y = 0;
   src_rect.width = state->screen_width << 16;
   src_rect.height = state->screen_height << 16;        

   dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
   dispman_update = vc_dispmanx_update_start( 0 );
         
   VC_DISPMANX_ALPHA_T alpha;
   alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
   alpha.opacity = 0xFF;
   alpha.mask = DISPMANX_NO_HANDLE;
   dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
      0/*layer*/, &dst_rect, 0/*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE, &alpha /*alpha*/, 0/*clamp*/, (DISPMANX_TRANSFORM_T)0/*transform*/);
      
   nativewindow.element = dispman_element;
   nativewindow.width = state->screen_width;
   nativewindow.height = state->screen_height;
   vc_dispmanx_update_submit_sync( dispman_update );
      
   check();
#endif

#ifdef OPENGL_ES2_A10
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
        state->screen_width = ret;

        args[0] = 0;
        ret = ioctl(fd_disp, DISP_CMD_SCN_GET_HEIGHT, args);
        if (ret < 0) {
        	fprintf(stderr, "Error: ioctl(SCN_GET_HEIGHT) failed: %s\n", strerror(-ret));
                exit(1);
        }
        state->screen_height = ret;
	native_window.width = state->screen_width;
	native_window.height = state->screen_height;

    int tty = open("/dev/tty0", O_RDWR); 
    ioctl(tty, KDSETMODE, KD_GRAPHICS);
    close(tty);
    atexit(A10RestoreGraphicsState);
    struct sigaction act;
    act.sa_sigaction = signal_quit;
    __sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGQUIT, &act, &signal_quit_oldact);
    act.sa_sigaction = signal_segv;
    sigaction(SIGSEGV, &act, &signal_segv_oldact);
    act.sa_sigaction = signal_int;
    sigaction(SIGINT, &act, &signal_int_oldact);

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
#endif

#ifdef OPENGL_ES2_X11
    state->screen_width = window_width;
    state->screen_height = window_height;
    state->surface = eglCreateWindowSurface(state->display, egl_config[egl_chosen_config],
        X11GetWindow(), window_attribute_list);
#else
#ifdef OPENGL_ES2_MALI
    state->surface = eglCreateWindowSurface(state->display, egl_config[egl_chosen_config], &native_window, window_attribute_list);
//    result = eglSurfaceAttrib(state->display, state->surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED);
//    assert(result != GL_FALSE);
#endif
#ifdef OPENGL_ES2_RPI
   state->surface = eglCreateWindowSurface(state->display, egl_config[egl_chosen_config], &native_window, NULL);
#endif
#endif
   assert(state->surface != EGL_NO_SURFACE);
   check();

   // connect the context to the surface
   result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
   assert(EGL_FALSE != result);
   check();

   // Set background color and clear buffers
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);

   check();
}

void DeinitializeGUI() {
#ifndef OPENGL_ES2_X11
   mouse_event_queue->terminate();
#endif

   // clear screen
   glClear( GL_COLOR_BUFFER_BIT );
   eglSwapBuffers(state->display, state->surface);

   // Release OpenGL resources
   eglMakeCurrent( state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
   eglDestroySurface( state->display, state->surface );
   eglDestroyContext( state->display, state->context );
   eglTerminate( state->display );
}

void GLSwapBuffers();

void InitializeGUI(int *argc, char ***argv) {
    state = (EGL_STATE_T *)malloc(sizeof(EGL_STATE_T));
    // Clear application state
    memset(state, 0, sizeof(*state ));

    // Start OGLES
    init_ogl(state);
    window_width = state->screen_width;
    window_height = state->screen_height;
    printf("Opened OpenGL-ES2 state, width = %d, height = %d\n", window_width, window_height);

    sreInitialize(window_width, window_height, GLSwapBuffers);
//    sreSetLightScissors(SRE_SCISSORS_NONE);
//    sreSetShadowVolumeVisibilityTest(false);

#ifndef OPENGL_ES2_X11
    mouse_event_queue = new MouseEventQueue;
    mouse_event_queue->initialize();
    mouse_event_queue->setScreenSize(window_width, window_height);
    // Eat up preexisting mouse events.
    while (mouse_event_queue->isEventAvailable()) {
        MouseEvent event = mouse_event_queue->getEvent();
    }
    mouse_event_queue->setPosition(window_width / 2, window_height / 2);
#endif
}

void GLSwapBuffers() {
    eglSwapBuffers(state->display, state->surface); 
}

void GUIGLSync() {
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(state->display, state->surface);
    eglWaitClient();
}

static void Accelerate(double dt) {
	input_acceleration += horizontal_acceleration * dt;
}

static void Decelerate(double dt) {
	input_acceleration -= horizontal_acceleration * dt;
}

#ifndef OPENGL_ES2_X11

// Mouse interface for the Linux framebuffer console.

static void MotionEvent(MouseEvent *event) {
    float x = event->x;
    float y = event->y;
    view->thetaz -= (x - window_width / 2) * 360 * 0.5 / window_width;
    view->thetax += (y - window_height / 2) * 360 * 0.5 / window_height;
    if (view->thetaz < - 180)
        view->thetaz += 360;
    if (view->thetaz >= 180)
        view->thetaz -= 360;
    if (view->thetax < - 80)
        view->thetax = - 80;
    if (view->thetax > 10)
        view->thetax = 10;
    mouse_event_queue->setPosition(window_width / 2, window_height /2);
}

static double left_pressed_date = 0;
static double right_pressed_date = 0;

void ProcessGUIEvents(double dt) {
    while (mouse_event_queue->isEventAvailable()) {
        MouseEvent event = mouse_event_queue->getEvent();
        if (event.type == MouseEvent::Passive || event.type == MouseEvent::Move) {
            MotionEvent(&event);
        }
        if (event.type == MouseEvent::Press && event.button == MouseEvent::LeftButton) {
            left_pressed_date = event.date;
        }
        if (event.type == MouseEvent::Release && event.button == MouseEvent::LeftButton) {
            // If the left mouse button is released, accelerate
            if (left_pressed_date != 0) 
                Accelerate(event.date - left_pressed_date);
            left_pressed_date = 0;
        }
        if (event.type == MouseEvent::Press && event.button == MouseEvent::RightButton) {
            right_pressed_date = event.date;
        }
        if (event.type == MouseEvent::Release && event.button == MouseEvent::RightButton) {
            if (right_pressed_date != 0)
                Decelerate(event.date - right_pressed_date);
            right_pressed_date = 0;
        }
        if (event.type == MouseEvent::Press && event.button == MouseEvent::MiddleButton) {
            jump_requested = true;
        }
    }
    double current_date = GetCurrentTime();
    // If the left mouse button is still pressed, accelerate.
    if (left_pressed_date != 0) {
        Accelerate(current_date - left_pressed_date);
        left_pressed_date = current_date;
    }
    if (right_pressed_date != 0) {
        Decelerate(current_date - right_pressed_date);
        right_pressed_date = current_date;
    }
}

#endif

#ifdef OPENGL_ES2_X11

static bool full_screen = false;
static bool pan_with_mouse = false;
static bool accelerate_pressed = false;
static bool decelerate_pressed = false;
static bool menu_mode = false;

static void HandleMotion(int x, int y) {
    view->thetaz -= (x - window_width / 2) * 360 * 0.5 / window_width;
    view->thetax -= (y - window_height / 2) * 360 * 0.5 / window_width;
    // The horizontal field of view wraps around.
    if (view->thetaz < - 180)
        view->thetaz += 360;
    if (view->thetaz >= 180)
        view->thetaz -= 360;
    // Restrict the vertical field of view.
    if (view->thetax < - 80)
        view->thetax = - 80;
    if (view->thetax > 10)
        view->thetax = 10;
    WarpPointer();
}

void ProcessGUIEvents(double dt) {
    XEvent e;
    bool motion_occurred = false;
    int motion_x, motion_y;
    bool accelerate_pressed_previously = accelerate_pressed;
    bool decelerate_pressed_previously = decelerate_pressed;
    for (;;) {
        if (!XPending(XDisplay))
		break;
        XNextEvent(XDisplay, &e);
	if (e.type == KeyPress) {
            XKeyEvent *ke = (XKeyEvent *)&e;
            KeySym ks = XLookupKeysym(ke, 0);
            switch (ks) {
            case XK_Q :
            case XK_q :
                DeinitializeGUI();
                exit(0);
                break;
            case XK_F :
            case XK_f :
                glClear(GL_COLOR_BUFFER_BIT);
                eglSwapBuffers(state->display, state->surface);
                eglWaitClient();
                X11ToggleFullScreenMode(XDisplay, Xwindow, window_width,
                    window_height, pan_with_mouse);
                break;
            case XK_M :
            case XK_m :
                if (pan_with_mouse) {
                    X11RestoreCursor();
                    pan_with_mouse = false;
                }
                else {
                    X11WarpPointer();
                    X11HideCursor(XDisplay, XWindow);
                    pan_with_mouse = true;
                }
                break;
            case XK_KP_Add :
                view->zoom *= 1.0 / 1.1;
                sreApplyNewZoom(view);
                break;
            case XK_KP_Subtract :
                view->zoom *= 1.1;
                sreApplyNewZoom(view);
                break;
            case XK_A :
            case XK_a :
                accelerate_pressed = true;
                break;
            case XK_Z :
            case XK_z :
                decelerate_pressed = true;
                break;
            }
        if (!menu_mode)
        switch (ks) {
        case XK_F1 :
            menu_mode = true;
            text_message[0] = "Rendering engine settings:";
            text_message[1] = "";
            text_message[2] = "1 -- No shadows";
            text_message[3] = "2 -- Shadow volumes";
            text_message[4] = "3 -- Shadow mapping";
            text_message[5] = "4 -- Standard reflection model";
            text_message[6] = "5 -- Microfacet reflection model";
            text_message[7] = "6 -- Single-pass rendering (only one light)";
            text_message[8] = "7 -- Multi-pass rendering";
            text_message[9] = "s -- Enable scissors optimization (light only)";
            text_message[10] = "g -- Enable scissors optimization with geometry scissors";
            text_message[11] = "h -- Enable scissors optimization with matrix geometry scissors";
            text_message[12] = "d -- Disable scissors optimization";
            text_message[13]= "v/b - Enable/disable shadow volume visibility test";
            text_message[14] = "l/k -- Enable/disable light attenuation";
            text_message[15] = "8 -- Enable light object list rendering";
            text_message[16] = "9 -- Disable light object lists rendering";
            text_message[17] = "F2/F3 -- Disable/enable HDR rendering";
            text_message[18] = "F4 -- Cycle HDR tone mapping shader";
            text_message[19] = "";
            text_message[20] = "";
            nu_text_message_lines = 21;
            text_message_time = GetCurrentTime() + 1000000.0;
            break;
        }
        else
        switch (ks) {
        case XK_F1 :
            menu_mode = false;
            nu_text_message_lines = 1;
            text_message[0] = "";
            break;
        }
        // Menu settings.
        int line_number = 0;
        if (menu_mode)
            line_number = 20;
        bool menu_message = true;        
        switch (ks) {
        case XK_2 :
            sreSetShadowsMethod(SRE_SHADOWS_SHADOW_VOLUMES);
            text_message[line_number] = "Shadow volumes enabled";
            break;
        case XK_1 :
            sreSetShadowsMethod(SRE_SHADOWS_NONE);
            text_message[line_number] = "Shadows disabled";
            break;
        case XK_7 :
            sreEnableMultiPassRendering();
            text_message[line_number] = "Multi-pass rendering enabled";
            break;
        case XK_6 :
            sreDisableMultiPassRendering();
            text_message[line_number] = "Multi-pass rendering disabled";
            break;
        case XK_4 :
            sreSetReflectionModel(SRE_REFLECTION_MODEL_STANDARD);
            text_message[line_number] = "Standard reflection model selected";
            break;
        case XK_5 :
            sreSetReflectionModel(SRE_REFLECTION_MODEL_MICROFACET);
            text_message[line_number] = "Microfacet reflection model selected";
            break;
        case XK_3 :
            sreSetShadowsMethod(SRE_SHADOWS_SHADOW_MAPPING);
            text_message[line_number] = "Shadow mapping enabled";
            break;
        case XK_L :
        case XK_l:
            sreSetLightAttenuation(true);
            text_message[line_number] = "Light attenuation enabled";
            break;
        case XK_K :
        case XK_k :
            sreSetLightAttenuation(false);
            text_message[line_number] = "Light attenuation disabled";
            break;
        case XK_S :
        case XK_s :
            sreSetLightScissors(SRE_SCISSORS_LIGHT);
            text_message[line_number] = "Light scissors enabled";
            break;
        case XK_G :
        case XK_g :
            sreSetLightScissors(SRE_SCISSORS_GEOMETRY);
            text_message[line_number] = "Geometry scissors enabled";
            break;
        case XK_D :
        case XK_d :
            sreSetLightScissors(SRE_SCISSORS_NONE);
            text_message[line_number] = "Light/geometry scissors disabled";
            break;
        case XK_H :
        case XK_h :
            sreSetLightScissors(SRE_SCISSORS_GEOMETRY_MATRIX);
            text_message[line_number] = "Geometry matrix scissors enabled";
            break;
        case XK_V :
        case XK_v :
            sreSetShadowVolumeVisibilityTest(true);
            text_message[line_number] = "Shadow volume visibility test enabled";
            break;
        case XK_B :
        case XK_b :
            sreSetShadowVolumeVisibilityTest(false);
            text_message[line_number] = "Shadow volume visibility test disabled";
            break;
        case XK_8 :
            sreSetLightObjectLists(true);
            text_message[line_number] = "Light object list rendering enabled";
            break;
        case XK_9 :
            sreSetLightObjectLists(false);
            text_message[line_number] = "Light object list rendering disabled";
            break;
        case XK_F2 :
            sreSetHDRRendering(false);
            text_message[line_number] = "HDR rendering disabled";
            break;
        case XK_F3 :
            sreSetHDRRendering(true);
            text_message[line_number] = "HDR rendering enabled";
            break;
        case XK_F4 :
            sreSetHDRToneMappingShader((sreGetCurrentHDRToneMappingShader() + 1) % SRE_NUMBER_OF_TONE_MAPPING_SHADERS);
	    text_message[line_number] = "HDR tone mapping shader changed";
            break;
        default :
            menu_message = false;
        }
        if (menu_message)
            text_message_time = GetCurrentTime();
	}
        else if (e.type == KeyRelease) {
            XKeyEvent *ke = (XKeyEvent *)&e;
            KeySym ks = XLookupKeysym(ke, 0);
            switch (ks) {
            case XK_A :
            case XK_a :
                accelerate_pressed = false;
                break;
            case XK_Z :
            case XK_z :
                decelerate_pressed = false;
                break;
            }
        }
        else if (e.type == MotionNotify) {
            if (pan_with_mouse) {
                XMotionEvent *me = (XMotionEvent *)&e;
                motion_x = me->x;
                motion_y = me->y;
                motion_occurred = true;
            }
        }
        else if (e.type == ButtonPress) {
            XButtonPressedEvent *be = (XButtonPressedEvent *)&e;
            if (be->button == Button1) {
                jump_requested = true;
            }
        }
    }
    if (motion_occurred) {
        HandleMotion(motion_x, motion_y);
    }
    if (accelerate_pressed) {
        if (!accelerate_pressed_previously)
            // If the accelerate key was pressed during the last frame, assume it was held down
            // for 1 / 60th of second
            Accelerate((double)1 / 60);
        else
            // Accelerate key was held down continuously during last frame.
            Accelerate(dt);
    }
    if (decelerate_pressed) {
        if (!decelerate_pressed_previously)
            Decelerate((double)1 / 60);
        else
            Decelerate(dt);
    }
}

#endif

double GetCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
}

const char *GUIGetBackendName() {
#ifdef OPENGL_ES2_X11
     return "OpenGL-ES2.0 X11";
#endif
#ifdef OPENGL_ES2_A10
     return "OpenGL-ES2.0 framebuffer Allwinner A1x/A20 (Mali-400)";
#endif
#ifdef OPENGL_RPI
     return "OpenGL-ES2.0 framebuffer Raspberry Pi (Broadcom VideoCore)";
#endif
}
