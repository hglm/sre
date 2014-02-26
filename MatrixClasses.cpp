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
#include "sreRandom.h"
#include "sre_internal.h"

Matrix3D::Matrix3D(const Vector3D& c1, const Vector3D& c2, const Vector3D& c3)
{
	n[0][0] = c1.x;
	n[0][1] = c1.y;
	n[0][2] = c1.z;
	n[1][0] = c2.x;
	n[1][1] = c2.y;
	n[1][2] = c2.z;
	n[2][0] = c3.x;
	n[2][1] = c3.y;
	n[2][2] = c3.z;
}

Matrix3D::Matrix3D(float n00, float n01, float n02, float n10, float n11, float n12, float n20, float n21, float n22)
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
}

Matrix3D& Matrix3D::Set(const Vector3D& c1, const Vector3D& c2, const Vector3D& c3)
{
	n[0][0] = c1.x;
	n[0][1] = c1.y;
	n[0][2] = c1.z;
	n[1][0] = c2.x;
	n[1][1] = c2.y;
	n[1][2] = c2.z;
	n[2][0] = c3.x;
	n[2][1] = c3.y;
	n[2][2] = c3.z;
	
	return (*this);
}

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

Matrix3D& Matrix3D::operator *=(const Matrix3D& m)
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

Matrix3D operator *(const Matrix3D& m1, const Matrix3D& m2)
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

Matrix4D::Matrix4D(const Vector4D& c1, const Vector4D& c2, const Vector4D& c3, const Vector4D& c4)
{
	n[0][0] = c1.x;
	n[0][1] = c1.y;
	n[0][2] = c1.z;
	n[0][3] = c1.w;
	n[1][0] = c2.x;
	n[1][1] = c2.y;
	n[1][2] = c2.z;
	n[1][3] = c2.w;
	n[2][0] = c3.x;
	n[2][1] = c3.y;
	n[2][2] = c3.z;
	n[2][3] = c3.w;
	n[3][0] = c4.x;
	n[3][1] = c4.y;
	n[3][2] = c4.z;
	n[3][3] = c4.w;
}

Matrix4D::Matrix4D(float n00, float n01, float n02, float n03, float n10, float n11, float n12, float n13, float n20, float n21, float n22, float n23, float n30, float n31, float n32, float n33)
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
}

Matrix4D::Matrix4D(const MatrixTransform& m) {
	n[0][0] = m.n[0][0];
	n[1][0] = m.n[1][0];
	n[2][0] = m.n[2][0];
	n[3][0] = m.n[3][0];
	n[0][1] = m.n[0][1];
	n[1][1] = m.n[1][1];
	n[2][1] = m.n[2][1];
	n[3][1] = m.n[3][1];
	n[0][2] = m.n[0][2];
	n[1][2] = m.n[1][2];
	n[2][2] = m.n[2][2];
	n[3][2] = m.n[3][2];
	n[0][3] = 0.0f;
	n[1][3] = 0.0f;
	n[2][3] = 0.0f;
	n[3][3] = 1.0f;	 
}

Matrix4D& Matrix4D::Set(const Vector4D& c1, const Vector4D& c2, const Vector4D& c3, const Vector4D& c4)
{
	n[0][0] = c1.x;
	n[0][1] = c1.y;
	n[0][2] = c1.z;
	n[0][3] = c1.w;
	n[1][0] = c2.x;
	n[1][1] = c2.y;
	n[1][2] = c2.z;
	n[1][3] = c2.w;
	n[2][0] = c3.x;
	n[2][1] = c3.y;
	n[2][2] = c3.z;
	n[2][3] = c3.w;
	n[3][0] = c4.x;
	n[3][1] = c4.y;
	n[3][2] = c4.z;
	n[3][3] = c4.w;
	
	return (*this);
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

Matrix4D& Matrix4D::operator *=(const Matrix4D& m)
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

Matrix4D& Matrix4D::operator *=(const Matrix3D& m)
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
	n[1][0] = n[2][0] = n[3][0] = n[0][1] = n[2][1] = n[3][1] = n[0][2] = n[1][2] = n[3][2] = n[0][3] = n[1][3] = n[2][3] = 0.0F;
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

Matrix4D operator *(const Matrix4D& m1, const Matrix4D& m2)
{
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
}

Matrix4D operator *(const Matrix4D& m1, const Matrix3D& m2)
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

// MatrixTransform class. Transform matrices are zero at n30, n31 and n32 and 1.0 at n33.
// (n[0][3], n[1][3], n[2][3] are 0 and n[3][3] is 1.0).

MatrixTransform::MatrixTransform(float n00, float n01, float n02, float n03, float n10, float n11, float n12, float n13, float n20, float n21, float n22, float n23)
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
}

MatrixTransform& MatrixTransform::Set(float n00, float n01, float n02, float n03, float n10, float n11, float n12, float n13, float n20, float n21, float n22, float n23)
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
	return (*this);
}

MatrixTransform& MatrixTransform::SetIdentity(void)
{
	n[0][0] = n[1][1] = n[2][2]  = 1.0F;
	n[1][0] = n[2][0] = n[3][0] = n[0][1] = n[2][1] = n[3][1] = n[0][2] = n[1][2] = n[3][2] = 0.0F;
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

MatrixTransform& MatrixTransform::operator *=(const MatrixTransform& m)
{
	float x = n[0][0];
	float y = n[1][0];
	float z = n[2][0];
	float w = n[3][0];
	n[0][0] = x * m.n[0][0] + y * m.n[0][1] + z * m.n[0][2];
	n[1][0] = x * m.n[1][0] + y * m.n[1][1] + z * m.n[1][2];
	n[2][0] = x * m.n[2][0] + y * m.n[2][1] + z * m.n[2][2];
	n[3][0] = x * m.n[3][0] + y * m.n[3][1] + z * m.n[3][2] + w;
	
	x = n[0][1];
	y = n[1][1];
	z = n[2][1];
	w = n[3][1];
	n[0][1] = x * m.n[0][0] + y * m.n[0][1] + z * m.n[0][2];
	n[1][1] = x * m.n[1][0] + y * m.n[1][1] + z * m.n[1][2];
	n[2][1] = x * m.n[2][0] + y * m.n[2][1] + z * m.n[2][2];
	n[3][1] = x * m.n[3][0] + y * m.n[3][1] + z * m.n[3][2];
	
	x = n[0][2];
	y = n[1][2];
	z = n[2][2];
	w = n[3][2];
	n[0][2] = x * m.n[0][0] + y * m.n[0][1] + z * m.n[0][2];
	n[1][2] = x * m.n[1][0] + y * m.n[1][1] + z * m.n[1][2];
	n[2][2] = x * m.n[2][0] + y * m.n[2][1] + z * m.n[2][2];
	n[3][2] = x * m.n[3][0] + y * m.n[3][1] + z * m.n[3][2] + w;
		
	return (*this);
}

MatrixTransform operator *(const MatrixTransform& m1, const MatrixTransform& m2)
{
	return (MatrixTransform(m1.n[0][0] * m2.n[0][0] + m1.n[1][0] * m2.n[0][1] + m1.n[2][0] * m2.n[0][2],
					 m1.n[0][0] * m2.n[1][0] + m1.n[1][0] * m2.n[1][1] + m1.n[2][0] * m2.n[1][2],
					 m1.n[0][0] * m2.n[2][0] + m1.n[1][0] * m2.n[2][1] + m1.n[2][0] * m2.n[2][2],
					 m1.n[0][0] * m2.n[3][0] + m1.n[1][0] * m2.n[3][1] + m1.n[2][0] * m2.n[3][2] + m1.n[3][0],
					 m1.n[0][1] * m2.n[0][0] + m1.n[1][1] * m2.n[0][1] + m1.n[2][1] * m2.n[0][2],
					 m1.n[0][1] * m2.n[1][0] + m1.n[1][1] * m2.n[1][1] + m1.n[2][1] * m2.n[1][2],
					 m1.n[0][1] * m2.n[2][0] + m1.n[1][1] * m2.n[2][1] + m1.n[2][1] * m2.n[2][2],
					 m1.n[0][1] * m2.n[3][0] + m1.n[1][1] * m2.n[3][1] + m1.n[2][1] * m2.n[3][2] + m1.n[3][1],
					 m1.n[0][2] * m2.n[0][0] + m1.n[1][2] * m2.n[0][1] + m1.n[2][2] * m2.n[0][2],
					 m1.n[0][2] * m2.n[1][0] + m1.n[1][2] * m2.n[1][1] + m1.n[2][2] * m2.n[1][2],
					 m1.n[0][2] * m2.n[2][0] + m1.n[1][2] * m2.n[2][1] + m1.n[2][2] * m2.n[2][2],
					 m1.n[0][2] * m2.n[3][0] + m1.n[1][2] * m2.n[3][1] + m1.n[2][2] * m2.n[3][2] + m1.n[3][2]));
}

Matrix4D operator *(const Matrix4D& m1, const MatrixTransform& m2)
{
	return (Matrix4D(m1.n[0][0] * m2.n[0][0] + m1.n[1][0] * m2.n[0][1] + m1.n[2][0] * m2.n[0][2],
					 m1.n[0][0] * m2.n[1][0] + m1.n[1][0] * m2.n[1][1] + m1.n[2][0] * m2.n[1][2],
					 m1.n[0][0] * m2.n[2][0] + m1.n[1][0] * m2.n[2][1] + m1.n[2][0] * m2.n[2][2],
					 m1.n[0][0] * m2.n[3][0] + m1.n[1][0] * m2.n[3][1] + m1.n[2][0] * m2.n[3][2] + m1.n[3][0],
					 m1.n[0][1] * m2.n[0][0] + m1.n[1][1] * m2.n[0][1] + m1.n[2][1] * m2.n[0][2],
					 m1.n[0][1] * m2.n[1][0] + m1.n[1][1] * m2.n[1][1] + m1.n[2][1] * m2.n[1][2],
					 m1.n[0][1] * m2.n[2][0] + m1.n[1][1] * m2.n[2][1] + m1.n[2][1] * m2.n[2][2],
					 m1.n[0][1] * m2.n[3][0] + m1.n[1][1] * m2.n[3][1] + m1.n[2][1] * m2.n[3][2] + m1.n[3][1],
					 m1.n[0][2] * m2.n[0][0] + m1.n[1][2] * m2.n[0][1] + m1.n[2][2] * m2.n[0][2],
					 m1.n[0][2] * m2.n[1][0] + m1.n[1][2] * m2.n[1][1] + m1.n[2][2] * m2.n[1][2],
					 m1.n[0][2] * m2.n[2][0] + m1.n[1][2] * m2.n[2][1] + m1.n[2][2] * m2.n[2][2],
					 m1.n[0][2] * m2.n[3][0] + m1.n[1][2] * m2.n[3][1] + m1.n[2][2] * m2.n[3][2] + m1.n[3][2],
					 m1.n[0][3] * m2.n[0][0] + m1.n[1][3] * m2.n[0][1] + m1.n[2][3] * m2.n[0][2],
					 m1.n[0][3] * m2.n[1][0] + m1.n[1][3] * m2.n[1][1] + m1.n[2][3] * m2.n[1][2],
					 m1.n[0][3] * m2.n[2][0] + m1.n[1][3] * m2.n[2][1] + m1.n[2][3] * m2.n[2][2],
					 m1.n[0][3] * m2.n[3][0] + m1.n[1][3] * m2.n[3][1] + m1.n[2][3] * m2.n[3][2] + m1.n[3][3]));
}

Vector4D operator *(const MatrixTransform& m, const Vector4D& v)
{
	return (Vector4D(m.n[0][0] * v.x + m.n[1][0] * v.y + m.n[2][0] * v.z + m.n[3][0] * v.w,
					 m.n[0][1] * v.x + m.n[1][1] * v.y + m.n[2][1] * v.z + m.n[3][1] * v.w,
					 m.n[0][2] * v.x + m.n[1][2] * v.y + m.n[2][2] * v.z + m.n[3][2] * v.w,
					 v.w));
}

Vector4D operator *(const MatrixTransform& m, const Vector3D& v)
{
	return (Vector4D(m.n[0][0] * v.x + m.n[1][0] * v.y + m.n[2][0] * v.z,
					 m.n[0][1] * v.x + m.n[1][1] * v.y + m.n[2][1] * v.z,
					 m.n[0][2] * v.x + m.n[1][2] * v.y + m.n[2][2] * v.z,
					 0));
}

Vector4D operator *(const MatrixTransform& m, const Point3D& p)
{
	return (Vector4D(m.n[0][0] * p.x + m.n[1][0] * p.y + m.n[2][0] * p.z + m.n[3][0],
					 m.n[0][1] * p.x + m.n[1][1] * p.y + m.n[2][1] * p.z + m.n[3][1],
					 m.n[0][2] * p.x + m.n[1][2] * p.y + m.n[2][2] * p.z + m.n[3][2],
					 1.0));
}


bool operator ==(const MatrixTransform& m1, const MatrixTransform& m2)
{
	return ((m1.n[0][0] == m2.n[0][0]) && (m1.n[0][1] == m2.n[0][1]) && (m1.n[0][2] == m2.n[0][2]) && (m1.n[1][0] == m2.n[1][0]) && (m1.n[1][1] == m2.n[1][1]) && (m1.n[1][2] == m2.n[1][2]) && (m1.n[2][0] == m2.n[2][0]) && (m1.n[2][1] == m2.n[2][1]) && (m1.n[2][2] == m2.n[2][2]) && (m1.n[3][0] == m2.n[3][0]) && (m1.n[3][1] == m2.n[3][1]) && (m1.n[3][2] == m2.n[3][2]));
}

bool operator !=(const MatrixTransform& m1, const MatrixTransform& m2)
{
	return ((m1.n[0][0] != m2.n[0][0]) || (m1.n[0][1] != m2.n[0][1]) || (m1.n[0][2] != m2.n[0][2]) || (m1.n[1][0] != m2.n[1][0]) || (m1.n[1][1] != m2.n[1][1]) || (m1.n[1][2] != m2.n[1][2]) || (m1.n[2][0] != m2.n[2][0]) || (m1.n[2][1] != m2.n[2][1]) || (m1.n[2][2] != m2.n[2][2]) || (m1.n[3][0] != m2.n[3][0]) || (m1.n[3][1] != m2.n[3][1]) || (m1.n[3][2] != m2.n[3][2]));
}

Matrix4D Inverse(const MatrixTransform& m)
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
	
	float n30 = 0;
	float n31 = 0;
	float n32 = 0;
	float n33 = 1.0;
	
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

Matrix4D Transpose(const MatrixTransform& m)
{
	return (Matrix4D(m(0,0), m(1,0), m(2,0), 0, m(0,1), m(1,1), m(2,1), 0, m(0,2), m(1,2), m(2,2), 0, m(0,3), m(1,3), m(2,3), 1.0));
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
    for (int i = 0; i < 3; i++) {
        Vector4D V = GetRow(i);
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
