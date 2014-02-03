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
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <errno.h>
#include <signal.h>

#include "MouseEventQueue.h"
#include "demo.h"
#include "gui-common.h"

static MouseEventQueue *mouse_event_queue;
int saved_kd_mode;

void LinuxFBRestoreConsoleState() {
    int tty = open("/dev/tty0", O_RDWR); 
    ioctl(tty, KDSETMODE, saved_kd_mode);
    close(tty);
}

struct sigaction signal_quit_oldact, signal_segv_oldact, signal_int_oldact;

void signal_quit(int num, siginfo_t *info, void *p) {
    LinuxFBRestoreConsoleState();
    if (signal_quit_oldact.sa_flags & SA_SIGINFO)
        signal_quit_oldact.sa_sigaction(num, info, p);
    else
        signal_quit_oldact.sa_handler(num);
}

void signal_segv(int num, siginfo_t *info, void *p) {
    LinuxFBRestoreConsoleState();
    if (signal_segv_oldact.sa_flags & SA_SIGINFO)
        signal_segv_oldact.sa_sigaction(num, info, p);
    else
        signal_segv_oldact.sa_handler(num);
}

void signal_int(int num, siginfo_t *info, void *p) {
    LinuxFBRestoreConsoleState();
    if (signal_int_oldact.sa_flags & SA_SIGINFO)
        signal_int_oldact.sa_sigaction(num, info, p);
    else
        signal_int_oldact.sa_handler(num);
}

void LinuxFBSetConsoleGraphics() {
    // Set console graphics mode and install some handlers to restore console textmode.
    int tty = open("/dev/tty0", O_RDWR);
    if (tty < 0)
        goto error_setting_graphics_mode;
    if (ioctl(tty, KDGETMODE, &saved_kd_mode) < 0)
        goto error_setting_graphics_mode;
    if (ioctl(tty, KDSETMODE, KD_GRAPHICS) < 0)
        goto error_setting_graphics_mode;
    close(tty);

    atexit(LinuxFBRestoreConsoleState);
    struct sigaction act;
    act.sa_sigaction = signal_quit;
    __sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGQUIT, &act, &signal_quit_oldact);
    act.sa_sigaction = signal_segv;
    sigaction(SIGSEGV, &act, &signal_segv_oldact);
    act.sa_sigaction = signal_int;
    sigaction(SIGINT, &act, &signal_int_oldact);
    return;

error_setting_graphics_mode :
    printf("Error setting console graphics mode.\n");
    exit(1);
}

void LinuxFBInitializeUI() {
    mouse_event_queue = new MouseEventQueue;
    mouse_event_queue->initialize();
    mouse_event_queue->setScreenSize(window_width, window_height);
    // Eat up preexisting mouse events.
    while (mouse_event_queue->isEventAvailable()) {
        MouseEvent event = mouse_event_queue->getEvent();
    }
    GUIWarpCursor(window_width / 2, window_height / 2);
}

void LinuxFBDeinitializeUI() {
   mouse_event_queue->terminate();
}

// Mouse interface for the Linux framebuffer console.

static const unsigned int linux_mouse_button_table[] = {
    MouseEvent::LeftButton, SRE_MOUSE_LEFT_BUTTON,
    MouseEvent::MiddleButton, SRE_MOUSE_MIDDLE_BUTTON,
    MouseEvent::RightButton, SRE_MOUSE_RIGHT_BUTTON,
    SRE_TRANSLATION_TABLE_END
};

static double left_pressed_date = 0;
static double right_pressed_date = 0;

void ProcessGUIEvents(double dt) {
    bool motion_occurred = false;
    int motion_x, motion_y;
    while (mouse_event_queue->isEventAvailable()) {
        MouseEvent event = mouse_event_queue->getEvent();
        if (event.type == MouseEvent::Passive || event.type == MouseEvent::Move) {
            motion_x = event.x;
            motion_y = event.y;
            motion_occurred = true;
        }
        else if (event.type == MouseEvent::Press) {
            unsigned int key=  GUITranslateKeycode(event.button, linux_mouse_button_table);
            if (key != 0)
                GUIMouseButtonCallbackNoKeyboard(button, SRE_PRESS);
        }
        else if (event.type == MouseEvent::Release) {
            unsigned int key =  GUITranslateKeycode(event.button, linux_mouse_button_table);
            if (key != 0)
                GUIMouseButtonCallbackNoKeyboard(button, SRE_RELEASE);
        }
    }
    if (motion_occurred)
        GUIProcessMouseMotion(motion_x, motion_y);
}

void GUIWarpCursor(int x, int y) {
    mouse_event_queue->setPosition(x, y);
}

// No-ops for functions not supported.

void GUIToggleFullScreenMode(int& window_width, int& window_height, bool pan_with_mouse) {
}

void GUIHideCursor() {
}

void GUIRestoreCursor() {
}

double GetCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
}
