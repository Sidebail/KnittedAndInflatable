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
#include <cstdint>

namespace KS
{
    /**
     * Holds time data.
     */
    class ksTime
    {
    public:
        /**
         * Constructor
         */
        ksTime() :
            m_delta{ 0.0f },
            m_gameTime{ 0.0 },
            m_uptime{ 0.0 },
            m_frame{ 0 }
        { }

        /**
         * @return  float& local game time in seconds since the last update.
         */
        float& Delta() { return m_delta; }

        /**
         * @return  const float& local game time in seconds since the last update.
         */
        const float& Delta() const { return m_delta; }

        /**
         * @return  double& total game time in seconds.
         */
        double& GameTime() { return m_gameTime; }

        /**
         * @return  const double& total game time in seconds.
         */
        const double& GameTime() const { return m_gameTime; }

        /**
         * @return  double& total time in seconds the server has been running.
         */
        double& Uptime() { return m_uptime; }

        /**
         * @return  const double& total time in seconds the server has been running.
         */
        const double& Uptime() const { return m_uptime; }

        /**
         * @return  uint64_t& server frame number.
         */
        uint64_t& Frame() { return m_frame; }

        /**
         * @return  const uint64_t& server frame number.
         */
        const uint64_t& Frame() const { return m_frame; }

    private:
        float m_delta;
        double m_gameTime;
        double m_uptime;
        uint64_t m_frame;
    };
}