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


#ifndef __GUI_COMMON_H__
#define __GUI_COMMON_H__

#define SRE_PRESS 0
#define SRE_RELEASE 1

#define SRE_MOUSE_BUTTON_UNKOWN
#define SRE_MOUSE_BUTTON_LEFT 1
#define SRE_MOUSE_BUTTON_MIDDLE 2
#define SRE_MOUSE_BUTTON_RIGHT 3

#define SRE_KEY_UNKOWN 0
#define SRE_KEY_F1 0x100
#define SRE_KEY_F2 0x101
#define SRE_KEY_F3 0x102
#define SRE_KEY_F4 0x103
#define SRE_KEY_F5 0x104
#define SRE_KEY_F6 0x105
#define SRE_KEY_F7 0x106
#define SRE_KEY_F8 0x107
#define SRE_KEY_F9 0x108
#define SRE_KEY_F10 0x109
#define SRE_KEY_F11 0x110
#define SRE_KEY_F12 0x111

#define SRE_KEY_ESC 0x120

#define SRE_TABLE_END_TOKEN 0xFFFFFFFF
#define SRE_KEY_MAPPING_RANGE_TOKEN 0x40000000
#define SRE_KEY_MAPPING_RANGE_WITH_OFFSET_MASK 0x80000000

#define SRE_TRANSLATION_TABLE_END SRE_TABLE_END_TOKEN, SRE_TABLE_END_TOKEN
// Translation table allows efficient encoding of ranges that map one-to-one for key codes <= 65535.
#define SRE_KEY_ONE_TO_ONE_MAPPING_RANGE(key0, key1) (unsigned int)key0 | ((unsigned int)key1 << 16), SRE_KEY_MAPPING_RANGE_TOKEN
// Translation table allows encoding of ranges that map one-to-one with offset for key codes <= 65535.
#define SRE_KEY_ONE_TO_ONE_MAPPING_RANGE_WITH_OFFSET(key0, key1, new_key0)  ((unsigned int)key0 | \
    ((unsigned int)key1 << 16)), (SRE_KEY_MAPPING_RANGE_WITH_OFFSET_MASK | (unsigned int)new_key0)

// Generic GUI functions.
void GUIMouseButtonCallback(int button, int state);
void GUIMouseButtonCallbackNoKeyboard(int button, int state);
void GUIProcessMouseMotion(int x, int y);
void GUIKeyPressCallback(unsigned int key);
void GUIKeyReleaseCallback(unsigned int key);
int GUITranslateKeycode(unsigned int platform_keycode, const unsigned int *table);
void GUIMovePlayer(double dt);
void GUITextMessageTimeoutCallback();

// GUI functions provided by the platform specific implementation.
void GUIToggleFullScreenMode(int& window_width, int& window_height, bool pan_with_mouse);
void GUIHideCursor();
void GUIRestoreCursor();
void GUIWarpCursor(int x, int y);
const char *GUIGetBackendName();

extern int window_width;
extern int window_height;

#endif
