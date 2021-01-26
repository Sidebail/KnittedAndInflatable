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

#include <memory>
#include <functional>

namespace KS {
    /**
     * Tracks an event handler and provides friend access to the EventSystem
     */
    template<typename ...Arguments>
    class ksEvent
    {
    public:
        typedef std::function<void(Arguments...)> Handler;
        typedef std::shared_ptr<ksEvent<Arguments...>> SPtr;
        typedef std::weak_ptr<ksEvent<Arguments...>> WPtr;

        /**
         * Shared pointer constructor
         *
         * @param   std::function<void(Arguments...)> callback
         * @return  std::shared_ptr<ksEvent<Arguments...>>
         */
        static SPtr CreateSPtr(Handler callback)
        {
            return std::make_shared<ksEvent<Arguments...>>(callback);
        }

        /**
         * Constructor
         *
         * @param   std::function<void(Arguments...)> callback
         */
        ksEvent(Handler callback)
            : m_callback{ callback } 
        {
        }
        
        /**
         * Destructor
         */
        virtual ~ksEvent()
        {
        }

        /**
         * Check if the EventSystem that created this event has been destroyed.
         *
         * @return  bool
         */
        bool IsExpired()
        {
            return (m_callback == nullptr);
        }

    private:
        Handler m_callback;
        template<typename ...T> friend class EventSystem;

        /**
         * Clear the callback. This function is called when EventSystem that spawned this event
         * is destroyed.
         */
        void Reset()
        {
            m_callback = nullptr;
        }

        /**
         * Execute the callback with a list of arguments
         *
         * @param   Arguments&&... argument list
         */
        void operator()(Arguments&&... args)
        {
            if (m_callback) {
                m_callback(std::forward<Arguments>(args)...);
            }
        }
    };
} // KS
