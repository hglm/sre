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

#ifndef __SRE_DOUBLE_MATH_H__
#define __SRE_DOUBLE_MATH_H__

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

// Vector classes adapted for double precision by Harm Hanemaaijer, 2014.

#define SqrtDouble(x) sqrt(x)
#define InverseSqrtDouble(x) (1.0d / sqrt(x))

class SRE_API VectorDouble2D
{
	public:
		
		double	x;
		double	y;
		
		VectorDouble2D() {}
		
		VectorDouble2D(double r, double s)
		{
			x = r;
			y = s;
		}
		
		VectorDouble2D& Set(double r, double s)
		{
			x = r;
			y = s;
			return (*this);
		}

		double& operator [](int k)
		{
			return ((&x)[k]);
		}

		const double& operator [](int k) const
		{
			return ((&x)[k]);
		}
		
		VectorDouble2D& operator +=(const VectorDouble2D& v)
		{
			x += v.x;
			y += v.y;
			return (*this);
		}
		
		VectorDouble2D& operator -=(const VectorDouble2D& v)
		{
			x -= v.x;
			y -= v.y;
			return (*this);
		}
		
		VectorDouble2D& operator *=(double t)
		{
			x *= t;
			y *= t;
			return (*this);
		}
		
		VectorDouble2D& operator /=(double t)
		{
			double f = 1.0F / t;
			x *= f;
			y *= f;
			return (*this);
		}
		
		VectorDouble2D& operator &=(const VectorDouble2D& v)
		{
			x *= v.x;
			y *= v.y;
			return (*this);
		}
		
		VectorDouble2D& Normalize(void)
		{
			return (*this *= InverseSqrtDouble(x * x + y * y));
		}
};


inline VectorDouble2D operator -(const VectorDouble2D& v)
{
	return (VectorDouble2D(-v.x, -v.y));
}

inline VectorDouble2D operator +(const VectorDouble2D& v1, const VectorDouble2D& v2)
{
	return (VectorDouble2D(v1.x + v2.x, v1.y + v2.y));
}

inline VectorDouble2D operator -(const VectorDouble2D& v1, const VectorDouble2D& v2)
{
	return (VectorDouble2D(v1.x - v2.x, v1.y - v2.y));
}

inline VectorDouble2D operator *(const VectorDouble2D& v, double t)
{
	return (VectorDouble2D(v.x * t, v.y * t));
}

inline VectorDouble2D operator *(double t, const VectorDouble2D& v)
{
	return (VectorDouble2D(t * v.x, t * v.y));
}

inline VectorDouble2D operator /(const VectorDouble2D& v, double t)
{
	double f = 1.0d / t;
	return (VectorDouble2D(v.x * f, v.y * f));
}

inline double operator *(const VectorDouble2D& v1, const VectorDouble2D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y);
}

inline VectorDouble2D operator &(const VectorDouble2D& v1, const VectorDouble2D& v2)
{
	return (VectorDouble2D(v1.x * v2.x, v1.y * v2.y));
}

inline bool operator ==(const VectorDouble2D& v1, const VectorDouble2D& v2)
{
	return ((v1.x == v2.x) && (v1.y == v2.y));
}

inline bool operator !=(const VectorDouble2D& v1, const VectorDouble2D& v2)
{
	return ((v1.x != v2.x) || (v1.y != v2.y));
}

class SRE_API PointDouble2D : public VectorDouble2D
{
	public:
		
		PointDouble2D() {}
		
		PointDouble2D(double r, double s) : VectorDouble2D(r, s) {}
		
		VectorDouble2D& GetVectorDouble2D(void)
		{
			return (*this);
		}
		
		const VectorDouble2D& GetVectorDouble2D(void) const
		{
			return (*this);
		}
		
		PointDouble2D& operator =(const VectorDouble2D& v)
		{
			x = v.x;
			y = v.y;
			return (*this);
		}
		
		PointDouble2D& operator *=(double t)
		{
			x *= t;
			y *= t;
			return (*this);
		}
		
		PointDouble2D& operator /=(double t)
		{
			double f = 1.0d / t;
			x *= f;
			y *= f;
			return (*this);
		}
};


inline PointDouble2D operator -(const PointDouble2D& p)
{
	return (PointDouble2D(-p.x, -p.y));
}

inline PointDouble2D operator +(const PointDouble2D& p1, const PointDouble2D& p2)
{
	return (PointDouble2D(p1.x + p2.x, p1.y + p2.y));
}

inline PointDouble2D operator +(const PointDouble2D& p, const VectorDouble2D& v)
{
	return (PointDouble2D(p.x + v.x, p.y + v.y));
}

inline PointDouble2D operator -(const PointDouble2D& p, const VectorDouble2D& v)
{
	return (PointDouble2D(p.x - v.x, p.y - v.y));
}

inline VectorDouble2D operator -(const PointDouble2D& p1, const PointDouble2D& p2)
{
	return (VectorDouble2D(p1.x - p2.x, p1.y - p2.y));
}

inline PointDouble2D operator *(const PointDouble2D& p, double t)
{
	return (PointDouble2D(p.x * t, p.y * t));
}

inline PointDouble2D operator *(double t, const PointDouble2D& p)
{
	return (PointDouble2D(t * p.x, t * p.y));
}

inline PointDouble2D operator /(const PointDouble2D& p, double t)
{
	double f = 1.0d / t;
	return (PointDouble2D(p.x * f, p.y * f));
}


inline double Dot(const VectorDouble2D& v1, const VectorDouble2D& v2)
{
	return (v1 * v2);
}

inline VectorDouble2D ProjectOnto(const VectorDouble2D& v1, const VectorDouble2D& v2)
{
	return (v2 * (v1 * v2));
}

inline double Magnitude(const VectorDouble2D& v)
{
	return (SqrtDouble(v.x * v.x + v.y * v.y));
}

inline double InverseMag(const VectorDouble2D& v)
{
	return (InverseSqrtDouble(v.x * v.x + v.y * v.y));
}

inline double SquaredMag(const VectorDouble2D& v)
{
	return (v.x * v.x + v.y * v.y);
}


class SRE_API VectorDouble3D
{
	public:

#ifndef SEPERATE_COLOR_CLASS
		union {
			double	x;
			double	r;
		};
		union {
			double	y;
			double  g;
		};
		union {
			double	z;
			double	b;
		};
#else
		double x;
		double y;
		double z;
#endif
		
		VectorDouble3D() {}
		
		VectorDouble3D(double r, double s, double t)
		{
			x = r;
			y = s;
			z = t;
		}

		VectorDouble3D(const Vector3D& v)
		{
			x = v.x;
			y = v.y;
			z = v.z;
		}

		VectorDouble3D(const VectorDouble2D& v)
		{
			x = v.x;
			y = v.y;
			z = 0.0d;
		}
		
		VectorDouble3D(const VectorDouble2D& v, double u)
		{
			x = v.x;
			y = v.y;
			z = u;
		}
		
		VectorDouble3D& Set(double r, double s, double t)
		{
			x = r;
			y = s;
			z = t;
			return (*this);
		}
		
		VectorDouble3D& Set(const VectorDouble2D& v, double u)
		{
			x = v.x;
			y = v.y;
			z = u;
			return (*this);
		}

                // Keep the non-const operator.
		double& operator [](int k)
		{
			return ((&x)[k]);
		}

		const double& operator [](int k) const
		{
			return ((&x)[k]);
		}

		Vector3D GetVector3D(void)
		{
			return Vector3D(x, y, z);
		}

		VectorDouble2D& GetVectorDouble2D(void)
		{
			return (*reinterpret_cast<VectorDouble2D *>(this));
		}
		
		const VectorDouble2D& GetVectorDouble2D(void) const
		{
			return (*reinterpret_cast<const VectorDouble2D *>(this));
		}
		
		PointDouble2D& GetPointDouble2D(void)
		{
			return (*reinterpret_cast<PointDouble2D *>(this));
		}
		
		const PointDouble2D& GetPointDouble2D(void) const
		{
			return (*reinterpret_cast<const PointDouble2D *>(this));
		}
		
		VectorDouble3D& operator =(const VectorDouble2D& v)
		{
			x = v.x;
			y = v.y;
			z = 0.0d;
			return (*this);
		}
		
		VectorDouble3D& operator +=(const VectorDouble3D& v)
		{
			x += v.x;
			y += v.y;
			z += v.z;
			return (*this);
		}
		
		VectorDouble3D& operator +=(const VectorDouble2D& v)
		{
			x += v.x;
			y += v.y;
			return (*this);
		}
		
		VectorDouble3D& operator -=(const VectorDouble3D& v)
		{
			x -= v.x;
			y -= v.y;
			z -= v.z;
			return (*this);
		}
		
		VectorDouble3D& operator -=(const VectorDouble2D& v)
		{
			x -= v.x;
			y -= v.y;
			return (*this);
		}
		
		VectorDouble3D& operator *=(double t)
		{
			x *= t;
			y *= t;
			z *= t;
			return (*this);
		}
		
		VectorDouble3D& operator /=(double t)
		{
			double f = 1.0d / t;
			x *= f;
			y *= f;
			z *= f;
			return (*this);
		}
		
		VectorDouble3D& operator %=(const VectorDouble3D& v)
		{
			double		r, s;
			
			r = y * v.z - z * v.y;
			s = z * v.x - x * v.z;
			z = x * v.y - y * v.x;
			x = r;
			y = s;
			
			return (*this);
		}
		
		VectorDouble3D& operator &=(const VectorDouble3D& v)
		{
			x *= v.x;
			y *= v.y;
			z *= v.z;
			return (*this);
		}
		
		VectorDouble3D& Normalize(void)
		{
			return (*this *= InverseSqrtDouble(x * x + y * y + z * z));
		}

		// Return text respresentation. To be freed with delete [].
		char *GetString() const;
};


inline VectorDouble3D operator -(const VectorDouble3D& v)
{
	return (VectorDouble3D(-v.x, -v.y, -v.z));
}

inline VectorDouble3D operator +(const VectorDouble3D& v1, const VectorDouble3D& v2)
{
	return (VectorDouble3D(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z));
}

inline VectorDouble3D operator +(const VectorDouble3D& v1, const VectorDouble2D& v2)
{
	return (VectorDouble3D(v1.x + v2.x, v1.y + v2.y, v1.z));
}

inline VectorDouble3D operator -(const VectorDouble3D& v1, const VectorDouble3D& v2)
{
	return (VectorDouble3D(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z));
}

inline VectorDouble3D operator -(const VectorDouble3D& v1, const VectorDouble2D& v2)
{
	return (VectorDouble3D(v1.x - v2.x, v1.y - v2.y, v1.z));
}

inline VectorDouble3D operator *(const VectorDouble3D& v, double t)
{
	return (VectorDouble3D(v.x * t, v.y * t, v.z * t));
}

inline VectorDouble3D operator *(double t, const VectorDouble3D& v)
{
	return (VectorDouble3D(t * v.x, t * v.y, t * v.z));
}

inline VectorDouble3D operator /(const VectorDouble3D& v, double t)
{
	double f = 1.0d / t;
	return (VectorDouble3D(v.x * f, v.y * f, v.z * f));
}

inline double operator *(const VectorDouble3D& v1, const VectorDouble3D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z);
}

inline double operator *(const VectorDouble3D& v1, const VectorDouble2D& v2)
{
	return (v1.x * v2.x + v1.y * v2.y);
}

inline VectorDouble3D operator %(const VectorDouble3D& v1, const VectorDouble3D& v2)
{
	return (VectorDouble3D(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x));
}

inline VectorDouble3D operator &(const VectorDouble3D& v1, const VectorDouble3D& v2)
{
	return (VectorDouble3D(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z));
}

inline bool operator ==(const VectorDouble3D& v1, const VectorDouble3D& v2)
{
	return ((v1.x == v2.x) && (v1.y == v2.y) && (v1.z == v2.z));
}

inline bool operator !=(const VectorDouble3D& v1, const VectorDouble3D& v2)
{
	return ((v1.x != v2.x) || (v1.y != v2.y) || (v1.z != v2.z));
}


class SRE_API PointDouble3D : public VectorDouble3D
{
	public:
		
		PointDouble3D() {}
		
		PointDouble3D(double r, double s, double t) : VectorDouble3D(r, s, t) {}
		PointDouble3D(const VectorDouble2D& v) : VectorDouble3D(v) {}
		PointDouble3D(const VectorDouble2D& v, double u) : VectorDouble3D(v, u) {}
		PointDouble3D(const VectorDouble3D& v)
		{
			x = v.x;
			y = v.y;
			z = v.z;
		}
                PointDouble3D(const Point3D& v)
                {
			x = v.x;
			y = v.y;
			z = v.z;
                }
		
		PointDouble3D(const Vector3D& v)
                {
			x = v.x;
			y = v.y;
			z = v.z;
                }

		VectorDouble3D& GetVectorDouble3D(void)
		{
			return (*this);
		}
		
		const VectorDouble3D& GetVectorDouble3D(void) const
		{
			return (*this);
		}

                Point3D GetPoint3D(void)
                {
                        return Point3D(x, y, z);
                }
		
		PointDouble2D& GetPointDouble2D(void)
		{
			return (*reinterpret_cast<PointDouble2D *>(this));
		}
		
		const PointDouble2D& GetPointDouble2D(void) const
		{
			return (*reinterpret_cast<const PointDouble2D *>(this));
		}
		
		PointDouble3D& operator =(const VectorDouble3D& v)
		{
			x = v.x;
			y = v.y;
			z = v.z;
			return (*this);
		}
		
		PointDouble3D& operator =(const VectorDouble2D& v)
		{
			x = v.x;
			y = v.y;
			z = 0.0d;
			return (*this);
		}
		
		PointDouble3D& operator *=(double t)
		{
			x *= t;
			y *= t;
			z *= t;
			return (*this);
		}
		
		PointDouble3D& operator /=(double t)
		{
			double f = 1.0d / t;
			x *= f;
			y *= f;
			z *= f;
			return (*this);
		}
		
		PointDouble3D& operator &=(const VectorDouble3D& v)
		{
			x *= v.x;
			y *= v.y;
			z *= v.z;
			return (*this);
		}
};


inline PointDouble3D operator -(const PointDouble3D& p)
{
	return (PointDouble3D(-p.x, -p.y, -p.z));
}

inline PointDouble3D operator +(const PointDouble3D& p1, const PointDouble3D& p2)
{
	return (PointDouble3D(p1.x + p2.x, p1.y + p2.y, p1.z + p2.z));
}

inline PointDouble3D operator +(const PointDouble3D& p, const VectorDouble3D& v)
{
	return (PointDouble3D(p.x + v.x, p.y + v.y, p.z + v.z));
}

inline PointDouble3D operator +(const VectorDouble3D& v, const PointDouble3D& p)
{
	return (PointDouble3D(v.x + p.x, v.y + p.y, v.z + p.z));
}

inline VectorDouble3D operator -(const PointDouble3D& p1, const PointDouble3D& p2)
{
	return (VectorDouble3D(p1.x - p2.x, p1.y - p2.y, p1.z - p2.z));
}

inline PointDouble3D operator -(const PointDouble3D& p, const VectorDouble3D& v)
{
	return (PointDouble3D(p.x - v.x, p.y - v.y, p.z - v.z));
}

inline PointDouble3D operator -(const VectorDouble3D& v, const PointDouble3D& p)
{
	return (PointDouble3D(v.x - p.x, v.y - p.y, v.z - p.z));
}

inline PointDouble3D operator *(const PointDouble3D& p, double t)
{
	return (PointDouble3D(p.x * t, p.y * t, p.z * t));
}

inline PointDouble3D operator *(double t, const PointDouble3D& p)
{
	return (PointDouble3D(t * p.x, t * p.y, t * p.z));
}

inline PointDouble3D operator /(const PointDouble3D& p, double t)
{
	double f = 1.0d / t;
	return (PointDouble3D(p.x * f, p.y * f, p.z * f));
}

inline double operator *(const PointDouble3D& p1, const PointDouble3D& p2)
{
	return (p1.x * p2.x + p1.y * p2.y + p1.z * p2.z);
}

inline double operator *(const PointDouble3D& p, const VectorDouble3D& v)
{
	return (p.x * v.x + p.y * v.y + p.z * v.z);
}

inline double operator *(const VectorDouble3D& v, const PointDouble3D& p)
{
	return (v.x * p.x + v.y * p.y + v.z * p.z);
}

inline double operator *(const PointDouble3D& p, const VectorDouble2D& v)
{
	return (p.x * v.x + p.y * v.y);
}

inline double operator *(const VectorDouble2D& v, const PointDouble3D& p)
{
	return (v.x * p.x + v.y * p.y);
}

inline VectorDouble3D operator %(const PointDouble3D& p1, const PointDouble3D& p2)
{
	return (VectorDouble3D(p1.y * p2.z - p1.z * p2.y, p1.z * p2.x - p1.x * p2.z, p1.x * p2.y - p1.y * p2.x));
}

inline VectorDouble3D operator %(const PointDouble3D& p, const VectorDouble3D& v)
{
	return (VectorDouble3D(p.y * v.z - p.z * v.y, p.z * v.x - p.x * v.z, p.x * v.y - p.y * v.x));
}

inline VectorDouble3D operator %(const VectorDouble3D& v, const PointDouble3D& p)
{
	return (VectorDouble3D(v.y * p.z - v.z * p.y, v.z * p.x - v.x * p.z, v.x * p.y - v.y * p.x));
}

inline PointDouble3D operator &(const PointDouble3D& p1, const PointDouble3D& p2)
{
	return (PointDouble3D(p1.x * p2.x, p1.y * p2.y, p1.z * p2.z));
}

inline PointDouble3D operator &(const PointDouble3D& p, const VectorDouble3D& v)
{
	return (PointDouble3D(p.x * v.x, p.y * v.y, p.z * v.z));
}

inline PointDouble3D operator &(const VectorDouble3D& v, const PointDouble3D& p)
{
	return (PointDouble3D(v.x * p.x, v.y * p.y, v.z * p.z));
}

inline double Dot(const VectorDouble3D& v1, const VectorDouble3D& v2)
{
	return (v1 * v2);
}

inline double Dot(const PointDouble3D& p, const VectorDouble3D& v)
{
	return (p * v);
}

inline VectorDouble3D Cross(const VectorDouble3D& v1, const VectorDouble3D& v2)
{
	return (v1 % v2);
}

inline VectorDouble3D Cross(const PointDouble3D& p, const VectorDouble3D& v)
{
	return (p % v);
}

inline VectorDouble3D ProjectOnto(const VectorDouble3D& v1, const VectorDouble3D& v2)
{
	return (v2 * (v1 * v2));
}

// Project v1 onto or in the direction of v2, with the angle limited by the specified value.

inline VectorDouble3D ProjectOntoWithLimit(const VectorDouble3D& v1, const VectorDouble3D& v2, double min_cos_angle)
{
	double dot = Dot(v1, v2);
        if (dot < min_cos_angle)
            dot = min_cos_angle;
	return v2 * dot;
}

inline double Magnitude(const VectorDouble3D& v)
{
	return (SqrtDouble(v.x * v.x + v.y * v.y + v.z * v.z));
}

inline double InverseMag(const VectorDouble3D& v)
{
	return (InverseSqrtDouble(v.x * v.x + v.y * v.y + v.z * v.z));
}

inline double SquaredMag(const VectorDouble3D& v)
{
	return (v.x * v.x + v.y * v.y + v.z * v.z);
}

inline VectorDouble3D CalculateNormal(const PointDouble3D &v1, const PointDouble3D &v2, const PointDouble3D &v3) {
    VectorDouble3D v = Cross(v2 - v1, v3 - v1);
    v.Normalize();
    return v;
}

#endif

class SRE_API MatrixDouble3D
{
	public:
	
		double	n[3][3];
	
	public:
		
		MatrixDouble3D() {}
		
		SRE_API MatrixDouble3D(const VectorDouble3D& c1, const VectorDouble3D& c2, const VectorDouble3D& c3);
		SRE_API MatrixDouble3D(double n00, double n01, double n02, double n10, double n11, double n12, double n20, double n21, double n22);
		
		SRE_API MatrixDouble3D& Set(const VectorDouble3D& c1, const VectorDouble3D& c2, const VectorDouble3D& c3);
		SRE_API MatrixDouble3D& Set(double n00, double n01, double n02, double n10, double n11, double n12, double n20, double n21, double n22);
		
		double& operator ()(int i, int j)
		{
			return (n[j][i]);
		}
		
		const double& operator ()(int i, int j) const
		{
			return (n[j][i]);
		}

		double& Get(int row, int column) {
			return (n[column][row]);
		}

		const double& Get(int row, int column) const {
			return (n[column][row]);
		}

		VectorDouble3D& operator [](int j)
		{
			return (*reinterpret_cast<VectorDouble3D *>(n[j]));
		}
		
		const VectorDouble3D& operator [](int j) const
		{
			return (*reinterpret_cast<const VectorDouble3D *>(n[j]));
		}
		
		VectorDouble3D GetRow(int i) const
		{
			return (VectorDouble3D(n[0][i], n[1][i], n[2][i]));
		}

                Matrix3D GetMatrix3D() const
                {
                    return Matrix3D(n[0][0], n[1][0], n[2][0],
                        n[1][0], n[1][1], n[1][2],
                        n[2][0], n[2][1], n[2][2]);
                }
		
		MatrixDouble3D& SetRow(int i, const VectorDouble3D& row)
		{
			n[0][i] = row.x;
			n[1][i] = row.y;
			n[2][i] = row.z;
			return (*this);
		}
		
		SRE_API MatrixDouble3D& operator *=(const MatrixDouble3D& m);
		SRE_API MatrixDouble3D& operator *=(double t);
		SRE_API MatrixDouble3D& operator /=(double t);
		
		SRE_API MatrixDouble3D& SetIdentity(void);
                SRE_API MatrixDouble3D& AssignRotationAlongAxis(const VectorDouble3D& axis, double angle);
                SRE_API MatrixDouble3D& AssignRotationAlongXAxis(double angle);
                SRE_API MatrixDouble3D& AssignRotationAlongYAxis(double angle);
                SRE_API MatrixDouble3D& AssignRotationAlongZAxis(double angle);
                SRE_API MatrixDouble3D& AssignTranslation(const Vector2D& translation);
                SRE_API MatrixDouble3D& AssignScaling(double scaling);
		SRE_API bool RotationMatrixPreservesAABB();
		// Return text respresentation. To be freed with delete [].
		SRE_API char *GetString() const;

		friend SRE_API MatrixDouble3D operator *(const MatrixDouble3D& m1, const MatrixDouble3D& m2);
		friend SRE_API MatrixDouble3D operator *(const MatrixDouble3D& m, double t);
		friend SRE_API MatrixDouble3D operator /(const MatrixDouble3D& m, double t);
		friend SRE_API VectorDouble3D operator *(const MatrixDouble3D& m, const VectorDouble3D& v);
		friend SRE_API VectorDouble3D operator *(const MatrixDouble3D& m, const Point3D& p);
		friend SRE_API VectorDouble3D operator *(const VectorDouble3D& v, const MatrixDouble3D& m);
		friend SRE_API VectorDouble3D operator *(const Point3D& p, const MatrixDouble3D& m);
		friend SRE_API bool operator ==(const MatrixDouble3D& m1, const MatrixDouble3D& m2);
		friend SRE_API bool operator !=(const MatrixDouble3D& m1, const MatrixDouble3D& m2);
};


inline MatrixDouble3D operator *(double t, const MatrixDouble3D& m)
{
	return (m * t);
}

SRE_API double Determinant(const MatrixDouble3D& m);
SRE_API MatrixDouble3D Inverse(const MatrixDouble3D& m);
SRE_API MatrixDouble3D Adjugate(const MatrixDouble3D& m);
SRE_API MatrixDouble3D Transpose(const MatrixDouble3D& m);
