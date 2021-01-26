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

#include "../../Public/sfProjectInfo.h"

#include <Json.h>
#include <JsonUtilities.h>

/**
 * Scene Fusion web api handler.  Provides abstract interface declaractions to be impementated in 
 * sfWebService and sfMockWebService. Contains information required to parse and forward web responses 
 * to the Scene Fusion UI.
 */
class sfBaseWebService
{
public:
    /**
     * Delegate for logout responses.
     */
    DECLARE_DELEGATE(OnLogoutDelegate);

    /**
     * Delegate for login responses.
     *
     * @param   const FString& - error message
     */
    DECLARE_DELEGATE_OneParam(OnLoginDelegate, const FString&);

    /**
     * Delegate for get session responses.
     *
     * @param   ProjectMap& - project information
     * @param   const FString& - error message
     */
    DECLARE_DELEGATE_TwoParams(OnGetSessionsDelegate, ProjectMap&, const FString&);

    /**
     * Delegate for start settsion responses.
     *
     * @param   TSharedPtr<sfSessionInfo> - session information
     * @param   const FString& - error message
     */
    DECLARE_DELEGATE_TwoParams(OnStartSessionDelegate, TSharedPtr<sfSessionInfo>, const FString&);

    /**
     * Delegate for stop session responses.
     *
     * @param   TSharedPtr<sfSessionInfo> - session information
     * @param   const FString& - error message
     */
    DECLARE_DELEGATE_TwoParams(OnStopSessionDelegate, TSharedPtr<sfSessionInfo>, const FString&);

    /**
     * Delegate for setting a user UI color responses.
     *
     * @param   FLinearColor - User UI color
     * @param   const FString& - error message
     */
    DECLARE_DELEGATE_TwoParams(OnSetUserColorDelegate, FLinearColor, const FString&);

    /**
     * Delegate for refresh token responses.
     *
     * @param   const FString& - error message
     */
    DECLARE_DELEGATE_OneParam(OnRefreshTokenDelegate, const FString&);

    /**
     * Delegate for downloading a Scene Fusion LAN server
     *
     * @param   TArray<uint8> - byte stream for the SceneFusionServer.zip file
     * @param   FString& - error message
     */
    DECLARE_DELEGATE_TwoParams(OnDownloadLANServerDelegate, TArray<uint8>, FString&);

    /**
     * Constructor
     */
    sfBaseWebService() :
            m_isLoggedIn{ false },
            m_isLoggingIn{ false },
            m_isFetchingSessions{ false },
            m_isStartingSession{ false },
            m_isStoppingSession{ false },
            m_isSettingUserColor{ false },
            m_isRefreshingToken{ false },
            m_isDownloadingLANServer{ false }
    {
    }
    virtual ~sfBaseWebService() {}

    /**
     * Logout
     *
     * @param   OnLogoutDelegate - response handler
     */
    virtual void Logout(OnLogoutDelegate onLogout) = 0;

    /**
     * Email/Password Login
     *
     * @param   const FString& - email address
     * @param   const FString& - password
     * @param   OnLoginDelegate - response handler
     */
    virtual void Login(const FString& email, const FString& pass, OnLoginDelegate onLogin) = 0;

    /**
     * @param   OnLoginDelegate - response handler
     */
    virtual void Authenticate(OnLoginDelegate onLogin) = 0;

    /**
     * Get Sessions
     *
     * @param   OnGetSessionsDelegate - response handler
     */
    virtual void GetSessions(OnGetSessionsDelegate onGetSessions) = 0;

    /**
     * Start Session
     *
     * @param   int - Scene Fusion project ID
     * @param   const FString& - UE level name
     * @param   OnStartSessionDelegate - response handler
     */
    virtual void StartSession(int projectId, const FString& level, OnStartSessionDelegate onStartSession) = 0;

    /**
     * Stop Session
     *
     * @param   TSharedPtr<sfSessionInfo> - session information
     * @param   OnStopSessionDelegate - response handler
     */
    virtual void StopSession(TSharedPtr<sfSessionInfo> sessionInfoPtr, OnStopSessionDelegate onStartSession) = 0;

    /**
     * Set User Color
     *
     * @param   int - Scene Fusion project ID
     * @param   const FLinearColor& - color
     * @param   OnSetUserColorDelegate - response handler
     */
    virtual void SetUserColor(int projectId, const FLinearColor& color, OnSetUserColorDelegate onSetUserColor) = 0;

    /**
     * Refresh Token
     *
     * @param   OnRefreshTokenDelegate - response handler
     */
    virtual void RefreshToken(OnRefreshTokenDelegate onRefreshToken) = 0;

    /**
     * Download a LAN server compatible with the current Scene Fusion version
     *
     * @param   TSharedPtr<sfProjectInfo> - project used for authentication
     * @param   OnDownloadLANServerDelegate - download callback
     */
    virtual void DownloadLANServer(TSharedPtr<sfProjectInfo> project, OnDownloadLANServerDelegate callback) = 0;

    /**
     * Register the SF installation
     */
    virtual void RegisterInstall() = 0;

    /**
     * Login Status
     *
     * @return  bool
     */
    bool IsLoggedIn();
    
    /**
     * Latest Scene Fusion version.
     */
    const FString& LatestVersion();

protected:
    static FString URL;
    FString m_latestVersion;
    bool m_isLoggedIn;

    // Transaction state
    bool m_isLoggingIn;
    bool m_isFetchingSessions;
    bool m_isStartingSession;
    bool m_isStoppingSession;
    bool m_isSettingUserColor;
    bool m_isRefreshingToken;
    bool m_isDownloadingLANServer;

    /**
     * Handle HTTP login request data and errors.
     *
     * @param   const FString& - response error
     * @param   OnLogoutDelegate - response handler
     */
    void HandleLogoutResponse(const FString& error, OnLogoutDelegate onLogout);

    /**
     * Handle login/authenticate responses
     *
     * @param   const FString& - email
     * @param   const FString& - login token 
     * @param   TSharedPtr<FJsonObject> - json response data
     * @param   const FString& - response error
     * @param   OnLoginDelegate - response handler
     */
    void HandleLoginResponse(
        const FString& email, 
        const FString& loginToken, 
        TSharedPtr<FJsonObject> jsonPtr, 
        const FString& error, 
        OnLoginDelegate onLogin);

    /**
     * Handle HTTP get sessions request data and errors.
     *
     * @param   TSharedPtr<FJsonObject> - json response data
     * @param   const FString& - response error
     * @param   OnGetSessionsDelegate - response handler
     */
    void HandleGetSessionsResponse(
        TSharedPtr<FJsonObject> jsonPtr, 
        const FString& error, 
        OnGetSessionsDelegate onGetSessions);

    /**
     * Handle HTTP start session request data and errors.
     *
     * @param   TSharedPtr<FJsonObject> - json response data
     * @param   const FString& - response error
     * @param   OnStartSessionDelegate - response handler
     */
    void HandleStartSessionResponse(
        TSharedPtr<FJsonObject> jsonPtr, 
        const FString& error, 
        OnStartSessionDelegate onStartSession);

    /**
     * Handle HTTP stop sessions request data and errors.
     *
     * @param   TSharedPtr<sfSessionInfo> session information
     * @param   const FString& - response error
     * @param   OnStopSessionDelegate - response handler
     */
    void HandleStopSessionResponse(
        TSharedPtr<sfSessionInfo>,
        const FString& error,
        OnStopSessionDelegate onStopSession);

    /**
     * Handle HTTP set user color request data and errors.
     *
     * @param   int - Scene Fusion project ID
     * @param   const FColor& - color
     * @param   const FString& - response error
     * @param   OnSetUserColorDelegate - response handler
     */
    void HandleSetUserColorResponse(
        int projectId,
        const FLinearColor& color,
        const FString& error, 
        OnSetUserColorDelegate onSetUserColor);

    /**
     * Handle HTTP refresh token request data and errors.
     *
     * @param   const FString& - response error
     * @param   OnRefreshTokenDelegate - response handler
     */
    void HandleRefreshTokenResponse(const FString& error, OnRefreshTokenDelegate onRefreshToken);

    /**
     * Handle HTTP download LAN server request data and errors.
     *
     * @param   TArray<uint8> data
     * @param   FString& - response error
     * @param   OnDownloadLANServerDelegate - response handler
     */
    void HandleDownloadLANServerResponse(
        TArray<uint8> data,
        FString& error,
        OnDownloadLANServerDelegate onDownloadLANServer);

    /**
     * Handle HTTP register install request data and errors.
     *
     * @param   TSharedPtr<FJsonObject> - json response data
     * @param   FString& - response error
     */
    void HandleRegisterInstallResponse(TSharedPtr<FJsonObject> jsonPtr, const FString& error);

    /**
     * Parse session information from a json object.
     *
     * @param   TSharedPtr<FJsonObject> - json response data
     * @return  TSharedPtr<sfSessionInfo> - null if the json data could not be parsed
     */
    TSharedPtr<sfSessionInfo> ParseJsonSession(TSharedPtr<FJsonObject> jsonPtr);

    /**
     * Parse room information from a json object.
     *
     * @param   TSharedPtr<FJsonObject> - json response data
     * @return  ksRoomInfo::SPtr - null if the json data could not be parsed
     */
    ksRoomInfo::SPtr ParseJsonRoom(TSharedPtr<FJsonObject> jsonPtr);

    /**
     * Validate and log json objects
     *
     * TSharedPtr<FJsonObject>
     */
    void LogJSON(TSharedPtr<FJsonObject> jsonPtr);
};
