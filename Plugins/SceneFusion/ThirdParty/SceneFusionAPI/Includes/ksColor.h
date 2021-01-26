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

namespace KS {
    /**
     * RGBA color values stored as floats.  Expected range is 0.0 to 1.0 however some
     * systems allow larger float values to be used during iterim calculations
     */
    struct ksColor
    {
    public:
        /**
         * Default constructor.  Black-Opaque
         */
        ksColor() : 
            m_r{ 0.0f },
            m_g{ 0.0f },
            m_b{ 0.0f },
            m_a{ 1.0f }
        {
        }

        /**
         * Copy Constructor
         *
         * @param   const ksColor& - color to copy
         */
        ksColor(const ksColor& other) :
            m_r{ other.m_r },
            m_g{ other.m_g },
            m_b{ other.m_b },
            m_a{ other.m_a }
        {
        }

        /**
         * Float value constructor
         *
         * @param   r - red value
         * @param   g - green value
         * @param   b - blue value
         * @param   a - alpha value (default 1.0 = opaque)
         */
        ksColor(float r, float g, float b, float a = 1.0f) :
            m_r{ r },
            m_g{ g },
            m_b{ b },
            m_a{ a }
        {
        }

        /**
         * Destructor
         */
        ~ksColor() {}

        /**
         * @return  float& red
         */
        float& R() { return m_r; }

        /**
         * @return  const float& red
         */
        const float& R() const { return m_r; }

        /**
        * @return  float& green
        */
        float& G() { return m_g; }

        /**
        * @return  const float& green
        */
        const float& G() const { return m_g; }

        /**
        * @return  float& blue
        */
        float& B() { return m_b; }

        /**
        * @return  const float& blue
        */
        const float& B() const { return m_b; }

        /**
        * @return  float& alpha
        */
        float& A() { return m_a; }

        /**
        * @return  const float& alpha
        */
        const float& A() const { return m_a; }

    private:
        float m_r;
        float m_g;
        float m_b;
        float m_a;
    };
} // KS