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

// Common X11 layer for X11 targets like GLES2_X11 and GL_X11.
// Has proper full screen toggle support on modern systems.

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "sre.h"
#include "demo.h"
#include "x11-common.h"
#include "gui-common.h"

static Display *XDisplay = NULL;
static Window XWindow;
static Colormap window_cmap;

void X11OpenDisplay() {
   XDisplay = XOpenDisplay(NULL);
   if (!XDisplay) {
       printf("Error: failed to open X display.\n");
       exit(1);
   }
}

void *X11GetDisplay() {
    return XDisplay;
}

long unsigned int X11GetWindow() {
    return XWindow;
}

int X11GetScreenIndex() {
    return DefaultScreen(XDisplay);
}

void X11DestroyWindow() {
    XDestroyWindow(XDisplay, XWindow);
    XFreeColormap(XDisplay, window_cmap);
}

void X11CloseDisplay() {
    XCloseDisplay(XDisplay);
}

// If vi is NULL, a truecolor visual with 32-bit pixels will searched for.

void X11CreateWindow(int width, int height, XVisualInfo *vi, const char *title) {
    if (XDisplay == NULL)
        X11OpenDisplay(); 

    XVisualInfo vinfo;
    if (vi == NULL) {
        int r;
        r = XMatchVisualInfo(XDisplay, X11GetScreenIndex(), 32, TrueColor, &vinfo);
        if (r == 0) {
            printf("Error: Failed to find visual with depth of 32.\n");
            exit(1);
        }
    }
    else
        vinfo = *vi;

    Window XRoot = RootWindow(XDisplay, vinfo.screen);

    XSetWindowAttributes XWinAttr;
    XWinAttr.event_mask  =  ExposureMask | PointerMotionMask | KeyPressMask |
        KeyReleaseMask | ButtonPressMask | StructureNotifyMask;
    printf( "Creating colormap\n" );
    XWinAttr.colormap = window_cmap = XCreateColormap(XDisplay,
        XRoot, vinfo.visual, AllocNone);
    XWinAttr.background_pixmap = None;
    XWinAttr.border_pixel = 0;

    XWindow = XCreateWindow(XDisplay, XRoot, 0, 0, width, height, 0,
        vinfo.depth, InputOutput, vinfo.visual,
        CWEventMask | CWBorderPixel | CWColormap, &XWinAttr);

    if (XWindow == 0) {
        printf("Error: Failed to create X window.\n");
        exit(1);
    }

     Atom XWMDeleteMessage =
         XInternAtom(XDisplay, "WM_DELETE_WINDOW", False);
     XMapWindow(XDisplay, XWindow);
     XStoreName(XDisplay, XWindow, title);
     XSetWMProtocols(XDisplay, XWindow, &XWMDeleteMessage, 1);
}


void X11ToggleFullScreenMode(int& width, int& height, bool pan_with_mouse) {
    XEvent xev;
    Atom wm_state = XInternAtom(XDisplay, "_NET_WM_STATE", False);
    Atom fullscreen = XInternAtom(XDisplay, "_NET_WM_STATE_FULLSCREEN", False);

    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = XWindow;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 2; // _NET_WM_STATE_TOGGLE
    xev.xclient.data.l[1] = fullscreen;
    xev.xclient.data.l[2] = 0;
    XSendEvent(XDisplay, DefaultRootWindow(XDisplay), False,
        SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XSync(XDisplay, False);

    XEvent e;
    for (;;) {
        XNextEvent(XDisplay, &e);
        if (e.type == ConfigureNotify)
            break;
    }
    XConfigureEvent *ce = (XConfigureEvent *)&e;
    width = ce->width;
    height = ce->height;

    if (pan_with_mouse)
        GUIWarpCursor(width / 2, height / 2);

    sreResize(view, ce->width, ce->height);
}


// Translate X11´s keycodes into the internal format.
static const unsigned int X11_key_translation_table[] = {
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE_WITH_OFFSET(XK_a, XK_z, 'A'),
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE_WITH_OFFSET(XK_A, XK_Z, 'A'),
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE_WITH_OFFSET(XK_0, XK_9, '0'),
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE_WITH_OFFSET(XK_F1, XK_F10, SRE_KEY_F1),
    SRE_KEY_ONE_TO_ONE_MAPPING_RANGE_WITH_OFFSET(XK_F11, XK_F12, SRE_KEY_F11),
    XK_KP_Add, '+',
    XK_plus, '+',
    XK_KP_Subtract, '-',
    XK_minus, '-',
    XK_comma, ',',      // Comma
    XK_period, '.',      // Period
    XK_bracketleft, '[',
    XK_bracketright, ']',
    XK_space, ' ',      // Space
    XK_backslash, '\\',   // Backslash
    XK_slash, '/',      // Slash
    XK_equal, '=',
    XK_BackSpace, SRE_KEY_BACKSPACE,
    XK_Escape, SRE_KEY_ESC,
    SRE_TRANSLATION_TABLE_END
};

static const unsigned int X11_button_translation_table[] = {
    Button1, SRE_MOUSE_BUTTON_LEFT,
    Button2, SRE_MOUSE_BUTTON_MIDDLE,
    Button3, SRE_MOUSE_BUTTON_RIGHT,
    SRE_TRANSLATION_TABLE_END
};

static void X11GUIProcessEvents() {
    XEvent e;
    bool motion_occurred = false;
    int motion_x, motion_y;
    for (;;) {
        if (!XPending(XDisplay))
		break;
        XNextEvent(XDisplay, &e);
	if (e.type == KeyPress) {
            XKeyEvent *ke = (XKeyEvent *)&e;
            KeySym ks = XLookupKeysym(ke, 0);
            unsigned int key = GUITranslateKeycode(ks, X11_key_translation_table);
            if (key != 0)
                GUIKeyPressCallback(key);
        }
        else if (e.type == KeyRelease) {
            XKeyEvent *ke = (XKeyEvent *)&e;
            KeySym ks = XLookupKeysym(ke, 0);
            unsigned int key = GUITranslateKeycode(ks, X11_key_translation_table);
            if (key != 0)
                GUIKeyReleaseCallback(key);
        }
        else if (e.type == MotionNotify) {
            XMotionEvent *me = (XMotionEvent *)&e;
            motion_x = me->x;
            motion_y = me->y;
            motion_occurred = true;
//            printf("Motion %d, %d\n", motion_x, motion_y);
        }
        else if (e.type == ButtonPress) {
            XButtonPressedEvent *be = (XButtonPressedEvent *)&e;
            int button =  GUITranslateKeycode(be->button, X11_button_translation_table);
            GUIMouseButtonCallback(button, SRE_PRESS);
        }
    }
    if (motion_occurred)
        GUIProcessMouseMotion(motion_x, motion_y);
}

void GUIProcessEvents(double dt) {
    X11GUIProcessEvents();
    GUIMovePlayer(dt);
}

void GUIToggleFullScreenMode(int& window_width, int& window_height, bool pan_with_mouse) {
    X11ToggleFullScreenMode(window_width, window_height, pan_with_mouse);
}

void GUIHideCursor() {
    Cursor invisibleCursor;
    Pixmap bitmapNoData;
    XColor black;
    static char noData[] = { 0,0,0,0,0,0,0,0 };
    black.red = black.green = black.blue = 0;

    bitmapNoData = XCreateBitmapFromData(XDisplay, XWindow, noData, 8 ,8);
    invisibleCursor = XCreatePixmapCursor(XDisplay, bitmapNoData,
        bitmapNoData, &black, &black, 0, 0);
    XDefineCursor(XDisplay, XWindow, invisibleCursor);
    XFreeCursor(XDisplay, invisibleCursor);
                XFreePixmap(XDisplay, bitmapNoData);
}

void GUIRestoreCursor() {
    XUndefineCursor(XDisplay, XWindow);
}

void GUIWarpCursor(int x, int y) {
    XWarpPointer(
        XDisplay,
        XWindow,
        XWindow,
        0, 0, 0, 0, x, y);
    XFlush(XDisplay);
}

// The following function more or less assumes Linux is used.

double GUIGetCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
}

