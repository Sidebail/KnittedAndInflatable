#include "sfMockWebService.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfConfig.h"

sfMockWebService::sfMockWebService(const FString& host, const FString& port) : sfBaseWebService{}
{
    m_host = host;
    m_port = FCString::Atoi(*port);
}

void sfMockWebService::Logout(OnLogoutDelegate onLogout)
{
    // Simulate a 1 second request delay
    FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this, onLogout](float time)->bool {
            HandleLogoutResponse("", onLogout);
            return false; // returning true will reschedule this delegate.
        }),
        1.0f // delay period
    );
}

void sfMockWebService::Login(const FString& email, const FString& pass, OnLoginDelegate onLogin)
{
    if (m_isLoggedIn || m_isLoggingIn){
        return;
    }

    if (email.IsEmpty() || pass.IsEmpty()) {
        HandleLoginResponse(email, pass, nullptr, "Missing email and/or password", onLogin);
        return;
    }
    m_isLoggingIn = true;

    // Create JSON request data
    TSharedPtr<FJsonObject> jsonPtr = MakeShareable(new FJsonObject);
    jsonPtr->SetStringField("token", pass);
    jsonPtr->SetStringField("name", email);

    // Simulate a 1 second request delay
    FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this, email, pass, jsonPtr, onLogin](float time)->bool {
            HandleLoginResponse(email, pass, jsonPtr, "", onLogin);
            return false; // returning true will reschedule this delegate.
        }),
        1.0f // delay period
    );
}

void sfMockWebService::Authenticate(OnLoginDelegate onLogin)
{
    if (m_isLoggedIn || m_isLoggingIn) {
        return;
    }

    sfConfig& settings = sfConfig::Get();
    if (settings.Email.IsEmpty() || settings.Token.IsEmpty()) {
        HandleLoginResponse(settings.Email, settings.Token, nullptr, "Missing email and/or token.", onLogin);
    }
    else {
        Login(settings.Email, settings.Token, onLogin);
    }
}

void sfMockWebService::GetSessions(OnGetSessionsDelegate onGetSessions)
{
    if (!m_isLoggedIn || m_isFetchingSessions) {
        return;
    }
    m_isFetchingSessions = true;

    TSharedPtr<FJsonObject> jsonPtr = MakeShareable(new FJsonObject);
    
    TSharedPtr<FJsonObject> jsonProjectPtr = GenerateProject(1, "Project 1", 1, "Test Company");
    TSharedPtr<FJsonObject> jsonSessionsPtr = jsonProjectPtr->GetObjectField("sessions");
    jsonSessionsPtr->SetObjectField("101", GenerateSession(101, "Level 1"));
    jsonSessionsPtr->SetObjectField("102", GenerateSession(102, "Level 2"));
    jsonPtr->SetObjectField("1", jsonProjectPtr);

    jsonProjectPtr = GenerateProject(2, "Project 2", 1, "Test Company");
    jsonSessionsPtr = jsonProjectPtr->GetObjectField("sessions");
    jsonSessionsPtr->SetObjectField("201", GenerateSession(201, "S201"));
    jsonSessionsPtr->SetObjectField("202", GenerateSession(202, "Test Session"));
    jsonPtr->SetObjectField("2", jsonProjectPtr);

    jsonProjectPtr = GenerateProject(3, "My Project", 2, "Another Company");
    jsonSessionsPtr = jsonProjectPtr->GetObjectField("sessions");
    jsonSessionsPtr->SetObjectField("301", GenerateSession(301, "Default"));
    jsonPtr->SetObjectField("3", jsonProjectPtr);
    
    // Simulate a 1 second request delay
    FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this, jsonPtr, onGetSessions](float time)->bool {
            HandleGetSessionsResponse(jsonPtr, "", onGetSessions);
            return false; // returning true will reschedule this delegate.
        }),
        1.0f // delay period
    );
}

void sfMockWebService::StartSession(int projectId, const FString& level, OnStartSessionDelegate onStartSession)
{
    if (!m_isLoggedIn || m_isStartingSession) {
        return;
    }
    m_isStartingSession = true;

    // Create JSON room info
    TSharedPtr<FJsonObject> jsonRoomInfoPtr = MakeShareable(new FJsonObject);
    jsonRoomInfoPtr->SetNumberField("id", 1);
    jsonRoomInfoPtr->SetStringField("scene", "SceneFusion Test");
    jsonRoomInfoPtr->SetStringField("room", "SceneFusion");
    jsonRoomInfoPtr->SetStringField("ip", m_host);
    jsonRoomInfoPtr->SetNumberField("port", m_port);

    // Create JSON session list
    TSharedPtr<FJsonObject> jsonSessionsPtr = MakeShareable(new FJsonObject);

    // Create JSON session data
    TSharedPtr<FJsonObject> jsonSessionPtr = MakeShareable(new FJsonObject);
    jsonSessionPtr->SetBoolField("canDelete", false);
    jsonSessionPtr->SetStringField("creator", "You");
    jsonSessionPtr->SetNumberField("startTime", (double)(FDateTime::Now().ToUnixTimestamp()));
    jsonSessionPtr->SetStringField("instanceName", "Test Level");
    jsonSessionPtr->SetStringField("version", SceneFusion::Version());
    jsonSessionPtr->SetStringField("application", FString::Printf(TEXT("Unreal %d.%d.%d"),
        ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION));
    jsonSessionPtr->SetObjectField("roomInfo", jsonRoomInfoPtr);
    jsonSessionsPtr->SetObjectField("1", jsonSessionPtr);

    // Create JSON project
    TSharedPtr<FJsonObject> jsonProjectPtr = MakeShareable(new FJsonObject);
    jsonProjectPtr->SetStringField("name", "Project");
    jsonProjectPtr->SetStringField("serviceId", "scene fusion");
    jsonProjectPtr->SetNumberField("companyId", 1);
    jsonProjectPtr->SetStringField("companyName", "Company");
    jsonProjectPtr->SetObjectField("sessions", jsonSessionsPtr);

    // Create JSON data
    TSharedPtr<FJsonObject> jsonPtr = MakeShareable(new FJsonObject);
    jsonPtr->SetObjectField("1", jsonProjectPtr);

    // Simulate a 1 second request delay
    FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this, jsonPtr, onStartSession](float time)->bool {
            HandleStartSessionResponse(jsonPtr, "", onStartSession);
            return false; // returning true will reschedule this delegate.
        }),
        1.0f // delay period
    );
}


void sfMockWebService::StopSession(TSharedPtr<sfSessionInfo> sessionInfoPtr, OnStopSessionDelegate onStopSession)
{
    if (!m_isLoggedIn || m_isStoppingSession) {
        return;
    }
    m_isStoppingSession = true;

    // Simulate a 1 second request delay
    FTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([this, sessionInfoPtr, onStopSession](float time)->bool {
            HandleStopSessionResponse(sessionInfoPtr, "", onStopSession);
            return false; // returning true will reschedule this delegate.
        }),
        1.0f // delay period
    );
}


void sfMockWebService::SetUserColor(int projectId, const FLinearColor& color, OnSetUserColorDelegate onSetUserColor)
{
    if (!m_isLoggedIn && m_isSettingUserColor) {
        return;
    }
    m_isSettingUserColor = true;

    // Simulate a 1 second request delay
    FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this, projectId, color, onSetUserColor](float time)->bool {
            HandleSetUserColorResponse(projectId, color, "", onSetUserColor);
            return false; // returning true will reschedule this delegate.
        }),
        1.0f // delay period
    );
}

void sfMockWebService::RefreshToken(OnRefreshTokenDelegate onRefreshToken)
{
    if (!m_isLoggedIn && m_isRefreshingToken) {
        return;
    }
    m_isRefreshingToken = true;

    // Simulate a 1 second request delay
    FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this, onRefreshToken](float time)->bool {
            HandleRefreshTokenResponse("", onRefreshToken);
            return false; // returning true will reschedule this delegate.
        }),
        1.0f // delay period
     );
}

void sfMockWebService::DownloadLANServer(
    TSharedPtr<sfProjectInfo> project,
    OnDownloadLANServerDelegate onDownLoadLANServer)
{
    TArray<uint8> emptyData;
    FString error("LAN Servers cannot be downloaded with sfMockWebService");
    onDownLoadLANServer.Execute(emptyData, error);
}

void sfMockWebService::RegisterInstall()
{
    // Do nothing
}

TSharedPtr<FJsonObject> sfMockWebService::GenerateProject(int projectId, const FString& projectName, int companyId, const FString& companyName) {
    // Limits data
    TSharedPtr<FJsonObject> jsonLimitsPtr = MakeShareable(new FJsonObject);
    jsonLimitsPtr->SetNumberField("object limit", sfConfig::Get().Token == "limit" ? 1000 : -1);
    jsonLimitsPtr->SetNumberField("session limit", -1);
    jsonLimitsPtr->SetNumberField("user limit", -1);

    // Usage data
    TSharedPtr<FJsonObject> jsonUsagePtr = MakeShareable(new FJsonObject);
    jsonUsagePtr->SetNumberField("sessions", 0);
    jsonUsagePtr->SetNumberField("users", 0);

    // Subscription
    TSharedPtr<FJsonObject> jsonSubscriptionPtr = MakeShareable(new FJsonObject);
    jsonSubscriptionPtr->SetStringField("name", "Scene Fusion Free");
    jsonSubscriptionPtr->SetStringField("id", "SF-FREE");

    // Session List
    TSharedPtr<FJsonObject> jsonSessionListPtr = MakeShareable(new FJsonObject);

    // Create JSON project
    TSharedPtr<FJsonObject> jsonProjectPtr = MakeShareable(new FJsonObject);
    jsonProjectPtr->SetNumberField("id", projectId);
    jsonProjectPtr->SetStringField("name", projectName);
    jsonProjectPtr->SetNumberField("companyId", companyId);
    jsonProjectPtr->SetStringField("companyName", companyName);
    jsonProjectPtr->SetObjectField("subscription", jsonSubscriptionPtr);
    jsonProjectPtr->SetObjectField("sessions", jsonSessionListPtr);
    jsonProjectPtr->SetObjectField("limits", jsonLimitsPtr);
    jsonProjectPtr->SetObjectField("usage", jsonUsagePtr);

    return jsonProjectPtr;
}

TSharedPtr<FJsonObject> sfMockWebService::GenerateSession(int sessionId, const FString& sessionName)
{
    // Create JSON room info
    TSharedPtr<FJsonObject> jsonRoomInfoPtr = MakeShareable(new FJsonObject);
    jsonRoomInfoPtr->SetNumberField("id", sessionId);
    jsonRoomInfoPtr->SetStringField("scene", "SceneFusion");
    jsonRoomInfoPtr->SetStringField("room", "SceneFusion");
    jsonRoomInfoPtr->SetStringField("ip", m_host);
    jsonRoomInfoPtr->SetNumberField("port", m_port);

    // Create JSON session data
    TSharedPtr<FJsonObject> jsonSessionPtr = MakeShareable(new FJsonObject);
    jsonSessionPtr->SetBoolField("canDelete", false);
    jsonSessionPtr->SetStringField("creator", "You");
    jsonSessionPtr->SetNumberField("startTime", 0);// (double)(FDateTime::Now().ToUnixTimestamp()));
    jsonSessionPtr->SetStringField("instanceName", sessionName);
    jsonSessionPtr->SetStringField("version", SceneFusion::Version());
    jsonSessionPtr->SetStringField("application", FString::Printf(TEXT("Unreal %d.%d.%d"),
        ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION));
    jsonSessionPtr->SetObjectField("roomInfo", jsonRoomInfoPtr);

    return jsonSessionPtr;
}