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

// This header file defines inline SIMD matrix and vector processing functions
// for data types defined in sreVectorMath.h. It include the file sreSIMDPlatform.h,
// which defines a common set of SIMD primitives, shared between platforms,
// with certain features depending on the platform. Most primitives map to a single
// SIMD instruction, others are emulated using multiple instructions, depending on
// on the platform.

// Proving these functions inline has benefits; they can be integrated into
// any pure or mixed SIMD processing function, which will enable the compiler
// to automatically mix/shuffle the code for advanced scheduling optimizations.

#ifndef __SRE_SIMD_H__
#define __SRE_SIMD_H__

#include "sreSIMDPlatform.h"

#ifdef USE_SIMD

// Loading/storing Vector3D/Vector3D/Point3D structures. The implementation
// of Vector3D/Point3D depends on whether the structures are aligned on
// 16-byte boundaries or packed every 12 bytes. Vector4D structures are assumed
// to have 16-byte alignment.

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

// Load a vector and set the fourth component w to 0.0f.

static inline __simd128_float simd128_load_and_set_w(const Vector3D *v) {
    // When Vector3D is stored as 16 bytes (the same amount of space as Vector4D),
    // a standard 16-byte aligned load can be used.
    // Rely on compiler optimization to eliminate the conditional and unused code
    // at compile time.
    if (sizeof(Vector3D) == 16) {
        __simd128_float m_v = simd128_load_float(&v->x);
        // The w component as loaded is undefined.
        // Set w to 0.0f.
        __simd128_float m_zeros = simd128_set_zero_float();
        __simd128_int m_mask_012x = simd128_shift_right_bytes_int(
            simd128_set_same_int32(0xFFFFFFFF), 4);
        return simd128_cast_int_float(simd128_or_int(
            simd128_and_int(m_mask_012x, simd128_cast_float_int(m_v)),
            simd128_andnot_int(m_mask_012x, simd128_cast_float_int(m_zeros))
            ));
    }
    else
        // Otherwise, take care not to read beyond the allocated data structure.
        // w is already set to 0.0f by simd128_load3_float.
        return simd128_load3_float(&v->x);
}

static inline __simd128_float simd128_load(const Point3D *p) {
    return simd128_load(&p->GetVector3D());
}

// Load a point vector and set the fourth component w to 1.0f.

static inline __simd128_float simd128_load_and_set_w(const Point3D *p) {
    __simd128_float m_v;
    if (sizeof(Vector3D) == 16)
         m_v = simd128_load_float(&p->x);
    else
         m_v = simd128_load3_float(&p->x);
    // The w component as loaded is undefined or 0.0f.
    // Set w to 1.0f.
    __simd128_float m_ones = simd128_set_same_float(1.0f);
    __simd128_int m_mask_012x = simd128_shift_right_bytes_int(
        simd128_set_same_int32(0xFFFFFFFF), 4);
    return simd128_cast_int_float(simd128_or_int(
        simd128_and_int(m_mask_012x, simd128_cast_float_int(m_v)),
        simd128_andnot_int(m_mask_012x, simd128_cast_float_int(m_ones))
        ));
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

static inline void simd128_store(Point3D *p, __simd128_float m_v) {
    simd128_store(&p->GetVector3D(), m_v);
}

// SSE matrix multiplication for matrices in column-major order.
// Requires 16-byte alignment of the matrices.

static inline void SIMDMatrixMultiply(const Matrix4D& __restrict m1,
const Matrix4D& __restrict__ m2, Matrix4D& __restrict m3) {
    simd_matrix_multiply_4x4CM_float(&m1.n[0][0], &m2.n[0][0], &m3.n[0][0]);
}

// Multiply 4x3 (4 rows, 3 rows) matrices in row-major order (different from
// other matrices which are in column-major order).
// The fourth row is implicitly defined as (0.0f, 0.0f, 0.0f, 1.0f).

static inline void SIMDMatrixMultiply(const MatrixTransform& __restrict m1,
const MatrixTransform& __restrict__ m2, MatrixTransform& __restrict m3) {
    simd_matrix_multiply_4x3RM_float(&m1.n[0][0], &m2.n[0][0], &m3.n[0][0]);
}

// Multiply 4x4 and 4x3 (4 rows, 3 columns) matrices. The 4x3 matrix is in row-major
// order (different from the 4x4 matrix which is in column-major order).
// The fourth row of the 4x3 matrix is implicitly defined as (0.0f, 0.0f, 0.0f, 1.0f).

static inline void SIMDMatrixMultiply(const Matrix4D& __restrict m1,
const MatrixTransform& __restrict__ m2, Matrix4D& __restrict m3) {
    simd_matrix_multiply_4x4CM_4x3RM_float(&m1.n[0][0], &m2.n[0][0], &m3.n[0][0]);
}

// Calculate four dot products from two vector arrays and return the results in a
// SIMD register.

static inline void SIMDCalculateFourDotProductsNoStore(
const Vector4D * __restrict v1, const Vector4D * __restrict v2, __simd128_float& result) {
    simd_four_dot_products_vector4_float(&v1[0].x, &v2[0].x, result);
}

static inline void SIMDCalculateFourDotProductsNoStore(
const Vector3D * __restrict v1, const Vector3D * __restrict v2, __simd128_float& result) {
    if (sizeof(Vector3D) == 16)
        simd_four_dot_products_vector3_storage4_float(&v1[0].x, &v2[0].x, result);
    else
        simd_four_dot_products_vector3_storage3_float(&v1[0].x, &v2[0].x, result);
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

// Calculate n dot products from one vector array and one constant vector,
// and store the result in an array of floats.

static inline void SIMDCalculateDotProductsWithConstantVector(int n,
const Vector4D * __restrict v1, const Vector4D& __restrict v2, float * __restrict dot) {
    simd_dot_product_nx1_vector4_float(n, &v1[0].x, &v2.x, dot);
}

static inline void SIMDCalculateDotProductsWithConstantVector(int n,
const Vector3D * __restrict v1, const Vector4D& __restrict v2, float * __restrict dot) {
    if (sizeof(Vector3D) == 16)
        simd_dot_product_nx1_vector3_storage4_vector4_float(n, &v1[0].x, &v2.x, dot);
    else
        simd_dot_product_nx1_vector3_storage3_float(n, &v1[0].x, &v2.x, dot);
}

static inline void SIMDCalculateDotProductsWithConstantVector(int n,
const Point3D * __restrict p1, const Vector4D& __restrict v2, float * __restrict dot) {
    if (sizeof(Vector3D) == 16)
        simd_dot_product_nx1_point3_storage4_vector4_float(n, &p1[0].x, &v2.x, dot);
    else
        simd_dot_product_nx1_point3_storage3_vector4_float(n, &p1[0].x, &v2.x, dot);
}

static inline void SIMDCalculateFourDotProductsWithConstantVector(
const Vector4D * __restrict v1, const Vector4D& __restrict v2, float * __restrict dot) {
    SIMDCalculateDotProductsWithConstantVector(4, v1, v2, dot);
}

static inline void SIMDCalculateFourDotProductsWithConstantVector(
const Vector3D * __restrict v1, const Vector3D& __restrict v2, float * __restrict dot) {
    SIMDCalculateDotProductsWithConstantVector(4, v1, v2, dot);
}

static inline void SIMDCalculateFourDotProductsWithConstantVector(
const Point3D * __restrict p1, const Vector4D& __restrict v2, float * __restrict dot) {
    SIMDCalculateDotProductsWithConstantVector(4, p1, v2, dot);
}

static inline void SIMDCalculateDotProductsWithConstantVectorAndCountNegative(int n,
const Point3D * __restrict p1, const Vector4D& __restrict v2, float * __restrict dot,
int& negative_count) {
   __simd128_float m_v2 = simd128_load(&v2);
    __simd128_float m_v2_x, m_v2_y, m_v2_z, m_v2_w;
    m_v2_x = simd128_select_float(m_v2, 0, 0, 0, 0);
    m_v2_y = simd128_select_float(m_v2, 1, 1, 1, 1);
    m_v2_z = simd128_select_float(m_v2, 2, 2, 2, 2);
    m_v2_w = simd128_select_float(m_v2, 3, 3, 3, 3);
    __simd128_float m_zeros = simd128_set_zero_float();
    int count = 0;
    int i = 0;
    for (; i + 3 < n; i += 4) {
        __simd128_float m_v1_0 = simd128_load(&p1[i + 0]);
        __simd128_float m_v1_1 = simd128_load(&p1[i + 1]);
        __simd128_float m_v1_2 = simd128_load(&p1[i + 2]);
        __simd128_float m_v1_3 = simd128_load(&p1[i + 3]);
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
        __simd128_float m_v1 = simd128_load_and_set_w(&p1[i]);
        __simd128_float m_dot = simd128_mul_float(m_v1, m_v2);
        __simd128_float m_result = simd128_horizontal_add4_float(m_dot);
        simd128_store_first_float(&dot[i], m_result);
        count += (simd128_get_float(m_result) < 0.0f);
    }
    negative_count = count;
}

static inline void SIMDCalculateFourDotProductsWithConstantVectorAndCountNegative(
const Point3D * __restrict p1, const Vector4D& __restrict v2, float * __restrict dot,
int& negative_count) {
    SIMDCalculateDotProductsWithConstantVectorAndCountNegative(4, p1, v2, dot,
        negative_count);
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
    __simd128_float m_v1 = simd128_load(v1p);
    __simd128_float m_result;
    m.Multiply(m_v1, m_result);
    simd128_store(v2p, m_result);
}

static inline void SIMDMatrixMultiplyFourVectorsNoStore(const Matrix4DSIMD& m,
const Vector4D *v_source_p,
__simd128_float& m_result_0, __simd128_float& m_result_1, __simd128_float& m_result_2,
__simd128_float& m_result_3) {
    // Load four vectors and transpose so that all similar coordinates are
    // stored in a single vector.
    __simd128_float m_v_x = simd128_load(&v_source_p[0]);
    __simd128_float m_v_y = simd128_load(&v_source_p[1]);
    __simd128_float m_v_z = simd128_load(&v_source_p[2]);
    __simd128_float m_v_w = simd128_load(&v_source_p[3]);
    simd128_transpose4_float(m_v_x, m_v_y, m_v_z, m_v_w);
    __simd128_float m_mul0, m_mul1, m_mul2, m_mul3;
    // Process the x coordinates.
    m_mul0 = simd128_mul_float(simd128_select_float(m.m_row0, 0, 0, 0, 0), m_v_x);
    m_mul1 = simd128_mul_float(simd128_select_float(m.m_row0, 1, 1, 1, 1), m_v_y);
    m_mul2 = simd128_mul_float(simd128_select_float(m.m_row0, 2, 2, 2, 2), m_v_z);
    m_mul3 = simd128_mul_float(simd128_select_float(m.m_row0, 3, 3, 3, 3), m_v_w);
    __simd128_float m_result_x =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Process the y coordinates.
    m_mul0 = simd128_mul_float(simd128_select_float(m.m_row1, 0, 0, 0, 0), m_v_x);
    m_mul1 = simd128_mul_float(simd128_select_float(m.m_row1, 1, 1, 1, 1), m_v_y);
    m_mul2 = simd128_mul_float(simd128_select_float(m.m_row1, 2, 2, 2, 2), m_v_z);
    m_mul3 = simd128_mul_float(simd128_select_float(m.m_row1, 3, 3, 3, 3), m_v_w);
    __simd128_float m_result_y =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Process the z coordinates.
    m_mul0 = simd128_mul_float(simd128_select_float(m.m_row2, 0, 0, 0, 0), m_v_x);
    m_mul1 = simd128_mul_float(simd128_select_float(m.m_row2, 1, 1, 1, 1), m_v_y);
    m_mul2 = simd128_mul_float(simd128_select_float(m.m_row2, 2, 2, 2, 2), m_v_z);
    m_mul3 = simd128_mul_float(simd128_select_float(m.m_row2, 3, 3, 3, 3), m_v_w);
    __simd128_float m_result_z =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Process the w coordinates.
    m_mul0 = simd128_mul_float(simd128_select_float(m.m_row3, 0, 0, 0, 0), m_v_x);
    m_mul1 = simd128_mul_float(simd128_select_float(m.m_row3, 1, 1, 1, 1), m_v_y);
    m_mul2 = simd128_mul_float(simd128_select_float(m.m_row3, 2, 2, 2, 2), m_v_z);
    m_mul3 = simd128_mul_float(simd128_select_float(m.m_row3, 3, 3, 3, 3), m_v_w);
    __simd128_float m_result_w =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Transpose results so that each vector holds multiplication product.
    simd128_transpose4to4_float(m_result_x, m_result_y, m_result_z, m_result_w,
        m_result_0, m_result_1, m_result_2, m_result_3); 
}

static inline void SIMDMatrixMultiplyFourVectors(const Matrix4DSIMD& m,
const Vector4D *v_source_p, Vector4D *v_result_p) {
    __simd128_float m_result_0, m_result_1, m_result_2, m_result_3;
    SIMDMatrixMultiplyFourVectorsNoStore(m, v_source_p,
        m_result_0, m_result_1, m_result_2, m_result_3);
    // Store the results as Vector4D.
    simd128_store(&v_result_p[0], m_result_0);
    simd128_store(&v_result_p[1], m_result_1);
    simd128_store(&v_result_p[2], m_result_2);
    simd128_store(&v_result_p[3], m_result_3);
}

static inline void SIMDMatrixMultiplyVector(const Matrix4DSIMD& m, const Point3D *p1p,
Point3D *p2p) {
    __simd128_float m_p1 = simd128_set_float(p1p->x, p1p->y, p1p->z, 1.0f);
    __simd128_float m_result;
    m.Multiply(m_p1, m_result);
    simd128_store(p2p, m_result);
}

static inline void SIMDMatrixMultiplyFourVectorsNoStore(const Matrix4DSIMD& m,
const Point3D *p_source_p,
__simd128_float& m_result_0, __simd128_float& m_result_1, __simd128_float& m_result_2,
__simd128_float& m_result_3) {
    // Load four vectors and transpose so that all similar coordinates are
    // stored in a single vector.
    __simd128_float m_v_x = simd128_load(&p_source_p[0]);
    __simd128_float m_v_y = simd128_load(&p_source_p[1]);
    __simd128_float m_v_z = simd128_load(&p_source_p[2]);
    __simd128_float m_v_w = simd128_load(&p_source_p[3]);
    simd128_transpose4_float(m_v_x, m_v_y, m_v_z, m_v_w);
    // Make sure the last component of each vector is set to 1.0f (i.e. m_v_w must
    // be all 1.0f), because the source vectors are points.
    m_v_w = simd128_set_same_float(1.0f);
    __simd128_float m_mul0, m_mul1, m_mul2, m_mul3;
    // Process the x coordinates.
    m_mul0 = simd128_mul_float(simd128_select_float(m.m_row0, 0, 0, 0, 0), m_v_x);
    m_mul1 = simd128_mul_float(simd128_select_float(m.m_row0, 1, 1, 1, 1), m_v_y);
    m_mul2 = simd128_mul_float(simd128_select_float(m.m_row0, 2, 2, 2, 2), m_v_z);
    m_mul3 = simd128_mul_float(simd128_select_float(m.m_row0, 3, 3, 3, 3), m_v_w);
    __simd128_float m_result_x =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Process the y coordinates.
    m_mul0 = simd128_mul_float(simd128_select_float(m.m_row1, 0, 0, 0, 0), m_v_x);
    m_mul1 = simd128_mul_float(simd128_select_float(m.m_row1, 1, 1, 1, 1), m_v_y);
    m_mul2 = simd128_mul_float(simd128_select_float(m.m_row1, 2, 2, 2, 2), m_v_z);
    m_mul3 = simd128_mul_float(simd128_select_float(m.m_row1, 3, 3, 3, 3), m_v_w);
    __simd128_float m_result_y =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Process the z coordinates.
    m_mul0 = simd128_mul_float(simd128_select_float(m.m_row2, 0, 0, 0, 0), m_v_x);
    m_mul1 = simd128_mul_float(simd128_select_float(m.m_row2, 1, 1, 1, 1), m_v_y);
    m_mul2 = simd128_mul_float(simd128_select_float(m.m_row2, 2, 2, 2, 2), m_v_z);
    m_mul3 = simd128_mul_float(simd128_select_float(m.m_row2, 3, 3, 3, 3), m_v_w);
    __simd128_float m_result_z =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Process the w coordinates.
    m_mul0 = simd128_mul_float(simd128_select_float(m.m_row3, 0, 0, 0, 0), m_v_x);
    m_mul1 = simd128_mul_float(simd128_select_float(m.m_row3, 1, 1, 1, 1), m_v_y);
    m_mul2 = simd128_mul_float(simd128_select_float(m.m_row3, 2, 2, 2, 2), m_v_z);
    m_mul3 = simd128_mul_float(simd128_select_float(m.m_row3, 3, 3, 3, 3), m_v_w);
    __simd128_float m_result_w =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            simd128_add_float(m_mul2, m_mul3));
    // Transpose results so that each result vector holds multiplication product.
    simd128_transpose4to4_float(m_result_x, m_result_y, m_result_z, m_result_w,
        m_result_0, m_result_1, m_result_2, m_result_3); 
}

static inline void SIMDMatrixMultiplyFourVectors(const Matrix4DSIMD& m,
const Point3D *p_source_p, Point3D *p_result_p) {
    __simd128_float m_result_0, m_result_1, m_result_2, m_result_3;
    SIMDMatrixMultiplyFourVectorsNoStore(m, p_source_p,
        m_result_0, m_result_1, m_result_2, m_result_3);
    // Store the results as Vector4D.
    simd128_store(&p_result_p[0], m_result_0);
    simd128_store(&p_result_p[1], m_result_1);
    simd128_store(&p_result_p[2], m_result_2);
    simd128_store(&p_result_p[3], m_result_3);
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
    m_mul0 = simd128_mul_float(simd128_select_float(m.m_row0, 0, 0, 0, 0), m_v_x);
    m_mul1 = simd128_mul_float(simd128_select_float(m.m_row0, 1, 1, 1, 1), m_v_y);
    m_mul2 = simd128_mul_float(simd128_select_float(m.m_row0, 2, 2, 2, 2), m_v_z);
    __simd128_float m_result_x =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            m_mul2);
    // Process the y coordinates.
    m_mul0 = simd128_mul_float(simd128_select_float(m.m_row1, 0, 0, 0, 0), m_v_x);
    m_mul1 = simd128_mul_float(simd128_select_float(m.m_row1, 1, 1, 1, 2), m_v_y);
    m_mul2 = simd128_mul_float(simd128_select_float(m.m_row1, 2, 2, 2, 2), m_v_z);
    __simd128_float m_result_y =
        simd128_add_float(
            simd128_add_float(m_mul0, m_mul1),
            m_mul2);
    // Process the z coordinates.
    m_mul0 = simd128_mul_float(simd128_select_float(m.m_row2, 0, 0, 0, 0), m_v_x);
    m_mul1 = simd128_mul_float(simd128_select_float(m.m_row2, 1, 1, 1, 1), m_v_y);
    m_mul2 = simd128_mul_float(simd128_select_float(m.m_row2, 2, 2, 2, 2), m_v_z);
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
