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

// This header file defines inline SIMD accelerator vector processing
// primitives using compiler intrinsics. The functions are only included
// if SIMD capability is detected and NO_SIMD is not defined.
//
// A common set of SIMD primitives for 128-bit SIMD vector registers, shared between
// platforms, is defined with certain features depending on the platform. Most
// primitives map to a single SIMD instruction, while others are emulated using
// multiple instructions, depending on on the platform.

// Proving these functions inline has significant benefits; they can be
// integrated into any pure or mixed SIMD processing function, which will
// enable the compiler to automatically mix/shuffle the code for advanced
// scheduling and register allocation optimizations.
//
// Data types:
//
// __simd128_float:  128-bit vector register with four 32-bit floats.
// __simd128_double: 128-bit vector register with two 64-bit double precision floats.
// __simd128_int:    128-bit vector register holding integer data.
//
// For most purposes and hardware implementations, register type are interchangable
// using provided cast functions which do not generate any instructions.
//
// The function names have the following form:
//
// simd128_<operator description>_<element_data_type>(operands)
//
// where <element_data_type> determines the size of the elements operated on and
// is one of the following:
//
// float:    32-bit float elements (four per register).
// double:   64-bit double elements (two per register).
// int32:    32-bit signed integer elements (four per register).
// uint32:   32-bit unsigned integer elements (four per register). When signedness
//           does not matter, only a primitive ending with _int32 is provided.
// int:      Generic integer data type, provided for integer primitives for which
//           the element data type does not matter.
//
// Examples:
//
// Declare three SIMD float register variables:
//     __simd128_float fv1, fv2, fv3;
// Add every corresponding component of fv1 and fv2, and store the result in fv3:
//     fv3 = simd128_add_float(fv1, fv2);
// Multiply components:
//     fv3 = simd128_mul_float(fv1, fv2);
// Load a float factor from 16-byte aligned memory:
//     fv1 = simd128_load_float(mem_pointer);
// Shift right by 1 float (so that fourth element becomes undefined, and every other
// element is moved one slot lower):
//     fv2 = simd128_shift_right_float(fv1);
// An identical shift could also be achieved using integer primitives and casts:
//     fv2 = simd128_cast_int_float(
//         simd128_shift_right_bytes_int(simd128_cast_float_int(fv1), 4)
//         );
// Load a constant into the first element, invalidating the other elements (bits cleared).
//     fv2 = simd128_set_first_and_clear_float(1.0f);
// Load the same constant into all four elements:
//     fv2 = simd128_set_same_float(1.0f);
// Set all four elements:
//     fv2 = simd128_set_float(1.0f, 2.0f, 3.0f, 4.0f);
// Note that the function definitions always assume the first argument is the lowest
// order element (this is opposite of the argument order used in some actual SSE
// intrinsic functions).
// Reverse the element order:
//     fv2 = simd128_select_float(fv1, 3, 2, 1, 0);
//     __simd128_int iv1, iv2, iv3;
// Add 32-bit integer components:
//     iv3 = simd128_add_int32(iv1, iv2);
//     __simd128_double dv1;
// Convert two floats in the first two elements of fv1 to two doubles:
//     dv1 = simd128_convert_float_double(fv1);

// Compile-time definitions:
//
// NO_SIMD disables automatic detection of SIMD capabilities. SIMD will still be
// used when USE_SSE2, USE_SSE3, USE_ARM_NEON etc is explicitly defined.
// USE_SSE2 forces the use of SSE2 SIMD extensions.
// USE_SSE3 forces the use of SSE2 and SSE3 SIMD extensions.
// USE_ARM_NEON forces the use of ARM NEON SIMD extensions.
//
// USE_SIMD will be defined when SIMD functions are available. This should be used
// for testing the availability of SIMD primitives.

#ifndef __SRE_SIMD_PLATFORM_H__
#define __SRE_SIMD_PLATFORM_H__

#if defined(_MSC_VEC)
/* Microsoft C/C++-compatible compiler */
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
/* GCC-compatible compiler, targeting x86/x86-64 */
#include <x86intrin.h>
#elif defined(__GNUC__) && defined(__ARM_NEON__)
/* GCC-compatible compiler, targeting ARM with NEON */
#include <arm_neon.h>
#elif defined(__GNUC__) && defined(__IWMMXT__)
/* GCC-compatible compiler, targeting ARM with WMMX */
#include <mmintrin.h>
#endif

// USE_SSE2, USE_SSE3 etc. are added as build flags in the Makefile.
// However, when SSE2 or SSE3 is a default platform feature (such as
// SSE2 on x86_64), always use it, unless all SIMD support is explicitly
// disabled.

// USE_SSE3, when specified, implies USE_SSE2.
#if defined(USE_SSE3) && !defined(USE_SSE2)
#define USE_SSE2
#endif

#ifndef NO_SIMD
#if !defined(USE_SSE2) && defined(__SSE2__)
#define USE_SSE2
#endif
#if !defined(USE_SSE3) && defined(__SSE3__)
#define USE_SSE3
#endif
#if !defined(USE_ARM_NEON) && defined (__ARM_NEON)
#define USE_ARM_NEON
#endif
#endif

// Set SIMD features that are implemented.

#ifdef USE_SSE2
// SSE3 is a superset of SSE2.
#ifdef USE_SSE3
// Horizontal pair-wise addition instruction.
#define SIMD_HAVE_HORIZONTAL_ADD2
#endif
#define SIMD_HAVE_TRANSPOSE_4TO3
#define SIMD_HAVE_TRANSPOSE_3TO4
#define USE_SIMD
#else // Not SSE2
#ifdef USE_ARM_NEON
// When implemented, define USE_SIMD and define feature flags.
#else
// No SIMD.
#endif
#endif

// SIMD utility features that depend on lower-level features.

#if defined(SIMD_HAVE_TRANSPOSE_4TO3) && defined(SIMD_HAVE_TRANSPOSE_3TO4)
#define SIMD_HAVE_MATRIX4X3_VECTOR_MULTIPLICATION
#endif

// Define a macro for always-inlining attributes that work with recent versions
// of gcc.

#ifdef __GNUC__
#define inline_only __attribute__((always_inline)) inline
#else
#define inline_only inline
#endif

// Define a unifying set of SIMD intrinsic functions.

#ifdef USE_SSE2

// PC class processor with SSE2 enabled. Optionally use additional features
// such as SSE3.

typedef __m128 __simd128_float;
typedef __m128i __simd128_int;
typedef __m128d __simd128_double;

#define SHUFFLE(w0, w1, w2, w3) \
    (w0 | (w1 << 2) | (w2 << 4) | (w3 << 6))

static inline_only __simd128_int simd128_cast_float_int(__simd128_float s) {
    return _mm_castps_si128(s);
}

static inline_only __simd128_float simd128_cast_int_float(__simd128_int s) {
    return _mm_castsi128_ps(s);
}

// Selection/merge operators. Generally usable based on the element data size in bits,
// independent of the element data type, using casting functions (which don't generate
// any instructions) to change the data type for arguments/results.

static inline_only __simd128_int simd128_select_int32(__simd128_int s1,
unsigned int word0, unsigned int word1, unsigned int word2, unsigned int word3) {
    return _mm_shuffle_epi32(s1, SHUFFLE(word0, word1, word2, word3));
}

static inline_only __simd128_float simd128_select_float(__simd128_float s1,
unsigned int word0, unsigned int word1, unsigned int word2, unsigned int word3) {
    return simd128_cast_int_float(simd128_select_int32(simd128_cast_float_int(s1),
        word0, word1, word2, word3));
}

// Return 128-bit SSE register with 32-bit values shuffled, starting from bit 0,
// with two words from m1 (counting from LSB) and two words from m2.

static inline_only __simd128_float simd128_merge_float(__simd128_float s1, __simd128_float s2,
unsigned int s1_word0, unsigned int s1_word1, unsigned int s2_word0,
unsigned int s2_word1) {
    return _mm_shuffle_ps(s1, s2, SHUFFLE(s1_word0, s1_word1, s2_word0, s2_word1));
}

static inline_only __simd128_int simd128_merge_int32(__simd128_int s1, __simd128_int s2,
unsigned int s1_word0, unsigned int s1_word1, unsigned int s2_word0,
unsigned int s2_word1) {
    return simd128_cast_float_int(simd128_merge_float(
        simd128_cast_int_float(s1), simd128_cast_int_float(s2),
        s1_word0, s1_word1, s2_word0, s2_word1));
}

// Merge and interleave word 0 of s1 and s2 in the lower, and word 1 in the upper half
// of the return value. The upper half of s1 and s2 is ignored.

static inline_only __simd128_float simd128_interleave_low_float(__simd128_float s1,
__simd128_float s2) {
    return _mm_unpacklo_ps(s1, s2);
}

// Merge and interleave word 2 of s1 and s2 in the lower, and word 3 in the upper half
// of the return value. The lower half of s1 and s2 is ignored.

static inline_only __simd128_float simd128_interleave_high_float(__simd128_float s1,
__simd128_float s2) {
    return _mm_unpackhi_ps(s1, s2);
}

// Return 128-bit SSE register with the lowest order 32-bit float from s1 and the
// remaining 32-bit floats from s2.

static inline_only __simd128_float simd128_merge1_float(__simd128_float s1,
__simd128_float s2) {
    return _mm_move_ss(s2, s1);
}

// float operators.

// Set all components to the same value.

static inline_only __simd128_float simd128_set_same_float(float f) {
    return _mm_set1_ps(f);
}

// Set four float components (from least significant, lowest order segment to highest).

static inline_only __simd128_float simd128_set_float(float f0, float f1, float f2, float f3) {
    return _mm_set_ps(f3, f2, f1, f0);
}

// Set only the first component, zeroing (bits) the other components.

static inline_only __simd128_float simd128_set_first_and_clear_float(float f) {
    return _mm_set_ss(f);
}

static inline_only __simd128_float simd128_set_zero_float() {
    return _mm_setzero_ps();
}

static inline_only float simd128_get_float(__simd128_float s) {
    return _mm_cvtss_f32(s);
}

// Load 16-byte-aligned float data.

static inline_only __simd128_float simd128_load_float(const float *fp) {
    return _mm_load_ps(fp);
}

// Store 16-byte-aligned float data.

static inline_only void simd128_store_float(float *fp, __simd128_float s) {
    _mm_store_ps(fp, s);
}

// Load one float into the lowest-order element. The other elements are filled with
// zero bits (not 0.0f float values).

static inline_only __simd128_float simd128_load_first_float(const float *fp) {
    return _mm_load_ps(fp);
}

static inline_only void simd128_store_first_float(float *fp, __simd128_float s) {
    _mm_store_ss(fp, s);
}

static inline_only __simd128_float simd128_mul_float(__simd128_float s1, __simd128_float s2) {
    return _mm_mul_ps(s1, s2);
}

static inline_only __simd128_float simd128_div_float(__simd128_float s1, __simd128_float s2) {
    return _mm_div_ps(s1, s2);
}

static inline_only __simd128_float simd128_add_float(__simd128_float s1, __simd128_float s2) {
    return _mm_add_ps(s1, s2);
}

static inline_only __simd128_float simd128_add1_float(__simd128_float s1, __simd128_float s2) {
    return _mm_add_ss(s1, s2);
}

static inline_only __simd128_float simd128_sub_float(__simd128_float s1, __simd128_float s2) {
    return _mm_sub_ps(s1, s2);
}

static inline_only __simd128_float simd128_sub1_float(__simd128_float s1, __simd128_float s2) {
    return _mm_sub_ss(s1, s2);
}

// Approximate reciprocal (1.0f / element[i]) with maximum relative error
// less than 1.5*2^-12.

static inline_only __simd128_float simd128_approximate_reciprocal_float(__simd128_float s) {
    return _mm_rcp_ps(s);
}

// Approximate reciprocal square root (1.0f / sqrtf(element[i])) with maximum relative
// error less than 1.5*2^-12.

static inline_only __simd128_float simd128_approximate_reciprocal_sqrt_float(__simd128_float s) {
    return _mm_rsqrt_ps(s);
}

// Calculate square root (accurately) of four floats.

static inline_only __simd128_float simd128_sqrt_float(__simd128_float s) {
    return _mm_sqrt_ps(s);
}

// int32 operators.

static inline_only __simd128_int simd128_set_int32(int i0, int i1, int i2, int i3) {
    return _mm_set_epi32(i3, i2, i1, i0);
}

static inline_only __simd128_int simd128_set_same_int32(int i) {
    return _mm_set1_epi32(i);
}

static inline_only int simd128_get_int32(__simd128_int s) {
    return _mm_cvtsi128_si32(s);
}

// int32 math operators.

static inline_only __simd128_int simd128_add_int32(__simd128_int s1, __simd128_int s2) {
    return _mm_add_epi32(s1, s2);
}

static inline_only __simd128_int simd128_sub_int32(__simd128_int s1, __simd128_int s2) {
    return _mm_sub_epi32(s1, s2);
}

// Multiply unpacked unsigned integers of s1 and s2 (in words 0 and 2, words 1 and 3
// are ignored) and store the result as two 64-bit unsigned integers.

static inline_only __simd128_int simd128_mul_unpacked_uint32_uint64(
__simd128_int s1, __simd128_int s2) {
    return _mm_mul_epu32(s1, s2);
}

// Multiply unsigned integers in lower half (words 0 and 1) of s1 and s2 and store the
// result as two 64-bit unsigned integers.

static inline_only __simd128_int simd128_mul_uint32_uint64(
__simd128_int s1, __simd128_int s2) {
    return _mm_mul_epu32(
        simd128_select_int32(s1, 0, 0, 1, 1),
        simd128_select_int32(s2, 0, 0, 1, 1));
}

// int64 operators.

static inline_only __simd128_int simd128_set_int64(int64_t i0, int64_t i1) {
    return _mm_set_epi64x(i1, i0);
}

static inline_only __simd128_int simd128_set_same_int64(int64_t i) {
    return _mm_set1_epi64x(i);
}

static inline_only int64_t simd128_get_int64(__simd128_int s) {
    return _mm_cvtsi128_si64(s);
}

// General integer operators.

static inline_only __simd128_int simd128_set_zero_int() {
    return _mm_setzero_si128();
}

// Load 16-byte-aligned integer data.

static inline_only __simd128_int simd128_load_int(const int *ip) {
    return _mm_load_si128((const __simd128_int *)ip);
}

// Store 16-byte-aligned integer data.

static inline_only void simd128_store_int(int *ip, __simd128_int s) {
    _mm_store_si128((__simd128_int *)ip, s);
}

// Integer logical operators.

static inline_only __simd128_int simd128_and_int(__simd128_int s1, __simd128_int s2) {
    return _mm_and_si128(s1, s2);
}

static inline_only __simd128_int simd128_andnot_int(__simd128_int s1, __simd128_int s2) {
    return _mm_andnot_si128(s1, s2);
}

static inline_only __simd128_int simd128_or_int(__simd128_int s1, __simd128_int s2) {
    return _mm_or_si128(s1, s2);
}

static inline_only __simd128_int simd128_not_int(__simd128_int s) {
    __simd128_int m_full_mask = simd128_set_same_int32(0xFFFFFFFF);
    return _mm_xor_si128(s, m_full_mask);
}

// Comparison functions. Returns integer vector with full bitmask set for each
// element for which the condition is true.

// float

static inline_only __simd128_int simd128_cmpge_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmpge_ps(s1, s2));
}

static inline_only __simd128_int simd128_cmpgt_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmpgt_ps(s1, s2));
}

static inline_only __simd128_int simd128_cmple_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmple_ps(s1, s2));
}

static inline_only __simd128_int simd128_cmplt_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmplt_ps(s1, s2));
}

static inline_only __simd128_int simd128_cmpeq_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmpeq_ps(s1, s2));
}

static inline_only __simd128_int simd128_cmpne_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmpneq_ps(s1, s2));
}

// int32

static inline_only __simd128_int simd128_cmpge_int32(__simd128_int s1, __simd128_int s2) {
    return simd128_not_int(_mm_cmplt_epi32(s1, s2));
}

static inline_only __simd128_int simd128_cmpgt_int32(__simd128_int s1, __simd128_int s2) {
    return _mm_cmpgt_epi32(s1, s2);
}

static inline_only __simd128_int simd128_cmple_int32(__simd128_int s1, __simd128_int s2) {
    return simd128_not_int(_mm_cmpgt_epi32(s1, s2));
}

static inline_only __simd128_int simd128_cmplt_int32(__simd128_int s1, __simd128_int s2) {
    return _mm_cmplt_epi32(s1, s2);
}

static inline_only __simd128_int simd128_cmpeq_int32(__simd128_int s1, __simd128_int s2) {
    return _mm_cmpeq_epi32(s1, s2);
}

static inline_only __simd128_int simd128_cmpneq_int32(__simd128_int s1, __simd128_int s2) {
    return simd128_not_int(_mm_cmpeq_epi32(s1, s2));
}

// Convert the lower two float elements to doubles occupying the lower and upper half
// of the return value.

static inline_only __simd128_double simd128_convert_float_double(__simd128_float s) {
    return _mm_cvtps_pd(s);
}

// Convert the two doubles to two floats occupying the lower half of the return value.

static inline_only __simd128_float simd128_convert_double_float(__simd128_double s) {
    return _mm_cvtpd_ps(s);
}

// Convert 32-bit signed integers to floats.

static inline_only __simd128_float simd128_convert_int32_float(__simd128_int s) {
    return _mm_cvtepi32_ps(s);
}

// Convert 32-bit signed integers to doubles.

static inline_only __simd128_double simd128_convert_int32_double(__simd128_int s) {
    return _mm_cvtepi32_pd(s);
}

// Convert 32-bit floats to 32-bit signed integers.

static inline_only __simd128_int simd128_convert_float_int32(__simd128_float s) {
    return _mm_cvtps_epi32(s);
}

// Convert 32-bit floats to 32-bit signed integers with truncation.

static inline_only __simd128_int simd128_convert_float_int32_truncate(__simd128_float s) {
    return _mm_cvttps_epi32(s);
}

// Convert 64-bit doubles to 32-bit signed integers.

static inline_only __simd128_int simd128_convert_double_int32_truncate(__simd128_double s) {
    return _mm_cvtpd_epi32(s);
}

// Convert four 32-bit signed integers to four 8-bit signed integers using signed
// saturation. zerosi must be initialized as an integer vector with zeros.

static inline_only __simd128_int simd128_convert_int32_int8_saturate(__simd128_int s,
__simd128_int zerosi) {
    return _mm_packs_epi16(_mm_packs_epi32(s, zerosi), zerosi);
}

// Convert four 32-bit integer masks (each either 0xFFFFFFFF or 0x00000000) to 1-bit
// masks using the highest-order bit of each 32-bit value.

static inline_only int simd128_convert_masks_int32_int1(__simd128_int s) {
     return _mm_movemask_ps(simd128_cast_int_float(s));
}

// Shift float register right by number of 32-bit floats, shifting in zero bits
// (i.e. invalid floating point values) at the high end. n must be constant.

static inline_only __simd128_float simd128_shift_right_float(__simd128_float s, const int n) {
    return _mm_castsi128_ps(
        _mm_srli_si128(_mm_castps_si128(s), n * 4));
}

// Shift 128-bit integer right by n * 8 bits (n bytes).

static inline_only __simd128_int simd128_shift_right_bytes_int(__simd128_int s, const int n) {
    return _mm_srli_si128(s, n);
}

// Shift 32-bit unsigned integers right by n bits.

static inline_only __simd128_int simd128_shift_right_uint32(__simd128_int s, const int n) {
    return _mm_srli_epi32(s, n);
}

// Shift 16-bit unsigned integers right by n bits.

static inline_only __simd128_int simd128_shift_right_uint16(__simd128_int s, const int n) {
    return _mm_srli_epi16(s, n);
}

// Shift 32-bit signed integers right left by n bits.

static inline_only __simd128_int simd128_shift_right_int32(__simd128_int s, const int n) {
    return _mm_srai_epi32(s, n);
}

// Shift float register left by number of 32-bit floats, shifting in zero bits
// (i.e. invalid floating point values). n must be constant.

static inline_only __simd128_float simd128_shift_left_float(__simd128_float s, const int n) {
    return _mm_castsi128_ps(
        _mm_slli_si128(_mm_castps_si128(s), n * 4));
}

// Shift 128-bit integer left by n * 8 bits (n bytes).

static inline_only __simd128_int simd128_shift_left_bytes_int(__simd128_int s, const int n) {
    return _mm_slli_si128(s, n);
}

// Shift 32-bit signed/unsigned integers left by n bits.

static inline_only __simd128_int simd128_shift_left_int32(__simd128_int s, const int n) {
    return _mm_slli_epi32(s, n);
}

// Shift 16-bit signed/unsigned integers left by n bits.

static inline_only __simd128_int simd128_shift_left_int16(__simd128_int s, const int n) {
    return _mm_slli_epi16(s, n);
}

// Calculate the mimimum of each component of two vectors.

static inline_only __simd128_float simd128_min_float(__simd128_float s1, __simd128_float s2) {
     return _mm_min_ps(s1, s2);
}

// Calculate the maximum of each component of two vectors.

static inline_only __simd128_float simd128_max_float(__simd128_float s1, __simd128_float s2) {
     return _mm_max_ps(s1, s2);
}

// Calculate the mimimum of the first component of two vectors.

static inline_only __simd128_float simd128_min1_float(__simd128_float s1, __simd128_float s2) {
     return _mm_min_ss(s1, s2);
}

// Calculate the maximum of the first component of two vectors.

static inline_only __simd128_float simd128_max1_float(__simd128_float s1, __simd128_float s2) {
     return _mm_max_ss(s1, s2);
}

#ifdef USE_SSE3

// Horizontally add pairs of elements over two four-float vectors and return
// in a single vector. Generally inefficient even when available as a single instruction
// (SSE3).

static inline_only __simd128_float simd128_horizontal_add2_float(__simd128_float s1,
__simd128_float s2) {
    return _mm_hadd_ps(s1, s2);
}

#endif

// Set only the last component, preserving the other components.

static inline_only __simd128_float simd128_set_last_float(__simd128_float s, float f) {
    __simd128_float m_f = simd128_cast_int_float(
        simd128_shift_left_bytes_int(
            simd128_cast_float_int(simd128_set_first_and_clear_float(f)),
            12)
        );
    __simd128_int m_mask_012x = simd128_shift_right_bytes_int(
        simd128_set_same_int32(0xFFFFFFFF), 4);
    return simd128_cast_int_float(simd128_or_int(
        simd128_and_int(m_mask_012x, simd128_cast_float_int(s)),
        simd128_andnot_int(m_mask_012x, simd128_cast_float_int(m_f))
        ));
}

static inline_only __simd128_float simd128_set_last_zero_float(__simd128_float s) {
    __simd128_float m_f = simd128_set_zero_float();
    __simd128_int m_mask_012x = simd128_shift_right_bytes_int(
        simd128_set_same_int32(0xFFFFFFFF), 4);
    return simd128_cast_int_float(simd128_or_int(
        simd128_and_int(m_mask_012x, simd128_cast_float_int(s)),
        simd128_andnot_int(m_mask_012x, simd128_cast_float_int(m_f))
        ));
}

// Load three floats without accessing memory beyond them.

static inline_only __simd128_float simd128_load3_float(const float *f) {
    // Take care not to read beyond the allocated data structure.
    __simd128_float m_zeros = simd128_set_zero_float();
    // temp1: Load v.x and v.y into the lower two words (with 0.0f for the rest),
    // temp2: Load v.z into the low word (with 0.0f for the rest).
    // Merge temp1 and temp2 so that (v.x, v.y, v.z, 0.0f) is returned.
    return simd128_merge_float(
        _mm_loadl_pi(m_zeros, (const __m64 *)f),
        simd128_load_first_float(&f[2]),
        0, 1, 0, 1
        );

}

// Store three floats without accessing memory beyond the stored elements.

static inline_only void simd128_store3_float(float *f, __simd128_float m_v) {
    // Take care not to write beyond the allocated data structure.
    _mm_storel_pi((__m64 *)f, m_v);
    simd128_store_first_float(&f[2], simd128_shift_right_float(m_v, 2));
}

// Symmetrically transpose a 4x4 matrix.

// In place.

static inline_only void simd128_transpose4_float(__simd128_float& row0,
__simd128_float& row1, __simd128_float& row2, __simd128_float& row3) {
   _MM_TRANSPOSE4_PS(row0, row1, row2, row3);
}

// Put result in different registers.

static inline_only void simd128_transpose4to4_float(const __simd128_float row0,
const __simd128_float& row1, const __simd128_float& row2, const __simd128_float row3,
__simd128_float& m_result_v0, __simd128_float& m_result_v1, __simd128_float& m_result_v2,
__simd128_float& m_result_v3) {
   m_result_v0 = row0;
   m_result_v1 = row1;
   m_result_v2 = row2;
   m_result_v3 = row3;
   _MM_TRANSPOSE4_PS(m_result_v0, m_result_v1, m_result_v2, m_result_v3);
}

// Transpose four three-float vectors, and store result in three four-float vectors.

static inline_only void simd128_transpose4to3_float(const __simd128_float m_v0,
const __simd128_float m_v1, const __simd128_float m_v2, const __simd128_float m_v3,
__simd128_float& m_result_x, __simd128_float& m_result_y, __simd128_float& m_result_z) {
    __simd128_float m_tmp0, m_tmp1, m_tmp2, m_tmp3;
    // tmp0 = (v0.x, v1.x, v0.y, v1.y)
    m_tmp0 = _mm_unpacklo_ps(m_v0, m_v1);
    // tmp1 = (v2.x, v2.x, v3.y, v3.y)
    m_tmp1 = _mm_unpacklo_ps(m_v2, m_v3);
    // tmp2 = (v0.z, v1.z, v0.w, v1.w)
    m_tmp2 = _mm_unpackhi_ps(m_v0, m_v1);
    // tmp3 = (v2.z, v2.z, v3.w, v3.w)
    m_tmp3 = _mm_unpackhi_ps(m_v2, m_v3);
    m_result_x = _mm_movelh_ps(m_tmp0, m_tmp1);
    m_result_y = _mm_movehl_ps(m_tmp1, m_tmp0);
    m_result_z = _mm_movelh_ps(m_tmp2, m_tmp3);
}

// Transpose three four-float vectors, and store result in four three-float vectors.

static inline_only void simd128_transpose3to4_float(const __simd128_float m_v0,
const __simd128_float m_v1, const __simd128_float m_v2,
__simd128_float& m_result_v0, __simd128_float& m_result_v1, __simd128_float& m_result_v2,
__simd128_float& m_result_v3) {
    __simd128_float m_tmp0, m_tmp1, m_tmp2, m_tmp3;
    __simd128_float m_zeros = simd128_set_zero_float();
    // tmp0 = (v0[0], v1[0], v0[1], v1[1])
    m_tmp0 = _mm_unpacklo_ps(m_v0, m_v1);
    // tmp1 = (v2[0], 0.0f, v2[1], 0.0f)
    m_tmp1 = _mm_unpacklo_ps(m_v2, m_zeros);
    // tmp2 = (v0[2], v1[2], v0[3], v1[3])
    m_tmp2 = _mm_unpackhi_ps(m_v0, m_v1);
    // tmp3 = (v2[2], 0.0f, v2[3], 0.0f)
    m_tmp3 = _mm_unpackhi_ps(m_v2, m_zeros);
    // result_v0 = (v0[0], v1[0], v2[0], 0.0f)
    m_result_v0 = _mm_movelh_ps(m_tmp0, m_tmp1);
    // result_v1 = (v0[1], v1[1], v2[1], 0.0f);
    m_result_v1 = _mm_movehl_ps(m_tmp1, m_tmp0);
    // result_v2 = (v0[2], v1[2], v2[2], 0.0f);
    m_result_v2 = _mm_movelh_ps(m_tmp2, m_tmp3);
    // result_v3 = (v0[3], v1[3], v2[3], 0.0f);
    m_result_v3 = _mm_movehl_ps(m_tmp3, m_tmp2);
}

// Double precision functions.

static inline_only __simd128_int simd128_cast_double_int(__simd128_double s) {
    return _mm_castpd_si128(s);
}

static inline_only __simd128_double simd128_cast_int_double(__simd128_int s) {
    return _mm_castsi128_pd(s);
}

static inline_only __simd128_double simd128_set_double(double d0, double d1) {
    return _mm_set_pd(d1, d0);
}

static inline_only __simd128_double simd128_set_same_double(double d) {
    return _mm_set1_pd(d);
}

// Set lower order double, and clear (zero bits) upper half of return value.

static inline_only __simd128_double simd128_set_first_and_clear_double(double d) {
    return _mm_set_sd(d);
}

static inline_only __simd128_double simd128_set_zero_double() {
    return _mm_setzero_pd();
}

static inline_only double simd128_get_double(__simd128_double s) {
    return _mm_cvtsd_f64(s);
}

// Set lower and higher double to either existing lower or higher double.

static inline_only __simd128_double simd128_select_double(__simd128_double s1,
unsigned int double0, unsigned int double1) {
    return simd128_cast_int_double(simd128_select_int32(simd128_cast_double_int(s1),
        double0 * 2, double0 * 2 + 1, double1 * 2, double1 * 2 + 1));
}

static inline_only __simd128_double simd128_add_double(__simd128_double s1, __simd128_double s2) {
    return _mm_add_pd(s1, s2);
}

static inline_only __simd128_double simd128_mul_double(__simd128_double s1, __simd128_double s2) {
    return _mm_mul_pd(s1, s2);
}

static inline_only __simd128_double simd128_div_double(__simd128_double s1, __simd128_double s2) {
    return _mm_div_pd(s1, s2);
}

// Calculate square root (accurately) of two doubles.

static inline_only __simd128_double simd128_sqrt_double(__simd128_double s) {
    return _mm_sqrt_pd(s);
}

#endif

#ifdef USE_ARM_NEON
#endif

// Define shared set of generic SIMD functions shared between different platforms
// that are implemented using lower-level SIMD functions.

#ifdef USE_SIMD

// Utility function to count bits (not actually uses SIMD instructions).

static const char sre_simd_internal_bit_count4[16] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
};

static inline_only int simd_count_bits_int4(int i) {
    return sre_simd_internal_bit_count4[i];
}

// Calculate the minimum of the four components of a vector, and store result in the
// first component.

static inline_only __simd128_float simd128_horizonal_min_float(__simd128_float s) {
    __simd128_float m_shifted_float1, m_shifted_float2, m_shifted_float3;
    __simd128_float m_min_01, m_min_23;
    m_shifted_float1 = simd128_shift_right_float(s, 1);
    m_shifted_float2 = simd128_shift_right_float(s, 2);
    m_shifted_float3 = simd128_shift_right_float(s, 3);
    m_min_01 = simd128_min1_float(s, m_shifted_float1);
    m_min_23 = simd128_min1_float(m_shifted_float2, m_shifted_float3);
    return simd128_min1_float(m_min_01, m_min_23);
}

// Calculate the maximum of the four components of a vector, and store result in the
// first component.

static inline_only __simd128_float simd128_horizonal_max_float(__simd128_float s) {
    __simd128_float m_shifted_float1, m_shifted_float2, m_shifted_float3;
    __simd128_float m_max_01, m_max_23;
    m_shifted_float1 = simd128_shift_right_float(s, 1);
    m_shifted_float2 = simd128_shift_right_float(s, 2);
    m_shifted_float3 = simd128_shift_right_float(s, 3);
    m_max_01 = simd128_max1_float(s, m_shifted_float1);
    m_max_23 = simd128_max1_float(m_shifted_float2, m_shifted_float3);
    return simd128_max1_float(m_max_01, m_max_23);
}

#ifndef SIMD_HAVE_HORIZONTAL_ADD2

// Implement higher-level pair-wise addition when not available as a low-level
// primitive.

static inline_only __simd128_float simd128_horizontal_add2_float(__simd128_float s1,
__simd128_float s2) {
    __simd128_float e0 = simd128_merge_float(s1, s2, 0, 2, 0, 2);
    __simd128_float e1 = simd128_merge_float(s1, s2, 1, 3, 1, 3);
    return simd128_add_float(e0, e1);
}

#endif

#ifndef SIMD_HAVE_HORIZONTAL_ADD4

// When not already defined as a primitive, implement function to horizontally add
// four elements and store in the lowest order element. Generally, this operation is
// not efficient even when achievable with two hadd instructions (SSE3), and should
// be avoided when possible.

static inline_only __simd128_float simd128_horizontal_add4_float(__simd128_float s) {
#ifdef SIMD_HAVE_HORIZONTAL_ADD2
    __simd128_float m_zeros = simd128_set_zero_float();
    return simd128_horizontal_add2_float(
        simd128_horizontal_add2_float(s, m_zeros),
        m_zeros);
#else
     __simd128_float m_shifted_float1, m_shifted_float2, m_shifted_float3;
    __simd128_float m_sum_01, m_sum_23;
    m_shifted_float1 = simd128_shift_right_float(s, 1);
    m_shifted_float2 = simd128_shift_right_float(s, 2);
    m_shifted_float3 = simd128_shift_right_float(s, 3);
    m_sum_01 = simd128_add1_float(s, m_shifted_float1);
    m_sum_23 = simd128_add1_float(m_shifted_float2, m_shifted_float3);
    return simd128_add1_float(m_sum_01, m_sum_23);
#endif
}

#endif

// The following more elaborate functions, for matrix multiplication and
// dot products, are provided both as an inline functions and as regular functions
// that are implemented with the inline function.
//
// In some cases, using the inline version may allow the compiler to simplify
// code (e.g. eliminate loads or stores) at the expense of greater overall code size
// of the application; in many other cases, there is not much benefit from using
// using the inline version while code size and CPU instruction cache utilization
// may significantly benefit from using a regular function call.

// SIMD 4x4 float matrix multiplication for matrices in column-major order.
// Requires 16-byte alignment of the matrices.

void simd_matrix_multiply_4x4CM_float(const float * __restrict m1,
const float * __restrict__ m2, float * __restrict m3);

static inline_only void simd_inline_matrix_multiply_4x4CM_float(const float * __restrict m1,
const float * __restrict__ m2, float * __restrict m3) {
    // m.n[column][row]
    // First value (row = 0, column = 0):
    // m1.n[0][0] * m2.n[0][0] + m1.n[1][0] * m2.n[0][1] +
    // m1.n[2][0] * m2.n[0][2] + m1.n[3][0] * m2.n[0][3]
    // Second value (row = 0, column = 1):
    // m1.n[0][0] * m2.n[1][0] + m1.n[1][0] * m2.n[1][1] +
    // m1.n[2][0] * m2.n[1][2] + m1.n[3][0] * m2.n[1][3]
    // Second row:
    // m1.n[0][1] * m2.n[0][0] + m1.n[1][1] * m2.n[0][1] +
    // m1.n[2][1] * m2.n[0][2] + m1.n[3][1] * m2.n[0][3],
    // Third row:
    // m1.n[0][2] * m2.n[0][0] + m1.n[1][2] * m2.n[0][1] +
    // m1.n[2][2] * m2.n[0][2] + m1.n[3][2] * m2.n[0][3],
    __simd128_float col0_m1 = simd128_load_float(&m1[0]);
    __simd128_float col1_m1 = simd128_load_float(&m1[4]);
    __simd128_float col2_m1 = simd128_load_float(&m1[8]);
    __simd128_float col3_m1 = simd128_load_float(&m1[12]);
    // For each result column.
    for (int i = 0; i < 4; i++) {
        __simd128_float col_m2 = simd128_load_float(&m2[i * 4]);
	__simd128_float v0 = simd128_select_float(col_m2, 0, 0, 0, 0);
        __simd128_float v1 = simd128_select_float(col_m2, 1, 1, 1, 1);
        __simd128_float v2 = simd128_select_float(col_m2, 2, 2, 2, 2);
        __simd128_float v3 = simd128_select_float(col_m2, 3, 3, 3, 3);
        // v0 = m1.n[0][i], v1 = m1.n[1][i], v2 = m1.n[2][i], ...
        __simd128_float result_col = simd128_add_float(
            simd128_add_float(
                simd128_mul_float(v0, col0_m1),
                simd128_mul_float(v1, col1_m1)),
            simd128_add_float(
                simd128_mul_float(v2, col2_m1),
                simd128_mul_float(v3, col3_m1))
            );
        simd128_store_float(&m3[i * 4], result_col);
    }
}

// Multiply 4x3 (4 rows, 3 rows) matrices in row-major order. The fourth
// row is implicitly defined as (0.0f, 0.0f, 0.0f, 1.0f).

void simd_matrix_multiply_4x3RM_float(const float * __restrict m1,
const float * __restrict__ m2, float * __restrict m3);

static inline_only void simd_inline_matrix_multiply_4x3RM_float(const float * __restrict m1,
const float * __restrict__ m2, float * __restrict m3) {
    __simd128_float row0 = simd128_load_float(&m2[0]);
    __simd128_float row1 = simd128_load_float(&m2[4]);
    __simd128_float row2 = simd128_load_float(&m2[8]);
    __simd128_float zeros = _mm_setzero_ps();
    // For each row in m1.
    for (int i = 0; i < 3; i++) {
        // Get each column from row i in m1.
        __simd128_float row = simd128_load_float(&m1[i * 4]);
	__simd128_float v0 = simd128_select_float(row, 0, 0, 0, 0);
        __simd128_float v1 = simd128_select_float(row, 1, 1, 1, 1);
        __simd128_float v2 = simd128_select_float(row, 2, 2, 2, 2);
        __simd128_float v3_mult = simd128_select_float(
            simd128_merge1_float(zeros, row), 0, 0, 0, 3);
        __simd128_float result_row = simd128_add_float(
            simd128_add_float(
                simd128_mul_float(v0, row0),
                simd128_mul_float(v1, row1)),
            simd128_add_float(
                simd128_mul_float(v2, row2),
                v3_mult)
            );
        simd128_store_float(&m3[i * 4], result_row);
    }
}

// Multiply 4x4 (column-major) and 4x3 (4 rows, 3 rows, row-major order) matrices.
// The fourth row of the 4x3 matrix is implicitly defined as (0.0f, 0.0f, 0.0f, 1.0f).

void simd_matrix_multiply_4x4CM_4x3RM_float(const float * __restrict m1,
const float * __restrict__ m2, float * __restrict m3);

static inline_only void simd_inline_matrix_multiply_4x4CM_4x3RM_float(const float * __restrict m1,
const float * __restrict__ m2, float * __restrict m3) {
    __simd128_float col0 = simd128_load_float(&m1[0]);
    __simd128_float col1 = simd128_load_float(&m1[4]);
    __simd128_float col2 = simd128_load_float(&m1[8]);
    __simd128_float col3 = simd128_load_float(&m1[12]);
    __simd128_float m2_row0 = simd128_load_float(&m2[0]);
    __simd128_float m2_row1 = simd128_load_float(&m2[4]);
    __simd128_float m2_row2 = simd128_load_float(&m2[8]);
    for (int i = 0; i < 3; i++) {
        // Get column i from each row of m2.
	__simd128_float m2_coli_row0 = simd128_select_float(m2_row0, 0, 0, 0, 0);
 	__simd128_float m2_coli_row1 = simd128_select_float(m2_row1, 0, 0, 0, 0);
	__simd128_float m2_coli_row2 = simd128_select_float(m2_row2, 0, 0, 0, 0);
        m2_row0 = simd128_shift_right_float(m2_row0, 1);
        m2_row1 = simd128_shift_right_float(m2_row1, 1);
        m2_row2 = simd128_shift_right_float(m2_row2, 1);
        __simd128_float result_col = simd128_add_float(
            simd128_add_float(
                simd128_mul_float(m2_coli_row0, col0),
                simd128_mul_float(m2_coli_row1, col1)),
            simd128_mul_float(m2_coli_row2, col2)
            );
        simd128_store_float(&m3[i * 4], result_col);
    }
    // Element 0 of m2_coli_row0/1/2 now contains column 3 of each row.
    __simd128_float m2_col3_row0 = simd128_select_float(m2_row0, 0, 0, 0, 0);
    __simd128_float m2_col3_row1 = simd128_select_float(m2_row1, 0, 0, 0, 0);
    __simd128_float m2_col3_row2 = simd128_select_float(m2_row2, 0, 0, 0, 0);
    __simd128_float result_col3 = simd128_add_float(
        simd128_add_float(
            simd128_mul_float(m2_col3_row0, col0),
            simd128_mul_float(m2_col3_row1, col1)),
        simd128_add_float(
            simd128_mul_float(m2_col3_row2, col2),
            col3)
        );
    simd128_store_float(&m3[3 * 4], result_col3);
}

// Classes for using SIMD to multiply a specific matrix with one or more vertices.

// The following class is for a 4x4 matrix and can be initialized with matrices
// (float arrays) in both row-major or column-major format.

class SIMDMatrix4x4 {
public :
    __simd128_float m_row0;
    __simd128_float m_row1;
    __simd128_float m_row2;
    __simd128_float m_row3;

    // Set row-major matrix data.
    inline_only void SetRM(const float *f) {
        // Load rows.
        m_row0 = simd128_load_float(&f[0]);
        m_row1 = simd128_load_float(&f[4]);
        m_row2 = simd128_load_float(&f[8]);
        m_row3 = simd128_load_float(&f[12]);
    }
    // Set column-major matrix data.
    inline_only void SetCM(const float *f) {
        // Load columns, then transpose to rows.
        SetRM(f);
        simd128_transpose4_float(m_row0, m_row1, m_row2, m_row3);
    }
    // Multiply with a four-float SIMD vector.
    // Note: This uses the inefficient horizontal addition primitive.
    inline_only void MultiplyVector4(const __simd128_float m_v, __simd128_float& m_result) const {
        __simd128_float m_mul0  = simd128_mul_float(m_row0, m_v);
        __simd128_float m_mul1  = simd128_mul_float(m_row1, m_v);
        __simd128_float m_mul2  = simd128_mul_float(m_row2, m_v);
        __simd128_float m_mul3  = simd128_mul_float(m_row3, m_v);
        m_result =
            simd128_horizontal_add2_float(
                simd128_horizontal_add2_float(m_mul0, m_mul1),
                simd128_horizontal_add2_float(m_mul2, m_mul3));
    }
    inline_only void MultiplyVector4(const __simd128_float m_v, float &result_x,
    float &result_y, float &result_z, float& result_w) const {
        __simd128_float m_result;
        MultiplyVector4(m_v, m_result);
        result_x = simd128_get_float(m_result);
        result_y = simd128_get_float(simd128_shift_right_float(m_result, 1));
        result_z = simd128_get_float(simd128_shift_right_float(m_result, 2));
        result_w = simd128_get_float(simd128_shift_right_float(m_result, 3));
    }
    inline_only void MultiplyVector4(const __simd128_float m_v, float *result) const {
        __simd128_float m_result;
        MultiplyVector4(m_v, m_result);
        simd128_store_float(result, m_result);
    }
    inline_only void MultiplyVector4(const float *v, __simd128_float m_result) const {
        __simd128_float m_v = simd128_load_float(&v[0]);
        MultiplyVector4(m_v, m_result);
    }
    inline_only void MultiplyVector4(const float *v, float &result_x,
    float &result_y, float &result_z, float& result_w) const {
        __simd128_float m_v = simd128_load_float(&v[0]);
        MultiplyVector4(m_v, result_x, result_y, result_z, result_w);
    }
    inline_only void MultiplyVector4(const float *v, float *result) const {
        __simd128_float m_v = simd128_load_float(&v[0]);
        MultiplyVector4(m_v, result);
    }
};

#ifdef SIMD_HAVE_MATRIX4X3_VECTOR_MULTIPLICATION

// The following class is for a 4x3 matrix and can be initialized with matrices
// (float arrays) in both row-major or column-major format.
//
// Requires simd128_transpose4to3_float and simd_transpose3to4_float.

class SIMDMatrix4x3 {
public :
    __simd128_float m_row0;
    __simd128_float m_row1;
    __simd128_float m_row2;

    // Set row-major matrix data.
    inline_only void SetRM(const float *f) {
        // Load rows.
        m_row0 = simd128_load_float(&f[0]);
        m_row1 = simd128_load_float(&f[4]);
        m_row2 = simd128_load_float(&f[8]);
    }
    // Set column-major matrix data.
    inline_only void SetCM(const float *f) {
        // Load columns, then transpose to rows.
        __simd128_float m_col0 = simd128_load3_float(&f[0]);
        __simd128_float m_col1 = simd128_load3_float(&f[3]);
        __simd128_float m_col2 = simd128_load3_float(&f[6]);
        __simd128_float m_col3 = simd128_load3_float(&f[9]);
        simd128_transpose4to3_float(m_col0, m_col1, m_col2, m_col3,
            m_row0, m_row1, m_row2);
    }
    // Multiply with a three-float SIMD vector.
    // Note: This uses the inefficient horizontal addition primitive.
    // The fourth component of m_v (0.0f or 1.0f) makes a difference.
    inline_only void MultiplyVector3(const __simd128_float m_v, __simd128_float& m_result) const {
        __simd128_float m_mul0  = simd128_mul_float(m_row0, m_v);
        __simd128_float m_mul1  = simd128_mul_float(m_row1, m_v);
        __simd128_float m_mul2  = simd128_mul_float(m_row2, m_v);
        __simd128_float m_zeros = simd128_set_zero_float();
        m_result =
            simd128_horizontal_add2_float(
                simd128_horizontal_add2_float(m_mul0, m_mul1),
                simd128_horizontal_add2_float(m_mul2, m_zeros));
    }
    // The fourth component of m_v (0.0f or 1.0f) makes a difference.
    inline_only void MultiplyVector3(const __simd128_float m_v, float &result_x,
    float &result_y, float &result_z) const {
        __simd128_float m_result;
        MultiplyVector3(m_v, m_result);
        result_x = simd128_get_float(m_result);
        result_y = simd128_get_float(simd128_shift_right_float(m_result, 1));
        result_z = simd128_get_float(simd128_shift_right_float(m_result, 2));
    }
    // Unpacked three-float result vector (16-byte aligned, with four bytes padding).
    // The fourth component of source vector m_v (0.0f or 1.0f) makes a difference.
    inline_only void MultiplyVector3(const __simd128_float m_v, float *result) const {
        __simd128_float m_result;
        MultiplyVector3(m_v, m_result);
        simd128_store_float(result, m_result);
    }
    // Unpacked three-float source vector (16-byte aligned, with four bytes padding).
    inline_only void MultiplyVector3(const float *v, __simd128_float m_result) const {
        // Make sure the fourth element is zero after loading.
        __simd128_float m_v = simd128_set_last_zero_float(simd128_load_float(&v[0]));
        MultiplyVector3(m_v, m_result);
    }
    // Unpacked three-float source vector (16-byte aligned, with four bytes padding).
    inline_only void MultiplyVector3(const float *v, float &result_x,
    float &result_y, float &result_z) const {
        __simd128_float m_v = simd128_load_float(&v[0]);
        MultiplyVector3(m_v, result_x, result_y, result_z);
    }
    // Unpacked three-float source and result vectors (16-byte aligned, with four bytes padding).
    inline_only void MultiplyVector3(const float *v, float *result) const {
        __simd128_float m_v = simd128_set_last_zero_float(simd128_load_float(&v[0]));
        MultiplyVector3(m_v, result);
    }
    // Packed three-float result vector (unaligned).
    // The fourth component of source vector m_v (0.0f or 1.0f) makes a difference.
    inline_only void MultiplyVector3Packed(const __simd128_float m_v, float *result) const {
        __simd128_float m_result;
        MultiplyVector3(m_v, m_result);
        simd128_store3_float(result, m_result);
    }
    // Packed three-float source vector (unaligned).
    inline_only void MultiplyVector3Packed(const float *v, __simd128_float m_result) const {
        __simd128_float m_v = simd128_load3_float(&v[0]);
        MultiplyVector3(m_v, m_result);
    }
    // Packed three-float source vector (unaligned).
    inline_only void MultiplyVector3Packed(const float *v, float &result_x,
    float &result_y, float &result_z) const {
        __simd128_float m_v = simd128_load3_float(&v[0]);
        MultiplyVector3(m_v, result_x, result_y, result_z);
    }
    // Packed three-float source and result vectors (unaligned).
    inline_only void MultiplyVector3Packed(const float *v, float *result) const {
        __simd128_float m_v = simd128_load3_float(&v[0]);
        MultiplyVector3Packed(m_v, result);
    }
    // Four-float vectors including (fourth) w component.
    inline_only void MultiplyVector4(const __simd128_float m_v, __simd128_float& m_result) const {
        __simd128_float m_mul0  = simd128_mul_float(m_row0, m_v);
        __simd128_float m_mul1  = simd128_mul_float(m_row1, m_v);
        __simd128_float m_mul2  = simd128_mul_float(m_row2, m_v);
        // Set m_mul3 to (0.0f, 0.0f, 0.0f, m_v.w)
        __simd128_float m_mul3  = simd128_select_float(
            simd128_merge_float(simd128_set_zero_float(), m_v, 0, 0, 3, 3),
            0, 0, 0, 3);
        m_result =
            simd128_horizontal_add2_float(
                simd128_horizontal_add2_float(m_mul0, m_mul1),
                simd128_horizontal_add2_float(m_mul2, m_mul3));
    }
    // Result is stored in three seperate floats.
    inline_only void MultiplyVector4(const __simd128_float m_v, float &result_x,
    float &result_y, float &result_z) const {
        __simd128_float m_result;
        MultiplyVector4(m_v, m_result);
        result_x = simd128_get_float(m_result);
        result_y = simd128_get_float(simd128_shift_right_float(m_result, 1));
        result_z = simd128_get_float(simd128_shift_right_float(m_result, 2));
    }
    // Result is stored in four seperate floats.
    inline_only void MultiplyVector4(const __simd128_float m_v, float &result_x,
    float &result_y, float &result_z, float& result_w) const {
        __simd128_float m_result;
        MultiplyVector4(m_v, m_result);
        result_x = simd128_get_float(m_result);
        result_y = simd128_get_float(simd128_shift_right_float(m_result, 1));
        result_z = simd128_get_float(simd128_shift_right_float(m_result, 2));
        result_w = simd128_get_float(simd128_shift_right_float(m_result, 3));
    }
    // Four-float result vector.
    inline_only void MultiplyVector4(const __simd128_float m_v, float *result) const {
        __simd128_float m_result;
        MultiplyVector4(m_v, m_result);
        simd128_store_float(result, m_result);
    }
    inline_only void MultiplyVector4(const float *v, __simd128_float m_result) const {
        __simd128_float m_v = simd128_load_float(&v[0]);
        MultiplyVector4(m_v, m_result);
    }
    inline_only void MultiplyVector4(const float *v, float &result_x,
    float &result_y, float &result_z, float& result_w) const {
        __simd128_float m_v = simd128_load_float(&v[0]);
        MultiplyVector4(m_v, result_x, result_y, result_z, result_w);
    }
    // Four-float result vector.
    inline_only void MultiplyVector4(const float *v, float *result) const {
        __simd128_float m_v = simd128_load_float(&v[0]);
        MultiplyVector4(m_v, result);
    }
    // Unpacked three-float source point vector (16-byte aligned, with four bytes padding).
    // Point vector has implicit w component of 1.0f.
    inline_only void MultiplyPoint3(const float *v, __simd128_float m_result) const {
        // Make sure the fourth element is 1.0f after loading.
        __simd128_float m_v = simd128_set_last_float(simd128_load_float(&v[0]), 1.0f);
        MultiplyVector3(m_v, m_result);
    }
    // Unpacked three-float source vector (16-byte aligned, with four bytes padding).
    inline_only void MultiplyPoint3(const float *v, float &result_x,
    float &result_y, float &result_z) const {
        __simd128_float m_v = simd128_set_last_float(simd128_load_float(&v[0]), 1.0f);
        MultiplyVector3(m_v, result_x, result_y, result_z);
    }
    // Unpacked three-float source and result vectors (16-byte aligned, with four bytes padding).
    inline_only void MultiplyPoint3(const float *v, float *result) const {
        __simd128_float m_v = simd128_set_last_float(simd128_load_float(&v[0]), 1.0f);
        MultiplyVector3(m_v, result);
    }
};

#endif

// Calculate dot products of four four-component float vectors stored at f1 and f2.

void simd_four_dot_products_vector4_float(
const float * __restrict f1, const float * __restrict f2, __simd128_float& result);

static inline_only void simd_inline_four_dot_products_vector4_float(
const float * __restrict f1, const float * __restrict f2, __simd128_float& result) {
#if 1
    // Loading and using tranpose seems to be more efficient than set_float calls, which
    // expand to numerous instructions.
    __simd128_float m_v1_0 = simd128_load_float(&f1[0]);
    __simd128_float m_v1_1 = simd128_load_float(&f1[4]);
    __simd128_float m_v1_2 = simd128_load_float(&f1[8]);
    __simd128_float m_v1_3 = simd128_load_float(&f1[12]);
    __simd128_float m_v2_0 = simd128_load_float(&f2[0]);
    __simd128_float m_v2_1 = simd128_load_float(&f2[4]);
    __simd128_float m_v2_2 = simd128_load_float(&f2[8]);
    __simd128_float m_v2_3 = simd128_load_float(&f2[12]);
    __simd128_float m_v1_x, m_v1_y, m_v1_z, m_v1_w;
    simd128_transpose4to4_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3, m_v1_x, m_v1_y, m_v1_z, m_v1_w);
    __simd128_float m_v2_x, m_v2_y, m_v2_z, m_v2_w;
    simd128_transpose4to4_float(m_v2_0, m_v2_1, m_v2_2, m_v2_3, m_v2_x, m_v2_y, m_v2_z, m_v2_w);
#else
    __simd128_float m_v1_x = simd128_set_float(f1[0], f1[4], f1[8], f1[12]);
    __simd128_float m_v1_y = simd128_set_float(f1[1], f1[5], f1[9], f1[13]);
    __simd128_float m_v1_z = simd128_set_float(f1[2], f1[6], f1[10], f1[14]);
    __simd128_float m_v1_w = simd128_set_float(f1[3], f1[7], f1[11], f1[15);
    __simd128_float m_v2_x = simd128_set_float(f2[0], f2[4], f2[8], f2[12]);
    __simd128_float m_v2_y = simd128_set_float(f2[1], f2[5], f2[9], f2[13]);
    __simd128_float m_v2_z = simd128_set_float(f2[2], f2[6], f2[10], f2[14]);
    __simd128_float m_v2_w = simd128_set_float(f2[3], f2[7], f2[11], f2[15]);
#endif
    __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
    __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
    __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
    __simd128_float m_dot_w = simd128_mul_float(m_v1_w, m_v2_w);
    result = simd128_add_float(
        simd128_add_float(m_dot_x, m_dot_y),
        simd128_add_float(m_dot_z, m_dot_w)
        );
}

// Calculate dot products of four three-component float vectors stored at f1 and f2. The
// vectors are assumed to be stored in 128-bit fields (the last four bytes are unused).

void simd_four_dot_products_vector3_storage4_float(const float * __restrict f1,
const float * __restrict f2, __simd128_float& result);

static inline_only void simd_inline_four_dot_products_vector3_storage4_float(
const float * __restrict f1, const float * __restrict f2, __simd128_float& result) {
#if 1
    __simd128_float m_v1_0 = simd128_load_float(&f1[0]);
    __simd128_float m_v1_1 = simd128_load_float(&f1[4]);
    __simd128_float m_v1_2 = simd128_load_float(&f1[8]);
    __simd128_float m_v1_3 = simd128_load_float(&f1[12]);
    __simd128_float m_v2_0 = simd128_load_float(&f2[0]);
    __simd128_float m_v2_1 = simd128_load_float(&f2[4]);
    __simd128_float m_v2_2 = simd128_load_float(&f2[8]);
    __simd128_float m_v2_3 = simd128_load_float(&f2[12]);
    __simd128_float m_v1_x, m_v1_y, m_v1_z;
    simd128_transpose4to3_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3, m_v1_x, m_v1_y, m_v1_z);
    __simd128_float m_v2_x, m_v2_y, m_v2_z;
    simd128_transpose4to3_float(m_v2_0, m_v2_1, m_v2_2, m_v2_3, m_v2_x, m_v2_y, m_v2_z);
#else
    __simd128_float m_v1_x = simd128_set_float(f1[0], f1[4], f1[8], f1[12]);
    __simd128_float m_v1_y = simd128_set_float(f1[1], f1[5], f1[9], f1[13]);
    __simd128_float m_v1_z = simd128_set_float(f1[2], f1[6], f1[10], f1[14]);
    __simd128_float m_v2_x = simd128_set_float(f2[0], f2[4], f2[8], f2[12]);
    __simd128_float m_v2_y = simd128_set_float(f2[1], f2[5], f2[9], f2[13]);
    __simd128_float m_v2_z = simd128_set_float(f2[2], f2[6], f2[10], f2[14]);
#endif
    __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
    __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
    __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
    result = simd128_add_float(
        simd128_add_float(m_dot_x, m_dot_y),
        m_dot_z
        );
}

// Calculate dot products of four three-component float vectors stored at f1 and f2. The
// vectors are assumed to be stored consecutively in packed 12-byte format.

void simd_four_dot_products_vector3_storage3_float(const float * __restrict f1,
const float * __restrict f2, __simd128_float& result);

static inline_only void simd_inline_four_dot_products_vector3_storage3_float(const float * __restrict f1,
const float * __restrict f2, __simd128_float& result) {
#if 1
    __simd128_float m_v1_0 = simd128_load3_float(&f1[0]);
    __simd128_float m_v1_1 = simd128_load3_float(&f1[4]);
    __simd128_float m_v1_2 = simd128_load3_float(&f1[8]);
    __simd128_float m_v1_3 = simd128_load3_float(&f1[12]);
    __simd128_float m_v2_0 = simd128_load3_float(&f2[0]);
    __simd128_float m_v2_1 = simd128_load3_float(&f2[4]);
    __simd128_float m_v2_2 = simd128_load3_float(&f2[8]);
    __simd128_float m_v2_3 = simd128_load3_float(&f2[12]);
    __simd128_float m_v1_x, m_v1_y, m_v1_z;
    simd128_transpose4to3_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3, m_v1_x, m_v1_y, m_v1_z);
    __simd128_float m_v2_x, m_v2_y, m_v2_z;
    simd128_transpose4to3_float(m_v2_0, m_v2_1, m_v2_2, m_v2_3, m_v2_x, m_v2_y, m_v2_z);
#else
    __simd128_float m_v1_x = simd128_set_float(f1[0], f1[4], f1[8], f1[12]);
    __simd128_float m_v1_y = simd128_set_float(f1[1], f1[5], f1[9], f1[13]);
    __simd128_float m_v1_z = simd128_set_float(f1[2], f1[6], f1[10], f1[14]);
    __simd128_float m_v2_x = simd128_set_float(f2[0], f2[4], f2[8], f2[12]);
    __simd128_float m_v2_y = simd128_set_float(f2[1], f2[5], f2[9], f2[13]);
    __simd128_float m_v2_z = simd128_set_float(f2[2], f2[6], f2[10], f2[14]);
#endif
    __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
    __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
    __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
    result = simd128_add_float(
        simd128_add_float(m_dot_x, m_dot_y),
        m_dot_z
        );
}

// Calculate n dot products from one array of four vectors and one single vector,
// and store the result in an array of floats.

void simd_dot_product_nx1_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot);

static inline_only void simd_inline_dot_product_nx1_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot) {
    __simd128_float m_v2 = simd128_load_float(f2);
    __simd128_float m_v2_x, m_v2_y, m_v2_z, m_v2_w;
    m_v2_x = simd128_select_float(m_v2, 0, 0, 0, 0);
    m_v2_y = simd128_select_float(m_v2, 1, 1, 1, 1);
    m_v2_z = simd128_select_float(m_v2, 2, 2, 2, 2);
    m_v2_w = simd128_select_float(m_v2, 3, 3, 3, 3);
    int i = 0;
    for (; i  + 3 < n; i += 4) {
        // Loading and using transpose seems to be more efficient than set_float calls,
        // which expand to numerous instructions.
        __simd128_float m_v1_0 = simd128_load_float(&f1[i * 4 + 0]);
        __simd128_float m_v1_1 = simd128_load_float(&f1[i * 4 + 4]);
        __simd128_float m_v1_2 = simd128_load_float(&f1[i * 4 + 8]);
        __simd128_float m_v1_3 = simd128_load_float(&f1[i * 4 + 12]);
        __simd128_float m_v1_x, m_v1_y, m_v1_z, m_v1_w;
        simd128_transpose4to4_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3,
            m_v1_x, m_v1_y, m_v1_z, m_v1_w);
        __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
        __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
        __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
        __simd128_float m_dot_w = simd128_mul_float(m_v1_w, m_v2_w);
        __simd128_float m_result = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            simd128_add_float(m_dot_z, m_dot_w)
            );
        simd128_store_float(&dot[i * 4], m_result);
    }
    for (; i < n; i++) {
        __simd128_float m_v1 = simd128_load_float(&f1[i * 4]);
        __simd128_float m_dot = simd128_mul_float(m_v1, m_v2);
        __simd128_float m_result = simd128_horizontal_add4_float(m_dot);
        simd128_store_first_float(&dot[i], m_result);
    }
}

// When vectors of three floats are stored in packed format, simd128_load3_float
// will make sure the fourth component of the SIMD register is set to 0.0f.

void simd_dot_product_nx1_vector3_storage3_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot);

static inline_only void simd_inline_dot_product_nx1_vector3_storage3_float(int n,
const float * __restrict f1, const __simd128_float m_v2, float * __restrict dot) {
    __simd128_float m_v2_x, m_v2_y, m_v2_z;
    m_v2_x = simd128_select_float(m_v2, 0, 0, 0, 0);
    m_v2_y = simd128_select_float(m_v2, 1, 1, 1, 1);
    m_v2_z = simd128_select_float(m_v2, 2, 2, 2, 2);
    int i = 0;
    for (; i + 3 < n; i += 4) {
        __simd128_float m_v1_0 = simd128_load3_float(&f1[i * 3 + 0]);
        __simd128_float m_v1_1 = simd128_load3_float(&f1[i * 3 + 3]);
        __simd128_float m_v1_2 = simd128_load3_float(&f1[i * 3 + 6]);
        __simd128_float m_v1_3 = simd128_load3_float(&f1[i * 3 + 9]);
        __simd128_float m_v1_x, m_v1_y, m_v1_z;
        simd128_transpose4to3_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3, m_v1_x, m_v1_y, m_v1_z);
        __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
        __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
        __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
        __simd128_float m_result = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            m_dot_z
            );
        simd128_store_float(&dot[i], m_result);
    }
    for (; i < n; i++) {
        __simd128_float m_v1 = simd128_load3_float(&f1[i * 3]);
        __simd128_float m_dot = simd128_mul_float(m_v1, m_v2);
        __simd128_float m_result = simd128_horizontal_add4_float(m_dot);
        simd128_store_first_float(&dot[i], m_result);
    }
}

static inline_only void simd_inline_dot_product_nx1_vector3_storage3_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot) {
    __simd128_float m_v2 = simd128_load3_float(f2);
    simd_inline_dot_product_nx1_vector3_storage3_float(n, f1, m_v2, dot);
}

// When vectors of three floats are stored in 16-byte aligned format,
// simd128_load_float can be used but the fourth element of the SIMD register
// needs to be initialized with 0.0f (v2 only).

void simd_dot_product_nx1_vector3_storage4_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot);

static inline_only void simd_inline_dot_product_nx1_vector3_storage4_vector4_float(int n,
const float * __restrict f1, const __simd128_float m_v2, float * __restrict dot) {
    __simd128_float m_v2_x, m_v2_y, m_v2_z;
    m_v2_x = simd128_select_float(m_v2, 0, 0, 0, 0);
    m_v2_y = simd128_select_float(m_v2, 1, 1, 1, 1);
    m_v2_z = simd128_select_float(m_v2, 2, 2, 2, 2);
    int i = 0;
    for (; i + 3 < n; i += 4) {
        __simd128_float m_v1_0 = simd128_load_float(&f1[i * 4 + 0]);
        __simd128_float m_v1_1 = simd128_load_float(&f1[i * 4 + 4]);
        __simd128_float m_v1_2 = simd128_load_float(&f1[i * 4 + 8]);
        __simd128_float m_v1_3 = simd128_load_float(&f1[i * 4 + 12]);
        __simd128_float m_v1_x, m_v1_y, m_v1_z;
        simd128_transpose4to3_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3, m_v1_x, m_v1_y, m_v1_z);
        __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
        __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
        __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
        __simd128_float m_result = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            m_dot_z
            );
        simd128_store_float(&dot[i], m_result);
    }
    for (; i < n; i++) {
        __simd128_float m_v1 = simd128_set_last_zero_float(simd128_load_float(&f1[i * 4]));
        __simd128_float m_dot = simd128_mul_float(m_v1, m_v2);
        __simd128_float m_result = simd128_horizontal_add4_float(m_dot);
        simd128_store_first_float(&dot[i], m_result);
    }
}

static inline_only void simd_inline_dot_product_nx1_vector3_storage4_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot) {
    __simd128_float m_v2 = simd128_load_float(f2);
    // The w component as loaded is undefined.
    // Set w to 0.0f.
    __simd128_float m_zeros = simd128_set_zero_float();
    __simd128_int m_mask_012x = simd128_shift_right_bytes_int(
        simd128_set_same_int32(0xFFFFFFFF), 4);
    m_v2 = simd128_cast_int_float(simd128_or_int(
        simd128_and_int(m_mask_012x, simd128_cast_float_int(m_v2)),
        simd128_andnot_int(m_mask_012x, simd128_cast_float_int(m_zeros))
        ));
    simd_inline_dot_product_nx1_vector3_storage4_vector4_float(n, f1, m_v2, dot);
}

// The array of vectors is an array of points with implicit w coordinate of 1.0f,
// and is stored aligned on 16-byte boundaries. The constant vector has four
// components.

void simd_dot_product_nx1_point3_storage4_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot);

static inline_only void simd_inline_dot_product_nx1_point3_storage4_vector4_float(int n,
const float * __restrict f1, const __simd128_float m_v2, float * __restrict dot) {
    __simd128_float m_v2_x, m_v2_y, m_v2_z, m_v2_w;
    m_v2_x = simd128_select_float(m_v2, 0, 0, 0, 0);
    m_v2_y = simd128_select_float(m_v2, 1, 1, 1, 1);
    m_v2_z = simd128_select_float(m_v2, 2, 2, 2, 2);
    m_v2_w = simd128_select_float(m_v2, 3, 3, 3, 3);
    int i = 0;
    for (; i + 3 < n; i += 4) {
        __simd128_float m_v1_0 = simd128_load_float(&f1[i * 4 + 0]);
        __simd128_float m_v1_1 = simd128_load_float(&f1[i * 4 + 4]);
        __simd128_float m_v1_2 = simd128_load_float(&f1[i * 4 + 8]);
        __simd128_float m_v1_3 = simd128_load_float(&f1[i * 4 + 12]);
        __simd128_float m_v1_x, m_v1_y, m_v1_z;
        simd128_transpose4to3_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3, m_v1_x, m_v1_y, m_v1_z);
        __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
        __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
        __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
        // m_v1_w would contain only 1.0f so m_dot_w is equal to m_v2_w.
        __simd128_float m_result = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            simd128_add_float(m_dot_z, m_v2_w)
            );
        simd128_store_float(&dot[i], m_result);
    }
    for (; i < n; i++) {
         // Load the point vector and make sure the w component is 1.0f.
        __simd128_float m_v1 = simd128_set_last_float(
            simd128_load_float(&f1[i * 4]), 1.0f);
        __simd128_float m_dot = simd128_mul_float(m_v1, m_v2);
        __simd128_float m_result = simd128_horizontal_add4_float(m_dot);
        simd128_store_first_float(&dot[i], m_result);
    }
}

static inline_only void simd_inline_dot_product_nx1_point3_storage4_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot) {
    __simd128_float m_v2 = simd128_load_float(f2);
    simd_inline_dot_product_nx1_point3_storage4_vector4_float(n, f1, m_v2, dot);
}

// The array of vectors is an array of points with implicit w coordinate of 1.0f,
// and is stored unaligned on 12-byte boundaries. The constant vector has four
// components.

void simd_dot_product_nx1_point3_storage3_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot);

static inline_only void simd_inline_dot_product_nx1_point3_storage3_vector4_float(int n,
const float * __restrict f1, const __simd128_float m_v2, float * __restrict dot) {
    __simd128_float m_v2_x, m_v2_y, m_v2_z, m_v2_w;
    m_v2_x = simd128_select_float(m_v2, 0, 0, 0, 0);
    m_v2_y = simd128_select_float(m_v2, 1, 1, 1, 1);
    m_v2_z = simd128_select_float(m_v2, 2, 2, 2, 2);
    m_v2_w = simd128_select_float(m_v2, 3, 3, 3, 3);
    int i = 0;
    for (; i + 3 < n; i += 4) {
        __simd128_float m_v1_0 = simd128_load3_float(&f1[i * 3 + 0]);
        __simd128_float m_v1_1 = simd128_load3_float(&f1[i * 3 + 3]);
        __simd128_float m_v1_2 = simd128_load3_float(&f1[i * 3 + 6]);
        __simd128_float m_v1_3 = simd128_load3_float(&f1[i * 3 + 9]);
        __simd128_float m_v1_x, m_v1_y, m_v1_z;
        simd128_transpose4to3_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3, m_v1_x, m_v1_y, m_v1_z);
        __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
        __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
        __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
        // m_v1_w would contain only 1.0f so m_dot_w is equal to m_v2_w.
        __simd128_float m_result = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            simd128_add_float(m_dot_z, m_v2_w)
            );
        simd128_store_float(&dot[i], m_result);
    }
    for (; i < n; i++) {
         // Load the point vector and make sure the w component is 1.0f.
        __simd128_float m_v1 = simd128_set_last_float(
            simd128_load3_float(&f1[i * 3]), 1.0f);
        __simd128_float m_dot = simd128_mul_float(m_v1, m_v2);
        __simd128_float m_result = simd128_horizontal_add4_float(m_dot);
        simd128_store_first_float(&dot[i], m_result);
    }
}

static inline_only void simd_inline_dot_product_nx1_point3_storage3_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot) {
    __simd128_float m_v2 = simd128_load_float(f2);
    simd_inline_dot_product_nx1_point3_storage3_vector4_float(n, f1, m_v2, dot);
}

// Calculate n dot products from one array of n point vectors stored on 16-byte
// boundaries and one single four-element vector, store the result in an array of
// floats, and count the number of dot products that is smaller than zero.

void simd_dot_product_nx1_point3_storage4_vector4_and_count_negative_float(
int n, const float * __restrict f1, const float * __restrict f2, float * __restrict dot,
int& negative_count);

static inline_only void simd_inline_dot_product_nx1_point3_storage4_vector4_and_count_negative_float(
int n, const float * __restrict f1, const float * __restrict f2, float * __restrict dot,
int& negative_count) {
    __simd128_float m_v2 = simd128_load_float(f2);
    __simd128_float m_v2_x, m_v2_y, m_v2_z, m_v2_w;
    m_v2_x = simd128_select_float(m_v2, 0, 0, 0, 0);
    m_v2_y = simd128_select_float(m_v2, 1, 1, 1, 1);
    m_v2_z = simd128_select_float(m_v2, 2, 2, 2, 2);
    m_v2_w = simd128_select_float(m_v2, 3, 3, 3, 3);
    __simd128_float m_zeros = simd128_set_zero_float();
    int count = 0;
    int i = 0;
    for (; i + 3 < n; i += 4) {
        __simd128_float m_v1_0 = simd128_load_float(&f1[i * 4 + 0]);
        __simd128_float m_v1_1 = simd128_load_float(&f1[i * 4 + 4]);
        __simd128_float m_v1_2 = simd128_load_float(&f1[i * 4 + 8]);
        __simd128_float m_v1_3 = simd128_load_float(&f1[i * 4 + 12]);
        __simd128_float m_v1_x, m_v1_y, m_v1_z;
        simd128_transpose4to3_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3, m_v1_x, m_v1_y, m_v1_z);
        __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
        __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
        __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
        // m_v1_w would contain only 1.0f so m_dot_w is equal to m_v2_w.
        __simd128_float m_result = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            simd128_add_float(m_dot_z, m_v2_w)
            );
        simd128_store_float(&dot[i], m_result);
        // Produce 32-bit bit masks indicating whether each result is smaller than zero.
        __simd128_int m_comp = simd128_cmplt_float(m_result, m_zeros);
        // Convert to 32-bit mask (bit 31) to packed bits and count the ones.
        count += simd_count_bits_int4(simd128_convert_masks_int32_int1(m_comp));
    }
    for (; i < n; i++) {
         // Load the point vector and make sure the w component is 1.0f.
        __simd128_float m_v1 = simd128_set_last_float(
            simd128_load_float(&f1[i * 4]), 1.0f);
        __simd128_float m_dot = simd128_mul_float(m_v1, m_v2);
        __simd128_float m_result = simd128_horizontal_add4_float(m_dot);
        simd128_store_first_float(&dot[i], m_result);
        count += (simd128_get_float(m_result) < 0.0f);
    }
    negative_count = count;
}

// Similar to the previous function, but the point vectors are packed at 12-byte
// boundaries.

void simd_dot_product_nx1_point3_storage3_vector4_and_count_negative_float(
int n, const float * __restrict f1, const float * __restrict f2, float * __restrict dot,
int& negative_count);

static inline_only void simd_inline_dot_product_nx1_point3_storage3_vector4_and_count_negative_float(
int n, const float * __restrict f1, const float * __restrict f2, float * __restrict dot,
int& negative_count) {
    __simd128_float m_v2 = simd128_load_float(f2);
    __simd128_float m_v2_x, m_v2_y, m_v2_z, m_v2_w;
    m_v2_x = simd128_select_float(m_v2, 0, 0, 0, 0);
    m_v2_y = simd128_select_float(m_v2, 1, 1, 1, 1);
    m_v2_z = simd128_select_float(m_v2, 2, 2, 2, 2);
    m_v2_w = simd128_select_float(m_v2, 3, 3, 3, 3);
    __simd128_float m_zeros = simd128_set_zero_float();
    int count = 0;
    int i = 0;
    for (; i + 3 < n; i += 4) {
        __simd128_float m_v1_0 = simd128_load3_float(&f1[i * 3 + 0]);
        __simd128_float m_v1_1 = simd128_load3_float(&f1[i * 3 + 3]);
        __simd128_float m_v1_2 = simd128_load3_float(&f1[i * 3 + 6]);
        __simd128_float m_v1_3 = simd128_load3_float(&f1[i * 3 + 9]);
        __simd128_float m_v1_x, m_v1_y, m_v1_z;
        simd128_transpose4to3_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3, m_v1_x, m_v1_y, m_v1_z);
        __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
        __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
        __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
        // m_v1_w would contain only 1.0f so m_dot_w is equal to m_v2_w.
        __simd128_float m_result = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            simd128_add_float(m_dot_z, m_v2_w)
            );
        simd128_store_float(&dot[i], m_result);
        // Produce 32-bit bit masks indicating whether each result is smaller than zero.
        __simd128_int m_comp = simd128_cmplt_float(m_result, m_zeros);
        // Convert to 32-bit mask (bit 31) to packed bits and count the ones.
        count += simd_count_bits_int4(simd128_convert_masks_int32_int1(m_comp));
    }
    for (; i < n; i++) {
         // Load the point vector and make sure the w component is 1.0f.
        __simd128_float m_v1 = simd128_set_last_float(
            simd128_load3_float(&f1[i * 3]), 1.0f);
        __simd128_float m_dot = simd128_mul_float(m_v1, m_v2);
        __simd128_float m_result = simd128_horizontal_add4_float(m_dot);
        simd128_store_first_float(&dot[i], m_result);
        count += (simd128_get_float(m_result) < 0.0f);
    }
    negative_count = count;
}

#endif // defined(USE_SIMD)

#endif

