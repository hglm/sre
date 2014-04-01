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

//============================================================================
//
// 3D/4D Matrix Classes
//
// Mathematics for 3D Game Programming and Computer Graphics, 3rd ed.
// By Eric Lengyel
//
// The code in this file may be freely used in any software. It is provided
// as-is, with no warranty of any kind.
//
//============================================================================

// Some changes and extensions by Harm Hanemaaijer, 2012-2014.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "sre.h"
#include "win32_compat.h"
#include "sreRandom.h"
#include "sre_internal.h"
#include "sre_simd.h"

// Initialize starting with the first row, then the next row, then subsequent columns.

Matrix3D& Matrix3D::Set(float n00, float n01, float n02, float n10, float n11, float n12, float n20, float n21, float n22)
{
	n[0][0] = n00;
	n[0][1] = n10;
	n[0][2] = n20;
	n[1][0] = n01;
	n[1][1] = n11;
	n[1][2] = n21;
	n[2][0] = n02;
	n[2][1] = n12;
	n[2][2] = n22;
	return (*this);
}

// Initialize with columns c1, c2, c3.

Matrix3D& Matrix3D::Set(const Vector3D& c1, const Vector3D& c2, const Vector3D& c3)
{
	return Set(c1.x, c2.x, c3.x, c1.y, c2.y, c3.y, c1.z, c2.z, c3.z);
}

Matrix3D::Matrix3D(const Vector3D& c1, const Vector3D& c2, const Vector3D& c3)
{
	Set(c1, c2, c3);
}

Matrix3D::Matrix3D(float n00, float n01, float n02, float n10, float n11, float n12, float n20, float n21, float n22)
{
	Set(n00, n01, n02, n10, n11, n12, n20, n21, n22);
}


Matrix3D& Matrix3D::operator *=(const Matrix3D& __restrict__ m) __restrict__
{
	float t = n[0][0] * m.n[0][0] + n[1][0] * m.n[0][1] + n[2][0] * m.n[0][2];
	float u = n[0][0] * m.n[1][0] + n[1][0] * m.n[1][1] + n[2][0] * m.n[1][2];
	n[2][0] = n[0][0] * m.n[2][0] + n[1][0] * m.n[2][1] + n[2][0] * m.n[2][2];
	n[0][0] = t;
	n[1][0] = u;
	
	t = n[0][1] * m.n[0][0] + n[1][1] * m.n[0][1] + n[2][1] * m.n[0][2];
	u = n[0][1] * m.n[1][0] + n[1][1] * m.n[1][1] + n[2][1] * m.n[1][2];
	n[2][1] = n[0][1] * m.n[2][0] + n[1][1] * m.n[2][1] + n[2][1] * m.n[2][2];
	n[0][1] = t;
	n[1][1] = u;
	
	t = n[0][2] * m.n[0][0] + n[1][2] * m.n[0][1] + n[2][2] * m.n[0][2];
	u = n[0][2] * m.n[1][0] + n[1][2] * m.n[1][1] + n[2][2] * m.n[1][2];
	n[2][2] = n[0][2] * m.n[2][0] + n[1][2] * m.n[2][1] + n[2][2] * m.n[2][2];
	n[0][2] = t;
	n[1][2] = u;
	
	return (*this);
}

Matrix3D& Matrix3D::operator *=(float t)
{
	n[0][0] *= t;
	n[0][1] *= t;
	n[0][2] *= t;
	n[1][0] *= t;
	n[1][1] *= t;
	n[1][2] *= t;
	n[2][0] *= t;
	n[2][1] *= t;
	n[2][2] *= t;
	
	return (*this);
}

Matrix3D& Matrix3D::operator /=(float t)
{
	float f = 1.0F / t;
	n[0][0] *= f;
	n[0][1] *= f;
	n[0][2] *= f;
	n[1][0] *= f;
	n[1][1] *= f;
	n[1][2] *= f;
	n[2][0] *= f;
	n[2][1] *= f;
	n[2][2] *= f;
	
	return (*this);
}

Matrix3D& Matrix3D::SetIdentity(void)
{
	n[0][0] = n[1][1] = n[2][2] = 1.0F;
	n[0][1] = n[0][2] = n[1][0] = n[1][2] = n[2][0] = n[2][1] = 0.0F;
	
	return (*this);
}

Matrix3D& Matrix3D::AssignRotationAlongAxis(const Vector3D& axis, float angle) {
	float c = cosf(angle);
	float s = sinf(angle);
	Set(c + (1.0 - c) * axis.x * axis.x, (1.0 - c) * axis.x * axis.y - s * axis.z,
		(1.0 - c) * axis.x * axis.z + s * axis.y,
		(1.0 - c) * axis.x * axis.y + s * axis.z, c + (1.0 - c) * axis.y * axis.y,
		(1.0 - c) * axis.y * axis.z - s * axis.x,
		(1.0 - c) * axis.x * axis.z - s * axis.y, (1.0 - c) * axis.y * axis.z + s * axis.x,
		c + (1 - c) * axis.z * axis.z);
	return (*this);
}

Matrix3D& Matrix3D::AssignRotationAlongXAxis(float angle) {
        Set(1.0, 0, 0,
		0, cosf(angle), - sinf(angle),
		0, sinf(angle), cosf(angle));
	return (*this);
}

Matrix3D& Matrix3D::AssignRotationAlongYAxis(float angle) {
	Set(cosf(angle), 0, sinf(angle),
		0, 1.0, 0,
		- sinf(angle), 0, cosf(angle));
	return (*this);
}

Matrix3D& Matrix3D::AssignRotationAlongZAxis(float angle) {
	Set(cosf(angle), - sinf(angle), 0,
		sinf(angle), cosf(angle), 0,
		0, 0, 1.0);
	return (*this);
}

Matrix3D& Matrix3D::AssignTranslation(const Vector2D& translation) {
    Set(1.0f, 0, translation.x,
        0, 1.0f, translation.y,
        0, 0, 1.0f);
    return (*this);
}

Matrix3D& Matrix3D::AssignScaling(float scaling) {
    Set(scaling, 0, 0,
        0, scaling, 0,
        0, 0, 1.0f);
    return (*this);
}

Matrix3D operator *(const Matrix3D& __restrict__ m1, const Matrix3D& __restrict__ m2)
{
	return (Matrix3D(m1.n[0][0] * m2.n[0][0] + m1.n[1][0] * m2.n[0][1] + m1.n[2][0] * m2.n[0][2],
					 m1.n[0][0] * m2.n[1][0] + m1.n[1][0] * m2.n[1][1] + m1.n[2][0] * m2.n[1][2],
					 m1.n[0][0] * m2.n[2][0] + m1.n[1][0] * m2.n[2][1] + m1.n[2][0] * m2.n[2][2],
					 m1.n[0][1] * m2.n[0][0] + m1.n[1][1] * m2.n[0][1] + m1.n[2][1] * m2.n[0][2],
					 m1.n[0][1] * m2.n[1][0] + m1.n[1][1] * m2.n[1][1] + m1.n[2][1] * m2.n[1][2],
					 m1.n[0][1] * m2.n[2][0] + m1.n[1][1] * m2.n[2][1] + m1.n[2][1] * m2.n[2][2],
					 m1.n[0][2] * m2.n[0][0] + m1.n[1][2] * m2.n[0][1] + m1.n[2][2] * m2.n[0][2],
					 m1.n[0][2] * m2.n[1][0] + m1.n[1][2] * m2.n[1][1] + m1.n[2][2] * m2.n[1][2],
					 m1.n[0][2] * m2.n[2][0] + m1.n[1][2] * m2.n[2][1] + m1.n[2][2] * m2.n[2][2]));
}

Matrix3D operator *(const Matrix3D& m, float t)
{
	return (Matrix3D(m.n[0][0] * t, m.n[1][0] * t, m.n[2][0] * t, m.n[0][1] * t, m.n[1][1] * t, m.n[2][1] * t, m.n[0][2] * t, m.n[1][2] * t, m.n[2][2] * t));
}

Matrix3D operator /(const Matrix3D& m, float t)
{
	float f = 1.0F / t;
	return (Matrix3D(m.n[0][0] * f, m.n[1][0] * f, m.n[2][0] * f, m.n[0][1] * f, m.n[1][1] * f, m.n[2][1] * f, m.n[0][2] * f, m.n[1][2] * f, m.n[2][2] * f));
}

Vector3D operator *(const Matrix3D& m, const Vector3D& v)
{
	return (Vector3D(m.n[0][0] * v.x + m.n[1][0] * v.y + m.n[2][0] * v.z, m.n[0][1] * v.x + m.n[1][1] * v.y + m.n[2][1] * v.z, m.n[0][2] * v.x + m.n[1][2] * v.y + m.n[2][2] * v.z));
}

Vector3D operator *(const Matrix3D& m, const Point3D& p)
{
	return (Point3D(m.n[0][0] * p.x + m.n[1][0] * p.y + m.n[2][0] * p.z, m.n[0][1] * p.x + m.n[1][1] * p.y + m.n[2][1] * p.z, m.n[0][2] * p.x + m.n[1][2] * p.y + m.n[2][2] * p.z));
}

Vector3D operator *(const Vector3D& v, const Matrix3D& m)
{
	return (Vector3D(m.n[0][0] * v.x + m.n[0][1] * v.y + m.n[0][2] * v.z, m.n[1][0] * v.x + m.n[1][1] * v.y + m.n[1][2] * v.z, m.n[2][0] * v.x + m.n[2][1] * v.y + m.n[2][2] * v.z));
}

Vector3D operator *(const Point3D& p, const Matrix3D& m)
{
	return (Point3D(m.n[0][0] * p.x + m.n[0][1] * p.y + m.n[0][2] * p.z, m.n[1][0] * p.x + m.n[1][1] * p.y + m.n[1][2] * p.z, m.n[2][0] * p.x + m.n[2][1] * p.y + m.n[2][2] * p.z));
}

bool operator ==(const Matrix3D& m1, const Matrix3D& m2)
{
	return ((m1.n[0][0] == m2.n[0][0]) && (m1.n[0][1] == m2.n[0][1]) && (m1.n[0][2] == m2.n[0][2]) && (m1.n[1][0] == m2.n[1][0]) && (m1.n[1][1] == m2.n[1][1]) && (m1.n[1][2] == m2.n[1][2]) && (m1.n[2][0] == m2.n[2][0]) && (m1.n[2][1] == m2.n[2][1]) && (m1.n[2][2] == m2.n[2][2]));
}

bool operator !=(const Matrix3D& m1, const Matrix3D& m2)
{
	return ((m1.n[0][0] != m2.n[0][0]) || (m1.n[0][1] != m2.n[0][1]) || (m1.n[0][2] != m2.n[0][2]) || (m1.n[1][0] != m2.n[1][0]) || (m1.n[1][1] != m2.n[1][1]) || (m1.n[1][2] != m2.n[1][2]) || (m1.n[2][0] != m2.n[2][0]) || (m1.n[2][1] != m2.n[2][1]) || (m1.n[2][2] != m2.n[2][2]));
}

float Determinant(const Matrix3D& m)
{
	return (m(0,0) * (m(1,1) * m(2,2) - m(1,2) * m(2,1)) - m(0,1) * (m(1,0) * m(2,2) - m(1,2) * m(2,0)) + m(0,2) * (m(1,0) * m(2,1) - m(1,1) * m(2,0)));
}

Matrix3D Inverse(const Matrix3D& m)
{
	float n00 = m(0,0);
	float n01 = m(0,1);
	float n02 = m(0,2);
	float n10 = m(1,0);
	float n11 = m(1,1);
	float n12 = m(1,2);
	float n20 = m(2,0);
	float n21 = m(2,1);
	float n22 = m(2,2);
	
	float p00 = n11 * n22 - n12 * n21;
	float p10 = n12 * n20 - n10 * n22;
	float p20 = n10 * n21 - n11 * n20;
	
	float t = 1.0F / (n00 * p00 + n01 * p10 + n02 * p20);
	
	return (Matrix3D(p00 * t, (n02 * n21 - n01 * n22) * t, (n01 * n12 - n02 * n11) * t,
					 p10 * t, (n00 * n22 - n02 * n20) * t, (n02 * n10 - n00 * n12) * t,
					 p20 * t, (n01 * n20 - n00 * n21) * t, (n00 * n11 - n01 * n10) * t));
}

Matrix3D Adjugate(const Matrix3D& m)
{
	float n00 = m(0,0);
	float n01 = m(0,1);
	float n02 = m(0,2);
	float n10 = m(1,0);
	float n11 = m(1,1);
	float n12 = m(1,2);
	float n20 = m(2,0);
	float n21 = m(2,1);
	float n22 = m(2,2);
	
	return (Matrix3D(n11 * n22 - n12 * n21, n02 * n21 - n01 * n22, n01 * n12 - n02 * n11,
					 n12 * n20 - n10 * n22, n00 * n22 - n02 * n20, n02 * n10 - n00 * n12,
					 n10 * n21 - n11 * n20, n01 * n20 - n00 * n21, n00 * n11 - n01 * n10));
}

Matrix3D Transpose(const Matrix3D& m)
{
	return (Matrix3D(m(0,0), m(1,0), m(2,0), m(0,1), m(1,1), m(2,1), m(0,2), m(1,2), m(2,2)));
}

bool Matrix3D::RotationMatrixPreservesAABB() {
   // Determine whether a rotation matrix rotates every axis through
   // a multiple of 90 degrees, even allowing reflections, so that AABB
   // bounds computed after rotation will still be a good fit.
   // To do so, we multiply the matrix with the vector (1, 1, 1);
   // if every component of the resulting vector has an absolute value
   // of 1, the rotation matrix fits the criteria.
   Vector3D V = (*this) * Vector3D(1.0f, 1.0f, 1.0f);
   if (AlmostEqual(fabsf(V.x), 1.0f) &&
   AlmostEqual(fabsf(V.y), 1.0f) &&
   AlmostEqual(fabsf(V.z), 1.0f))
       return true;
   else
       return false;
}

Matrix4D& Matrix4D::Set(float n00, float n01, float n02, float n03, float n10, float n11, float n12, float n13, float n20, float n21, float n22, float n23, float n30, float n31, float n32, float n33)
{
	n[0][0] = n00;
	n[1][0] = n01;
	n[2][0] = n02;
	n[3][0] = n03;
	n[0][1] = n10;
	n[1][1] = n11;
	n[2][1] = n12;
	n[3][1] = n13;
	n[0][2] = n20;
	n[1][2] = n21;
	n[2][2] = n22;
	n[3][2] = n23;
	n[0][3] = n30;
	n[1][3] = n31;
	n[2][3] = n32;
	n[3][3] = n33;
	
	return (*this);
}

// Set columns.

Matrix4D& Matrix4D::Set(const Vector4D& c1, const Vector4D& c2, const Vector4D& c3,
const Vector4D& c4) {
	return Set(c1.x, c2.x, c3.x, c4.x, c1.y, c2.y, c3.y, c4.y, c1.z, c2.z, c3.z, c4.z,
            c1.w, c2.w, c3.w, c4.w);
}

Matrix4D::Matrix4D(const Vector4D& c1, const Vector4D& c2, const Vector4D& c3, const Vector4D& c4)
{
	Set(c1, c2, c3 ,c4);
}

Matrix4D::Matrix4D(float n00, float n01, float n02, float n03, float n10, float n11, float n12, float n13, float n20, float n21, float n22, float n23, float n30, float n31, float n32, float n33)
{
	Set(n00, n01, n02, n03, n10, n11, n12, n13, n20, n21, n22, n23, n30, n31, n32, n33);
}

Matrix4D::Matrix4D(const MatrixTransform& m) {
	Set(m.GetColumn(0), m.GetColumn(1), m.GetColumn(2), m.GetColumn(3));
}

Matrix4D& Matrix4D::operator =(const Matrix3D& m)
{
	n[0][0] = m.n[0][0];
	n[0][1] = m.n[0][1];
	n[0][2] = m.n[0][2];
	n[1][0] = m.n[1][0];
	n[1][1] = m.n[1][1];
	n[1][2] = m.n[1][2];
	n[2][0] = m.n[2][0];
	n[2][1] = m.n[2][1];
	n[2][2] = m.n[2][2];
	
	n[0][3] = n[1][3] = n[2][3] = n[3][0] = n[3][1] = n[3][2] = 0.0F;
	n[3][3] = 1.0F;
	
	return (*this);
}

Matrix4D& Matrix4D::operator *=(const Matrix4D& __restrict__ m) __restrict__
{
	float x = n[0][0];
	float y = n[1][0];
	float z = n[2][0];
	float w = n[3][0];
	n[0][0] = x * m.n[0][0] + y * m.n[0][1] + z * m.n[0][2] + w * m.n[0][3];
	n[1][0] = x * m.n[1][0] + y * m.n[1][1] + z * m.n[1][2] + w * m.n[1][3];
	n[2][0] = x * m.n[2][0] + y * m.n[2][1] + z * m.n[2][2] + w * m.n[2][3];
	n[3][0] = x * m.n[3][0] + y * m.n[3][1] + z * m.n[3][2] + w * m.n[3][3];
	
	x = n[0][1];
	y = n[1][1];
	z = n[2][1];
	w = n[3][1];
	n[0][1] = x * m.n[0][0] + y * m.n[0][1] + z * m.n[0][2] + w * m.n[0][3];
	n[1][1] = x * m.n[1][0] + y * m.n[1][1] + z * m.n[1][2] + w * m.n[1][3];
	n[2][1] = x * m.n[2][0] + y * m.n[2][1] + z * m.n[2][2] + w * m.n[2][3];
	n[3][1] = x * m.n[3][0] + y * m.n[3][1] + z * m.n[3][2] + w * m.n[3][3];
	
	x = n[0][2];
	y = n[1][2];
	z = n[2][2];
	w = n[3][2];
	n[0][2] = x * m.n[0][0] + y * m.n[0][1] + z * m.n[0][2] + w * m.n[0][3];
	n[1][2] = x * m.n[1][0] + y * m.n[1][1] + z * m.n[1][2] + w * m.n[1][3];
	n[2][2] = x * m.n[2][0] + y * m.n[2][1] + z * m.n[2][2] + w * m.n[2][3];
	n[3][2] = x * m.n[3][0] + y * m.n[3][1] + z * m.n[3][2] + w * m.n[3][3];
	
	x = n[0][3];
	y = n[1][3];
	z = n[2][3];
	w = n[3][3];
	n[0][3] = x * m.n[0][0] + y * m.n[0][1] + z * m.n[0][2] + w * m.n[0][3];
	n[1][3] = x * m.n[1][0] + y * m.n[1][1] + z * m.n[1][2] + w * m.n[1][3];
	n[2][3] = x * m.n[2][0] + y * m.n[2][1] + z * m.n[2][2] + w * m.n[2][3];
	n[3][3] = x * m.n[3][0] + y * m.n[3][1] + z * m.n[3][2] + w * m.n[3][3];
	
	return (*this);
}

Matrix4D& Matrix4D::operator *=(const Matrix3D& __restrict__ m) __restrict__
{
	float x = n[0][0];
	float y = n[1][0];
	float z = n[2][0];
	n[0][0] = x * m.n[0][0] + y * m.n[0][1] + z * m.n[0][2];
	n[1][0] = x * m.n[1][0] + y * m.n[1][1] + z * m.n[1][2];
	n[2][0] = x * m.n[2][0] + y * m.n[2][1] + z * m.n[2][2];
	
	x = n[0][1];
	y = n[1][1];
	z = n[2][1];
	n[0][1] = x * m.n[0][0] + y * m.n[0][1] + z * m.n[0][2];
	n[1][1] = x * m.n[1][0] + y * m.n[1][1] + z * m.n[1][2];
	n[2][1] = x * m.n[2][0] + y * m.n[2][1] + z * m.n[2][2];
	
	x = n[0][2];
	y = n[1][2];
	z = n[2][2];
	n[0][2] = x * m.n[0][0] + y * m.n[0][1] + z * m.n[0][2];
	n[1][2] = x * m.n[1][0] + y * m.n[1][1] + z * m.n[1][2];
	n[2][2] = x * m.n[2][0] + y * m.n[2][1] + z * m.n[2][2];
	
	x = n[0][3];
	y = n[1][3];
	z = n[2][3];
	n[0][3] = x * m.n[0][0] + y * m.n[0][1] + z * m.n[0][2];
	n[1][3] = x * m.n[1][0] + y * m.n[1][1] + z * m.n[1][2];
	n[2][3] = x * m.n[2][0] + y * m.n[2][1] + z * m.n[2][2];
	
	return (*this);
}

Matrix4D& Matrix4D::SetIdentity(void)
{
	n[0][0] = n[1][1] = n[2][2] = n[3][3] = 1.0F;
	n[1][0] = n[2][0] = n[3][0] = n[0][1] = n[2][1] = n[3][1] = n[0][2] =
            n[1][2] = n[3][2] = n[0][3] = n[1][3] = n[2][3] = 0.0F;
	return (*this);
}

Matrix4D& Matrix4D::AssignRotationAlongXAxis(float angle) {
        Set(1.0, 0, 0, 0,
	    0, cosf(angle), - sinf(angle), 0,
            0, sinf(angle), cosf(angle), 0,
            0, 0, 0, 1.0);
	return (*this);
}

Matrix4D& Matrix4D::AssignRotationAlongYAxis(float angle) {
    Set(cosf(angle), 0, sinf(angle), 0,
        0, 1.0, 0, 0,
        - sinf(angle), 0, cosf(angle), 0,
        0, 0, 0, 1.0);
	return (*this);
}

Matrix4D& Matrix4D::AssignRotationAlongZAxis(float angle) {
	Set(cosf(angle), - sinf(angle), 0, 0,
            sinf(angle), cosf(angle), 0, 0,
            0, 0, 1.0, 0,
            0, 0, 0, 1.0);
    return (*this);
}

Matrix4D& Matrix4D::AssignTranslation(const Vector3D& translation) {
    Set(1.0f, 0, 0, translation.x,
        0, 1.0f, 0, translation.y,
        0, 0, 1.0f, translation.z,
        0, 0, 0, 1.0f);
    return (*this);
}

Matrix4D& Matrix4D::AssignScaling(float scaling) {
    Set(scaling, 0, 0, 0,
        0, scaling, 0, 0,
        0, 0, scaling, 0,
        0, 0, 0, 1.0f);
    return (*this);
}

Matrix4D operator *(const Matrix4D& __restrict__ m1, const Matrix4D& __restrict__ m2)
{
#ifdef USE_SSE2
    Matrix4D m3;
    SIMDMatrixMultiply4x4(&m1.n[0][0], &m2.n[0][0], &m3.n[0][0]);
    return m3;
#else
	return (Matrix4D(m1.n[0][0] * m2.n[0][0] + m1.n[1][0] * m2.n[0][1] + m1.n[2][0] * m2.n[0][2] + m1.n[3][0] * m2.n[0][3],
					 m1.n[0][0] * m2.n[1][0] + m1.n[1][0] * m2.n[1][1] + m1.n[2][0] * m2.n[1][2] + m1.n[3][0] * m2.n[1][3],
					 m1.n[0][0] * m2.n[2][0] + m1.n[1][0] * m2.n[2][1] + m1.n[2][0] * m2.n[2][2] + m1.n[3][0] * m2.n[2][3],
					 m1.n[0][0] * m2.n[3][0] + m1.n[1][0] * m2.n[3][1] + m1.n[2][0] * m2.n[3][2] + m1.n[3][0] * m2.n[3][3],
					 m1.n[0][1] * m2.n[0][0] + m1.n[1][1] * m2.n[0][1] + m1.n[2][1] * m2.n[0][2] + m1.n[3][1] * m2.n[0][3],
					 m1.n[0][1] * m2.n[1][0] + m1.n[1][1] * m2.n[1][1] + m1.n[2][1] * m2.n[1][2] + m1.n[3][1] * m2.n[1][3],
					 m1.n[0][1] * m2.n[2][0] + m1.n[1][1] * m2.n[2][1] + m1.n[2][1] * m2.n[2][2] + m1.n[3][1] * m2.n[2][3],
					 m1.n[0][1] * m2.n[3][0] + m1.n[1][1] * m2.n[3][1] + m1.n[2][1] * m2.n[3][2] + m1.n[3][1] * m2.n[3][3],
					 m1.n[0][2] * m2.n[0][0] + m1.n[1][2] * m2.n[0][1] + m1.n[2][2] * m2.n[0][2] + m1.n[3][2] * m2.n[0][3],
					 m1.n[0][2] * m2.n[1][0] + m1.n[1][2] * m2.n[1][1] + m1.n[2][2] * m2.n[1][2] + m1.n[3][2] * m2.n[1][3],
					 m1.n[0][2] * m2.n[2][0] + m1.n[1][2] * m2.n[2][1] + m1.n[2][2] * m2.n[2][2] + m1.n[3][2] * m2.n[2][3],
					 m1.n[0][2] * m2.n[3][0] + m1.n[1][2] * m2.n[3][1] + m1.n[2][2] * m2.n[3][2] + m1.n[3][2] * m2.n[3][3],
					 m1.n[0][3] * m2.n[0][0] + m1.n[1][3] * m2.n[0][1] + m1.n[2][3] * m2.n[0][2] + m1.n[3][3] * m2.n[0][3],
					 m1.n[0][3] * m2.n[1][0] + m1.n[1][3] * m2.n[1][1] + m1.n[2][3] * m2.n[1][2] + m1.n[3][3] * m2.n[1][3],
					 m1.n[0][3] * m2.n[2][0] + m1.n[1][3] * m2.n[2][1] + m1.n[2][3] * m2.n[2][2] + m1.n[3][3] * m2.n[2][3],
					 m1.n[0][3] * m2.n[3][0] + m1.n[1][3] * m2.n[3][1] + m1.n[2][3] * m2.n[3][2] + m1.n[3][3] * m2.n[3][3]));
#endif
}

Matrix4D operator *(const Matrix4D& __restrict__ m1, const Matrix3D& __restrict__ m2)
{
	return (Matrix4D(m1.n[0][0] * m2.n[0][0] + m1.n[1][0] * m2.n[0][1] + m1.n[2][0] * m2.n[0][2],
					 m1.n[0][0] * m2.n[1][0] + m1.n[1][0] * m2.n[1][1] + m1.n[2][0] * m2.n[1][2],
					 m1.n[0][0] * m2.n[2][0] + m1.n[1][0] * m2.n[2][1] + m1.n[2][0] * m2.n[2][2],
					 m1.n[3][0],
					 m1.n[0][1] * m2.n[0][0] + m1.n[1][1] * m2.n[0][1] + m1.n[2][1] * m2.n[0][2],
					 m1.n[0][1] * m2.n[1][0] + m1.n[1][1] * m2.n[1][1] + m1.n[2][1] * m2.n[1][2],
					 m1.n[0][1] * m2.n[2][0] + m1.n[1][1] * m2.n[2][1] + m1.n[2][1] * m2.n[2][2],
					 m1.n[3][1],
					 m1.n[0][2] * m2.n[0][0] + m1.n[1][2] * m2.n[0][1] + m1.n[2][2] * m2.n[0][2],
					 m1.n[0][2] * m2.n[1][0] + m1.n[1][2] * m2.n[1][1] + m1.n[2][2] * m2.n[1][2],
					 m1.n[0][2] * m2.n[2][0] + m1.n[1][2] * m2.n[2][1] + m1.n[2][2] * m2.n[2][2],
					 m1.n[3][2],
					 m1.n[0][3] * m2.n[0][0] + m1.n[1][3] * m2.n[0][1] + m1.n[2][3] * m2.n[0][2],
					 m1.n[0][3] * m2.n[1][0] + m1.n[1][3] * m2.n[1][1] + m1.n[2][3] * m2.n[1][2],
					 m1.n[0][3] * m2.n[2][0] + m1.n[1][3] * m2.n[2][1] + m1.n[2][3] * m2.n[2][2],
					 m1.n[3][3]));
}

Vector4D operator *(const Matrix4D& m, const Vector4D& v)
{
	return (Vector4D(m.n[0][0] * v.x + m.n[1][0] * v.y + m.n[2][0] * v.z + m.n[3][0] * v.w,
					 m.n[0][1] * v.x + m.n[1][1] * v.y + m.n[2][1] * v.z + m.n[3][1] * v.w,
					 m.n[0][2] * v.x + m.n[1][2] * v.y + m.n[2][2] * v.z + m.n[3][2] * v.w,
					 m.n[0][3] * v.x + m.n[1][3] * v.y + m.n[2][3] * v.z + m.n[3][3] * v.w));
}

Vector4D operator *(const Vector4D& v, const Matrix4D& m)
{
	return (Vector4D(m.n[0][0] * v.x + m.n[0][1] * v.y + m.n[0][2] * v.z + m.n[0][3] * v.w,
					 m.n[1][0] * v.x + m.n[1][1] * v.y + m.n[1][2] * v.z + m.n[1][3] * v.w,
					 m.n[2][0] * v.x + m.n[2][1] * v.y + m.n[2][2] * v.z + m.n[2][3] * v.w,
					 m.n[3][0] * v.x + m.n[3][1] * v.y + m.n[3][2] * v.z + m.n[3][3] * v.w));
}

Vector4D operator *(const Matrix4D& m, const Vector3D& v)
{
	return (Vector4D(m.n[0][0] * v.x + m.n[1][0] * v.y + m.n[2][0] * v.z,
					 m.n[0][1] * v.x + m.n[1][1] * v.y + m.n[2][1] * v.z,
					 m.n[0][2] * v.x + m.n[1][2] * v.y + m.n[2][2] * v.z,
					 m.n[0][3] * v.x + m.n[1][3] * v.y + m.n[2][3] * v.z));
}

Vector4D operator *(const Vector3D& v, const Matrix4D& m)
{
	return (Vector4D(m.n[0][0] * v.x + m.n[0][1] * v.y + m.n[0][2] * v.z,
					 m.n[1][0] * v.x + m.n[1][1] * v.y + m.n[1][2] * v.z,
					 m.n[2][0] * v.x + m.n[2][1] * v.y + m.n[2][2] * v.z,
					 m.n[3][0] * v.x + m.n[3][1] * v.y + m.n[3][2] * v.z));
}

Vector4D operator *(const Matrix4D& m, const Point3D& p)
{
	return (Vector4D(m.n[0][0] * p.x + m.n[1][0] * p.y + m.n[2][0] * p.z + m.n[3][0],
					 m.n[0][1] * p.x + m.n[1][1] * p.y + m.n[2][1] * p.z + m.n[3][1],
					 m.n[0][2] * p.x + m.n[1][2] * p.y + m.n[2][2] * p.z + m.n[3][2],
					 m.n[0][3] * p.x + m.n[1][3] * p.y + m.n[2][3] * p.z + m.n[3][3]));
}

Vector4D operator *(const Point3D& p, const Matrix4D& m)
{
	return (Vector4D(m.n[0][0] * p.x + m.n[0][1] * p.y + m.n[0][2] * p.z + m.n[0][3],
					 m.n[1][0] * p.x + m.n[1][1] * p.y + m.n[1][2] * p.z + m.n[1][3],
					 m.n[2][0] * p.x + m.n[2][1] * p.y + m.n[2][2] * p.z + m.n[2][3],
					 m.n[3][0] * p.x + m.n[3][1] * p.y + m.n[3][2] * p.z + m.n[3][3]));
}

Vector4D operator *(const Matrix4D& m, const Vector2D& v)
{
	return (Vector4D(m.n[0][0] * v.x + m.n[1][0] * v.y,
					 m.n[0][1] * v.x + m.n[1][1] * v.y,
					 m.n[0][2] * v.x + m.n[1][2] * v.y,
					 m.n[0][3] * v.x + m.n[1][3] * v.y));
}

Vector4D operator *(const Vector2D& v, const Matrix4D& m)
{
	return (Vector4D(m.n[0][0] * v.x + m.n[0][1] * v.y,
					 m.n[1][0] * v.x + m.n[1][1] * v.y,
					 m.n[2][0] * v.x + m.n[2][1] * v.y,
					 m.n[3][0] * v.x + m.n[3][1] * v.y));
}

Vector4D operator *(const Matrix4D& m, const Point2D& p)
{
	return (Vector4D(m.n[0][0] * p.x + m.n[1][0] * p.y + m.n[3][0],
					 m.n[0][1] * p.x + m.n[1][1] * p.y + m.n[3][1],
					 m.n[0][2] * p.x + m.n[1][2] * p.y + m.n[3][2],
					 m.n[0][3] * p.x + m.n[1][3] * p.y + m.n[3][3]));
}

Vector4D operator *(const Point2D& p, const Matrix4D& m)
{
	return (Vector4D(m.n[0][0] * p.x + m.n[0][1] * p.y + m.n[0][3],
					 m.n[1][0] * p.x + m.n[1][1] * p.y + m.n[1][3],
					 m.n[2][0] * p.x + m.n[2][1] * p.y + m.n[2][3],
					 m.n[3][0] * p.x + m.n[3][1] * p.y + m.n[3][3]));
}

bool operator ==(const Matrix4D& m1, const Matrix4D& m2)
{
	return ((m1.n[0][0] == m2.n[0][0]) && (m1.n[0][1] == m2.n[0][1]) && (m1.n[0][2] == m2.n[0][2]) && (m1.n[0][3] == m2.n[0][3]) && (m1.n[1][0] == m2.n[1][0]) && (m1.n[1][1] == m2.n[1][1]) && (m1.n[1][2] == m2.n[1][2]) && (m1.n[1][3] == m2.n[1][3]) && (m1.n[2][0] == m2.n[2][0]) && (m1.n[2][1] == m2.n[2][1]) && (m1.n[2][2] == m2.n[2][2]) && (m1.n[2][3] == m2.n[2][3]) && (m1.n[3][0] == m2.n[3][0]) && (m1.n[3][1] == m2.n[3][1]) && (m1.n[3][2] == m2.n[3][2]) && (m1.n[3][3] == m2.n[3][3]));
}

bool operator !=(const Matrix4D& m1, const Matrix4D& m2)
{
	return ((m1.n[0][0] != m2.n[0][0]) || (m1.n[0][1] != m2.n[0][1]) || (m1.n[0][2] != m2.n[0][2]) || (m1.n[0][3] != m2.n[0][3]) || (m1.n[1][0] != m2.n[1][0]) || (m1.n[1][1] != m2.n[1][1]) || (m1.n[1][2] != m2.n[1][2]) || (m1.n[1][3] != m2.n[1][3]) || (m1.n[2][0] != m2.n[2][0]) || (m1.n[2][1] != m2.n[2][1]) || (m1.n[2][2] != m2.n[2][2]) || (m1.n[2][3] != m2.n[2][3]) || (m1.n[3][0] != m2.n[3][0]) || (m1.n[3][1] != m2.n[3][1]) || (m1.n[3][2] != m2.n[3][2]) || (m1.n[3][3] != m2.n[3][3]));
}

float Determinant(const Matrix4D& m)
{
	float n00 = m(0,0);
	float n01 = m(0,1);
	float n02 = m(0,2);
	float n03 = m(0,3);
	
	float n10 = m(1,0);
	float n11 = m(1,1);
	float n12 = m(1,2);
	float n13 = m(1,3);
	
	float n20 = m(2,0);
	float n21 = m(2,1);
	float n22 = m(2,2);
	float n23 = m(2,3);
	
	float n30 = m(3,0);
	float n31 = m(3,1);
	float n32 = m(3,2);
	float n33 = m(3,3);
	
	return (n00 * (n11 * (n22 * n33 - n23 * n32) + n12 * (n23 * n31 - n21 * n33) + n13 * (n21 * n32 - n22 * n31)) +
			n01 * (n10 * (n23 * n32 - n22 * n33) + n12 * (n20 * n33 - n23 * n30) + n13 * (n22 * n30 - n20 * n32)) +
			n02 * (n10 * (n21 * n33 - n23 * n31) + n11 * (n23 * n30 - n20 * n33) + n13 * (n20 * n31 - n21 * n30)) +
			n03 * (n10 * (n22 * n31 - n21 * n32) + n11 * (n20 * n32 - n22 * n30) + n12 * (n21 * n30 - n20 * n31)));
}

Matrix4D Inverse(const Matrix4D& m)
{
	float n00 = m(0,0);
	float n01 = m(0,1);
	float n02 = m(0,2);
	float n03 = m(0,3);
	
	float n10 = m(1,0);
	float n11 = m(1,1);
	float n12 = m(1,2);
	float n13 = m(1,3);
	
	float n20 = m(2,0);
	float n21 = m(2,1);
	float n22 = m(2,2);
	float n23 = m(2,3);
	
	float n30 = m(3,0);
	float n31 = m(3,1);
	float n32 = m(3,2);
	float n33 = m(3,3);
	
	float p00 = n11 * (n22 * n33 - n23 * n32) + n12 * (n23 * n31 - n21 * n33) + n13 * (n21 * n32 - n22 * n31);
	float p10 = n10 * (n23 * n32 - n22 * n33) + n12 * (n20 * n33 - n23 * n30) + n13 * (n22 * n30 - n20 * n32);
	float p20 = n10 * (n21 * n33 - n23 * n31) + n11 * (n23 * n30 - n20 * n33) + n13 * (n20 * n31 - n21 * n30);
	float p30 = n10 * (n22 * n31 - n21 * n32) + n11 * (n20 * n32 - n22 * n30) + n12 * (n21 * n30 - n20 * n31);
	
	float t = 1.0F / (n00 * p00 + n01 * p10 + n02 * p20 + n03 * p30);
	
	return (Matrix4D(p00 * t,
					 (n01 * (n23 * n32 - n22 * n33) + n02 * (n21 * n33 - n23 * n31) + n03 * (n22 * n31 - n21 * n32)) * t,
					 (n01 * (n12 * n33 - n13 * n32) + n02 * (n13 * n31 - n11 * n33) + n03 * (n11 * n32 - n12 * n31)) * t,
					 (n01 * (n13 * n22 - n12 * n23) + n02 * (n11 * n23 - n13 * n21) + n03 * (n12 * n21 - n11 * n22)) * t,
					 p10 * t,
					 (n00 * (n22 * n33 - n23 * n32) + n02 * (n23 * n30 - n20 * n33) + n03 * (n20 * n32 - n22 * n30)) * t,
					 (n00 * (n13 * n32 - n12 * n33) + n02 * (n10 * n33 - n13 * n30) + n03 * (n12 * n30 - n10 * n32)) * t,
					 (n00 * (n12 * n23 - n13 * n22) + n02 * (n13 * n20 - n10 * n23) + n03 * (n10 * n22 - n12 * n20)) * t,
					 p20 * t,
					 (n00 * (n23 * n31 - n21 * n33) + n01 * (n20 * n33 - n23 * n30) + n03 * (n21 * n30 - n20 * n31)) * t,
					 (n00 * (n11 * n33 - n13 * n31) + n01 * (n13 * n30 - n10 * n33) + n03 * (n10 * n31 - n11 * n30)) * t,
					 (n00 * (n13 * n21 - n11 * n23) + n01 * (n10 * n23 - n13 * n20) + n03 * (n11 * n20 - n10 * n21)) * t,
					 p30 * t,
					 (n00 * (n21 * n32 - n22 * n31) + n01 * (n22 * n30 - n20 * n32) + n02 * (n20 * n31 - n21 * n30)) * t,
					 (n00 * (n12 * n31 - n11 * n32) + n01 * (n10 * n32 - n12 * n30) + n02 * (n11 * n30 - n10 * n31)) * t,
					 (n00 * (n11 * n22 - n12 * n21) + n01 * (n12 * n20 - n10 * n22) + n02 * (n10 * n21 - n11 * n20)) * t));
}

Matrix4D Adjugate(const Matrix4D& m)
{
	float n00 = m(0,0);
	float n01 = m(0,1);
	float n02 = m(0,2);
	float n03 = m(0,3);
	
	float n10 = m(1,0);
	float n11 = m(1,1);
	float n12 = m(1,2);
	float n13 = m(1,3);
	
	float n20 = m(2,0);
	float n21 = m(2,1);
	float n22 = m(2,2);
	float n23 = m(2,3);
	
	float n30 = m(3,0);
	float n31 = m(3,1);
	float n32 = m(3,2);
	float n33 = m(3,3);
	
	return (Matrix4D(n11 * (n22 * n33 - n23 * n32) + n12 * (n23 * n31 - n21 * n33) + n13 * (n21 * n32 - n22 * n31),
					 n01 * (n23 * n32 - n22 * n33) + n02 * (n21 * n33 - n23 * n31) + n03 * (n22 * n31 - n21 * n32),
					 n01 * (n12 * n33 - n13 * n32) + n02 * (n13 * n31 - n11 * n33) + n03 * (n11 * n32 - n12 * n31),
					 n01 * (n13 * n22 - n12 * n23) + n02 * (n11 * n23 - n13 * n21) + n03 * (n12 * n21 - n11 * n22),
					 n10 * (n23 * n32 - n22 * n33) + n12 * (n20 * n33 - n23 * n30) + n13 * (n22 * n30 - n20 * n32),
					 n00 * (n22 * n33 - n23 * n32) + n02 * (n23 * n30 - n20 * n33) + n03 * (n20 * n32 - n22 * n30),
					 n00 * (n13 * n32 - n12 * n33) + n02 * (n10 * n33 - n13 * n30) + n03 * (n12 * n30 - n10 * n32),
					 n00 * (n12 * n23 - n13 * n22) + n02 * (n13 * n20 - n10 * n23) + n03 * (n10 * n22 - n12 * n20),
					 n10 * (n21 * n33 - n23 * n31) + n11 * (n23 * n30 - n20 * n33) + n13 * (n20 * n31 - n21 * n30),
					 n00 * (n23 * n31 - n21 * n33) + n01 * (n20 * n33 - n23 * n30) + n03 * (n21 * n30 - n20 * n31),
					 n00 * (n11 * n33 - n13 * n31) + n01 * (n13 * n30 - n10 * n33) + n03 * (n10 * n31 - n11 * n30),
					 n00 * (n13 * n21 - n11 * n23) + n01 * (n10 * n23 - n13 * n20) + n03 * (n11 * n20 - n10 * n21),
					 n10 * (n22 * n31 - n21 * n32) + n11 * (n20 * n32 - n22 * n30) + n12 * (n21 * n30 - n20 * n31),
					 n00 * (n21 * n32 - n22 * n31) + n01 * (n22 * n30 - n20 * n32) + n02 * (n20 * n31 - n21 * n30),
					 n00 * (n12 * n31 - n11 * n32) + n01 * (n10 * n32 - n12 * n30) + n02 * (n11 * n30 - n10 * n31),
					 n00 * (n11 * n22 - n12 * n21) + n01 * (n12 * n20 - n10 * n22) + n02 * (n10 * n21 - n11 * n20)));
}

Matrix4D Transpose(const Matrix4D& m)
{
	return (Matrix4D(m(0,0), m(1,0), m(2,0), m(3,0), m(0,1), m(1,1), m(2,1), m(3,1), m(0,2), m(1,2), m(2,2), m(3,2), m(0,3), m(1,3), m(2,3), m(3,3)));
}

// MatrixTransform class. Transform matrices are zero in row 3 at n30, n31 and n32 and
// 1.0 at n33. Unlike other matrices, they are stored in row-major formated. They
// have to be transposed when uploaded as a shader uniform.

MatrixTransform& MatrixTransform::Set(float n00, float n01, float n02, float n03, float n10, float n11, float n12, float n13, float n20, float n21, float n22, float n23)
{
	n[0][0] = n00;
	n[0][1] = n01;
	n[0][2] = n02;
	n[0][3] = n03;
	n[1][0] = n10;
	n[1][1] = n11;
	n[1][2] = n12;
	n[1][3] = n13;
	n[2][0] = n20;
	n[2][1] = n21;
	n[2][2] = n22;
	n[2][3] = n23;
	return (*this);
}

// Set columns.

MatrixTransform& MatrixTransform::Set(const Vector3D& c1, const Vector3D& c2,
const Vector3D& c3, const Vector3D& c4) {
	return Set(c1.x, c2.x, c3.x, c4.x, c1.y, c2.y, c3.y, c4.y, c1.z, c2.z, c3.z, c4.z);
}

MatrixTransform::MatrixTransform(const Vector3D& c1, const Vector3D& c2, const Vector3D& c3,
const Vector3D& c4)
{
	Set(c1, c2, c3, c4);
}

MatrixTransform::MatrixTransform(float n00, float n01, float n02, float n03, float n10, float n11, float n12, float n13, float n20, float n21, float n22, float n23)
{
	Set(n00, n01, n02, n03, n10, n11, n12, n13, n20, n21, n22, n23);
}

MatrixTransform& MatrixTransform::SetIdentity(void)
{
	n[0][0] = n[1][1] = n[2][2]  = 1.0F;
	n[0][1] = n[0][2] = n[0][3] = n[1][0] = n[1][2] = n[1][3] = n[2][0] = n[2][1] =
	    n[2][3] = 0.0F;
	return (*this);
}

MatrixTransform& MatrixTransform::AssignRotationAlongXAxis(float angle) {
        Set(1, 0, 0, 0,
	    0, cosf(angle), - sinf(angle), 0,
            0, sinf(angle), cosf(angle), 0);
	return (*this);
}

MatrixTransform& MatrixTransform::AssignRotationAlongYAxis(float angle) {
    Set(cosf(angle), 0, sinf(angle), 0,
        0, 1, 0, 0,
        - sinf(angle), 0, cosf(angle), 0);
	return (*this);
}

MatrixTransform& MatrixTransform::AssignRotationAlongZAxis(float angle) {
	Set(cosf(angle), - sinf(angle), 0, 0,
            sinf(angle), cosf(angle), 0, 0,
            0, 0, 1, 0);
    return (*this);
}

MatrixTransform& MatrixTransform::AssignTranslation(const Vector3D& translation) {
    Set(1, 0, 0, translation.x,
        0, 1, 0, translation.y,
        0, 0, 1, translation.z);
    return (*this);
}

MatrixTransform& MatrixTransform::AssignScaling(float scaling) {
    Set(scaling, 0, 0, 0,
        0, scaling, 0, 0,
        0, 0, scaling, 0);
    return (*this);
}

MatrixTransform& MatrixTransform::operator *=(const MatrixTransform& __restrict__ m) __restrict__
{
	float x = Get(0, 0);
	float y = Get(1, 0);
	float z = Get(2, 0);
	float w = Get(3, 0);
	n[0][0] = x * m.Get(0, 0) + y * m.Get(0, 1) + z * m.Get(0, 2);
	n[0][1] = x * m.Get(1, 0) + y * m.Get(1, 1) + z * m.Get(1, 2);
	n[0][2] = x * m.Get(2, 0) + y * m.Get(2, 1) + z * m.Get(2, 2);
	n[0][3] = x * m.Get(3, 0) + y * m.Get(3, 1) + z * m.Get(3, 2) + w;
	
	x = Get(0, 1);
	y = Get(1, 1);
	z = Get(2, 1);
	w = Get(3, 1);
	n[1][0] = x * m.Get(0, 0) + y * m.Get(0, 1) + z * m.Get(0, 2);
	n[1][1] = x * m.Get(1, 0) + y * m.Get(1, 1) + z * m.Get(1, 2);
	n[1][2] = x * m.Get(2, 0) + y * m.Get(2, 1) + z * m.Get(2, 2);
	n[1][3] = x * m.Get(3, 0) + y * m.Get(3, 1) + z * m.Get(3, 2);
	
	x = Get(0, 2);
	y = Get(1, 2);
	z = Get(2, 2);
	w = Get(3, 2);
	n[2][0] = x * m.Get(0, 0) + y * m.Get(0, 1) + z * m.Get(0, 2);
	n[2][1] = x * m.Get(1, 0) + y * m.Get(1, 1) + z * m.Get(1, 2);
	n[2][2] = x * m.Get(2, 0) + y * m.Get(2, 1) + z * m.Get(2, 2);
	n[2][3] = x * m.Get(3, 0) + y * m.Get(3, 1) + z * m.Get(3, 2) + w;
		
	return (*this);
}

MatrixTransform operator *(const MatrixTransform& __restrict__ m1,
const MatrixTransform& __restrict__ m2)
{
#ifdef USE_SSE2
    MatrixTransform m3;
    SIMDMatrixMultiply4x3(&m1.n[0][0], &m2.n[0][0], &m3.n[0][0]);
#if 0
    char *s1 = m3.GetString();
    Matrix4D m4 = Matrix4D(m1) * Matrix4D(m2);
    char *s2 = m4.GetString();
    sreMessage(SRE_MESSAGE_INFO, "%s\n%s", s1, s2);
    delete [] s1;
    delete [] s2;
#endif
    return m3;
#else
    return
        MatrixTransform(
            m1.Get(0, 0) * m2.Get(0, 0) + m1.Get(1, 0) * m2.Get(0, 1) + m1.Get(2, 0) * m2.Get(0, 2),
            m1.Get(0, 0) * m2.Get(1, 0) + m1.Get(1, 0) * m2.Get(1, 1) + m1.Get(2, 0) * m2.Get(1, 2),
            m1.Get(0, 0) * m2.Get(2, 0) + m1.Get(1, 0) * m2.Get(2, 1) + m1.Get(2, 0) * m2.Get(2, 2),
            m1.Get(0, 0) * m2.Get(3, 0) + m1.Get(1, 0) * m2.Get(3, 1) + m1.Get(2, 0) * m2.Get(3, 2) + m1.Get(3, 0),
            m1.Get(0, 1) * m2.Get(0, 0) + m1.Get(1, 1) * m2.Get(0, 1) + m1.Get(2, 1) * m2.Get(0, 2),
            m1.Get(0, 1) * m2.Get(1, 0) + m1.Get(1, 1) * m2.Get(1, 1) + m1.Get(2, 1) * m2.Get(1, 2),
            m1.Get(0, 1) * m2.Get(2, 0) + m1.Get(1, 1) * m2.Get(2, 1) + m1.Get(2, 1) * m2.Get(2, 2),
            m1.Get(0, 1) * m2.Get(3, 0) + m1.Get(1, 1) * m2.Get(3, 1) + m1.Get(2, 1) * m2.Get(3, 2) + m1.Get(3, 1),
            m1.Get(0, 2) * m2.Get(0, 0) + m1.Get(1, 2) * m2.Get(0, 1) + m1.Get(2, 2) * m2.Get(0, 2),
            m1.Get(0, 2) * m2.Get(1, 0) + m1.Get(1, 2) * m2.Get(1, 1) + m1.Get(2, 2) * m2.Get(1, 2),
            m1.Get(0, 2) * m2.Get(2, 0) + m1.Get(1, 2) * m2.Get(2, 1) + m1.Get(2, 2) * m2.Get(2, 2),
            m1.Get(0, 2) * m2.Get(3, 0) + m1.Get(1, 2) * m2.Get(3, 1) + m1.Get(2, 2) * m2.Get(3, 2) + m1.Get(3, 2));
#endif
}

Matrix4D operator *(const Matrix4D& __restrict__ m1, const MatrixTransform& __restrict__ m2)
{
#ifdef USE_SSE2
    Matrix4D m3;
    SIMDMatrixMultiply4x4By4x3(&m1.n[0][0], &m2.n[0][0], &m3.n[0][0]);
#if 0
    char *s1 = m3.GetString();
    Matrix4D m4 = m1 * Matrix4D(m2);
    char *s2 = m4.GetString();
    sreMessage(SRE_MESSAGE_INFO, "SIMD %s\nNon-SIMD %s", s1, s2);
    delete [] s1;
    delete [] s2;
#endif
    return m3;
#else
    return
        Matrix4D(m1.Get(0, 0) * m2.Get(0, 0) + m1.Get(1, 0) * m2.Get(0, 1) + m1.Get(2, 0) * m2.Get(0, 2),
            m1.Get(0, 0) * m2.Get(1, 0) + m1.Get(1, 0) * m2.Get(1, 1) + m1.Get(2, 0) * m2.Get(1, 2),
            m1.Get(0, 0) * m2.Get(2, 0) + m1.Get(1, 0) * m2.Get(2, 1) + m1.Get(2, 0) * m2.Get(2, 2),
            m1.Get(0, 0) * m2.Get(3, 0) + m1.Get(1, 0) * m2.Get(3, 1) + m1.Get(2, 0) * m2.Get(3, 2) + m1.Get(3, 0],
            m1.Get(0, 1) * m2.Get(0, 0) + m1.Get(1, 1) * m2.Get(0, 1) + m1.Get(2, 1) * m2.Get(0, 2),
            m1.Get(0, 1) * m2.Get(1, 0) + m1.Get(1, 1) * m2.Get(1, 1) + m1.Get(2, 1) * m2.Get(1, 2),
            m1.Get(0, 1) * m2.Get(2, 0) + m1.Get(1, 1) * m2.Get(2, 1) + m1.Get(2, 1) * m2.Get(2, 2),
            m1.Get(0, 1) * m2.Get(3, 0) + m1.Get(1, 1) * m2.Get(3, 1) + m1.Get(2, 1) * m2.Get(3, 2) + m1.Get(3, 1],
            m1.Get(0, 2) * m2.Get(0, 0) + m1.Get(1, 2) * m2.Get(0, 1) + m1.Get(2, 2) * m2.Get(0, 2),
            m1.Get(0, 2) * m2.Get(1, 0) + m1.Get(1, 2) * m2.Get(1, 1) + m1.Get(2, 2) * m2.Get(1, 2),
            m1.Get(0, 2) * m2.Get(2, 0) + m1.Get(1, 2) * m2.Get(2, 1) + m1.Get(2, 2) * m2.Get(2, 2),
            m1.Get(0, 2) * m2.Get(3, 0) + m1.Get(1, 2) * m2.Get(3, 1) + m1.Get(2, 2) * m2.Get(3, 2) + m1.Get(3, 2],
            m1.Get(0, 3) * m2.Get(0, 0) + m1.Get(1, 3) * m2.Get(0, 1) + m1.Get(2, 3) * m2.Get(0, 2),
            m1.Get(0, 3) * m2.Get(1, 0) + m1.Get(1, 3) * m2.Get(1, 1) + m1.Get(2, 3) * m2.Get(1, 2),
            m1.Get(0, 3) * m2.Get(2, 0) + m1.Get(1, 3) * m2.Get(2, 1) + m1.Get(2, 3) * m2.Get(2, 2),
            m1.Get(0, 3) * m2.Get(3, 0) + m1.Get(1, 3) * m2.Get(3, 1) + m1.Get(2, 3) * m2.Get(3, 2) + m1.Get(3, 3]));
#endif
}

Vector4D operator *(const MatrixTransform&  __restrict__ m, const Vector4D& __restrict__ v)
{
    return Vector4D(
        m.Get(0, 0) * v.x + m.Get(1, 0) * v.y + m.Get(2, 0) * v.z + m.Get(3, 0) * v.w,
        m.Get(0, 1) * v.x + m.Get(1, 1) * v.y + m.Get(2, 1) * v.z + m.Get(3, 1) * v.w,
        m.Get(0, 2) * v.x + m.Get(1, 2) * v.y + m.Get(2, 2) * v.z + m.Get(3, 2) * v.w,
        v.w);
}

Vector4D operator *(const MatrixTransform& __restrict__ m, const Vector3D& __restrict__ v)
{
    return Vector4D(
        m.Get(0, 0) * v.x + m.Get(1, 0) * v.y + m.Get(2, 0) * v.z,
        m.Get(0, 1) * v.x + m.Get(1, 1) * v.y + m.Get(2, 1) * v.z,
        m.Get(0, 2) * v.x + m.Get(1, 2) * v.y + m.Get(2, 2) * v.z,
        0.0f);
}

Vector4D operator *(const MatrixTransform& __restrict__ m, const Point3D& __restrict__ p)
{
    return Vector4D(
        m.Get(0, 0) * p.x + m.Get(1, 0) * p.y + m.Get(2, 0) * p.z + m.Get(3, 0),
        m.Get(0, 1) * p.x + m.Get(1, 1) * p.y + m.Get(2, 1) * p.z + m.Get(3, 1),
        m.Get(0, 2) * p.x + m.Get(1, 2) * p.y + m.Get(2, 2) * p.z + m.Get(3, 2),
        1.0f);
}


bool operator ==(const MatrixTransform& m1, const MatrixTransform& m2)
{
    return ((m1.Get(0, 0) == m2.Get(0, 0)) && (m1.Get(0, 1) == m2.Get(0, 1)) &&
        (m1.Get(0, 2) == m2.Get(0, 2)) && (m1.Get(1, 0) == m2.Get(1, 0)) &&
        (m1.Get(1, 1) == m2.Get(1, 1)) && (m1.Get(1, 2) == m2.Get(1, 2)) &&
        (m1.Get(2, 0) == m2.Get(2, 0)) && (m1.Get(2, 1) == m2.Get(2, 1)) &&
        (m1.Get(2, 2) == m2.Get(2, 2)) && (m1.Get(3, 0) == m2.Get(3, 0)) &&
        (m1.Get(3, 1) == m2.Get(3, 1)) && (m1.Get(3, 2) == m2.Get(3, 2)));
}

bool operator !=(const MatrixTransform& m1, const MatrixTransform& m2)
{
    return ((m1.Get(0, 0) != m2.Get(0, 0)) || (m1.Get(0, 1) != m2.Get(0, 1)) ||
        (m1.Get(0, 2) != m2.Get(0, 2)) || (m1.Get(1, 0) != m2.Get(1, 0)) ||
        (m1.Get(1, 1) != m2.Get(1, 1)) || (m1.Get(1, 2) != m2.Get(1, 2)) ||
        (m1.Get(2, 0) != m2.Get(2, 0)) || (m1.Get(2, 1) != m2.Get(2, 1)) ||
        (m1.Get(2, 2) != m2.Get(2, 2)) || (m1.Get(3, 0) != m2.Get(3, 0)) ||
        (m1.Get(3, 1) != m2.Get(3, 1)) || (m1.Get(3, 2) != m2.Get(3, 2)));
}

Matrix4D Inverse(const MatrixTransform& m)
{
	Matrix4D m2 = Matrix4D(m);
	return Inverse(m2);
}

Matrix4D Transpose(const MatrixTransform& m)
{
	return (Matrix4D(m(0,0), m(1,0), m(2,0), 0.0f,
		m(0,1), m(1,1), m(2,1), 0.0f,
		m(0,2), m(1,2), m(2,2), 0.0f,
		m(0,3), m(1,3), m(2,3), 1.0f));
}

// Additional member functions for debugging.

char *Vector3D::GetString() const {
    char *s = new char[64];
    sprintf(s, "Vector3D(%f, %f, %f)", x, y, z);
    return s;
}

char *Vector4D::GetString() const {
    char *s = new char[64];
    sprintf(s, "Vector4D(%f, %f, %f, %f)", x, y, z, w);
    return s;
}

char *Matrix3D::GetString() const {
    char *s = new char[128];
    sprintf(s, "Matrix3D( ");
    for (int i = 0; i < 3; i++) {
        Vector3D V = GetRow(i);
        char rowstr[64];
        sprintf(rowstr, "(%f, %f, %f) ", V.x, V.y, V.z);
        strcat(s, rowstr);
    }
    strcat(s, ")");
    return s;
}

char *Matrix4D::GetString() const {
    char *s = new char[256];
    sprintf(s, "Matrix4D( ");
    for (int i = 0; i < 4; i++) {
        Vector4D V = GetRow(i);
        char rowstr[64];
        sprintf(rowstr, "(%f, %f, %f, %f) ", V.x, V.y, V.z, V.w);
        strcat(s, rowstr);
    }
    strcat(s, ")");
    return s;
}

char *MatrixTransform::GetString() const {
    char *s = new char[256];
    sprintf(s, "MatrixTransform( ");
    for (int i = 0; i < 4; i++) {
        Vector4D V;
        if (i < 3)
            V = GetRow(i);
        else
            V = Vector4D(0.0f, 0.0f, 0.0f, 1.0f);
        char rowstr[64];
        sprintf(rowstr, "(%f, %f, %f, %f) ", V.x, V.y, V.z, V.w);
        strcat(s, rowstr);
    }
    strcat(s, ")");
    return s;
}

// Additional color member functions.

Color& Color::SetRandom() {
    sreRNG *rng = sreGetDefaultRNG();
    r = rng->RandomFloat(1.0f);
    g = rng->RandomFloat(1.0f);
    b = rng->RandomFloat(1.0f);
    return (*this);
}

static const Vector3D Crgb = Vector3D(
    0.212655f, // Red factor
    0.715158f, // Green factor
    0.072187f  // Blue factor
    );

// Inverse of sRGB "gamma" function. (approx 2.2).
float InverseSRGBGamma(float c) {
    if (c <= 0.04045f)
        return c / 12.92f;
    else 
        return powf(((c + 0.055f) / (1.055f)), 2.4);
}

// sRGB "gamma" function (approx 2.2).
float SRGBGamma(float v) {
    if (v <= 0.0031308f)
        v *= 12.92f;
    else
        v = 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
    return v;
}

Color Color::GetLinearFromSRGB() const {
    return Color(
        InverseSRGBGamma(r),
        InverseSRGBGamma(g),
        InverseSRGBGamma(b)
        );
}

Color Color::GetSRGBFromLinear() const {
    return Color(
        SRGBGamma(r),
        SRGBGamma(g),
        SRGBGamma(b)
        );
}

float Color::LinearIntensity() const {
    return Dot((*this), Crgb);
}

float Color::SRGBIntensity() const {
    return SRGBGamma(GetLinearFromSRGB().LinearIntensity());
}

unsigned int Color::GetRGBX8() const {
    unsigned int pixel;
    pixel = (unsigned int)floorf(r * 255.0f + 0.5f) +
        ((unsigned int)floorf(g * 255.0f + 0.5f) << 8) +
        ((unsigned int)floorf(b * 255.0f + 0.5f) << 16) + 0xFF000000;
    return pixel;
}

// Calculate an array of dot products.

void CalculateDotProducts(int n, const Vector3D *v1, const Vector3D *v2,
float *dot) {
    int i = 0;
#ifdef USE_SSE2
    for (; i + 3 < n; i += 4) {
        SIMDCalculateFourDotProducts(&v1[i], &v2[i], &dot[i]);
    }
#else
    for (; i + 3 < n; i += 4) {
        dot[i] = Dot(v1[i], v2[i]);
        dot[i + 1] = Dot(v1[i + 1], v2[i + 1]);
        dot[i + 2] = Dot(v1[i + 2], v2[i + 2]);
        dot[i + 3] = Dot(v1[i + 3], v2[i + 3]);
    }
#endif
    for (; i < n; i++)
        dot[i] = Dot(v1[i], v2[i]);
}

void CalculateDotProducts(int n, const Vector4D *v1, const Vector4D *v2,
float *dot) {
    int i = 0;
#ifdef USE_SSE2
    for (; i + 3 < n; i += 4) {
        SIMDCalculateFourDotProducts(&v1[i], &v2[i], &dot[i]);
    }
#else
    for (; i + 3 < n; i += 4) {
        dot[i] = Dot(v1[i], v2[i]);
        dot[i + 1] = Dot(v1[i + 1], v2[i + 1]);
        dot[i + 2] = Dot(v1[i + 2], v2[i + 2]);
        dot[i + 3] = Dot(v1[i + 3], v2[i + 3]);
    }
#endif
    for (; i < n; i++)
        dot[i] = Dot(v1[i], v2[i]);
}

// Determine the minimum and maximum dot product of an array of vertices with a
// given constant vector.

void CalculateMinAndMaxDotProduct(int nu_vertices, Vector3D *vertex,
const Vector3D& v2, float& min_dot_product, float& max_dot_product) {
    int i = 0;
#ifdef USE_SSE2
    __simd128_float m_v2_x = simd128_set1_float(v2.x);
    __simd128_float m_v2_y = simd128_set1_float(v2.y);
    __simd128_float m_v2_z = simd128_set1_float(v2.z);
    __simd128_float m_min_dot = simd128_set1_float(POSITIVE_INFINITY_FLOAT);
    __simd128_float m_max_dot = simd128_set1_float(NEGATIVE_INFINITY_FLOAT);
    for (; i + 3 < nu_vertices; i += 4) {
        __simd128_float m_vertex_x = simd128_set_float(
            vertex[i].x, vertex[i + 1].x, vertex[i + 2].x, vertex[i + 3].x);
        __simd128_float m_vertex_y = simd128_set_float(
            vertex[i].y, vertex[i + 1].y, vertex[i + 2].y, vertex[i + 3].y);
        __simd128_float m_vertex_z = simd128_set_float(
            vertex[i].z, vertex[i + 1].z, vertex[i + 2].z, vertex[i + 3].z);
        __simd128_float m_dot_x, m_dot_y, m_dot_z, m_dot;
        m_dot_x = simd128_mul_float(m_vertex_x, m_v2_x);
        m_dot_y = simd128_mul_float(m_vertex_y, m_v2_y);
        m_dot_z = simd128_mul_float(m_vertex_z, m_v2_z);
        m_dot = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y), m_dot_z);
        m_min_dot = simd128_min_float(m_min_dot, m_dot);
        m_max_dot = simd128_max_float(m_max_dot, m_dot);
    }
    __simd128_float shifted_float1, shifted_float2, shifted_float3;
    __simd128_float m_min_dot_23, m_max_dot_23;
    shifted_float1 = simd128_shift_right_float(m_min_dot, 1);
    shifted_float2 = simd128_shift_right_float(m_min_dot, 2);
    shifted_float3 = simd128_shift_right_float(m_min_dot, 3);
    m_min_dot = simd128_min1_float(m_min_dot, shifted_float1);
    m_min_dot_23 = simd128_min1_float(shifted_float2, shifted_float3);
    m_min_dot = simd128_min1_float(m_min_dot, m_min_dot_23);
    min_dot_product = simd128_get_float(m_min_dot);
    shifted_float1 = simd128_shift_right_float(m_max_dot, 1);
    shifted_float2 = simd128_shift_right_float(m_max_dot, 2);
    shifted_float3 = simd128_shift_right_float(m_max_dot, 3);
    m_min_dot = simd128_max1_float(m_max_dot, shifted_float1);
    m_max_dot_23 = simd128_max1_float(shifted_float2, shifted_float3);
    m_max_dot = simd128_max1_float(m_max_dot, m_max_dot_23);
    max_dot_product = simd128_get_float(m_max_dot);
#else
    min_dot_product = POSITIVE_INFINITY_FLOAT;
    max_dot_product = NEGATIVE_INFINITY_FLOAT;
#endif
    // Process the remaining vertices.
    for (; i < nu_vertices; i++) {
        min_dot_product = minf(min_dot_product, Dot(vertex[i], v2));
        max_dot_product = maxf(max_dot_product, Dot(vertex[i], v2));
    }
}

void CalculateMinAndMaxDotProduct(int nu_vertices, Vector4D *vertex,
const Vector4D& v2, float& min_dot_product, float& max_dot_product) {
    int i = 0;
#ifdef USE_SSE2
    __simd128_float m_v2_x = simd128_set1_float(v2.x);
    __simd128_float m_v2_y = simd128_set1_float(v2.y);
    __simd128_float m_v2_z = simd128_set1_float(v2.z);
    __simd128_float m_v2_w = simd128_set1_float(v2.w);
    __simd128_float m_min_dot = simd128_set1_float(POSITIVE_INFINITY_FLOAT);
    __simd128_float m_max_dot = simd128_set1_float(NEGATIVE_INFINITY_FLOAT);
    for (; i + 3 < nu_vertices; i += 4) {
        __simd128_float m_vertex_x = simd128_set_float(
            vertex[i].x, vertex[i + 1].x, vertex[i + 2].x, vertex[i + 3].x);
        __simd128_float m_vertex_y = simd128_set_float(
            vertex[i].y, vertex[i + 1].y, vertex[i + 2].y, vertex[i + 3].y);
        __simd128_float m_vertex_z = simd128_set_float(
            vertex[i].z, vertex[i + 1].z, vertex[i + 2].z, vertex[i + 3].z);
        __simd128_float m_vertex_w = simd128_set_float(
            vertex[i].w, vertex[i + 1].w, vertex[i + 2].w, vertex[i + 3].w);
        __simd128_float m_dot_x, m_dot_y, m_dot_z, m_dot_w, m_dot;
        m_dot_x = simd128_mul_float(m_vertex_x, m_v2_x);
        m_dot_y = simd128_mul_float(m_vertex_y, m_v2_y);
        m_dot_z = simd128_mul_float(m_vertex_z, m_v2_z);
        m_dot_w = simd128_mul_float(m_vertex_w, m_v2_w);
        m_dot = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            simd128_add_float(m_dot_z, m_dot_w)
            );
        m_min_dot = simd128_min_float(m_min_dot, m_dot);
        m_max_dot = simd128_max_float(m_max_dot, m_dot);
    }
    __simd128_float shifted_float1, shifted_float2, shifted_float3;
    __simd128_float m_min_dot_23, m_max_dot_23;
    shifted_float1 = simd128_shift_right_float(m_min_dot, 1);
    shifted_float2 = simd128_shift_right_float(m_min_dot, 2);
    shifted_float3 = simd128_shift_right_float(m_min_dot, 3);
    m_min_dot = simd128_min1_float(m_min_dot, shifted_float1);
    m_min_dot_23 = simd128_min1_float(shifted_float2, shifted_float3);
    m_min_dot = simd128_min1_float(m_min_dot, m_min_dot_23);
    min_dot_product = simd128_get_float(m_min_dot);
    shifted_float1 = simd128_shift_right_float(m_max_dot, 1);
    shifted_float2 = simd128_shift_right_float(m_max_dot, 2);
    shifted_float3 = simd128_shift_right_float(m_max_dot, 3);
    m_min_dot = simd128_max1_float(m_max_dot, shifted_float1);
    m_max_dot_23 = simd128_max1_float(shifted_float2, shifted_float3);
    m_max_dot = simd128_max1_float(m_max_dot, m_max_dot_23);
    max_dot_product = simd128_get_float(m_max_dot);
#else
    min_dot_product = POSITIVE_INFINITY_FLOAT;
    max_dot_product = NEGATIVE_INFINITY_FLOAT;
#endif
    // Process the remaining vertices.
    for (; i < nu_vertices; i++) {
        min_dot_product = minf(min_dot_product, Dot(vertex[i], v2));
        max_dot_product = maxf(max_dot_product, Dot(vertex[i], v2));
    }
}

// Determine the minimum and maximum dot products of an array of vertices with three
// constant vectors.

void CalculateMinAndMaxDotProductWithThreeConstantVectors(int nu_vertices,
Vector3D *vertex, const Vector3D *C, float *min_dot_product, float *max_dot_product) {
    int i = 0;
#ifdef USE_SSE2
    __simd128_float m_v2_0_x = simd128_set1_float(C[0].x);
    __simd128_float m_v2_0_y = simd128_set1_float(C[0].y);
    __simd128_float m_v2_0_z = simd128_set1_float(C[0].z);
    __simd128_float m_v2_1_x = simd128_set1_float(C[1].x);
    __simd128_float m_v2_1_y = simd128_set1_float(C[1].y);
    __simd128_float m_v2_1_z = simd128_set1_float(C[1].z);
    __simd128_float m_v2_2_x = simd128_set1_float(C[2].x);
    __simd128_float m_v2_2_y = simd128_set1_float(C[2].y);
    __simd128_float m_v2_2_z = simd128_set1_float(C[2].z);
    __simd128_float m_min_dot_C0 = simd128_set1_float(POSITIVE_INFINITY_FLOAT);
    __simd128_float m_min_dot_C1 = simd128_set1_float(POSITIVE_INFINITY_FLOAT);
    __simd128_float m_min_dot_C2 = simd128_set1_float(POSITIVE_INFINITY_FLOAT);
    __simd128_float m_max_dot_C0 = simd128_set1_float(NEGATIVE_INFINITY_FLOAT);
    __simd128_float m_max_dot_C1 = simd128_set1_float(NEGATIVE_INFINITY_FLOAT);
    __simd128_float m_max_dot_C2 = simd128_set1_float(NEGATIVE_INFINITY_FLOAT);
    for (; i + 3 < nu_vertices; i += 4) {
        __simd128_float m_vertex_x = simd128_set_float(
            vertex[i].x, vertex[i + 1].x, vertex[i + 2].x, vertex[i + 3].x);
        __simd128_float m_vertex_y = simd128_set_float(
            vertex[i].y, vertex[i + 1].y, vertex[i + 2].y, vertex[i + 3].y);
        __simd128_float m_vertex_z = simd128_set_float(
            vertex[i].z, vertex[i + 1].z, vertex[i + 2].z, vertex[i + 3].z);
        __simd128_float m_dot_x, m_dot_y, m_dot_z, m_dot;
        m_dot_x = simd128_mul_float(m_vertex_x, m_v2_0_x);
        m_dot_y = simd128_mul_float(m_vertex_y, m_v2_0_y);
        m_dot_z = simd128_mul_float(m_vertex_z, m_v2_0_z);
        m_dot = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y), m_dot_z);
        m_min_dot_C0 = simd128_min_float(m_min_dot_C0, m_dot);
        m_max_dot_C0 = simd128_max_float(m_max_dot_C0, m_dot);
        m_dot_x = simd128_mul_float(m_vertex_x, m_v2_1_x);
        m_dot_y = simd128_mul_float(m_vertex_y, m_v2_1_y);
        m_dot_z = simd128_mul_float(m_vertex_z, m_v2_1_z);
        m_dot = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y), m_dot_z);
        m_min_dot_C1 = simd128_min_float(m_min_dot_C1, m_dot);
        m_max_dot_C1 = simd128_max_float(m_max_dot_C1, m_dot);
        m_dot_x = simd128_mul_float(m_vertex_x, m_v2_2_x);
        m_dot_y = simd128_mul_float(m_vertex_y, m_v2_2_y);
        m_dot_z = simd128_mul_float(m_vertex_z, m_v2_2_z);
        m_dot = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y), m_dot_z);
        m_min_dot_C2 = simd128_min_float(m_min_dot_C2, m_dot);
        m_max_dot_C2 = simd128_max_float(m_max_dot_C2, m_dot);
    }
    __simd128_float shifted_float1, shifted_float2, shifted_float3;
    __simd128_float m_min_dot_23, m_max_dot_23;
    shifted_float1 = simd128_shift_right_float(m_min_dot_C0, 1);
    shifted_float2 = simd128_shift_right_float(m_min_dot_C0, 2);
    shifted_float3 = simd128_shift_right_float(m_min_dot_C0, 3);
    m_min_dot_C0 = simd128_min1_float(m_min_dot_C0, shifted_float1);
    m_min_dot_23 = simd128_min1_float(shifted_float2, shifted_float3);
    m_min_dot_C0 = simd128_min1_float(m_min_dot_C0, m_min_dot_23);
    min_dot_product[0] = simd128_get_float(m_min_dot_C0);
    shifted_float1 = simd128_shift_right_float(m_max_dot_C0, 1);
    shifted_float2 = simd128_shift_right_float(m_max_dot_C0, 2);
    shifted_float3 = simd128_shift_right_float(m_max_dot_C0, 3);
    m_min_dot_C0 = simd128_max1_float(m_max_dot_C0, shifted_float1);
    m_max_dot_23 = simd128_max1_float(shifted_float2, shifted_float3);
    m_max_dot_C0 = simd128_max1_float(m_max_dot_C0, m_max_dot_23);
    max_dot_product[0] = simd128_get_float(m_max_dot_C0);

    shifted_float1 = simd128_shift_right_float(m_min_dot_C1, 1);
    shifted_float2 = simd128_shift_right_float(m_min_dot_C1, 2);
    shifted_float3 = simd128_shift_right_float(m_min_dot_C1, 3);
    m_min_dot_C1 = simd128_min1_float(m_min_dot_C1, shifted_float1);
    m_min_dot_23 = simd128_min1_float(shifted_float2, shifted_float3);
    m_min_dot_C1 = simd128_min1_float(m_min_dot_C1, m_min_dot_23);
    min_dot_product[1] = simd128_get_float(m_min_dot_C1);
    shifted_float1 = simd128_shift_right_float(m_max_dot_C1, 1);
    shifted_float2 = simd128_shift_right_float(m_max_dot_C1, 2);
    shifted_float3 = simd128_shift_right_float(m_max_dot_C1, 3);
    m_max_dot_C1 = simd128_max1_float(m_max_dot_C1, shifted_float1);
    m_max_dot_23 = simd128_max1_float(shifted_float2, shifted_float3);
    m_max_dot_C1 = simd128_max1_float(m_max_dot_C1, m_max_dot_23);
    max_dot_product[1] = simd128_get_float(m_max_dot_C1);

    shifted_float1 = simd128_shift_right_float(m_min_dot_C2, 1);
    shifted_float2 = simd128_shift_right_float(m_min_dot_C2, 2);
    shifted_float3 = simd128_shift_right_float(m_min_dot_C2, 3);
    m_min_dot_C2 = simd128_min1_float(m_min_dot_C2, shifted_float1);
    m_min_dot_23 = simd128_min1_float(shifted_float2, shifted_float3);
    m_min_dot_C2 = simd128_min1_float(m_min_dot_C2, m_min_dot_23);
    min_dot_product[2] = simd128_get_float(m_min_dot_C2);
    shifted_float1 = simd128_shift_right_float(m_max_dot_C2, 1);
    shifted_float2 = simd128_shift_right_float(m_max_dot_C2, 2);
    shifted_float3 = simd128_shift_right_float(m_max_dot_C2, 3);
    m_max_dot_C2 = simd128_max1_float(m_max_dot_C2, shifted_float1);
    m_max_dot_23 = simd128_max1_float(shifted_float2, shifted_float3);
    m_max_dot_C2 = simd128_max1_float(m_max_dot_C2, m_max_dot_23);
    max_dot_product[2] = simd128_get_float(m_max_dot_C2);
#else
    for (int j = 0; j < 3; j++) {
        min_dot_product[j] = POSITIVE_INFINITY_FLOAT;
        max_dot_product[j] = NEGATIVE_INFINITY_FLOAT;
    }
#endif
    // Process the remaining vertices.
    for (; i < nu_vertices; i++)
        for (int j = 0; j < 3; j++) {
            min_dot_product[j] = minf(min_dot_product[j], Dot(vertex[i], C[j]));
            max_dot_product[j] = maxf(max_dot_product[j], Dot(vertex[i], C[j]));
        }
}

void CalculateMinAndMaxDotProductWithThreeConstantVectors(int nu_vertices,
Vector4D *vertex, const Vector4D *C, float *min_dot_product, float *max_dot_product) {
    int i = 0;
#ifdef USE_SSE2
    __simd128_float m_v2_0_x = simd128_set1_float(C[0].x);
    __simd128_float m_v2_0_y = simd128_set1_float(C[0].y);
    __simd128_float m_v2_0_z = simd128_set1_float(C[0].z);
    __simd128_float m_v2_0_w = simd128_set1_float(C[0].w);
    __simd128_float m_v2_1_x = simd128_set1_float(C[1].x);
    __simd128_float m_v2_1_y = simd128_set1_float(C[1].y);
    __simd128_float m_v2_1_z = simd128_set1_float(C[1].z);
    __simd128_float m_v2_1_w = simd128_set1_float(C[1].w);
    __simd128_float m_v2_2_x = simd128_set1_float(C[2].x);
    __simd128_float m_v2_2_y = simd128_set1_float(C[2].y);
    __simd128_float m_v2_2_z = simd128_set1_float(C[2].z);
    __simd128_float m_v2_2_w = simd128_set1_float(C[2].w);
    __simd128_float m_min_dot_C0 = simd128_set1_float(POSITIVE_INFINITY_FLOAT);
    __simd128_float m_min_dot_C1 = simd128_set1_float(POSITIVE_INFINITY_FLOAT);
    __simd128_float m_min_dot_C2 = simd128_set1_float(POSITIVE_INFINITY_FLOAT);
    __simd128_float m_max_dot_C0 = simd128_set1_float(NEGATIVE_INFINITY_FLOAT);
    __simd128_float m_max_dot_C1 = simd128_set1_float(NEGATIVE_INFINITY_FLOAT);
    __simd128_float m_max_dot_C2 = simd128_set1_float(NEGATIVE_INFINITY_FLOAT);
    for (; i + 3 < nu_vertices; i += 4) {
        __simd128_float m_vertex_x = simd128_set_float(
            vertex[i].x, vertex[i + 1].x, vertex[i + 2].x, vertex[i + 3].x);
        __simd128_float m_vertex_y = simd128_set_float(
            vertex[i].y, vertex[i + 1].y, vertex[i + 2].y, vertex[i + 3].y);
        __simd128_float m_vertex_z = simd128_set_float(
            vertex[i].z, vertex[i + 1].z, vertex[i + 2].z, vertex[i + 3].z);
        __simd128_float m_vertex_w = simd128_set_float(
            vertex[i].w, vertex[i + 1].w, vertex[i + 2].w, vertex[i + 3].w);
        __simd128_float m_dot_x, m_dot_y, m_dot_z, m_dot_w, m_dot;
        m_dot_x = simd128_mul_float(m_vertex_x, m_v2_0_x);
        m_dot_y = simd128_mul_float(m_vertex_y, m_v2_0_y);
        m_dot_z = simd128_mul_float(m_vertex_z, m_v2_0_z);
        m_dot_w = simd128_mul_float(m_vertex_w, m_v2_0_w);
        m_dot = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            simd128_add_float(m_dot_z, m_dot_w)
            );
        m_min_dot_C0 = simd128_min_float(m_min_dot_C0, m_dot);
        m_max_dot_C0 = simd128_max_float(m_max_dot_C0, m_dot);
        m_dot_x = simd128_mul_float(m_vertex_x, m_v2_1_x);
        m_dot_y = simd128_mul_float(m_vertex_y, m_v2_1_y);
        m_dot_z = simd128_mul_float(m_vertex_z, m_v2_1_z);
        m_dot_w = simd128_mul_float(m_vertex_w, m_v2_1_w);
        m_dot = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            simd128_add_float(m_dot_z, m_dot_w)
            );
        m_min_dot_C1 = simd128_min_float(m_min_dot_C1, m_dot);
        m_max_dot_C1 = simd128_max_float(m_max_dot_C1, m_dot);
        m_dot_x = simd128_mul_float(m_vertex_x, m_v2_2_x);
        m_dot_y = simd128_mul_float(m_vertex_y, m_v2_2_y);
        m_dot_z = simd128_mul_float(m_vertex_z, m_v2_2_z);
        m_dot_w = simd128_mul_float(m_vertex_w, m_v2_2_w);
        m_dot = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            simd128_add_float(m_dot_z, m_dot_w)
            );
        m_min_dot_C2 = simd128_min_float(m_min_dot_C2, m_dot);
        m_max_dot_C2 = simd128_max_float(m_max_dot_C2, m_dot);
    }
    __simd128_float shifted_float1, shifted_float2, shifted_float3;
    __simd128_float m_min_dot_23, m_max_dot_23;
    shifted_float1 = simd128_shift_right_float(m_min_dot_C0, 1);
    shifted_float2 = simd128_shift_right_float(m_min_dot_C0, 2);
    shifted_float3 = simd128_shift_right_float(m_min_dot_C0, 3);
    m_min_dot_C0 = simd128_min1_float(m_min_dot_C0, shifted_float1);
    m_min_dot_23 = simd128_min1_float(shifted_float2, shifted_float3);
    m_min_dot_C0 = simd128_min1_float(m_min_dot_C0, m_min_dot_23);
    min_dot_product[0] = simd128_get_float(m_min_dot_C0);
    shifted_float1 = simd128_shift_right_float(m_max_dot_C0, 1);
    shifted_float2 = simd128_shift_right_float(m_max_dot_C0, 2);
    shifted_float3 = simd128_shift_right_float(m_max_dot_C0, 3);
    m_min_dot_C0 = simd128_max1_float(m_max_dot_C0, shifted_float1);
    m_max_dot_23 = simd128_max1_float(shifted_float2, shifted_float3);
    m_max_dot_C0 = simd128_max1_float(m_max_dot_C0, m_max_dot_23);
    max_dot_product[0] = simd128_get_float(m_max_dot_C0);

    shifted_float1 = simd128_shift_right_float(m_min_dot_C1, 1);
    shifted_float2 = simd128_shift_right_float(m_min_dot_C1, 2);
    shifted_float3 = simd128_shift_right_float(m_min_dot_C1, 3);
    m_min_dot_C1 = simd128_min1_float(m_min_dot_C1, shifted_float1);
    m_min_dot_23 = simd128_min1_float(shifted_float2, shifted_float3);
    m_min_dot_C1 = simd128_min1_float(m_min_dot_C1, m_min_dot_23);
    min_dot_product[1] = simd128_get_float(m_min_dot_C1);
    shifted_float1 = simd128_shift_right_float(m_max_dot_C1, 1);
    shifted_float2 = simd128_shift_right_float(m_max_dot_C1, 2);
    shifted_float3 = simd128_shift_right_float(m_max_dot_C1, 3);
    m_max_dot_C1 = simd128_max1_float(m_max_dot_C1, shifted_float1);
    m_max_dot_23 = simd128_max1_float(shifted_float2, shifted_float3);
    m_max_dot_C1 = simd128_max1_float(m_max_dot_C1, m_max_dot_23);
    max_dot_product[1] = simd128_get_float(m_max_dot_C1);

    shifted_float1 = simd128_shift_right_float(m_min_dot_C2, 1);
    shifted_float2 = simd128_shift_right_float(m_min_dot_C2, 2);
    shifted_float3 = simd128_shift_right_float(m_min_dot_C2, 3);
    m_min_dot_C2 = simd128_min1_float(m_min_dot_C2, shifted_float1);
    m_min_dot_23 = simd128_min1_float(shifted_float2, shifted_float3);
    m_min_dot_C2 = simd128_min1_float(m_min_dot_C2, m_min_dot_23);
    min_dot_product[2] = simd128_get_float(m_min_dot_C2);
    shifted_float1 = simd128_shift_right_float(m_max_dot_C2, 1);
    shifted_float2 = simd128_shift_right_float(m_max_dot_C2, 2);
    shifted_float3 = simd128_shift_right_float(m_max_dot_C2, 3);
    m_max_dot_C2 = simd128_max1_float(m_max_dot_C2, shifted_float1);
    m_max_dot_23 = simd128_max1_float(shifted_float2, shifted_float3);
    m_max_dot_C2 = simd128_max1_float(m_max_dot_C2, m_max_dot_23);
    max_dot_product[2] = simd128_get_float(m_max_dot_C2);
#else
    for (int j = 0; j < 3; j++) {
        min_dot_product[j] = POSITIVE_INFINITY_FLOAT;
        max_dot_product[j] = NEGATIVE_INFINITY_FLOAT;
    }
#endif
    // Process the remaining vertices.
    for (; i < nu_vertices; i++)
        for (int j = 0; j < 3; j++) {
            min_dot_product[j] = minf(min_dot_product[j], Dot(vertex[i], C[j]));
            max_dot_product[j] = maxf(max_dot_product[j], Dot(vertex[i], C[j]));
        }
}

// Determine te indices within an array of vertices that have the minimum and
// maximum dot product with the given constant vector.

void GetIndicesWithMinAndMaxDotProduct(int nu_vertices, Vector3D *vertex,
const Vector3D& v2, int& i_Pmin, int& i_Pmax) {
    int i = 0;
#ifdef USE_SSE2
    __simd128_float m_v2_x = simd128_set1_float(v2.x);
    __simd128_float m_v2_y = simd128_set1_float(v2.y);
    __simd128_float m_v2_z = simd128_set1_float(v2.z);
    __simd128_float m_min_dot = simd128_set1_float(POSITIVE_INFINITY_FLOAT);
    __simd128_float m_max_dot = simd128_set1_float(NEGATIVE_INFINITY_FLOAT);
    __simd128_int m_min_dot_index = simd128_set_zero_int();
    __simd128_int m_max_dot_index = simd128_set_zero_int();
    // Keep track of four minimum and four maximum dot products (each representing the
    // min/max for a quarter of the vertices).
    for (; i + 3 < nu_vertices; i += 4) {
        __simd128_float m_vertex_x = simd128_set_float(
            vertex[i].x, vertex[i + 1].x, vertex[i + 2].x, vertex[i + 3].x);
        __simd128_float m_vertex_y = simd128_set_float(
            vertex[i].y, vertex[i + 1].y, vertex[i + 2].y, vertex[i + 3].y);
        __simd128_float m_vertex_z = simd128_set_float(
            vertex[i].z, vertex[i + 1].z, vertex[i + 2].z, vertex[i + 3].z);
        __simd128_float m_dot_x, m_dot_y, m_dot_z, m_dot;
        m_dot_x = simd128_mul_float(m_vertex_x, m_v2_x);
        m_dot_y = simd128_mul_float(m_vertex_y, m_v2_y);
        m_dot_z = simd128_mul_float(m_vertex_z, m_v2_z);
        m_dot = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y), m_dot_z);
        m_min_dot = simd128_min_float(m_min_dot, m_dot);
        m_max_dot = simd128_max_float(m_max_dot, m_dot);
        // Update the index for each component for which the min or max was updated.
        __simd128_int comp_min, comp_max;
        __simd128_int indices = simd128_set_int32(i, i + 1, i + 2, i + 3);
        // Set the bitmask for components for which the minimum/maximum was updated.
        comp_min = simd128_cmpeq_float(m_min_dot, m_dot);
        comp_max = simd128_cmpeq_float(m_max_dot, m_dot);
        m_min_dot_index = simd128_or_int(
            simd128_andnot_int(comp_min, m_min_dot_index),
            simd128_and_int(comp_min, indices)
            );
        m_max_dot_index = simd128_or_int(
            simd128_andnot_int(comp_max, m_max_dot_index),
            simd128_and_int(comp_max, indices)
            );
    }
    // Combine the minimum/maximum calculated for each quarter of the vertices.
    float min_dot_product = simd128_get_float(m_min_dot);
    i_Pmin = simd128_get_int32(m_min_dot_index);
    float max_dot_product = simd128_get_float(m_max_dot);
    i_Pmax = simd128_get_int32(m_max_dot_index);
    for (int j = 1; j < 4; j++) {
        m_min_dot = simd128_shift_right_float(m_min_dot, 1);
        m_max_dot = simd128_shift_right_float(m_max_dot, 1);
        m_min_dot_index = simd128_shift_right_bytes_int(m_min_dot_index, 4);
        m_max_dot_index = simd128_shift_right_bytes_int(m_max_dot_index, 4);
        float v_min = simd128_get_float(m_min_dot);
        float v_max = simd128_get_float(m_max_dot);
        if (v_min < min_dot_product) {
            min_dot_product = v_min;
            i_Pmin = simd128_get_int32(m_min_dot_index);
        }
        if (v_max > max_dot_product) {
            max_dot_product = v_max;
            i_Pmax = simd128_get_int32(m_max_dot_index);
        }
    }
#else
    float min_dot_product = POSITIVE_INFINITY_FLOAT;
    float max_dot_product = NEGATIVE_INFINITY_FLOAT;
#endif
    for (; i < nu_vertices; i++) {
        float dot = Dot(vertex[i], v2);
        if (dot < min_dot_product) {
            min_dot_product = dot;
            i_Pmin = i;
        }
        if (dot > max_dot_product) {
            max_dot_product = dot;
            i_Pmax = i;
        }
    }
}

void GetIndicesWithMinAndMaxDotProduct(int nu_vertices, Vector4D *vertex,
const Vector4D& v2, int& i_Pmin, int& i_Pmax) {
    int i = 0;
#ifdef USE_SSE2
    __simd128_float m_v2_x = simd128_set1_float(v2.x);
    __simd128_float m_v2_y = simd128_set1_float(v2.y);
    __simd128_float m_v2_z = simd128_set1_float(v2.z);
    __simd128_float m_v2_w = simd128_set1_float(v2.w);
    __simd128_float m_min_dot = simd128_set1_float(POSITIVE_INFINITY_FLOAT);
    __simd128_float m_max_dot = simd128_set1_float(NEGATIVE_INFINITY_FLOAT);
    __simd128_int m_min_dot_index = simd128_set_zero_int();
    __simd128_int m_max_dot_index = simd128_set_zero_int();
    // Keep track of four minimum and four maximum dot products (each representing the
    // min/max for a quarter of the vertices).
    for (; i + 3 < nu_vertices; i += 4) {
        __simd128_float m_vertex_x = simd128_set_float(
            vertex[i].x, vertex[i + 1].x, vertex[i + 2].x, vertex[i + 3].x);
        __simd128_float m_vertex_y = simd128_set_float(
            vertex[i].y, vertex[i + 1].y, vertex[i + 2].y, vertex[i + 3].y);
        __simd128_float m_vertex_z = simd128_set_float(
            vertex[i].z, vertex[i + 1].z, vertex[i + 2].z, vertex[i + 3].z);
        __simd128_float m_vertex_w = simd128_set_float(
            vertex[i].w, vertex[i + 1].w, vertex[i + 2].w, vertex[i + 3].w);
        __simd128_float m_dot_x, m_dot_y, m_dot_z, m_dot_w, m_dot;
        m_dot_x = simd128_mul_float(m_vertex_x, m_v2_x);
        m_dot_y = simd128_mul_float(m_vertex_y, m_v2_y);
        m_dot_z = simd128_mul_float(m_vertex_z, m_v2_z);
        m_dot_w = simd128_mul_float(m_vertex_w, m_v2_w);
        m_dot = simd128_add_float(
            simd128_add_float(m_dot_x, m_dot_y),
            simd128_add_float(m_dot_z, m_dot_w)
            );
        m_min_dot = simd128_min_float(m_min_dot, m_dot);
        m_max_dot = simd128_max_float(m_max_dot, m_dot);
        // Update the index for each component for which the min or max was updated.
        __simd128_int comp_min, comp_max;
        __simd128_int indices = simd128_set_int32(i, i + 1, i + 2, i + 3);
        // Set the bitmask for components for which the minimum/maximum was updated.
        comp_min = simd128_cmpeq_float(m_min_dot, m_dot);
        comp_max = simd128_cmpeq_float(m_max_dot, m_dot);
        m_min_dot_index = simd128_or_int(
            simd128_andnot_int(comp_min, m_min_dot_index),
            simd128_and_int(comp_min, indices)
            );
        m_max_dot_index = simd128_or_int(
            simd128_andnot_int(comp_max, m_max_dot_index),
            simd128_and_int(comp_max, indices)
            );
    }
    // Combine the minimum/maximum calculated for each quarter of the vertices.
    float min_dot_product = simd128_get_float(m_min_dot);
    i_Pmin = simd128_get_int32(m_min_dot_index);
    float max_dot_product = simd128_get_float(m_max_dot);
    i_Pmax = simd128_get_int32(m_max_dot_index);
    for (int j = 1; j < 4; j++) {
        m_min_dot = simd128_shift_right_float(m_min_dot, 1);
        m_max_dot = simd128_shift_right_float(m_max_dot, 1);
        m_min_dot_index = simd128_shift_right_bytes_int(m_min_dot_index, 4);
        m_max_dot_index = simd128_shift_right_bytes_int(m_max_dot_index, 4);
        float v_min = simd128_get_float(m_min_dot);
        float v_max = simd128_get_float(m_max_dot);
        if (v_min < min_dot_product) {
            min_dot_product = v_min;
            i_Pmin = simd128_get_int32(m_min_dot_index);
        }
        if (v_max > max_dot_product) {
            max_dot_product = v_max;
            i_Pmax = simd128_get_int32(m_max_dot_index);
        }
    }
#else
    float min_dot_product = POSITIVE_INFINITY_FLOAT;
    float max_dot_product = NEGATIVE_INFINITY_FLOAT;
#endif
    for (; i < nu_vertices; i++) {
        float dot = Dot(vertex[i], v2);
        if (dot < min_dot_product) {
            min_dot_product = dot;
            i_Pmin = i;
        }
        if (dot > max_dot_product) {
            max_dot_product = dot;
            i_Pmax = i;
        }
    }
}
