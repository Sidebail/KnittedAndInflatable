/*************************************************************************
 *
 * KINEMATICOUP CONFIDENTIAL
 * __________________
 *
 *  Copyright (2017-2020) KinematicSoup Technologies Incorporated
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of KinematicSoup Technologies Incorporated and its
 * suppliers, if any.  The intellectual and technical concepts contained
 * herein are proprietary to KinematicSoup Technologies Incorporated
 * and its suppliers and may be covered by Canadian and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from KinematicSoup Technologies Incorporated.
 */
#pragma once

#include <iostream>
#include <cmath>
#include <string>
#include <stdexcept>
#include "Exports.h"
#include "ksVector3.h"

#define KS_PI 3.14159265358979323846264338327950288419716939937510
#define KS_DEGREES_TO_RADIANS 0.01745329251994329576923690768489
#define KS_RADIANS_TO_DEGREES 57.295779513082320876798154814105
#define KS_FDEGREES_TO_RADIANS 0.01745329251994329576923690768489f
#define KS_FRADIANS_TO_DEGREES 57.295779513082320876798154814105f

namespace KS 
{
    class EXTERNAL ksQuaternion
    {
    private:
        Scalar m_values[4];
        enum{ X = 0, Y = 1, Z = 2, W = 3 };
    public:
        /**
         * Default constructor.
         */
        ksQuaternion()
        {
            m_values[X] = (Scalar)0.0;
            m_values[Y] = (Scalar)0.0;
            m_values[Z] = (Scalar)0.0;
            m_values[W] = (Scalar)1.0;
        }

        /**
         * Copy constructor.
         */
        ksQuaternion(const ksQuaternion& q)
        {
            m_values[X] = q.x();
            m_values[Y] = q.y();
            m_values[Z] = q.z();
            m_values[W] = q.w();
        }

        /**
         * Initialized constructor.
         */
        ksQuaternion(Scalar x, Scalar y, Scalar z, Scalar w) 
        {
            m_values[X] = x;
            m_values[Y] = y;
            m_values[Z] = z;
            m_values[W] = w;
        }

        /**
         * Destructor
         */
        ~ksQuaternion() {}

        /* Component access */
        Scalar& x() { return m_values[X]; }
        Scalar& y()	{ return m_values[Y]; }
        Scalar& z()	{ return m_values[Z]; }
        Scalar& w()	{ return m_values[W]; }
        const Scalar& x() const { return m_values[X]; }
        const Scalar& y() const { return m_values[Y]; }
        const Scalar& z() const { return m_values[Z]; }
        const Scalar& w() const { return m_values[W]; }
        Scalar& operator[] (const int index) { return m_values[index]; }


        /**
         * Return a vector component of this ksQuaternion
         *
         * @return  ksVector3
         */
        ksVector3 Vec() const
        {
            return ksVector3{ m_values[X], m_values[Y], m_values[Z] };
        }

        /**
         * Return first nonzero component's sign. If all components are zeros, return true.
         *
         * @return  bool
         */
        bool GetFirstNonZeroComponentSign()
        {
            for (int i = 0; i < 4; i++)
            {
                if (m_values[i] != 0)
                {
                    return m_values[i] > 0;
                }
            }
            return true;
        }


        /**
         * Rotation composition
         *
         * @param	const ksQuaternion& q
         * @return	ksQuaternion
         */
        inline ksQuaternion operator* (const ksQuaternion& q) const
        {
            double tw = (q.w() * m_values[W]) - (q.x() * m_values[X]) - (q.y() * m_values[Y]) - (q.z() * m_values[Z]);
            double tx = (q.w() * m_values[X]) + (q.x() * m_values[W]) - (q.y() * m_values[Z]) + (q.z() * m_values[Y]);
            double ty = (q.w() * m_values[Y]) + (q.x() * m_values[Z]) + (q.y() * m_values[W]) - (q.z() * m_values[X]);
            double tz = (q.w() * m_values[Z]) - (q.x() * m_values[Y]) + (q.y() * m_values[X]) + (q.z() * m_values[W]);

            return ksQuaternion{ (Scalar)tx, (Scalar)ty, (Scalar)tz, (Scalar)tw };
        }

        /**
        * Rotates a vector by a ksQuaternion.
        *
        * @param   ksVector3 point to rotate.
        * @return  ksVector3 rotated point.
        */
        inline ksVector3 operator* (ksVector3 point)
        {
            double num1 = m_values[X] * 2.0f;
            double num2 = m_values[Y] * 2.0f;
            double num3 = m_values[Z] * 2.0f;
            double num4 = m_values[X] * num1;
            double num5 = m_values[Y] * num2;
            double num6 = m_values[Z] * num3;
            double num7 = m_values[X] * num2;
            double num8 = m_values[X] * num3;
            double num9 = m_values[Y] * num3;
            double num10 = m_values[W] * num1;
            double num11 = m_values[W] * num2;
            double num12 = m_values[W] * num3;
            ksVector3 result;
            result.x() = (float)((1.0 - (num5 + num6)) * point.x() + (num7 - num12) * point.y() + (num8 + num11) * point.z());
            result.y() = (float)((num7 + num12) * point.x() + (1.0 - (num4 + num6)) * point.y() + (num9 - num10) * point.z());
            result.z() = (float)((num8 - num11) * point.x() + (num9 + num10) * point.y() + (1.0 - (num4 + num5)) * point.z());
            return result;
        }


        /**
         * Unary minus.
         *
         * @return	ksQuaternion
         */
        inline ksQuaternion operator- () const
        {
            return ksQuaternion{ -m_values[X], -m_values[Y], -m_values[Z], -m_values[W] };
        }


        /**
         * Rotation composition
         * 
         * @param	const ksQuaternion& q
         * @return	ksQuaternion&
         */
        inline ksQuaternion& operator*= (const ksQuaternion& q)
        {
            double tw = (q.w() * m_values[W]) - (q.x() * m_values[X]) - (q.y() * m_values[Y]) - (q.z() * m_values[Z]);
            double tx = (q.w() * m_values[X]) + (q.x() * m_values[W]) - (q.y() * m_values[Z]) + (q.z() * m_values[Y]);
            double ty = (q.w() * m_values[Y]) + (q.x() * m_values[Z]) + (q.y() * m_values[W]) - (q.z() * m_values[X]);
            double tz = (q.w() * m_values[Z]) - (q.x() * m_values[Y]) + (q.y() * m_values[X]) + (q.z() * m_values[W]);

            m_values[X] = (Scalar)tx;
            m_values[Y] = (Scalar)ty;
            m_values[Z] = (Scalar)tz;
            m_values[W] = (Scalar)tw;

            return *this;
        }

        /**
         * Equivalence.
         *
         * @param   const ksQuaternion& q
         * @return  bool
         */
        inline bool operator== (const ksQuaternion& q)
        {
            return m_values[X] == q.x() && m_values[Y] == q.y() && m_values[Z] == q.z() && m_values[W] == q.w();
        }

        /**
         * Not equivalence.
         *
         * @param   const ksQuaternion& q
         * @return  bool
         */
        inline bool operator!= (const ksQuaternion& q)
        {
            return !(*this == q);
        }


        /**
         * Normalize the ksQuaternion
         */
        void Normalize()
        {
            // Computation of length can overflow easily because it
            // first computes squared length, so we first divide by
            // the largest coefficient.
            Scalar m = (m_values[X] < 0.0f) ? -m_values[X] : m_values[X];
            Scalar absy = (m_values[Y] < 0.0f) ? -m_values[Y] : m_values[Y];
            Scalar absz = (m_values[Z] < 0.0f) ? -m_values[Z] : m_values[Z];
            Scalar absw = (m_values[W] < 0.0f) ? -m_values[W] : m_values[W];

            m = (absy > m) ? absy : m;
            m = (absz > m) ? absz : m;
            m = (absw > m) ? absw : m;

            //std::cout << " m = " << m << ", absy = " << absy << ", absz = " << absz << ", absw = " << absw << std::endl;

            // Scaling
            m_values[X] /= m;
            m_values[Y] /= m;
            m_values[Z] /= m;
            m_values[W] /= m;

            // Normalize
            Scalar length = (Scalar)sqrt(
                m_values[X] * m_values[X] 
                + m_values[Y] * m_values[Y] 
                + m_values[Z] * m_values[Z] 
                + m_values[W] * m_values[W]);

            m_values[X] /= length;
            m_values[Y] /= length;
            m_values[Z] /= length;
            m_values[W] /= length;
        }

        /**
         * Return the inverse of the ksQuaternion
         *
         * @return  ksQuaternion inversed ksQuaternion
         */
        ksQuaternion Inverse() const
        {
            ksQuaternion ksQuaternion2;
            float num2 = (((m_values[X] * m_values[X])
                + (m_values[Y] * m_values[Y]))
                + (m_values[Z] * m_values[Z]))
                + (m_values[W] * m_values[W]);
            float num = 1 / num2;
            
            ksQuaternion2.x() = (m_values[X] == 0.0f) ? 0.0f : -m_values[X] * num;
            ksQuaternion2.y() = (m_values[Y] == 0.0f) ? 0.0f : -m_values[Y] * num;
            ksQuaternion2.z() = (m_values[Z] == 0.0f) ? 0.0f : -m_values[Z] * num;
            ksQuaternion2.w() = m_values[W] * num;
            return ksQuaternion2;
        }

        /**
        * Constructs a ksQuaternion that rotates one vector to another.
        *
        * @param    ksVector3 startDirection
        * @param    ksVector3 endDirection
        * @return   ksQuaternion
        */
        static ksQuaternion FromVectorDelta(ksVector3 startDirection, ksVector3 endDirection)
        {
            ksQuaternion q;
            ksVector3 a = ksVector3::Cross(startDirection, endDirection);
            q.w() = (float)std::sqrt((startDirection.MagnitudeSquared()) * (endDirection.MagnitudeSquared())) + ksVector3::Dot(startDirection, endDirection);
            q.x() = a.x();
            q.y() = a.y();
            q.z() = a.z();
            q.Normalize();
            return q;
        }

        /**
        * Constructs a ksQuaternion from an axis-angle.
        *
        * @param   ksVector3 axis of rotation.
        * @param   float angle of rotation in degrees.
        * @return  ksQuaternion
        */
        static ksQuaternion FromAxisAngle(ksVector3 axis, float angle)
        {
            angle *= (float)KS_DEGREES_TO_RADIANS;
            return FromAxisAngleRadians(axis, angle);
        }

        /**
        * Constructs a ksQuaternion from an axis-angle.
        *
        * @param   ksVector3 axis of rotation.
        * @param   float angle of rotation in radians.
        * @return  ksQuaternion
        */
        static ksQuaternion FromAxisAngleRadians(ksVector3 axis, float angle)
        {
            if (axis.MagnitudeSquared() == 0)
                throw std::runtime_error("axis cannot be 0 vector");
            axis.Normalize();
            ksVector3 v = axis * (float)std::sin(0.5 * angle);
            ksQuaternion result(v.x(), v.y(), v.z(), (float)std::cos(0.5 * angle));
            result.Normalize();
            return result;
        }

        /**
        * Converts the ksQuaternion to axis-angle representation.
        *
        * @param   ksVector3 &axis of rotation.
        * @param   float &angle of rotation in degrees.
        */
        void ToAxisAngle(ksVector3 &axis, float &angle)
        {
            ToAxisAngleRadians(axis, angle);
            angle *= (float)KS_RADIANS_TO_DEGREES;
        }

        /**
        * Converts the ksQuaternion to axis-angle representation.
        *
        * @param   ksVector3 &axis of rotation.
        * @param   float &angle of rotation in radians.
        */
        void ToAxisAngleRadians(ksVector3 &axis, float &angle)
        {
            if (m_values[X] == 0 && m_values[Y] == 0 && m_values[Z] == 0)
            {
                axis.x() = 1.0f;
                axis.y() = 0.0f;
                axis.z() = 0.0f;
            }
            else
            {
                ksVector3 v(m_values[X], m_values[Y], m_values[Z]);
                v.Normalize();
                axis = v;
            }

            double msin = std::sqrt(m_values[X] * m_values[X] + m_values[Y] * m_values[Y] + m_values[Z] * m_values[Z]);
            double mcos = m_values[W];

            if (msin != msin)
            {
                double maxcoeff = std::fmax(std::abs(X), std::fmax(std::abs(Y), std::abs(Z)));
                double _x = m_values[X] / maxcoeff;
                double _y = m_values[Y] / maxcoeff;
                double _z = m_values[Z] / maxcoeff;
                msin = std::sqrt(_x * _x + _y * _y + _z * _z);
                mcos = m_values[W] / maxcoeff;
            }

            angle = (float)std::atan2(msin, mcos) * 2;
            if (angle > KS_PI)
                angle -= 2 * (float)KS_PI;
            else if (angle <= -KS_PI)
                angle += 2 * (float)KS_PI;
        }


        /**
         * Dot product of two ksQuaternions.
         *
         * @param	const ksQuaternion& ksQuaternion 1
         * @param	const ksQuaternion& ksQuaternion 2
         * @return	Scalar
         */
        static Scalar Dot(const ksQuaternion& q1, const ksQuaternion& q2)
        {
            return q1.x() * q2.x() + q1.y() * q2.y() + q1.z() * q2.z() + q1.w() * q2.w();
        }


        /**
         * Apply a ksQuaternion rotation to a vector
         *
         * @param	const ksVector3& vector
         * @param	const ksQuaternion& ksQuaternion
         * @return	ksVector3
         */
        static ksVector3 TransformVector(const ksVector3& v, const ksQuaternion& q)
        {
            ksVector3 t = 2.0f * ksVector3::Cross(q.Vec(), v);
            return v + (q.w() * t) + ksVector3::Cross(q.Vec(), t);
        }


        /**
         * Spherically interpolates between two ksQuaternions.
         *
         * @param   ksQuaternion& from - ksQuaternion to interpolate from.
         * @param   ksQuaternion& to - ksQuaternion to interpolate to.
         * @param   float t - value between 0 and 1 that determines the amount of interpolation.
         * @return  ksQuaternion interpolated ksQuaternion.
         */
        static ksQuaternion Slerp(const ksQuaternion& from, const ksQuaternion& to, float t)
        {
            float num2;
            float num3;
            ksQuaternion ksQuaternion;
            float num = t;
            float num4 = (((from.x() * to.x()) + (from.y() * to.y())) + (from.z() * to.z())) + (from.w() * to.w());
            bool flag = false;
            if (num4 < 0.0f)
            {
                flag = true;
                num4 = -num4;
            }
            if (num4 > 0.999999f)
            {
                num3 = 1.0f - num;
                num2 = flag ? -num : num;
            }
            else
            {
                float num5 = std::acos(num4);
                float num6 = 1.0f / std::sin(num5);
                num3 = std::sin((1.0f - num) * num5) * num6;
                num2 = flag ? (-std::sin(num * num5) * num6) : std::sin(num * num5) * num6;
            }
            ksQuaternion.x() = (num3 * from.x()) + (num2 * to.x());
            ksQuaternion.y() = (num3 * from.y()) + (num2 * to.y());
            ksQuaternion.z() = (num3 * from.z()) + (num2 * to.z());
            ksQuaternion.w() = (num3 * from.w()) + (num2 * to.w());
            return ksQuaternion;
        }


        /**
         * Rotates a ksQuaternion using angular displacement.
         *
         * @param   ksQuaternion& ksQuaternion to rotate.
         * @param   ksVector3& angularDisplacement in radians.
         * @return  ksQuaternion
         */
        static ksQuaternion AddAngularDisplacementRadians(const ksQuaternion& quaternion, 
            const ksVector3& angularDisplacement)
        {
            float x = angularDisplacement.x();
            float y = angularDisplacement.y();
            float z = angularDisplacement.z();
            float magnitude = std::sqrt(x * x + y * y + z * z);
            if (magnitude <= 0)
                return quaternion;
            float cos = std::cos(magnitude / 2.0f );
            float sin = std::sin(magnitude / 2.0f );
            ksQuaternion rot;
            rot.x() = x * sin / magnitude;
            rot.y() = y * sin / magnitude;
            rot.z() = z * sin / magnitude;
            rot.w() = cos;
            
            ksQuaternion result;
            result.x() = rot.w() * quaternion.x() + rot.x() * quaternion.w()
                + rot.y() * quaternion.z() - rot.z() * quaternion.y();
            result.y() = rot.w() * quaternion.y() - rot.x() * quaternion.z()
                + rot.y() * quaternion.w() + rot.z() * quaternion.x();
            result.z() = rot.w() * quaternion.z() + rot.x() * quaternion.y()
                - rot.y() * quaternion.x() + rot.z() * quaternion.w();
            result.w() = rot.w() * quaternion.w() - rot.x() * quaternion.x()
                - rot.y() * quaternion.y() - rot.z() * quaternion.z();
            return result;
        }


        /**
         * String
         *
         * @return std::string
         */
        std::string ToString()
        {
            return	"X=" + std::to_string(m_values[X]) + 
                  ", Y=" + std::to_string(m_values[Y]) + 
                  ", Z=" + std::to_string(m_values[Z]) + 
                  ", W=" + std::to_string(m_values[W]);
        }
    };
}

#undef KS_PI
#undef KS_DEGREES_TO_RADIANS
#undef KS_RADIANS_TO_DEGREES
#undef KS_FDEGREES_TO_RADIANS
#undef KS_FRADIANS_TO_DEGREES