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
#include <linux/vt.h>

#include "sre.h"
#include "sreBackend.h"
#include "MouseEventQueue.h"
#include "demo.h"
#include "gui-common.h"

static MouseEventQueue *mouse_event_queue;
static int saved_kd_mode;

void LinuxFBRestoreConsoleState() {
    fflush(stdout);
    fflush(stderr);
    int tty = open("/dev/tty0", O_RDWR); 
    // First check whether the console is already in the mode to
    // be restored (this function may be called multiple times due
    // to signals and atexit).
    int current_kd_mode;
    ioctl(tty, KDGETMODE, &current_kd_mode);
    if (current_kd_mode == saved_kd_mode) {
        close(tty);
        return;
    }
    ioctl(tty, KDSETMODE, saved_kd_mode);
    usleep(1000000);
    // Switch to another VT and back to restore the text content.
    struct vt_stat vtstat;
    ioctl(tty, VT_GETSTATE, &vtstat);
    int current_vt = vtstat.v_active;
    int temp_vt;
    if (current_vt == 1)
        temp_vt = 2;
    else
        temp_vt = 1;
    ioctl(tty, VT_ACTIVATE, temp_vt);
    ioctl(tty, VT_WAITACTIVE, temp_vt);
    ioctl(tty, VT_ACTIVATE, current_vt);
    ioctl(tty, VT_WAITACTIVE, current_vt);
//    fwrite("\033c\n", 1, 5, stdout);
    fflush(stdout);
    close(tty);
}

struct sigaction signal_quit_oldact, signal_segv_oldact, signal_int_oldact,
    signal_abort_oldact;

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

void signal_abort(int num, siginfo_t *info, void *p) {
    LinuxFBRestoreConsoleState();
    if (signal_abort_oldact.sa_flags & SA_SIGINFO)
        signal_abort_oldact.sa_sigaction(num, info, p);
    else
        signal_abort_oldact.sa_handler(num);
}

void LinuxFBSetConsoleGraphics() {
    fflush(stdout);
    fflush(stderr);
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
    // SIGABRT is raised by assert().
    act.sa_sigaction = signal_abort;
    sigaction(SIGABRT, &act, &signal_abort_oldact);
    return;

error_setting_graphics_mode :
    printf("Error setting console graphics mode.\n");
    exit(1);
}

void LinuxFBWarpCursor(int x, int y) {
    mouse_event_queue->setPosition(x, y);
}

void LinuxFBInitializeUI(int width, int height) {
    mouse_event_queue = new MouseEventQueue;
    mouse_event_queue->initialize();
    mouse_event_queue->setScreenSize(width, height);
    // Eat up preexisting mouse events.
    while (mouse_event_queue->isEventAvailable()) {
        MouseEvent event = mouse_event_queue->getEvent();
    }
    LinuxFBWarpCursor(width / 2, height / 2);
}

void LinuxFBDeinitializeUI() {
   mouse_event_queue->terminate();
}

// Mouse interface for the Linux framebuffer console.

static const unsigned int linux_mouse_button_table[] = {
    MouseEvent::LeftButton, SRE_MOUSE_BUTTON_LEFT,
    MouseEvent::MiddleButton, SRE_MOUSE_BUTTON_MIDDLE,
    MouseEvent::RightButton, SRE_MOUSE_BUTTON_RIGHT,
    SRE_TRANSLATION_TABLE_END
};

static double left_pressed_date = 0;
static double right_pressed_date = 0;

void LinuxFBProcessGUIEvents() {
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
            unsigned int button = GUITranslateKeycode(event.button, linux_mouse_button_table);
            if (button != 0)
                GUIMouseButtonCallbackNoKeyboard(button, SRE_PRESS);
        }
        else if (event.type == MouseEvent::Release) {
            unsigned int button = GUITranslateKeycode(event.button, linux_mouse_button_table);
            if (button != 0)
                GUIMouseButtonCallbackNoKeyboard(button, SRE_RELEASE);
        }
    }
    if (motion_occurred)
        GUIProcessMouseMotion(motion_x, motion_y);
}

double LinuxFBGetCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
}
