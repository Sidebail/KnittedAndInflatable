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

#include "sfBaseWebService.h"

/**
 * Scene Fusion mock web api handler.
 */
class sfMockWebService : public sfBaseWebService
{
public:
    /**
     * Constructor
     *
     * @param   const FString& - server address
     * @param   const FString& - server port
     */
    sfMockWebService(const FString& host, const FString& port);

    /**
     * Logout
     *
     * @param   OnLogoutDelegate - response handler
     */
    virtual void Logout(OnLogoutDelegate onLogout);

    /**
     * sBaseWebSerive Interface. Email/Password Login
     *
     * @param   const FString& - email address
     * @param   const FString& - password
     * @param   OnLoginDelegate - response handler
     */
    virtual void Login(const FString& email, const FString& pass, OnLoginDelegate onLogin);

    /**
     * sBaseWebSerive Interface. Email/Token Login
     *
     * @param   OnLoginDelegate - response handler
     */
    virtual void Authenticate(OnLoginDelegate onLogin);

    /**
     * sBaseWebSerive Interface. Get Sessions
     *
     * @param   OnGetSessionsDelegate - response handler
     */
    virtual void GetSessions(OnGetSessionsDelegate onGetSessions);

    /**
     * Start Session
     *
     * @param   int - Scene Fusion project ID
     * @param   const FString& - UE level name
     * @param   OnStartSessionDelegate - response handler
     */
    virtual void StartSession(int projectId, const FString& level, OnStartSessionDelegate onStartSession);

    /**
     * sBaseWebSerive Interface. Stop Session
     *
     * @param   TSharedPtr<sfSessionInfo> - session information
     * @param   OnStopSessionDelegate - response handler
     */
    virtual void StopSession(TSharedPtr<sfSessionInfo> sessionInfoPtr, OnStopSessionDelegate onStartSession);

    /**
     * sBaseWebSerive Interface. Set User Color
     *
     * @param   int - Scene Fusion project ID
     * @param   const FLinearColor& - color
     * @param   OnSetUserColorDelegate - response handler
     */
    virtual void SetUserColor(int projectId, const FLinearColor& color, OnSetUserColorDelegate onSetUserColor);

    /**
     * sBaseWebSerive Interface. Refresh Token
     *
     * @param   OnRefreshTokenDelegate - response handler
     */
    virtual void RefreshToken(OnRefreshTokenDelegate onRefreshToken);

    /**
     * Download a LAN server compatible with the current Scene Fusion version
     *
     * @param   TSharedPtr<sfProjectInfo> - project used for authentication
     * @param   OnDownloadLANServerDelegate - download callback
     */
    virtual void DownloadLANServer(TSharedPtr<sfProjectInfo> project, OnDownloadLANServerDelegate callback);

    /**
     * Register the SF installation
     */
    virtual void RegisterInstall();

private:
    FString m_host;
    uint16 m_port;

    /**
     * Generate the JSON required for a Scene Fusion project.
     *
     * @param   int - Project ID
     * @param   const FString& - Project Name
     * @param   int - Company ID
     * @param   const FString& - Company Name
     * @return  TSharedPtr<FJsonObject>
     */
    TSharedPtr<FJsonObject> GenerateProject(int projectId, const FString& projectName, int companyId, const FString& companyName);

    /**
     * Generate the JSON required for a Scene Fusion session.
     *
     * @param   int - Session ID
     * @param   const FString& - Session Name
     * @return  TSharedPtr<FJsonObject>
     */
    TSharedPtr<FJsonObject> GenerateSession(int sessionId, const FString& sessionName);
};