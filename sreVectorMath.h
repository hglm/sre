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

#ifndef __SRE_VECTOR_MATH_H__
#define __SRE_VECTOR_MATH_H__

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

#define Sqrt(x) sqrtf(x)
#define InverseSqrt(x) (1 / sqrtf(x))

//============================================================================
//
// 2D/3D/4D Vector Classes
//
// Mathematics for 3D Game Programming and Computer Graphics, 3rd ed.
// By Eric Lengyel
//
// The code in this file may be freely used in any software. It is provided
// as-is, with no warranty of any kind.
//
//============================================================================

#ifdef NO_VECTOR_MEMORY_INDEXING

static const float sre_internal_vector4d_select_table[16] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};

#endif

class SRE_API Vector2D
{
	public:
		
		float	x;
		float	y;
		
		Vector2D() {}
		
		Vector2D(float r, float s)
		{
			x = r;
			y = s;
		}
		
		Vector2D& Set(float r, float s)
		{
			x = r;
			y = s;
			return (*this);
		}

		float& operator [](int k)
		{
			return ((&x)[k]);
		}

#ifndef NO_VECTOR_MEMORY_INDEXING
		const float& operator [](int k) const
		{
			return ((&x)[k]);
		}
#else
		// [] operator using external table. The table has four floats
		// per entry, which makes addressing easier and allow sharing
                // the table with the Vector4D [] operator.
                float operator [](int k) const {
                    return
			sre_internal_vector4d_select_table[k * 4] * x +
			sre_internal_vector4d_select_table[k * 4 + 1] * y;
                }

#endif
		
		Vector2D& operator +=(const Vector2D& v)
		{
			x += v.x;
			y += v.y;
			return (*this);
		}
		
		Vector2D& operator -=(const Vector2D& v)
		{
			x -= v.x;
			y -= v.y;
			return (*this);
		}
		
		Vector2D& operator *=(float t)
		{
			x *= t;
			y *= t;
			return (*this);
		}
		
		Vector2D& operator /=(float t)
		{
			float f = 1.0F / t;
			x *= f;
			y *= f;
			return (*this);
		}
		
		Vector2D& operator &=(const Vector2D& v)
		{
			x *= v.x;
			y *= v.y;
			return (*this);
		}
		
		Vector2D& Normalize(void)
		{
			return (*this *= InverseSqrt(x * x + y * y));
		}
};


inline Vector2D operator -(const Vector2D& v)
{
	return (Vector2D(-v.x, -v.y));
}

inline Vector2D operator +(const Vector2D& v1, const Vector2D& v2)
{
	return (Vector2D(v1.x + v2.x, v1.y + v2.y));
}

inline Vector2D operator -(const Vector2D& v1, const Vector2D& v2)
{
	return (Vector2D(v1.x - v2.x, v1.y - v2.y));
}

inline Vector2D operator *(const Vector2D& v, float t)
{
	return (Vector2D(v.x * t, v.y * t));
}

inline Vector2D operator *(float t, const Vector2D& v)
{
	return (Vector2D(t * v.x, t * v.y));
}

inline Vector2D operator /(const Vector2D& v, float t)
{
	float f = 1.0F / t;
	return (Vector2D(v.x * f, v.y * f));
}

inline float operator *(const Vector2D& v1, const Vector2D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y);
}

inline Vector2D operator &(const Vector2D& v1, const Vector2D& v2)
{
	return (Vector2D(v1.x * v2.x, v1.y * v2.y));
}

inline bool operator ==(const Vector2D& v1, const Vector2D& v2)
{
	return ((v1.x == v2.x) && (v1.y == v2.y));
}

inline bool operator !=(const Vector2D& v1, const Vector2D& v2)
{
	return ((v1.x != v2.x) || (v1.y != v2.y));
}


class SRE_API Point2D : public Vector2D
{
	public:
		
		Point2D() {}
		
		Point2D(float r, float s) : Vector2D(r, s) {}
		
		Vector2D& GetVector2D(void)
		{
			return (*this);
		}
		
		const Vector2D& GetVector2D(void) const
		{
			return (*this);
		}
		
		Point2D& operator =(const Vector2D& v)
		{
			x = v.x;
			y = v.y;
			return (*this);
		}
		
		Point2D& operator *=(float t)
		{
			x *= t;
			y *= t;
			return (*this);
		}
		
		Point2D& operator /=(float t)
		{
			float f = 1.0F / t;
			x *= f;
			y *= f;
			return (*this);
		}
};


inline Point2D operator -(const Point2D& p)
{
	return (Point2D(-p.x, -p.y));
}

inline Point2D operator +(const Point2D& p1, const Point2D& p2)
{
	return (Point2D(p1.x + p2.x, p1.y + p2.y));
}

inline Point2D operator +(const Point2D& p, const Vector2D& v)
{
	return (Point2D(p.x + v.x, p.y + v.y));
}

inline Point2D operator -(const Point2D& p, const Vector2D& v)
{
	return (Point2D(p.x - v.x, p.y - v.y));
}

inline Vector2D operator -(const Point2D& p1, const Point2D& p2)
{
	return (Vector2D(p1.x - p2.x, p1.y - p2.y));
}

inline Point2D operator *(const Point2D& p, float t)
{
	return (Point2D(p.x * t, p.y * t));
}

inline Point2D operator *(float t, const Point2D& p)
{
	return (Point2D(t * p.x, t * p.y));
}

inline Point2D operator /(const Point2D& p, float t)
{
	float f = 1.0F / t;
	return (Point2D(p.x * f, p.y * f));
}


inline float Dot(const Vector2D& v1, const Vector2D& v2)
{
	return (v1 * v2);
}

inline Vector2D ProjectOnto(const Vector2D& v1, const Vector2D& v2)
{
	return (v2 * (v1 * v2));
}

inline float Magnitude(const Vector2D& v)
{
	return (Sqrt(v.x * v.x + v.y * v.y));
}

inline float InverseMag(const Vector2D& v)
{
	return (InverseSqrt(v.x * v.x + v.y * v.y));
}

inline float SquaredMag(const Vector2D& v)
{
	return (v.x * v.x + v.y * v.y);
}


class SRE_API Vector3D
{
	public:

#ifndef SEPERATE_COLOR_CLASS
		union {
			float	x;
			float	r;
		};
		union {
			float	y;
			float   g;
		};
		union {
			float	z;
			float	b;
		};
#else
		float x;
		float y;
		float z;
#endif
		
		Vector3D() {}
		
		Vector3D(float r, float s, float t)
		{
			x = r;
			y = s;
			z = t;
		}
		
		Vector3D(const Vector2D& v)
		{
			x = v.x;
			y = v.y;
			z = 0.0F;
		}
		
		Vector3D(const Vector2D& v, float u)
		{
			x = v.x;
			y = v.y;
			z = u;
		}
		
		Vector3D& Set(float r, float s, float t)
		{
			x = r;
			y = s;
			z = t;
			return (*this);
		}
		
		Vector3D& Set(const Vector2D& v, float u)
		{
			x = v.x;
			y = v.y;
			z = u;
			return (*this);
		}

                // Keep the non-const operator.
		float& operator [](int k)
		{
			return ((&x)[k]);
		}

#ifndef NO_VECTOR_MEMORY_INDEXING
		const float& operator [](int k) const
		{
			return ((&x)[k]);
		}
#else

		// [] operator using external table. The table has four floats
		// per entry, which makes addressing easier and allow sharing
                // the table with the Vector4D [] operator.
                float operator [](int k) const {
                    return
			sre_internal_vector4d_select_table[k * 4] * x +
			sre_internal_vector4d_select_table[k * 4 + 1] * y +
			sre_internal_vector4d_select_table[k * 4 + 2] * z;
                }

#endif

		Vector2D& GetVector2D(void)
		{
			return (*reinterpret_cast<Vector2D *>(this));
		}
		
		const Vector2D& GetVector2D(void) const
		{
			return (*reinterpret_cast<const Vector2D *>(this));
		}
		
		Point2D& GetPoint2D(void)
		{
			return (*reinterpret_cast<Point2D *>(this));
		}
		
		const Point2D& GetPoint2D(void) const
		{
			return (*reinterpret_cast<const Point2D *>(this));
		}
		
		Vector3D& operator =(const Vector2D& v)
		{
			x = v.x;
			y = v.y;
			z = 0.0F;
			return (*this);
		}
		
		Vector3D& operator +=(const Vector3D& v)
		{
			x += v.x;
			y += v.y;
			z += v.z;
			return (*this);
		}
		
		Vector3D& operator +=(const Vector2D& v)
		{
			x += v.x;
			y += v.y;
			return (*this);
		}
		
		Vector3D& operator -=(const Vector3D& v)
		{
			x -= v.x;
			y -= v.y;
			z -= v.z;
			return (*this);
		}
		
		Vector3D& operator -=(const Vector2D& v)
		{
			x -= v.x;
			y -= v.y;
			return (*this);
		}
		
		Vector3D& operator *=(float t)
		{
			x *= t;
			y *= t;
			z *= t;
			return (*this);
		}
		
		Vector3D& operator /=(float t)
		{
			float f = 1.0F / t;
			x *= f;
			y *= f;
			z *= f;
			return (*this);
		}
		
		Vector3D& operator %=(const Vector3D& v)
		{
			float		r, s;
			
			r = y * v.z - z * v.y;
			s = z * v.x - x * v.z;
			z = x * v.y - y * v.x;
			x = r;
			y = s;
			
			return (*this);
		}
		
		Vector3D& operator &=(const Vector3D& v)
		{
			x *= v.x;
			y *= v.y;
			z *= v.z;
			return (*this);
		}
		
		Vector3D& Normalize(void)
		{
			return (*this *= InverseSqrt(x * x + y * y + z * z));
		}

		// Return text respresentation. To be freed with delete [].
		char *GetString() const;
};


inline Vector3D operator -(const Vector3D& v)
{
	return (Vector3D(-v.x, -v.y, -v.z));
}

inline Vector3D operator +(const Vector3D& v1, const Vector3D& v2)
{
	return (Vector3D(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z));
}

inline Vector3D operator +(const Vector3D& v1, const Vector2D& v2)
{
	return (Vector3D(v1.x + v2.x, v1.y + v2.y, v1.z));
}

inline Vector3D operator -(const Vector3D& v1, const Vector3D& v2)
{
	return (Vector3D(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z));
}

inline Vector3D operator -(const Vector3D& v1, const Vector2D& v2)
{
	return (Vector3D(v1.x - v2.x, v1.y - v2.y, v1.z));
}

inline Vector3D operator *(const Vector3D& v, float t)
{
	return (Vector3D(v.x * t, v.y * t, v.z * t));
}

inline Vector3D operator *(float t, const Vector3D& v)
{
	return (Vector3D(t * v.x, t * v.y, t * v.z));
}

inline Vector3D operator /(const Vector3D& v, float t)
{
	float f = 1.0F / t;
	return (Vector3D(v.x * f, v.y * f, v.z * f));
}

inline float operator *(const Vector3D& v1, const Vector3D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z);
}

inline float operator *(const Vector3D& v1, const Vector2D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y);
}

inline Vector3D operator %(const Vector3D& v1, const Vector3D& v2)
{
	return (Vector3D(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x));
}

inline Vector3D operator &(const Vector3D& v1, const Vector3D& v2)
{
	return (Vector3D(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z));
}

inline bool operator ==(const Vector3D& v1, const Vector3D& v2)
{
	return ((v1.x == v2.x) && (v1.y == v2.y) && (v1.z == v2.z));
}

inline bool operator !=(const Vector3D& v1, const Vector3D& v2)
{
	return ((v1.x != v2.x) || (v1.y != v2.y) || (v1.z != v2.z));
}


class SRE_API Point3D : public Vector3D
{
	public:
		
		Point3D() {}
		
		Point3D(float r, float s, float t) : Vector3D(r, s, t) {}
		Point3D(const Vector2D& v) : Vector3D(v) {}
		Point3D(const Vector2D& v, float u) : Vector3D(v, u) {}
		Point3D(const Vector3D& v)
		{
			x = v.x;
			y = v.y;
			z = v.z;
		}
		
		Vector3D& GetVector3D(void)
		{
			return (*this);
		}
		
		const Vector3D& GetVector3D(void) const
		{
			return (*this);
		}
		
		Point2D& GetPoint2D(void)
		{
			return (*reinterpret_cast<Point2D *>(this));
		}
		
		const Point2D& GetPoint2D(void) const
		{
			return (*reinterpret_cast<const Point2D *>(this));
		}
		
		Point3D& operator =(const Vector3D& v)
		{
			x = v.x;
			y = v.y;
			z = v.z;
			return (*this);
		}
		
		Point3D& operator =(const Vector2D& v)
		{
			x = v.x;
			y = v.y;
			z = 0.0F;
			return (*this);
		}
		
		Point3D& operator *=(float t)
		{
			x *= t;
			y *= t;
			z *= t;
			return (*this);
		}
		
		Point3D& operator /=(float t)
		{
			float f = 1.0F / t;
			x *= f;
			y *= f;
			z *= f;
			return (*this);
		}
		
		Point3D& operator &=(const Vector3D& v)
		{
			x *= v.x;
			y *= v.y;
			z *= v.z;
			return (*this);
		}
};


inline Point3D operator -(const Point3D& p)
{
	return (Point3D(-p.x, -p.y, -p.z));
}

inline Point3D operator +(const Point3D& p1, const Point3D& p2)
{
	return (Point3D(p1.x + p2.x, p1.y + p2.y, p1.z + p2.z));
}

inline Point3D operator +(const Point3D& p, const Vector3D& v)
{
	return (Point3D(p.x + v.x, p.y + v.y, p.z + v.z));
}

inline Point3D operator +(const Vector3D& v, const Point3D& p)
{
	return (Point3D(v.x + p.x, v.y + p.y, v.z + p.z));
}

inline Vector3D operator -(const Point3D& p1, const Point3D& p2)
{
	return (Vector3D(p1.x - p2.x, p1.y - p2.y, p1.z - p2.z));
}

inline Point3D operator -(const Point3D& p, const Vector3D& v)
{
	return (Point3D(p.x - v.x, p.y - v.y, p.z - v.z));
}

inline Point3D operator -(const Vector3D& v, const Point3D& p)
{
	return (Point3D(v.x - p.x, v.y - p.y, v.z - p.z));
}

inline Point3D operator *(const Point3D& p, float t)
{
	return (Point3D(p.x * t, p.y * t, p.z * t));
}

inline Point3D operator *(float t, const Point3D& p)
{
	return (Point3D(t * p.x, t * p.y, t * p.z));
}

inline Point3D operator /(const Point3D& p, float t)
{
	float f = 1.0F / t;
	return (Point3D(p.x * f, p.y * f, p.z * f));
}

inline float operator *(const Point3D& p1, const Point3D& p2)
{
	return (p1.x * p2.x + p1.y * p2.y + p1.z * p2.z);
}

inline float operator *(const Point3D& p, const Vector3D& v)
{
	return (p.x * v.x + p.y * v.y + p.z * v.z);
}

inline float operator *(const Vector3D& v, const Point3D& p)
{
	return (v.x * p.x + v.y * p.y + v.z * p.z);
}

inline float operator *(const Point3D& p, const Vector2D& v)
{
	return (p.x * v.x + p.y * v.y);
}

inline float operator *(const Vector2D& v, const Point3D& p)
{
	return (v.x * p.x + v.y * p.y);
}

inline Vector3D operator %(const Point3D& p1, const Point3D& p2)
{
	return (Vector3D(p1.y * p2.z - p1.z * p2.y, p1.z * p2.x - p1.x * p2.z, p1.x * p2.y - p1.y * p2.x));
}

inline Vector3D operator %(const Point3D& p, const Vector3D& v)
{
	return (Vector3D(p.y * v.z - p.z * v.y, p.z * v.x - p.x * v.z, p.x * v.y - p.y * v.x));
}

inline Vector3D operator %(const Vector3D& v, const Point3D& p)
{
	return (Vector3D(v.y * p.z - v.z * p.y, v.z * p.x - v.x * p.z, v.x * p.y - v.y * p.x));
}

inline Point3D operator &(const Point3D& p1, const Point3D& p2)
{
	return (Point3D(p1.x * p2.x, p1.y * p2.y, p1.z * p2.z));
}

inline Point3D operator &(const Point3D& p, const Vector3D& v)
{
	return (Point3D(p.x * v.x, p.y * v.y, p.z * v.z));
}

inline Point3D operator &(const Vector3D& v, const Point3D& p)
{
	return (Point3D(v.x * p.x, v.y * p.y, v.z * p.z));
}


inline float Dot(const Vector3D& v1, const Vector3D& v2)
{
	return (v1 * v2);
}

inline float Dot(const Point3D& p, const Vector3D& v)
{
	return (p * v);
}

inline Vector3D Cross(const Vector3D& v1, const Vector3D& v2)
{
	return (v1 % v2);
}

inline Vector3D Cross(const Point3D& p, const Vector3D& v)
{
	return (p % v);
}

inline Vector3D ProjectOnto(const Vector3D& v1, const Vector3D& v2)
{
	return (v2 * (v1 * v2));
}

// Project v1 onto or in the direction of v2, with the angle limited by the specified value.

inline Vector3D ProjectOntoWithLimit(const Vector3D& v1, const Vector3D& v2, float min_cos_angle)
{
	float dot = Dot(v1, v2);
        if (dot < min_cos_angle)
            dot = min_cos_angle;
	return v2 * dot;
}

inline float Magnitude(const Vector3D& v)
{
	return (Sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
}

inline float InverseMag(const Vector3D& v)
{
	return (InverseSqrt(v.x * v.x + v.y * v.y + v.z * v.z));
}

inline float SquaredMag(const Vector3D& v)
{
	return (v.x * v.x + v.y * v.y + v.z * v.z);
}

inline Vector3D CalculateNormal(const Point3D &v1, const Point3D &v2, const Point3D &v3) {
    Vector3D v = Cross(v2 - v1, v3 - v1);
    v.Normalize();
    return v;
}

class SRE_API Vector4D
{
	public:
		
		float	x;
		float	y;
		float	z;
		float	w;
		
		Vector4D() {}
		
		Vector4D(float r, float s, float t, float u)
		{
			x = r;
			y = s;
			z = t;
			w = u;
		}
		
		Vector4D(const Vector3D& v, float u)
		{
			x = v.x;
			y = v.y;
			z = v.z;
			w = u;
		}
		
		Vector4D(const Vector3D& v, const Point3D& q)
		{
			x = v.x;
			y = v.y;
			z = v.z;
			w = -(v * q);
		}
		
		Vector4D(const Vector3D& v)
		{
			x = v.x;
			y = v.y;
			z = v.z;
			w = 0.0F;
		}
		
		Vector4D(const Point3D& p)
		{
			x = p.x;
			y = p.y;
			z = p.z;
			w = 1.0F;
		}
		
		Vector4D(const Vector2D& v)
		{
			x = v.x;
			y = v.y;
			z = 0.0F;
			w = 0.0F;
		}
		
		Vector4D(const Point2D& p)
		{
			x = p.x;
			y = p.y;
			z = 0.0F;
			w = 1.0F;
		}
		
		Vector4D& Set(float r, float s, float t, float u)
		{
			x = r;
			y = s;
			z = t;
			w = u;
			return (*this);
		}
		
		Vector4D& Set(const Vector3D &v, float u)
		{
			x = v.x;
			y = v.y;
			z = v.z;
			w = u;
			return (*this);
		}
		
		Vector4D& Set(const Vector3D &v, const Point3D& q)
		{
			x = v.x;
			y = v.y;
			z = v.z;
			w = -(v * q);
			return (*this);
		}
		
		float& operator [](int k)
		{
			return ((&x)[k]);
		}

#ifndef NO_VECTOR_MEMORY_INDEXING
		const float& operator [](int k) const
		{
			return ((&x)[k]);
		}
#else
		// [] operator using external table. The table has four floats
		// per entry. It is shared, with the Vector2D/Vector3D [] operators.
                float operator [](int k) const {
                    return
			sre_internal_vector4d_select_table[k * 4] * x +
			sre_internal_vector4d_select_table[k * 4 + 1] * y +
			sre_internal_vector4d_select_table[k * 4 + 2] * z +
			sre_internal_vector4d_select_table[k * 4 + 2] * z;
                }
#endif
		const Vector3D& GetVector3D(void) const
		{
			return (*reinterpret_cast<const Vector3D *>(this));
		}

		const Point3D& GetPoint3D(void) const
		{
			return (*reinterpret_cast<const Point3D *>(this));
		}

#if 0
		Vector3D& GetVector3D(void)
		{
			return (*reinterpret_cast<Vector3D *>(this));
		}

		Point3D& GetPoint3D(void)
		{
			return (*reinterpret_cast<Point3D *>(this));
		}
#else
		// Work around issues encountered with gcc 4.8.2 and -O2 and higher
		// on armhf.
		Vector3D GetVector3D(void)
		{
			return Vector3D(x, y, z);
		}

		Point3D GetPoint3D(void)
		{
			return Point3D(x, y, z);
		}
#endif
		
		Vector4D& operator =(const Vector3D& v)
		{
			x = v.x;
			y = v.y;
			z = v.z;
			w = 0.0F;
			return (*this);
		}
		
		Vector4D& operator =(const Point3D& p)
		{
			x = p.x;
			y = p.y;
			z = p.z;
			w = 1.0F;
			return (*this);
		}
		
		Vector4D& operator =(const Vector2D& v)
		{
			x = v.x;
			y = v.y;
			z = 0.0F;
			w = 0.0F;
			return (*this);
		}
		
		Vector4D& operator =(const Point2D& p)
		{
			x = p.x;
			y = p.y;
			z = 0.0F;
			w = 1.0F;
			return (*this);
		}
		
		Vector4D& operator +=(const Vector4D& v)
		{
			x += v.x;
			y += v.y;
			z += v.z;
			w += v.w;
			return (*this);
		}
		
		Vector4D& operator +=(const Vector3D& v)
		{
			x += v.x;
			y += v.y;
			z += v.z;
			return (*this);
		}
		
		Vector4D& operator +=(const Vector2D& v)
		{
			x += v.x;
			y += v.y;
			return (*this);
		}
		
		Vector4D& operator -=(const Vector4D& v)
		{
			x -= v.x;
			y -= v.y;
			z -= v.z;
			w -= v.w;
			return (*this);
		}
		
		Vector4D& operator -=(const Vector3D& v)
		{
			x -= v.x;
			y -= v.y;
			z -= v.z;
			return (*this);
		}
		
		Vector4D& operator -=(const Vector2D& v)
		{
			x -= v.x;
			y -= v.y;
			return (*this);
		}
		
		Vector4D& operator *=(float t)
		{
			x *= t;
			y *= t;
			z *= t;
			w *= t;
			return (*this);
		}
		
		Vector4D& operator /=(float t)
		{
			float f = 1.0F / t;
			x *= f;
			y *= f;
			z *= f;
			w *= f;
			return (*this);
		}
		
		Vector4D& operator &=(const Vector4D& v)
		{
			x *= v.x;
			y *= v.y;
			z *= v.z;
			w *= v.w;
			return (*this);
		}
		
		Vector4D& Normalize(void)
		{
			return (*this *= InverseSqrt(x * x + y * y + z * z + w * w));
		}

                Vector4D OrientPlaneTowardsPoint(const Point3D &P);

		// Return text respresentation. To be freed with delete [].
		char *GetString() const;
};


inline Vector4D operator -(const Vector4D& v)
{
	return (Vector4D(-v.x, -v.y, -v.z, -v.w));
}

inline Vector4D operator +(const Vector4D& v1, const Vector4D& v2)
{
	return (Vector4D(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v1.w + v2.w));
}

inline Vector4D operator +(const Vector4D& v1, const Vector3D& v2)
{
	return (Vector4D(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v1.w));
}

inline Vector4D operator +(const Vector3D& v1, const Vector4D& v2)
{
	return (Vector4D(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v2.w));
}

inline Vector4D operator +(const Vector4D& v1, const Vector2D& v2)
{
	return (Vector4D(v1.x + v2.x, v1.y + v2.y, v1.z, v1.w));
}

inline Vector4D operator +(const Vector2D& v1, const Vector4D& v2)
{
	return (Vector4D(v1.x + v2.x, v1.y + v2.y, v2.z, v2.w));
}

inline Vector4D operator -(const Vector4D& v1, const Vector4D& v2)
{
	return (Vector4D(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, v1.w - v2.w));
}

inline Vector4D operator -(const Vector4D& v1, const Vector3D& v2)
{
	return (Vector4D(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, v1.w));
}

inline Vector4D operator -(const Vector3D& v1, const Vector4D& v2)
{
	return (Vector4D(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, -v2.w));
}

inline Vector4D operator -(const Vector4D& v1, const Vector2D& v2)
{
	return (Vector4D(v1.x - v2.x, v1.y - v2.y, v1.z, v1.w));
}

inline Vector4D operator -(const Vector2D& v1, const Vector4D& v2)
{
	return (Vector4D(v1.x - v2.x, v1.y - v2.y, -v2.z, -v2.w));
}

inline Vector4D operator *(const Vector4D& v, float t)
{
	return (Vector4D(v.x * t, v.y * t, v.z * t, v.w * t));
}

inline Vector4D operator *(float t, const Vector4D& v)
{
	return (Vector4D(t * v.x, t * v.y, t * v.z, t * v.w));
}

inline Vector4D operator /(const Vector4D& v, float t)
{
	float f = 1.0F / t;
	return (Vector4D(v.x * f, v.y * f, v.z * f, v.w * f));
}

inline float operator *(const Vector4D& v1, const Vector4D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w);
}

inline float operator *(const Vector4D& v1, const Vector3D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z);
}

inline float operator *(const Vector3D& v1, const Vector4D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z);
}

inline float operator *(const Vector4D& v, const Point3D& p)
{
	return (v.x * p.x + v.y * p.y + v.z * p.z + v.w);
}

inline float operator *(const Point3D& v1, const Vector4D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v2.w);
}

inline float operator *(const Vector4D& v1, const Vector2D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y);
}

inline float operator *(const Vector2D& v1, const Vector4D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y);
}

inline float operator *(const Vector4D& v, const Point2D& p)
{
	return (v.x * p.x + v.y * p.y + v.w);
}

inline float operator *(const Point2D& v1, const Vector4D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y + v2.w);
}

inline Vector3D operator %(const Vector4D& v1, const Vector3D& v2)
{
	return (Vector3D(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x));
}

inline Vector4D operator &(const Vector4D& v1, const Vector4D& v2)
{
	return (Vector4D(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z, v1.w * v2.w));
}

inline bool operator ==(const Vector4D& v1, const Vector4D& v2)
{
	return ((v1.x == v2.x) && (v1.y == v2.y) && (v1.z == v2.z) && (v1.w == v2.w));
}

inline bool operator !=(const Vector4D& v1, const Vector4D& v2)
{
	return ((v1.x != v2.x) || (v1.y != v2.y) || (v1.z != v2.z) || (v1.w != v2.w));
}


inline float Dot(const Vector4D& v1, const Vector4D& v2)
{
	return (v1 * v2);
}

inline Vector4D ProjectOnto(const Vector4D& v1, const Vector4D& v2)
{
	return (v2 * (v1 * v2));
}

inline float Magnitude(const Vector4D& v)
{
	return (Sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w));
}

inline float InverseMag(const Vector4D& v)
{
	return (InverseSqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w));
}

inline float SquaredMag(const Vector4D& v)
{
	return (v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}

inline Vector4D PlaneFromPoints(const Point3D& v1, const Point3D& v2, const Point3D& v3) {
    Vector3D aux1 = v2 - v1;
    Vector3D aux2 = v3 - v1;
    Vector3D normal = Cross(aux2, aux1);
    normal.Normalize();
    float distance = - Dot(normal, v2);
    return Vector4D(normal, distance);
}

inline Vector4D Vector4D::OrientPlaneTowardsPoint(const Point3D &P) {
	if ((*this) * P < 0)
		return (*this) = - (*this);
        else
		return (*this);
}

class SRE_API Matrix3D
{
	public:
	
		float	n[3][3];
	
	public:
		
		Matrix3D() {}
		
		SRE_API Matrix3D(const Vector3D& c1, const Vector3D& c2, const Vector3D& c3);
		SRE_API Matrix3D(float n00, float n01, float n02, float n10, float n11, float n12, float n20, float n21, float n22);
		
		SRE_API Matrix3D& Set(const Vector3D& c1, const Vector3D& c2, const Vector3D& c3);
		SRE_API Matrix3D& Set(float n00, float n01, float n02, float n10, float n11, float n12, float n20, float n21, float n22);
		
		float& operator ()(int i, int j)
		{
			return (n[j][i]);
		}
		
		const float& operator ()(int i, int j) const
		{
			return (n[j][i]);
		}
		
		Vector3D& operator [](int j)
		{
			return (*reinterpret_cast<Vector3D *>(n[j]));
		}
		
		const Vector3D& operator [](int j) const
		{
			return (*reinterpret_cast<const Vector3D *>(n[j]));
		}
		
		Vector3D GetRow(int i) const
		{
			return (Vector3D(n[0][i], n[1][i], n[2][i]));
		}
		
		Matrix3D& SetRow(int i, const Vector3D& row)
		{
			n[0][i] = row.x;
			n[1][i] = row.y;
			n[2][i] = row.z;
			return (*this);
		}
		
		SRE_API Matrix3D& operator *=(const Matrix3D& m);
		SRE_API Matrix3D& operator *=(float t);
		SRE_API Matrix3D& operator /=(float t);
		
		SRE_API Matrix3D& SetIdentity(void);
                SRE_API Matrix3D& AssignRotationAlongAxis(const Vector3D& axis, float angle);
                SRE_API Matrix3D& AssignRotationAlongXAxis(float angle);
                SRE_API Matrix3D& AssignRotationAlongYAxis(float angle);
                SRE_API Matrix3D& AssignRotationAlongZAxis(float angle);
                SRE_API Matrix3D& AssignTranslation(const Vector2D& translation);
                SRE_API Matrix3D& AssignScaling(float scaling);
		SRE_API bool RotationMatrixPreservesAABB();
		// Return text respresentation. To be freed with delete [].
		SRE_API char *GetString() const;

		friend SRE_API Matrix3D operator *(const Matrix3D& m1, const Matrix3D& m2);
		friend SRE_API Matrix3D operator *(const Matrix3D& m, float t);
		friend SRE_API Matrix3D operator /(const Matrix3D& m, float t);
		friend SRE_API Vector3D operator *(const Matrix3D& m, const Vector3D& v);
		friend SRE_API Vector3D operator *(const Matrix3D& m, const Point3D& p);
		friend SRE_API Vector3D operator *(const Vector3D& v, const Matrix3D& m);
		friend SRE_API Vector3D operator *(const Point3D& p, const Matrix3D& m);
		friend SRE_API bool operator ==(const Matrix3D& m1, const Matrix3D& m2);
		friend SRE_API bool operator !=(const Matrix3D& m1, const Matrix3D& m2);
};


inline Matrix3D operator *(float t, const Matrix3D& m)
{
	return (m * t);
}


SRE_API float Determinant(const Matrix3D& m);
SRE_API Matrix3D Inverse(const Matrix3D& m);
SRE_API Matrix3D Adjugate(const Matrix3D& m);
SRE_API Matrix3D Transpose(const Matrix3D& m);

class MatrixTransform;

class SRE_API Matrix4D
{
	public:
		
		float	n[4][4];
	
	public:
		
		Matrix4D() {}
		
		Matrix4D(const Vector4D& c1, const Vector4D& c2, const Vector4D& c3, const Vector4D& c4);
		Matrix4D(float n00, float n01, float n02, float n03, float n10, float n11, float n12, float n13, float n20, float n21, float n22, float n23, float n30, float n31, float n32, float n33);
		Matrix4D(const MatrixTransform& m);
		
		Matrix4D& Set(const Vector4D& c1, const Vector4D& c2, const Vector4D& c3, const Vector4D& c4);
		Matrix4D& Set(float n00, float n01, float n02, float n03, float n10, float n11, float n12, float n13, float n20, float n21, float n22, float n23, float n30, float n31, float n32, float n33);
		
		float& operator ()(int i, int j)
		{
			return (n[j][i]);
		}
		
		const float& operator ()(int i, int j) const
		{
			return (n[j][i]);
		}
		
		Vector4D& operator [](int j)
		{
			return (*reinterpret_cast<Vector4D *>(n[j]));
		}
		
		const Vector4D& operator [](int j) const
		{
			return (*reinterpret_cast<const Vector4D *>(n[j]));
		}
		
		Vector4D GetRow(int i) const
		{
			return (Vector4D(n[0][i], n[1][i], n[2][i], n[3][i]));
		}
		
		Matrix4D& SetRow(int i, const Vector3D& row)
		{
			n[0][i] = row.x;
			n[1][i] = row.y;
			n[2][i] = row.z;
			n[3][i] = 0.0F;
			return (*this);
		}
		
		Matrix4D& SetRow(int i, const Vector4D& row)
		{
			n[0][i] = row.x;
			n[1][i] = row.y;
			n[2][i] = row.z;
			n[3][i] = row.w;
			return (*this);
		}
		
		SRE_API Matrix4D& operator =(const Matrix3D& m);
		SRE_API Matrix4D& operator *=(const Matrix4D& m);
		SRE_API Matrix4D& operator *=(const Matrix3D& m);
		
		SRE_API Matrix4D& SetIdentity(void);
                SRE_API Matrix4D& AssignRotationAlongXAxis(float angle);
                SRE_API Matrix4D& AssignRotationAlongYAxis(float angle);
                SRE_API Matrix4D& AssignRotationAlongZAxis(float angle);
                SRE_API Matrix4D& AssignTranslation(const Vector3D& translation);
                SRE_API Matrix4D& AssignScaling(float scaling);
		// Return text respresentation. To be freed with delete [].
		char *GetString() const;
		
		friend SRE_API Matrix4D operator *(const Matrix4D& m1, const Matrix4D& m2);
		friend SRE_API Matrix4D operator *(const Matrix4D& m1, const Matrix3D& m2);
		friend SRE_API Vector4D operator *(const Matrix4D& m, const Vector4D& v);
		friend SRE_API Vector4D operator *(const Vector4D& v, const Matrix4D& m);
		friend SRE_API Vector4D operator *(const Matrix4D& m, const Vector3D& v);
		friend SRE_API Vector4D operator *(const Vector3D& v, const Matrix4D& m);
		friend SRE_API Vector4D operator *(const Matrix4D& m, const Point3D& p);
		friend SRE_API Vector4D operator *(const Point3D& p, const Matrix4D& m);
		friend SRE_API Vector4D operator *(const Matrix4D& m, const Vector2D& v);
		friend SRE_API Vector4D operator *(const Vector2D& v, const Matrix4D& m);
		friend SRE_API Vector4D operator *(const Matrix4D& m, const Point2D& p);
		friend SRE_API Vector4D operator *(const Point2D& p, const Matrix4D& m);
		friend SRE_API bool operator ==(const Matrix4D& m1, const Matrix4D& m2);
		friend SRE_API bool operator !=(const Matrix4D& m1, const Matrix4D& m2);
};


SRE_API float Determinant(const Matrix4D& m);
SRE_API Matrix4D Inverse(const Matrix4D& m);
SRE_API Matrix4D Adjugate(const Matrix4D& m);
SRE_API Matrix4D Transpose(const Matrix4D& m);

// Custom MatrixTransform class. Transform matrices are zero at n30, n31 and n32 and 1.0 at n33.

class SRE_API MatrixTransform
{
	public:
		
		float	n[4][3];
	
	public:
		MatrixTransform() {}
		
		MatrixTransform(float n00, float n01, float n02, float n03, float n10, float n11, float n12, float n13, float n20, float n21, float n22, float n23);
		MatrixTransform& Set(float n00, float n01, float n02, float n03, float n10, float n11, float n12, float n13, float n20, float n21, float n22, float n23);

		float& operator ()(int i, int j)
		{
			return (n[j][i]);
		}

		const float& operator ()(int i, int j) const
		{
			return (n[j][i]);
		}

		SRE_API MatrixTransform& operator *=(const MatrixTransform& m);

		SRE_API MatrixTransform& SetIdentity(void);
                SRE_API MatrixTransform& AssignRotationAlongXAxis(float angle);
                SRE_API MatrixTransform& AssignRotationAlongYAxis(float angle);
                SRE_API MatrixTransform& AssignRotationAlongZAxis(float angle);
                SRE_API MatrixTransform& AssignTranslation(const Vector3D& translation);
                SRE_API MatrixTransform& AssignScaling(float scaling);

		friend SRE_API MatrixTransform operator *(const MatrixTransform& m1, const MatrixTransform& m2);
		friend SRE_API Matrix4D operator *(const Matrix4D& m1, const MatrixTransform& m2);
		friend SRE_API Vector4D operator *(const MatrixTransform& m, const Vector4D& v);
		friend SRE_API Vector4D operator *(const Vector4D& v, const MatrixTransform& m);
		friend SRE_API Vector4D operator *(const MatrixTransform& m, const Vector3D& v);
		friend SRE_API Vector4D operator *(const Vector3D& v, const MatrixTransform& m);
		friend SRE_API Vector4D operator *(const MatrixTransform& m, const Point3D& p);
		friend SRE_API Vector4D operator *(const Point3D& p, const MatrixTransform& m);
		friend SRE_API bool operator ==(const MatrixTransform& m1, const MatrixTransform& m2);
		friend SRE_API bool operator !=(const MatrixTransform& m1, const MatrixTransform& m2);
};

SRE_API Matrix4D Inverse(const MatrixTransform& m);
SRE_API Matrix4D Transpose(const MatrixTransform& m);

#ifdef SEPERATE_COLOR_CLASS

// Custom Color class, modeled on Vector3D.
// It has less operators and the result of the * operator between two colors is another
// pair-wise multiplied color, instead of the dot product. The & operator has been dropped. 

class SRE_API Color
{
	public:
		
		float	r;
		float	g;
		float	b;
		
		Color() {}
	
		Color(float x, float y, float z)
		{
			r = x;
			g = y;
			b = z;
		}
		Color& Set(float x, float y, float z)
		{
			r = x;
			g = y;
			b = z;
			return (*this);
		}

		float& operator [](int k)
		{
			return ((&r)[k]);
		}
		
		const float& operator [](int k) const
		{
			return ((&r)[k]);
		}

		Color& operator +=(const Color& v)
		{
			r += v.r;
			g += v.g;
			b += v.b;
			return (*this);
		}
		
		Color& operator -=(const Color& v)
		{
			r -= v.r;
			g -= v.g;
			b -= v.b;
			return (*this);
		}

		Color& operator *=(float t)
		{
			r *= t;
			g *= t;
			b *= t;
			return (*this);
		}
		
		Color& operator /=(float t)
		{
			float f = 1.0F / t;
			r *= f;
			g *= f;
			b *= f;
			return (*this);
		}

		Color& operator *=(const Color& v)
		{
			r *= v.r;
			g *= v.g;
			b *= v.b;
			return (*this);
		}
		
		SRE_API Color& Normalize(void)
		{
			return (*this *= InverseSqrt(r * r + g * g + b * b));
		}

		SRE_API Color& SetRandom(void);

		SRE_API Color GetLinearFromSRGB() const;
		SRE_API Color GetSRGBFromLinear() const;
		SRE_API float LinearIntensity() const;
                SRE_API float SRGBIntensity() const;
};

// Provide (duplicate) similar functions as provided by Vector3D.

inline Color operator +(const Color& p1, const Color& p2)
{
	return (Color(p1.r + p2.r, p1.g + p2.g, p1.b + p2.b));
}

inline Color operator +(const Color& p, const Vector3D& v)
{
	return (Color(p.r + v.x, p.g + v.y, p.b + v.z));
}

inline Color operator +(const Vector3D& v, const Color& p)
{
	return (Color(v.x + p.r, v.y + p.g, v.z + p.b));
}

inline Color operator -(const Color& p1, const Color& p2)
{
	return (Color(p1.r - p2.r, p1.g - p2.g, p1.b - p2.b));
}

inline Color operator -(const Color& p, const Vector3D& v)
{
	return (Color(p.r - v.x, p.g - v.y, p.b - v.z));
}

inline Color operator -(const Vector3D& v, const Color& p)
{
	return (Color(v.x - p.r, v.y - p.g, v.z - p.b));
}

inline Color operator *(const Color& p, float t)
{
	return (Color(p.r * t, p.g * t, p.b * t));
}

inline Color operator *(float t, const Color& p)
{
	return (Color(t * p.r, t * p.g, t * p.b));
}

inline Color operator /(const Color& p, float t)
{
	float f = 1.0F / t;
	return (Color(p.r * f, p.g * f, p.b * f));
}

inline bool operator ==(const Color& v1, const Color& v2)
{
	return ((v1.r == v2.r) && (v1.g == v2.g) && (v1.b == v2.b));
}

inline bool operator !=(const Color& v1, const Color& v2)
{
	return ((v1.r != v2.r) || (v1.g != v2.g) || (v1.b != v2.b));
}

inline float Dot(const Color& c, const Vector3D& v)
{
	return (c.r * v.x + c.g * v.y + c.b * v.z);
}

#else

// Color class derived from Vector3D.

class SRE_API Color : public Vector3D
{
	public:
		Color() {}
		
		Color(float r, float g, float b) : Vector3D(r, g, b) {}

		Color(const Vector3D& v)
		{
			r = v.x;
			g = v.y;
			b = v.z;
		}

		Vector3D& GetVector3D(void)
		{
			return (*this);
		}
		
		const Vector3D& GetVector3D(void) const
		{
			return (*this);
		}

		SRE_API Color& SetRandom(void);

		SRE_API Color GetLinearFromSRGB() const;
		SRE_API Color GetSRGBFromLinear() const;
		SRE_API float LinearIntensity() const;
                SRE_API float SRGBIntensity() const;
                SRE_API unsigned int GetRGBX8() const;
};

#endif

// In both cases (derived or seperate classes), we want overload
// or provide the * operator to do pairwise multiplication.

inline Color operator *(const Color& c1, const Color& c2)
{
	return (Color(c1.r * c2.r, c1.g * c2.g, c1.b * c2.b));
}

inline Color operator *(const Color& c, const Vector3D& v)
{
	return (Color(c.r * v.x, c.g * v.y, c.b * v.z));
}

inline Color operator *(const Vector3D& v, const Color& c)
{
	return (Color(v.x * c.r, v.y * c.g, v.z * c.b));
}

float InverseSRGBGamma(float c);

float SRGBGamma(float c);

// Define approximate equality inline functions, useful for geometrical
// calculations.

#define EPSILON_DEFAULT 0.0001f

static inline bool AlmostEqual(float x, float y) {
    return (x >= y - EPSILON_DEFAULT) && (x <= y + EPSILON_DEFAULT);
}

static inline bool AlmostEqual(const Vector2D& v1, const Vector2D& v2) {
    return AlmostEqual(v1.x, v2.x) && AlmostEqual(v1.y, v2.y);
}

static inline bool AlmostEqual(const Vector3D& v1, const Vector3D& v2) {
    return AlmostEqual(v1.x, v2.x) && AlmostEqual(v1.y, v2.y) && AlmostEqual(v1.z, v2.z);
}

static inline bool AlmostEqual(float x, float y, float epsilon) {
    return (x >= y - epsilon) && (x <= y + epsilon);
}

static inline bool AlmostEqual(const Vector2D& v1, const Vector2D& v2, float epsilon) {
    return AlmostEqual(v1.x, v2.x, epsilon) && AlmostEqual(v1.y, v2.y, epsilon);
}

static inline bool AlmostEqual(const Vector3D& v1, const Vector3D& v2, float epsilon) {
    return AlmostEqual(v1.x, v2.x, epsilon) && AlmostEqual(v1.y, v2.y, epsilon) &&
        AlmostEqual(v1.z, v2.z, epsilon);
}

// Also define inline functions for general use such as square and the minimum and
// maximum of two or three values.

static inline float sqrf(float x) {
    return x * x;
}

static inline float minf(float x, float y) {
    if (x < y)
        return x;
    return y;
}

static inline float min3f(float x, float y, float z) {
    float m = x;
    if (y < m)
        m = y;
    if (z < m)
        m = z;
    return m;
}

static inline float maxf(float x, float y) {
    if (y > x)
        return y;
    return x;
}

static inline float max3f(float x, float y, float z) {
    float m = x;
    if (y > m)
        m = y;
    if (z > m)
        m = z;
    return m;
}

static inline float max3f(const Vector3D& V) {
    float m = V.x;
    if (V.y > m)
        m = V.y;
    if (V.z > m)
        m = V.z;
    return m;
}

static inline Vector3D maxf(const Vector3D& V1, const Vector3D& V2) {
    return Vector3D(maxf(V1.x, V2.x), maxf(V1.y, V2.y), maxf(V1.z, V2.z));
}

static inline float clampf(float x, float min_value, float max_value) {
    if (x < min_value)
        return min_value;
    if (x > max_value)
        return max_value;
    return x;
}   

static inline int mini(int x, int y) {
    if (x < y)
        return x;
    return y;
}

static inline int maxi(int x, int y) {
    if (y > x)
        return y;
    return x;
}

#endif

