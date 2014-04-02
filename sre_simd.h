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

// This is a header file defines inline SIMD accelerator vector processing
// primitives. The functions are only included if the USE_SSE2 flag is
// are defined.

// Proving these functions inline has signficant benefits; they can be
// integrated into any pure or mixed SIMD processing function, which will
// enable the compiler to automatically mix/shuffle the code for advanced
// scheduling optimizations.

#ifndef __SRE_SIMD_H__
#define __SRE_SIMD_H__

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

// Define a unifying set of SIMD intrinsic functions.

#ifdef USE_SSE2

// PC class processor with SSE2 enabled. Optionally use additional features
// such as SSE3.

typedef __m128 __simd128_float;
typedef __m128i __simd128_int;

#define SHUFFLE(w0, w1, w2, w3) \
    (w0 | (w1 << 2) | (w2 << 4) | (w3 << 6))

static inline __simd128_int simd128_cast_float_int(__simd128_float s) {
    return _mm_castps_si128(s);
}

static inline __simd128_float simd128_cast_int_float(__simd128_int s) {
    return _mm_castsi128_ps(s);
}

static inline __simd128_float simd128_set1_float(float f) {
    return _mm_set1_ps(f);
}

// Set four float components (from least significant, lowest order segment to highest).

static inline __simd128_float simd128_set_float(float f0, float f1, float f2, float f3) {
    return _mm_set_ps(f3, f2, f1, f0);
}

static inline __simd128_float simd128_set_zero_float() {
    return _mm_setzero_ps();
}

static inline __simd128_float simd128_load_float(const float *fp) {
    return _mm_load_ps(fp);
}

static inline void simd128_store_float(float *fp, __simd128_float s) {
    _mm_store_ps(fp, s);
}

static inline __simd128_float simd128_mul_float(__simd128_float s1, __simd128_float s2) {
    return _mm_mul_ps(s1, s2);
}

static inline __simd128_float simd128_add_float(__simd128_float s1, __simd128_float s2) {
    return _mm_add_ps(s1, s2);
}

static inline __simd128_float simd128_add1_float(__simd128_float s1, __simd128_float s2) {
    return _mm_add_ss(s1, s2);
}

static inline __simd128_float simd128_sub_float(__simd128_float s1, __simd128_float s2) {
    return _mm_sub_ps(s1, s2);
}

static inline __simd128_float simd128_sub1_float(__simd128_float s1, __simd128_float s2) {
    return _mm_sub_ss(s1, s2);
}

static inline __simd128_int simd128_select_int32(__simd128_int s1,
unsigned int word0, unsigned int word1, unsigned int word2, unsigned int word3) {
    return _mm_shuffle_epi32(s1, SHUFFLE(word0, word1, word2, word3));
}

static inline __simd128_float simd128_select_float(__simd128_float s1,
unsigned int word0, unsigned int word1, unsigned int word2, unsigned int word3) {
    return simd128_cast_int_float(simd128_select_int32(simd128_cast_float_int(s1),
        word0, word1, word2, word3));
}

// Return 128-bit SSE register with 32-bit values shuffled, starting from bit 0,
// with two words from m1 (counting from LSB) and two words from m2.

static inline __simd128_float simd128_merge_float(__simd128_float s1, __simd128_float s2,
unsigned int s1_word0, unsigned int s1_word1, unsigned int s2_word0,
unsigned int s2_word1) {
    return _mm_shuffle_ps(s1, s2, SHUFFLE(s1_word0, s1_word1, s2_word0, s2_word1));
}

static inline __simd128_int simd128_merge_int32(__simd128_int s1, __simd128_int s2,
unsigned int s1_word0, unsigned int s1_word1, unsigned int s2_word0,
unsigned int s2_word1) {
    return simd128_cast_float_int(simd128_merge_float(
        simd128_cast_int_float(s1), simd128_cast_int_float(s2),
        s1_word0, s1_word1, s2_word0, s2_word1));
}

// Return 128-bit SSE register with the lowest order 32-bit float from s1 and the
// remaining 32-bit floats from s2.

static inline __simd128_float simd128_merge1_float(__simd128_float s1,
__simd128_float s2) {
    return _mm_move_ss(s2, s1);
}

static inline __simd128_int simd128_set_int32(int i0, int i1, int i2, int i3) {
    return _mm_set_epi32(i3, i2, i1, i0);
}

static inline __simd128_int simd128_set1_int32(int i) {
    return _mm_set1_epi32(i);
}

static inline __simd128_int simd128_set_zero_int() {
    return _mm_setzero_si128();
}

static inline __simd128_int simd128_and_int(__simd128_int s1, __simd128_int s2) {
    return _mm_and_si128(s1, s2);
}

static inline __simd128_int simd128_andnot_int(__simd128_int s1, __simd128_int s2) {
    return _mm_andnot_si128(s1, s2);
}

static inline __simd128_int simd128_or_int(__simd128_int s1, __simd128_int s2) {
    return _mm_or_si128(s1, s2);
}

static inline __simd128_int simd128_not_int(__simd128_int s) {
    __simd128_int m_full_mask = simd128_set1_int32(0xFFFFFFFF);
    return _mm_xor_si128(s, m_full_mask);
}

static inline int simd128_get_int32(__simd128_int s) {
    return _mm_cvtsi128_si32(s);
}

static inline float simd128_get_float(__simd128_float s) {
    return _mm_cvtss_f32(s);
}

// Comparison functions. Returns integer vector with full bitmask set for each
// element for which the condition is true.

// float

static inline __simd128_int simd128_cmpge_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmpge_ps(s1, s2));
}

static inline __simd128_int simd128_cmpgt_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmpgt_ps(s1, s2));
}

static inline __simd128_int simd128_cmple_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmple_ps(s1, s2));
}

static inline __simd128_int simd128_cmplt_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmplt_ps(s1, s2));
}

static inline __simd128_int simd128_cmpeq_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmpeq_ps(s1, s2));
}

static inline __simd128_int simd128_cmpne_float(__simd128_float s1, __simd128_float s2) {
    return simd128_cast_float_int(_mm_cmpneq_ps(s1, s2));
}

// int32

static inline __simd128_int simd128_cmpge_int32(__simd128_int s1, __simd128_int s2) {
    return simd128_not_int(_mm_cmplt_epi32(s1, s2));
}

static inline __simd128_int simd128_cmpgt_int32(__simd128_int s1, __simd128_int s2) {
    return _mm_cmpgt_epi32(s1, s2);
}

static inline __simd128_int simd128_cmple_int32(__simd128_int s1, __simd128_int s2) {
    return simd128_not_int(_mm_cmpgt_epi32(s1, s2));
}

static inline __simd128_int simd128_cmplt_int32(__simd128_int s1, __simd128_int s2) {
    return _mm_cmplt_epi32(s1, s2);
}

static inline __simd128_int simd128_cmpeq_int32(__simd128_int s1, __simd128_int s2) {
    return _mm_cmpeq_epi32(s1, s2);
}

static inline __simd128_int simd128_cmpneq_int32(__simd128_int s1, __simd128_int s2) {
    return simd128_not_int(_mm_cmpeq_epi32(s1, s2));
}

// Convert four 32-bit signed integers to four 8-bit signed integers using signed
// saturation. zerosi must be initialized as an integer vector with zeros.

static inline __simd128_int simd128_convert_int32_int8_saturate(__simd128_int s,
__simd128_int zerosi) {
    return _mm_packs_epi16(_mm_packs_epi32(s, zerosi), zerosi);
}

// Convert four 32-bit integer masks (each either 0xFFFFFFFF or 0x00000000) to 1-bit
// masks using the highest-order bit of each 32-bit value.

static inline int simd128_convert_masks_int32_int1(__simd128_int s) {
     return _mm_movemask_ps(simd128_cast_int_float(s));
}

// Shift float register right by number of 32-bit floats, shifting in zero bits
// (i.e. invalid floating point values) at the high end. n must be constant.

static inline __simd128_float simd128_shift_right_float(__simd128_float s, const int n) {
    return _mm_castsi128_ps(
        _mm_srli_si128(_mm_castps_si128(s), n * 4));
}

// Shift 128-bit integer right by n * 8 bits (n bytes).

static inline __simd128_int simd128_shift_right_bytes_int(__simd128_int s, const int n) {
    return _mm_srli_si128(s, n);
}

// Shift 32-bit unsigned integers right by n bits.

static inline __simd128_int simd128_shift_right_uint32(__simd128_int s, const int n) {
    return _mm_srli_epi32(s, n);
}

// Shift 16-bit unsigned integers right by n bits.

static inline __simd128_int simd128_shift_right_uint16(__simd128_int s, const int n) {
    return _mm_srli_epi16(s, n);
}

// Shift 32-bit signed integers right left by n bits.

static inline __simd128_int simd128_shift_right_int32(__simd128_int s, const int n) {
    return _mm_srai_epi32(s, n);
}

// Shift float register left by number of 32-bit floats, shifting in zero bits
// (i.e. invalid floating point values). n must be constant.

static inline __simd128_float simd128_shift_left_float(__simd128_float s, const int n) {
    return _mm_castsi128_ps(
        _mm_slli_si128(_mm_castps_si128(s), n * 4));
}

// Shift 128-bit integer left by n * 8 bits (n bytes).

static inline __simd128_int simd128_shift_left_bytes_int(__simd128_int s, const int n) {
    return _mm_slli_si128(s, n);
}

// Shift 32-bit signed/unsigned integers left by n bits.

static inline __simd128_int simd128_shift_left_int32(__simd128_int s, const int n) {
    return _mm_slli_epi32(s, n);
}

// Shift 16-bit signed/unsigned integers left by n bits.

static inline __simd128_int simd128_shift_left_int16(__simd128_int s, const int n) {
    return _mm_slli_epi16(s, n);
}

// Calculate the mimimum of each component of two vectors.

static inline __simd128_float simd128_min_float(__simd128_float s1, __simd128_float s2) {
     return _mm_min_ps(s1, s2);
}

// Calculate the maximum of each component of two vectors.

static inline __simd128_float simd128_max_float(__simd128_float s1, __simd128_float s2) {
     return _mm_max_ps(s1, s2);
}

// Calculate the mimimum of the first component of two vectors.

static inline __simd128_float simd128_min1_float(__simd128_float s1, __simd128_float s2) {
     return _mm_min_ss(s1, s2);
}

// Calculate the maximum of the first component of two vectors.

static inline __simd128_float simd128_max1_float(__simd128_float s1, __simd128_float s2) {
     return _mm_max_ss(s1, s2);
}

#ifdef USE_SSE3

// Horizontally add pairs of elements over two four-float vectors and return
// in a single vector. Generally inefficient even when available as a single instruction
// (SSE3).

static inline __simd128_float simd128_horizontal_add2_float(__simd128_float s1,
__simd128_float s2) {
    return _mm_hadd_ps(s1, s2);
}

#endif

// Load three floats without accessing memory beyond them.

static inline __simd128_float simd128_load3_float(const float *f) {
    // Take care not to read beyond the allocated data structure.
    __simd128_float m_zeros = simd128_set_zero_float();
    // temp1: Load v.x and v.y into the lower two words (with 0.0f for the rest),
    // temp2: Load v.z into the low word (with 0.0f for the rest).
    // Merge temp1 and temp2 so that (v.x, v.y, v.z, 0.0f) is returned.
    return simd128_merge_float(
        _mm_loadl_pi(m_zeros, (const __m64 *)f),
        _mm_load_ss(&f[2]),
        0, 1, 0, 1
        );
}
// Store three floats without accessing memory beyond the stored elements.

static inline void simd128_store3_float(float *f, __simd128_float m_v) {
    // Take care not to write beyond the allocated data structure.
    _mm_storel_pi((__m64 *)f, m_v);
    _mm_store_ss(&f[2], simd128_shift_right_float(m_v, 2));
}

// Symmetrically transpose a 4x4 matrix.

// In place.

static inline void simd128_transpose4_float(__simd128_float& row0,
__simd128_float& row1, __simd128_float& row2, __simd128_float& row3) {
   _MM_TRANSPOSE4_PS(row0, row1, row2, row3);
}

// Put result in different registers.

static inline void simd128_transpose4to4_float(const __simd128_float row0,
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

static inline void simd128_transpose4to3_float(const __simd128_float m_v0,
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

static inline void simd128_transpose3to4_float(const __simd128_float m_v0,
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

#endif

#ifdef USE_ARM_NEON
#endif

// Define shared set of generic SIMD functions shared between different platforms
// that are implemented using lower-level SIMD functions.

#ifdef USE_SIMD

static inline __simd128_float simd128_load(const Vector4D *v) {
    return simd128_load_float(&v->x);
}

static inline __simd128_float simd128_load(const Vector3D *v) {
    // When Vector3D is stored as 16 bytes (the same amount of space as Vector4D),
    // a standard 16-byte aligned load can be used.
    // Rely on compiler optimization to eliminate the conditional and unused code
    // at compile time.
    if (sizeof(Vector3D) == 16)
        return simd128_load_float(&v->x);
    else
        // Otherwise, take care not to read beyond the allocated data structure.
        return simd128_load3_float(&v->x);
}

static inline void simd128_store(Vector4D *v, __simd128_float m_v) {
    simd128_store_float(&v->x, m_v);
}

static inline void simd128_store(Vector3D *v, __simd128_float m_v) {
    // When Vector3D is stored as 16 bytes (the same amount of space as Vector4D),
    // a standard 16-byte aligned store can be used.
    // Rely on compiler optimization to eliminate the conditional and unused code
    // at compile time.
    if (sizeof(Vector3D) == 16) {
        simd128_store_float(&v->x, m_v);
        return;
    }
    else
        // Otherwise, take care not to write beyond the allocated data structure.
        simd128_store3_float(&v->x, m_v);
}

// Calculate the minimum of the four components of a vector, and store result in the
// first component.

static inline __simd128_float simd128_horizonal_min_float(__simd128_float s) {
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

static inline __simd128_float simd128_horizonal_max_float(__simd128_float s) {
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

static inline __simd128_float simd128_horizontal_add2_float(__simd128_float s1,
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

static inline __simd128_float simd128_horizontal_add4_float(__simd128_float s) {
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

#endif // defined(USE_SIMD)

#ifdef USE_SIMD

// SSE matrix multiplication for matrices in column-major order.
// Requires 16-byte alignment of the matrices.

static inline void SIMDMatrixMultiply4x4(const float * __restrict m1,
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

// Multiply 4x3 (4 rows, 3 rows) matrices in row-major order (different from
// other matrices which are in column-major order).

static inline void SIMDMatrixMultiply4x3(const float * __restrict m1,
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

// Multiply 4x4 and 4x3 (4 rows, 3 columns) matrices. The 4x3 matrix is in row-major
// order (different from the 4x4 matrix which is in column-major order).

static inline void SIMDMatrixMultiply4x4By4x3(const float * __restrict m1,
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

static inline void SIMDCalculateFourDotProductsNoStore(
const Vector4D * __restrict v1, const Vector4D * __restrict v2, __simd128_float& result) {
#if 1
    // Loading and using tranpose seems to be more efficient than set_float calls, which
    // expand to numerous instructions.
    __simd128_float m_v1_0 = simd128_load(&v1[0]);
    __simd128_float m_v1_1 = simd128_load(&v1[1]);
    __simd128_float m_v1_2 = simd128_load(&v1[2]);
    __simd128_float m_v1_3 = simd128_load(&v1[3]);
    __simd128_float m_v2_0 = simd128_load(&v2[0]);
    __simd128_float m_v2_1 = simd128_load(&v2[1]);
    __simd128_float m_v2_2 = simd128_load(&v2[2]);
    __simd128_float m_v2_3 = simd128_load(&v2[3]);
    __simd128_float m_v1_x, m_v1_y, m_v1_z, m_v1_w;
    simd128_transpose4to4_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3, m_v1_x, m_v1_y, m_v1_z, m_v1_w);
    __simd128_float m_v2_x, m_v2_y, m_v2_z, m_v2_w;
    simd128_transpose4to4_float(m_v2_0, m_v2_1, m_v2_2, m_v2_3, m_v2_x, m_v2_y, m_v2_z, m_v2_w);
#else
    __simd128_float m_v1_x = simd128_set_float(v1[0].x, v1[1].x, v1[2].x, v1[3].x);
    __simd128_float m_v1_y = simd128_set_float(v1[0].y, v1[1].y, v1[2].y, v1[3].y);
    __simd128_float m_v1_z = simd128_set_float(v1[0].z, v1[1].z, v1[2].z, v1[3].z);
    __simd128_float m_v1_w = simd128_set_float(v1[0].w, v1[1].w, v1[2].w, v1[3].w);
    __simd128_float m_v2_x = simd128_set_float(v2[0].x, v2[1].x, v2[2].x, v2[3].x);
    __simd128_float m_v2_y = simd128_set_float(v2[0].y, v2[1].y, v2[2].y, v2[3].y);
    __simd128_float m_v2_z = simd128_set_float(v2[0].z, v2[1].z, v2[2].z, v2[3].z);
    __simd128_float m_v2_w = simd128_set_float(v2[0].w, v2[1].w, v2[2].w, v2[3].w);
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

static inline void SIMDCalculateFourDotProductsNoStore(
const Vector3D * __restrict v1, const Vector3D * __restrict v2, __simd128_float& result) {
#if 1
    __simd128_float m_v1_0 = simd128_load(&v1[0]);
    __simd128_float m_v1_1 = simd128_load(&v1[1]);
    __simd128_float m_v1_2 = simd128_load(&v1[2]);
    __simd128_float m_v1_3 = simd128_load(&v1[3]);
    __simd128_float m_v2_0 = simd128_load(&v2[0]);
    __simd128_float m_v2_1 = simd128_load(&v2[1]);
    __simd128_float m_v2_2 = simd128_load(&v2[2]);
    __simd128_float m_v2_3 = simd128_load(&v2[3]);
    __simd128_float m_v1_x, m_v1_y, m_v1_z;
    simd128_transpose4to3_float(m_v1_0, m_v1_1, m_v1_2, m_v1_3, m_v1_x, m_v1_y, m_v1_z);
    __simd128_float m_v2_x, m_v2_y, m_v2_z;
    simd128_transpose4to3_float(m_v2_0, m_v2_1, m_v2_2, m_v2_3, m_v2_x, m_v2_y, m_v2_z);
#else
    __simd128_float m_v1_x = simd128_set_float(v1[0].x, v1[1].x, v1[2].x, v1[3].x);
    __simd128_float m_v1_y = simd128_set_float(v1[0].y, v1[1].y, v1[2].y, v1[3].y);
    __simd128_float m_v1_z = simd128_set_float(v1[0].z, v1[1].z, v1[2].z, v1[3].z);
    __simd128_float m_v2_x = simd128_set_float(v2[0].x, v2[1].x, v2[2].x, v2[3].x);
    __simd128_float m_v2_y = simd128_set_float(v2[0].y, v2[1].y, v2[2].y, v2[3].y);
    __simd128_float m_v2_z = simd128_set_float(v2[0].z, v2[1].z, v2[2].z, v2[3].z);
#endif
    __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
    __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
    __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
    result = simd128_add_float(
          simd128_add_float(m_dot_x, m_dot_y),
          m_dot_z
          );
}

static inline void SIMDCalculateFourDotProducts(const Vector4D * __restrict v1,
const Vector4D * __restrict v2, float * __restrict dot) {
    __simd128_float m_result;
    SIMDCalculateFourDotProductsNoStore(v1, v2, m_result);
    simd128_store_float(dot, m_result);
}

static inline void SIMDCalculateFourDotProducts(const Vector3D * __restrict v1,
const Vector3D * __restrict v2, float * __restrict dot) {
    __simd128_float m_result;
    SIMDCalculateFourDotProductsNoStore(v1, v2, m_result);
    simd128_store_float(dot, m_result);
}

// Data type for using SIMD to multiply a specific matrix with one or more vertices.

class Matrix4DSIMD {
public :
    // When performing single vector multiplication, matrix row SIMD variables are
    // needed.
    __simd128_float m_row0;
    __simd128_float m_row1;
    __simd128_float m_row2;
    __simd128_float m_row3;

    void Set(const Matrix4D& m) {
        // Load columns, then transpose to rows.
        m_row0 = simd128_load_float(&m.n[0][0]);
        m_row1 = simd128_load_float(&m.n[1][0]);
        m_row2 = simd128_load_float(&m.n[2][0]);
        m_row3 = simd128_load_float(&m.n[3][0]);
        simd128_transpose4_float(m_row0, m_row1, m_row2, m_row3);
    }
    inline void Multiply(const __simd128_float m_v, __simd128_float& m_result) const {
        __simd128_float m_mul0  = simd128_mul_float(m_row0, m_v);
        __simd128_float m_mul1  = simd128_mul_float(m_row1, m_v);
        __simd128_float m_mul2  = simd128_mul_float(m_row2, m_v);
        __simd128_float m_mul3  = simd128_mul_float(m_row3, m_v);
        m_result =
            simd128_horizontal_add2_float(
                simd128_horizontal_add2_float(m_mul0, m_mul1),
                simd128_horizontal_add2_float(m_mul2, m_mul3));
    }
    inline Vector4D Multiply(const __simd128_float m_v) const {
        __simd128_float m_result;
        Multiply(m_v, m_result);
        return Vector4D(
            simd128_get_float(m_result),
            simd128_get_float(simd128_shift_right_float(m_result, 1)),
            simd128_get_float(simd128_shift_right_float(m_result, 2)),
            simd128_get_float(simd128_shift_right_float(m_result, 3))
            );
    }
    friend inline Vector4D operator *(const Matrix4DSIMD& m, const Vector4D& V) {
        return m.Multiply(simd128_set_float(V.x, V.y, V.z, V.w));
    }
    friend inline Vector4D operator *(const Matrix4DSIMD& m, const Vector4D *Vp) {
        return m.Multiply(simd128_load_float(&Vp[0][0]));
    }
};

// Multiple a 4x4 Matrix by a vector, and store the result in a vector.
// Matrix4DSIMD m must be initialized.

static inline void SIMDMatrixMultiplyVector(const Matrix4DSIMD& m, const Vector4D *v1p,
Vector4D *v2p) {
    __simd128_float m_v1 = simd128_load_float(&v1p[0][0]);
    __simd128_float m_result;
    m.Multiply(m_v1, m_result);
    simd128_store_float(&v2p[0][0], m_result);
}

static inline void SIMDMatrixMultiplyFourVectors(const Matrix4DSIMD& m,
const Vector4D *v_source_p, Vector4D *v_result_p) {
    // Load four vectors and transpose so that all similar coordinates are
    // stored in a single vector.
    __simd128_float m_v_x = simd128_load(&v_source_p[0]);
    __simd128_float m_v_y = simd128_load(&v_source_p[1]);
    __simd128_float m_v_z = simd128_load(&v_source_p[2]);
    __simd128_float m_v_w = simd128_load(&v_source_p[3]);
    simd128_transpose4_float(m_v_x, m_v_y, m_v_z, m_v_w);
    __simd128_float m_mul0, m_mul1, m_mul2, m_mul3;
    // Process the x coordinates.
    m_mul0 = simd128_mul_float(m.m_row0, m_v_x);
    m_mul1 = simd128_mul_float(m.m_row0, m_v_y);
    m_mul2 = simd128_mul_float(m.m_row0, m_v_z);
    m_mul3 = simd128_mul_float(m.m_row0, m_v_w);
    __simd128_float m_result_x =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Process the y coordinates.
    m_mul0 = simd128_mul_float(m.m_row1, m_v_x);
    m_mul1 = simd128_mul_float(m.m_row1, m_v_y);
    m_mul2 = simd128_mul_float(m.m_row1, m_v_z);
    m_mul3 = simd128_mul_float(m.m_row1, m_v_w);
    __simd128_float m_result_y =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Process the z coordinates.
    m_mul0 = simd128_mul_float(m.m_row2, m_v_x);
    m_mul1 = simd128_mul_float(m.m_row2, m_v_y);
    m_mul2 = simd128_mul_float(m.m_row2, m_v_z);
    m_mul3 = simd128_mul_float(m.m_row2, m_v_w);
    __simd128_float m_result_z =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Process the w coordinates.
    m_mul0 = simd128_mul_float(m.m_row3, m_v_x);
    m_mul1 = simd128_mul_float(m.m_row3, m_v_y);
    m_mul2 = simd128_mul_float(m.m_row3, m_v_z);
    m_mul3 = simd128_mul_float(m.m_row3, m_v_w);
    __simd128_float m_result_w =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Transpose results so that each vector holds multiplication product.
    simd128_transpose4_float(m_result_x, m_result_y, m_result_z, m_result_w); 
    // Store the results as Vector4D.
    simd128_store(&v_result_p[0], m_result_x);
    simd128_store(&v_result_p[1], m_result_y);
    simd128_store(&v_result_p[2], m_result_z);
    simd128_store(&v_result_p[3], m_result_w);
}

#ifdef SIMD_HAVE_MATRIX4X3_VECTOR_MULTIPLICATION

// Requires simd128_transpose4to3_float and simd_transpose3to4_float.

class MatrixTransformSIMD {
public :
    __simd128_float m_row0;
    __simd128_float m_row1;
    __simd128_float m_row2;

    void Set(const MatrixTransform& m) {
        m_row0 = simd128_load_float(&m.n[0][0]);
        m_row1 = simd128_load_float(&m.n[1][0]);
        m_row2 = simd128_load_float(&m.n[2][0]);
    }
    inline void Multiply(const __simd128_float m_v, __simd128_float& m_result) const {
        __simd128_float m_mul0  = simd128_mul_float(m_row0, m_v);
        __simd128_float m_mul1  = simd128_mul_float(m_row1, m_v);
        __simd128_float m_mul2  = simd128_mul_float(m_row2, m_v);
        __simd128_float m_zeros = simd128_set_zero_float();
        m_result =
            simd128_horizontal_add2_float(
                simd128_horizontal_add2_float(m_mul0, m_mul1),
                simd128_horizontal_add2_float(m_mul2, m_zeros));
    }
    inline Vector3D Multiply(const __simd128_float m_v) const {
        __simd128_float m_result;
        Multiply(m_v, m_result);
        return Vector3D(
            simd128_get_float(m_result),
            simd128_get_float(simd128_shift_right_float(m_result, 1)),
            simd128_get_float(simd128_shift_right_float(m_result, 2))
            );
    }
    friend inline Vector3D operator *(const MatrixTransformSIMD& m, const Vector3D& V) {
        return m.Multiply(simd128_set_float(V.x, V.y, V.z, 0.0f));
    }
    friend inline Vector3D operator *(const MatrixTransformSIMD& m, const Vector3D *Vp) {
        return m.Multiply(simd128_load_float(&Vp[0][0]));
    }
};

// Multiple a 4x3 matrix by a vector, and store the result in a vector.
// MatrixTransformSIMD m must be initialized.

static inline void SIMDMatrixMultiplyVector(const MatrixTransformSIMD& m,
const Vector3D *v1p, Vector3D *v2p) {
    // Using simd128_load(const Vector3D *v).
    __simd128_float m_v1 = simd128_load(&v1p[0]);
    __simd128_float m_result;
    m.Multiply(m_v1, m_result);
    // Using simd128_store(Vector3D *v).
    simd128_store(&v2p[0], m_result);
}

static inline void SIMDMatrixMultiplyFourVectors(const MatrixTransformSIMD& m,
const Vector3D *v_source_p, Vector3D *v_result_p) {
    // Load four vectors and transpose so that all similar coordinates are
    // stored in a single vector.
    __simd128_float m_v0 = simd128_load(&v_source_p[0]);
    __simd128_float m_v1 = simd128_load(&v_source_p[1]);
    __simd128_float m_v2 = simd128_load(&v_source_p[2]);
    __simd128_float m_v3 = simd128_load(&v_source_p[3]);
    // Transpose four three-float vectors, and store result in m_v_x, m_v_y and m_v_z.
    __simd128_float m_v_x, m_v_y, m_v_z;
    simd128_transpose4to3_float(m_v0, m_v1, m_v2, m_v3, m_v_x, m_v_y, m_v_z);
    __simd128_float m_mul0, m_mul1, m_mul2;
    // Process the x coordinates.
    m_mul0 = simd128_mul_float(m.m_row0, m_v_x);
    m_mul1 = simd128_mul_float(m.m_row0, m_v_y);
    m_mul2 = simd128_mul_float(m.m_row0, m_v_z);
    __simd128_float m_result_x =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            m_mul2);
    // Process the y coordinates.
    m_mul0 = simd128_mul_float(m.m_row1, m_v_x);
    m_mul1 = simd128_mul_float(m.m_row1, m_v_y);
    m_mul2 = simd128_mul_float(m.m_row1, m_v_z);
    __simd128_float m_result_y =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            m_mul2);
    // Process the z coordinates.
    m_mul0 = simd128_mul_float(m.m_row2, m_v_x);
    m_mul1 = simd128_mul_float(m.m_row2, m_v_y);
    m_mul2 = simd128_mul_float(m.m_row2, m_v_z);
    __simd128_float m_result_z =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            m_mul2);
    // Transpose results so that each vector holds multiplication product.
    __simd128_float m_result_v0, m_result_v1, m_result_v2, m_result_v3;
    simd128_transpose3to4_float(m_result_x, m_result_y, m_result_z,
        m_result_v0, m_result_v1, m_result_v2, m_result_v3); 
    simd128_store(&v_result_p[0], m_result_v0);
    simd128_store(&v_result_p[1], m_result_v1);
    simd128_store(&v_result_p[2], m_result_v2);
    simd128_store(&v_result_p[3], m_result_v3);
}

#endif // SIMD_HAVE_MATRIX4X3_VECTOR_MULTIPLICATION

#endif // USE_SIMD

#endif