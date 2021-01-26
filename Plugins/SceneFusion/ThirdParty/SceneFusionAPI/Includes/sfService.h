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
#include <functional>
#include <Exports.h>
#include <ksRoomInfo.h>
#include "sfSession.h"

namespace KS {
namespace SceneFusion2 {
    using namespace KS::Reactor;

    /**
     * Scene Fusion service class responsible for establishing server connections and handling client connection events.
     */
    class EXTERNAL sfService
    {
    public:
        typedef std::shared_ptr<sfService> SPtr;

        /**
         * Callback for a connect event.
         * 
         * @param   sfSession::SPtr& - Session that was joined. nullptr if an error occurred or the user aborted the
         *          connection.
         * @param   const std::string& - Error message or an empty string if no errors occurred.
         */
        typedef std::function<void(sfSession::SPtr&, const std::string&)> ConnectHandler;

        /**
         * Connect event handle.
         */
        typedef ksEvent<sfSession::SPtr&, const std::string&>::SPtr ConnectEventHandle;

        /**
         * Callback for a disconnect event.
         * 
         * @param   sfSession::SPtr& - Session we disconnected from.
         * @param   const std::string& - Error message or an empty string if no errors occurred.
         */
        typedef std::function<void(sfSession::SPtr&, const std::string&)> DisconnectHandler;

        /**
         * Disconnect event handle.
         */
        typedef ksEvent<sfSession::SPtr&, const std::string&>::SPtr DisconnectEventHandle;

        /**
         * Static shared pointer constructor.
         *
         * @return  SPtr
         */
        static SPtr Create();

        /**
         * Destructor.
         */
        virtual ~sfService() {};

        /**
         * Registers an on connect event handler that is invoked when we connect to a session.
         *
         * @param   ConnectHandler - handler to register.
         * @return  ConnectEventHandle - registered event. The handler
         *          will be unregistered if all shared references to this go out of scope.
         */
        virtual ConnectEventHandle RegisterOnConnectHandler(ConnectHandler handler) = 0;

        /**
         * Unregisters an on connect event handler.
         *
         * @param   ConnectEventHandle - event to unregister.
         */
        virtual void UnregisterOnConnectHandler(ConnectEventHandle eventPtr) = 0;

        /**
         * Registers an on disconnect event handler that is invoked when we disconnect from the server.
         *
         * @param   DisconnectHandler - handler to register.
         * @return  DisconnectEventHandle - registered event. The handler
         *          will be unregistered if all shared references to this go out of scope.
         */
        virtual DisconnectEventHandle RegisterOnDisconnectHandler(DisconnectHandler handler) = 0;

        /**
         * Unregisters an on disconnect event handler.
         *
         * @param   DisconnectEventHandle - event to unregister.
         */
        virtual void UnregisterOnDisconnectHandler(DisconnectEventHandle eventPtr) = 0;

        /**
         * Get the session that we are connected to or nullptr if we are not connected to a session.
         *
         * @return  sfSession::SPtr
         */
        virtual sfSession::SPtr Session() = 0;

        /**
         * Updates the service. Call this every frame.
         *
         * @param   float - delta time in seconds since the last time this was called.
         */
        virtual void Update(float deltaTime) = 0;

        /**
         * Connects to an editing session.
         * 
         * @param   ksRoomInfo::SPtr - roomInfoPtr for session to connect to.
         * @param   const std::string& - authentication Token
         * @param   const std::string& - username
         * @param   const std::string& - application
         */
        virtual void JoinSession(ksRoomInfo::SPtr roomInfoPtr,
            const std::string& authenticationToken,
            const std::string& username,
            const std::string& application) = 0;

        /**
         * Disconnects from the current editing session, or cancels a connection attempt. Does 
         * nothing if not connected or connecting to a session.
         */
        virtual void LeaveSession() = 0;
    };
} // SceneFusion2
} // KS