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

// Define a unifying set of SIMD intrinsic functions.

#ifdef USE_SSE2

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

static inline __simd128_float simd128_sub_float(__simd128_float s1, __simd128_float s2) {
    return _mm_sub_ps(s1, s2);
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
// saturation. zerosi must be initialized as an integer vector with zeroes.

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

#endif

#ifdef USE_ARM_NEON
#endif

#ifdef USE_SSE2

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

#endif

#ifdef USE_SSE2

static inline void SIMDCalculateFourDotProductsV4NoStore(
const Vector4D * __restrict v1, const Vector4D * __restrict v2, __simd128_float& result) {
    __simd128_float m_v1_x = simd128_set_float(v1[3].x, v1[2].x, v1[1].x, v1[0].x);
    __simd128_float m_v1_y = simd128_set_float(v1[3].y, v1[2].y, v1[1].y, v1[0].y);
    __simd128_float m_v1_z = simd128_set_float(v1[3].z, v1[2].z, v1[1].z, v1[0].z);
    __simd128_float m_v1_w = simd128_set_float(v1[3].w, v1[2].w, v1[1].w, v1[0].w);
    __simd128_float m_v2_x = simd128_set_float(v2[3].x, v2[2].x, v2[1].x, v2[0].x);
    __simd128_float m_v2_y = simd128_set_float(v2[3].y, v2[2].y, v2[1].y, v2[0].y);
    __simd128_float m_v2_z = simd128_set_float(v2[3].z, v2[2].z, v2[1].z, v2[0].z);
    __simd128_float m_v2_w = simd128_set_float(v2[3].w, v2[2].w, v2[1].w, v2[0].w);
    __simd128_float m_dot_x = simd128_mul_float(m_v1_x, m_v2_x);
    __simd128_float m_dot_y = simd128_mul_float(m_v1_y, m_v2_y);
    __simd128_float m_dot_z = simd128_mul_float(m_v1_z, m_v2_z);
    __simd128_float m_dot_w = simd128_mul_float(m_v1_w, m_v2_w);
    result = simd128_add_float(
          simd128_add_float(m_dot_x, m_dot_y),
          simd128_add_float(m_dot_z, m_dot_w)
          );
}

static inline void SIMDCalculateFourDotProductsV3NoStore(
const Vector3D * __restrict v1, const Vector3D * __restrict v2, __simd128_float& result) {
    __simd128_float m_v1_x = simd128_set_float(v1[3].x, v1[2].x, v1[1].x, v1[0].x);
    __simd128_float m_v1_y = simd128_set_float(v1[3].y, v1[2].y, v1[1].y, v1[0].y);
    __simd128_float m_v1_z = simd128_set_float(v1[3].z, v1[2].z, v1[1].z, v1[0].z);
    __simd128_float m_v2_x = simd128_set_float(v2[3].x, v2[2].x, v2[1].x, v2[0].x);
    __simd128_float m_v2_y = simd128_set_float(v2[3].y, v2[2].y, v2[1].y, v2[0].y);
    __simd128_float m_v2_z = simd128_set_float(v2[3].z, v2[2].z, v2[1].z, v2[0].z);
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
    __simd128_float result;
    SIMDCalculateFourDotProductsV4NoStore(v1, v2, result);
    simd128_store_float(dot, result);
}

static inline void SIMDCalculateFourDotProducts(const Vector3D * __restrict v1,
const Vector3D * __restrict v2, float * __restrict dot) {
    __simd128_float result;
    SIMDCalculateFourDotProductsV3NoStore(v1, v2, result);
    simd128_store_float(dot, result);
}

#endif

#endif
