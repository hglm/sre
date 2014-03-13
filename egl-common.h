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


// Functions provided by device-specific EGL back-end driver.

// The native display and window are treated as void *, which, although ugly,
// should be sufficient even with architectures with 64-bit pointers. Keeping
// the types generic make it easier to share code between back-ends and
// eventually allow multiple back-ends to be compiled in simultaneously.

void *EGLGetNativeDisplay();
void EGLInitializeSubsystemWindow(int requested_width, int requested_height,
    int& width, int &height, void *&window);
void EGLDeinitializeSubsystem();

// Functions defined in egl-common.cpp.

void EGLInitialize(int *argc, char ***argv, int window_width, int window_height);
void EGLFinalize();
void EGLSwapBuffers();
void EGLSync();
