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

/*
    win32-compat.h -- header file with definitions for compatability with Windows.

*/

// Note: Just using the built-in maximum and minimum values on all system may be safer/
// give more consistent results than using 'INFINITY', so use of the latter is disabled.

// #if !defined(__GNUC__ ) || defined(OPENGL_ES2)

#if 1

#define POSITIVE_INFINITY_DOUBLE DBL_MAX
#define NEGATIVE_INFINITY_DOUBLE DBL_MIN
#define POSITIVE_INFINITY_FLOAT FLT_MAX
#define NEGATIVE_INFINITY_FLOAT FLT_MIN

#else

#define POSITIVE_INFINITY_DOUBLE INFINITY
#define NEGATIVE_INFINITY_DOUBLE (- INFINITY)
#define POSITIVE_INFINITY_FLOAT INFINITY
#define NEGATIVE_INFINITY_FLOAT (- INFINITY)

#endif

#ifndef __GNUC__
#define isnan(x) _isnan(x)
#endif

