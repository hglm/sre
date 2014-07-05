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

// SIMD library functions, mostly implemented using inline functions defined
// in sreSIMDPlatform.h.

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "sreSIMDPlatform.h"

#ifdef USE_SIMD

void simd_matrix_multiply_4x4CM_float(const float * __restrict m1,
const float * __restrict__ m2, float * __restrict m3) {
    simd_inline_matrix_multiply_4x4CM_float(m1, m2, m3);
}

void simd_matrix_multiply_4x3RM_float(const float * __restrict m1,
const float * __restrict__ m2, float * __restrict m3) {
    simd_inline_matrix_multiply_4x3RM_float(m1, m2, m3);
}

void simd_matrix_multiply_4x4CM_4x3RM_float(const float * __restrict m1,
const float * __restrict__ m2, float * __restrict m3) {
    simd_inline_matrix_multiply_4x4CM_4x3RM_float(m1, m2, m3);
}

void simd_four_dot_products_vector4_float(
const float * __restrict f1, const float * __restrict f2, __simd128_float& result) {
    simd_inline_four_dot_products_vector4_float(f1, f2, result);
}

void simd_four_dot_products_vector3_storage4_float(const float * __restrict f1,
const float * __restrict f2, __simd128_float& result) {
    simd_four_dot_products_vector3_storage4_float(f1, f2, result);
}

void simd_four_dot_products_vector3_storage3_float(const float * __restrict f1,
const float * __restrict f2, __simd128_float& result) {
    simd_four_dot_products_vector3_storage3_float(f1, f2, result);
}

void simd_dot_product_nx1_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot) {
    simd_inline_dot_product_nx1_vector4_float(n, f1, f2, dot);
}

void simd_dot_product_nx1_vector3_storage3_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot) {
    simd_inline_dot_product_nx1_vector3_storage3_float(n, f1, f2, dot);
}

void simd_dot_product_nx1_vector3_storage4_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot) {
    simd_inline_dot_product_nx1_vector3_storage4_vector4_float(n, f1, f2, dot);
}

void simd_dot_product_nx1_point3_storage4_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot) {
    simd_inline_dot_product_nx1_point3_storage4_vector4_float(n, f1, f2, dot);
}

void simd_dot_product_nx1_point3_storage3_vector4_float(int n,
const float * __restrict f1, const float * __restrict f2, float * __restrict dot) {
    simd_inline_dot_product_nx1_point3_storage3_vector4_float(n, f1, f2, dot);
}

void simd_dot_product_nx1_point3_storage4_vector4_and_count_negative_float(
int n, const float * __restrict f1, const float * __restrict f2, float * __restrict dot,
int& negative_count) {
    simd_inline_dot_product_nx1_point3_storage4_vector4_and_count_negative_float(
        n, f1, f2, dot, negative_count);
}

void simd_dot_product_nx1_point3_storage3_vector4_and_count_negative_float(
int n, const float * __restrict f1, const float * __restrict f2, float * __restrict dot,
int& negative_count) {
    simd_inline_dot_product_nx1_point3_storage3_vector4_and_count_negative_float(
        n, f1, f2, dot, negative_count);
}

#endif
