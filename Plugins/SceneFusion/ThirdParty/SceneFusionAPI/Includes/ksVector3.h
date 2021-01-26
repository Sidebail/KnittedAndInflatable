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

#include <cmath>
#include <string>
#include "Exports.h"

namespace KS
{
    typedef float Scalar;

    extern "C"
    {
        class EXTERNAL ksVector3
        {
        private:
            Scalar m_values[3];
            enum { X = 0, Y = 1, Z = 2 };

        public:
            /**
             * Default constructor.
             */
            ksVector3()
            {
                m_values[X] = (Scalar)0.0;
                m_values[Y] = (Scalar)0.0;
                m_values[Z] = (Scalar)0.0;
            }

            /**
             * Copy constructor.
             */
            ksVector3(const ksVector3& v)
            {
                m_values[X] = v.x();
                m_values[Y] = v.y();
                m_values[Z] = v.z();
            }

            /**
             * Initialized constructor.
             */
            ksVector3(Scalar x, Scalar y, Scalar z)
            {
                m_values[X] = x;
                m_values[Y] = y;
                m_values[Z] = z;
            }

            /**
             * Destructor
             */
            ~ksVector3() {}

            /* Component access */
            Scalar& x() { return m_values[X]; }
            Scalar& y() { return m_values[Y]; }
            Scalar& z() { return m_values[Z]; }
            const Scalar& x() const { return m_values[X]; }
            const Scalar& y() const { return m_values[Y]; }
            const Scalar& z() const { return m_values[Z]; }

            /**
             * Vector scaling.
             *
             * @param   const Scalar& scaling constant
             * @return  ksVector3
             */
            inline ksVector3 operator* (const Scalar& c) const
            {
                return ksVector3{ m_values[X] * c, m_values[Y] * c, m_values[Z] * c };
            }

            /**
             * Vector scaling.
             *
             * @param   const Scalar& scaling constant
             * @return  ksVector3
             */
            inline ksVector3 operator/ (const Scalar& c) const
            {
                return ksVector3{ m_values[X] / c, m_values[Y] / c, m_values[Z] / c };
            }

            /**
             * Vector translation.
             *
             * @param   const ksVector3& v
             * @return  ksVector3
             */
            inline ksVector3 operator+ (const ksVector3& v) const
            {
                return ksVector3{ m_values[X] + v.x(), m_values[Y] + v.y(), m_values[Z] + v.z() };
            }

            /**
             *  Vector translation.
             *
             * @param   const ksVector3& v
             * @return  ksVector3
             */
            inline ksVector3 operator- (const ksVector3& v) const
            {
                return ksVector3{ m_values[X] - v.x(), m_values[Y] - v.y(), m_values[Z] - v.z() };
            }

            /**
             * Unary minus.
             *
             * @return  ksVector3
             */
            inline ksVector3 operator- () const
            {
                return ksVector3{ -m_values[X], -m_values[Y], -m_values[Z] };
            }

            /**
             * Vector scaling.
             *
             * @param   const ksVector3& v
             * @return  ksVector3&
             */
            inline ksVector3& operator*= (const Scalar& c)
            {
                m_values[X] *= c;
                m_values[Y] *= c;
                m_values[Z] *= c;
                return *this;
            }

            /**
             * Vector scaling.
             *
             * @param   const ksVector3& v
             * @return  ksVector3&
             */
            inline ksVector3& operator/= (const Scalar& c)
            {
                m_values[X] /= c;
                m_values[Y] /= c;
                m_values[Z] /= c;
                return *this;
            }

            /**
             *  Vector translation.
             *
             * @param   const ksVector3& v
             * @return  ksVector3&
             */
            inline ksVector3& operator+= (const ksVector3& v)
            {
                m_values[X] += v.x();
                m_values[Y] += v.y();
                m_values[Z] += v.z();
                return *this;
            }

            /**
             *  Vector translation.
             *
             * @param   const ksVector3& v
             * @return  ksVector3&
             */
            inline ksVector3& operator-= (const ksVector3& v)
            {
                m_values[X] -= v.x();
                m_values[Y] -= v.y();
                m_values[Z] -= v.z();
                return *this;
            }

            /**
             * Equivalence.
             *
             * @param   const ksVector3& v
             * @return  bool
             */
            inline bool operator== (const ksVector3& v)
            {
                return m_values[X] == v.x() && m_values[Y] == v.y() && m_values[Z] == v.z();
            }

            /**
             * Not equivalence.
             *
             * @param   const ksVector3& v
             * @return  bool
             */
            inline bool operator!= (const ksVector3& v)
            {
                return !(*this == v);
            }

            /**
             * Array reference
             *
             * @param   const int& i
             * @return  Scalar&
             */
            inline Scalar& operator[](const int& i)
            {
                return m_values[i % 3];
            }

            /**
             * @return  float magnitude squared.
             */
            inline float MagnitudeSquared()
            {
                return m_values[X] * m_values[X] + m_values[Y] * m_values[Y] + m_values[Z] * m_values[Z];
            }

            /**
             * @return  float magnitude.
             */
            inline float Magnitude()
            {
                return std::sqrt(MagnitudeSquared());
            }

            /**
             * Normalize this vector.
             */
            void Normalize()
            {
                // Computation of length can overflow easily because it
                // first computes squared length, so we first divide by
                // the largest coefficient.
                Scalar m = (m_values[X] < 0.0f) ? -m_values[X] : m_values[X];
                Scalar absy = (m_values[Y] < 0.0f) ? -m_values[Y] : m_values[Y];
                Scalar absz = (m_values[Z] < 0.0f) ? -m_values[Z] : m_values[Z];

                m = (absy > m) ? absy : m;
                m = (absz > m) ? absz : m;

                if (m == 0 || m == -0)
                {
                    return;
                }

                // Scaling
                m_values[X] /= m;
                m_values[Y] /= m;
                m_values[Z] /= m;

                // Normalize
                Scalar length = (Scalar)sqrt(m_values[X] * m_values[X] + m_values[Y] * m_values[Y] + m_values[Z] * m_values[Z]);
                m_values[X] /= length;
                m_values[Y] /= length;
                m_values[Z] /= length;
            }

            /**
             * Return true if the vector length squared is less than the tolerance
             *
             * @return  Scalar tolerance (Defaults)
             * @return  bool
             */
            bool IsZero(Scalar tolerance = 0.000001f) const
            {
                return ((m_values[X] * m_values[X] + m_values[Y] * m_values[Y] + m_values[Z] * m_values[Z]) < tolerance);
            }

            /**
             * Distance between two vectors.
             *
             * @param   const ksVector3& vector 1
             * @param   const ksVector3& vector 2
             * @return  Scalar
             */
            static Scalar Distance(const ksVector3& v1, const ksVector3& v2)
            {
                return (v1 - v2).Magnitude();
            }

            /**
             * Distance between two vectors.
             *
             * @param   const ksVector3& vector 1
             * @param   const ksVector3& vector 2
             * @return  Scalar
             */
            static Scalar DistanceSquared(const ksVector3& v1, const ksVector3& v2)
            {
                return (v1 - v2).MagnitudeSquared();
            }

            /**
             * Dot product of two vectors.
             *
             * @param   const ksVector3& vector 1
             * @param   const ksVector3& vector 2
             * @return  Scalar
             */
            static Scalar Dot(const ksVector3& v1, const ksVector3& v2)
            {
                return v1.x() * v2.x() + v1.y() * v2.y() + v1.z() * v2.z();
            }

            /**
             * Cross Product of two vectors.
             *
             * @param   const ksVector3& vector 1
             * @param   const ksVector3& vector 2
             * @return  ksVector3
             */
            static ksVector3 Cross(const ksVector3& v1, const ksVector3& v2)
            {
                return ksVector3{ v1.y() * v2.z() - v1.z() * v2.y(),
                    v1.z() * v2.x() - v1.x() * v2.z(),
                    v1.x() * v2.y() - v1.y() * v2.x() };
            }

            /**
             * String
             *
             * @return  std::string
             */
            std::string ToString()
            {
                return	"X=" + std::to_string(m_values[X]) + ", Y=" + std::to_string(m_values[Y]) + ", Z=" + std::to_string(m_values[Z]);
            }
        };
    }

    /**
     * Operator overload for Scale * Vector.
     *
     * @param   const Scalar& scaling constant
     * @param   const ksVector3& vector
     * @return  ksVector3
     */
    inline ksVector3 operator* (const Scalar& c, const ksVector3& v)
    {
        return ksVector3{ v * c };
    }
}

#undef DLL_EXPORT