#include "sfBaseWebService.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfConfig.h"

#define LOG_CHANNEL "sfBaseWebService"

FString sfBaseWebService::URL = "https://matchmaker-console.kinematicsoup.com/api/";

bool sfBaseWebService::IsLoggedIn()
{
    return m_isLoggedIn; 
}

const FString& sfBaseWebService::LatestVersion()
{
    return m_latestVersion;
}

void sfBaseWebService::HandleLogoutResponse(const FString& error, OnLogoutDelegate onLogout)
{
    m_isLoggedIn = false;
    sfConfig& settings = sfConfig::Get();
    settings.Token = "";
    settings.Save();
    onLogout.Execute();
}

void sfBaseWebService::HandleLoginResponse(const FString& email, const FString& loginToken, TSharedPtr<FJsonObject> jsonPtr, const FString& error, OnLoginDelegate onLogin)
{
    m_isLoggingIn = false;
        
    // Success
    if (error.IsEmpty() && jsonPtr.IsValid()) {
        m_isLoggedIn = true;
        sfConfig& settings = sfConfig::Get();
        settings.Name = jsonPtr->GetStringField("name");
        settings.Email = email;
        settings.Token = loginToken;
        settings.SFToken = jsonPtr->GetStringField("token");
        settings.Save();
        if (jsonPtr->HasField("version"))
        {
            m_latestVersion = jsonPtr->GetStringField("version");
        }
        onLogin.Execute(error);
        return;
    }

    // Clear token on failed logins
    sfConfig& settings = sfConfig::Get();
    settings.Token = "";
    settings.SFToken = "";
    settings.Save();

    // Error
    if (!error.IsEmpty()) {
        onLogin.Execute(error);
        return;
    }

    // Unexpected error
    KS::Log::Error("Login response did not contain an error message or valid json response.", LOG_CHANNEL);
    onLogin.Execute("Unexpected login error.");
}

void sfBaseWebService::HandleGetSessionsResponse(
    TSharedPtr<FJsonObject> jsonPtr, 
    const FString& error, 
    OnGetSessionsDelegate onGetSessionsDelegate)
{
    m_isFetchingSessions = false;
    ProjectMap projectMap;

    // Success
    if (error.IsEmpty() && jsonPtr.IsValid()) {
        for (auto pair : jsonPtr->Values) {
            TSharedPtr<FJsonObject> jsonProject = pair.Value->AsObject();
            TSharedPtr<sfProjectInfo> projectInfoPtr = MakeShareable(new sfProjectInfo());
            projectInfoPtr->Id = FCString::Atoi(*(pair.Key));
            projectInfoPtr->CompanyId = jsonProject->GetNumberField("companyId");
            jsonProject->TryGetStringField("name", projectInfoPtr->Name);
            if (projectInfoPtr->Name.IsEmpty()) {
                projectInfoPtr->Name = "Project " + FString::FromInt(projectInfoPtr->Id);
            }

            jsonProject->TryGetStringField("companyName", projectInfoPtr->CompanyName);
            if (projectInfoPtr->CompanyName.IsEmpty()) {
                projectInfoPtr->CompanyName = "Account " + FString::FromInt(projectInfoPtr->CompanyId);
            }

            // Subscription
            if (jsonProject->HasField("subscription")) {
                TSharedPtr<FJsonObject> jsonSubscription = jsonProject->GetObjectField("subscription");
                projectInfoPtr->SubscriptionId = jsonSubscription->GetStringField("id");
                projectInfoPtr->SubscriptionName = jsonSubscription->GetStringField("name");
                FString dateTimeString;
                if (jsonSubscription->TryGetStringField("expiry", dateTimeString))
                {
                    FDateTime dateTime = FDateTime::UtcNow();
                    FDateTime::Parse(dateTimeString, dateTime);
                    projectInfoPtr->SubscriptionExpiry = dateTime;
                }
            }

            // Check if this is a project owner
            if (jsonProject->HasField("isOwner")) {
                projectInfoPtr->IsOwner = jsonProject->GetBoolField("isOwner");
            }

            // Check if this is a project manager
            if (jsonProject->HasField("roles")) {
                TArray<TSharedPtr<FJsonValue>> jsonRoles = jsonProject->GetArrayField("roles");
                for (auto& value : jsonRoles) {
                    if (value->AsString().Equals("admin")) {
                        projectInfoPtr->IsManager = true;
                        break;
                    }
                }
            }

            // Users
            if (jsonProject->HasField("users")) {
                TArray<TSharedPtr<FJsonValue>> jsonUsers = jsonProject->GetArrayField("users");
                for (auto& value : jsonUsers) {
                    projectInfoPtr->Users.Add(value->AsString());
                }
            }

            // limits
            if (jsonProject->HasField("limits")) {
                TSharedPtr<FJsonObject> jsonLimits = jsonProject->GetObjectField("limits");
                projectInfoPtr->SessionLimit = jsonLimits->GetIntegerField("session limit");
                projectInfoPtr->UserLimit = jsonLimits->GetIntegerField("user limit");
                projectInfoPtr->ObjectLimit = jsonLimits->GetIntegerField("object limit");
                if (!jsonLimits->TryGetBoolField("SF LAN Enabled", projectInfoPtr->LANEnabled))
                {
                    projectInfoPtr->LANEnabled = false;
                }
            }

            // Usage
            if (jsonProject->HasField("usage")) {
                TSharedPtr<FJsonObject> jsonUsage = jsonProject->GetObjectField("usage");
                projectInfoPtr->SessionCount = jsonUsage->GetIntegerField("sessions");
                projectInfoPtr->UserCount = jsonUsage->GetIntegerField("users");
            }

            // Get session data
            if (jsonProject->HasField("sessions")) {
                TSharedPtr<FJsonObject> jsonSessionsPtr = jsonProject->GetObjectField("sessions");
                for (auto sessionPair : jsonSessionsPtr->Values) {
                    TSharedPtr<sfSessionInfo> sessionInfoPtr = ParseJsonSession(sessionPair.Value->AsObject());
                    if (sessionInfoPtr.IsValid()) {
                        sessionInfoPtr->ProjectId = projectInfoPtr->Id;
                        sessionInfoPtr->ProjectName = projectInfoPtr->Name;
                        projectInfoPtr->Sessions.Add(sessionInfoPtr);
                    }
                }
                projectInfoPtr->Sessions.Sort([](const TSharedPtr<sfSessionInfo>& A, const TSharedPtr<sfSessionInfo>& B) { 
                    return A->RoomInfoPtr->Id() < B->RoomInfoPtr->Id();
                });
            }

            projectMap.Add(projectInfoPtr->Id, projectInfoPtr);
        }

        projectMap.KeySort([](const int& A, const int& B) { return A < B; });
        onGetSessionsDelegate.Execute(projectMap, error);
        return;
    }

    // Error
    if (!error.IsEmpty()) {
        onGetSessionsDelegate.Execute(projectMap, error);
        return;
    }

    // Unexpected error
    KS::Log::Error("GetSessions response did not contain an error message or valid json response.", LOG_CHANNEL);
    onGetSessionsDelegate.Execute(projectMap, "Unexpected get sessions error.");
}

void sfBaseWebService::HandleStartSessionResponse(
    TSharedPtr<FJsonObject> jsonPtr, 
    const FString& error, 
    OnStartSessionDelegate onStartSession)
{
    m_isStartingSession = false;

    // Success
    if (error.IsEmpty() && jsonPtr.IsValid()) {
        for (auto pair : jsonPtr->Values) {
            TSharedPtr<FJsonObject> jsonProject = pair.Value->AsObject();
            TSharedPtr<sfProjectInfo> projectInfoPtr = MakeShareable(new sfProjectInfo());
            projectInfoPtr->Id = FCString::Atoi(*(pair.Key));
            projectInfoPtr->Name = jsonProject->GetStringField("name");

            // Get session data
            if (jsonProject->HasField("sessions")) {
                TSharedPtr<FJsonObject> jsonSessionsPtr = jsonProject->GetObjectField("sessions");
                for (auto sessionPair : jsonSessionsPtr->Values) {
                    TSharedPtr<sfSessionInfo> sessionInfoPtr = ParseJsonSession(sessionPair.Value->AsObject());
                    if (sessionInfoPtr.IsValid()) {
                        sessionInfoPtr->ProjectId = projectInfoPtr->Id;
                        sessionInfoPtr->ProjectName = projectInfoPtr->Name;
                        projectInfoPtr->Sessions.Add(sessionInfoPtr);
                        //Sleep for two secconds
                        FPlatformProcess::Sleep(2.0f);
                        onStartSession.Execute(sessionInfoPtr, error);
                        return;
                    }
                }
            }
        }
    }

    // Error
    if (!error.IsEmpty()) {
        onStartSession.Execute(nullptr, error);
        return;
    }

    // Unexpected error
    KS::Log::Error("StartSession response did not contain an error message or valid json response.", LOG_CHANNEL);
    onStartSession.Execute(nullptr, "Unexpected start session error.");
}

void sfBaseWebService::HandleStopSessionResponse(
    TSharedPtr<sfSessionInfo> sessionInfoPtr, 
    const FString& error, 
    OnStopSessionDelegate onStopSession)
{
    m_isStoppingSession = false;
    onStopSession.Execute(sessionInfoPtr, error);
}

void sfBaseWebService::HandleSetUserColorResponse(
    int projectId,
    const FLinearColor& color,
    const FString& error,
    OnSetUserColorDelegate onSetUserColor)
{
    m_isSettingUserColor = false;
    onSetUserColor.Execute(color, error);
}

void sfBaseWebService::HandleRefreshTokenResponse(const FString& error, OnRefreshTokenDelegate onRefreshToken)
{
    m_isRefreshingToken = false;
    onRefreshToken.Execute(error);
}

void sfBaseWebService::HandleDownloadLANServerResponse(
    TArray<uint8> data,
    FString& error,
    OnDownloadLANServerDelegate onDownloadLANServer)
{
    m_isDownloadingLANServer = false;
    onDownloadLANServer.Execute(data, error);
}

void sfBaseWebService::HandleRegisterInstallResponse(TSharedPtr<FJsonObject> jsonPtr, const FString& error)
{
    if (!error.IsEmpty()) {
        KS::Log::Error("RegisterInstall response error: " + std::string(TCHAR_TO_UTF8(*error)), LOG_CHANNEL);
        return;
    }

    if (jsonPtr.IsValid() && jsonPtr->HasField("token")) {
        sfConfig::Get().InstallToken = jsonPtr->GetStringField("token");
    }
}

TSharedPtr<sfSessionInfo> sfBaseWebService::ParseJsonSession(TSharedPtr<FJsonObject> jsonPtr)
{
    if (!jsonPtr.IsValid()) {
        return nullptr;
    }

    TSharedPtr<sfSessionInfo> sessionInfoPtr = MakeShareable(new sfSessionInfo());
    if (jsonPtr->HasField("roomInfo"))
    {
        sessionInfoPtr->RoomInfoPtr = ParseJsonRoom(jsonPtr->GetObjectField("roomInfo"));
        FString instanceName;
        if (sessionInfoPtr->RoomInfoPtr != nullptr &&
            jsonPtr->TryGetStringField("creator", sessionInfoPtr->Creator) &&
            jsonPtr->TryGetStringField("instanceName", instanceName) &&
            jsonPtr->TryGetStringField("version", sessionInfoPtr->RequiredVersion) &&
            jsonPtr->TryGetBoolField("canDelete", sessionInfoPtr->CanStop) &&
            jsonPtr->TryGetStringField("application", sessionInfoPtr->LaunchApplication))
        {
            // If this is not an unreal session, then return null.
            if (!sessionInfoPtr->LaunchApplication.StartsWith("Unreal")) {
                return nullptr;
            }
            
            TArray<FString> strArray;
            if (instanceName.ParseIntoArray(strArray, TEXT(";"), true) > 1)
            {
                sessionInfoPtr->ProjectName = strArray[0];
                sessionInfoPtr->LevelName = strArray[1];
            }
            else
            {
                sessionInfoPtr->LevelName = instanceName;
            }
            
            // Default start time to a unix timestamp in seconds
            int64 startTime = FDateTime::Now().ToUnixTimestamp();
            jsonPtr->TryGetNumberField("startTime", startTime);
            sessionInfoPtr->StartTime = FDateTime::FromUnixTimestamp(startTime);
            
            return sessionInfoPtr;
        }
    }
    return nullptr;
}

ksRoomInfo::SPtr sfBaseWebService::ParseJsonRoom(TSharedPtr<FJsonObject> jsonPtr)
{
    if (!jsonPtr.IsValid()) {
        return nullptr;
    }
    FString scene, ip, room;
    if (jsonPtr->HasField("id") &&
        jsonPtr->HasField("port") &&
        jsonPtr->TryGetStringField("scene", scene) &&
        jsonPtr->TryGetStringField("ip", ip) &&
        jsonPtr->TryGetStringField("room", room)) 
    {
        ksRoomInfo::SPtr roomInfoPtr = ksRoomInfo::Create();
        roomInfoPtr->Id() = jsonPtr->GetIntegerField("id");
        roomInfoPtr->Port() = jsonPtr->GetIntegerField("port");
        roomInfoPtr->Scene() = TCHAR_TO_UTF8(*scene);
        roomInfoPtr->Host() = TCHAR_TO_UTF8(*ip);
        roomInfoPtr->Type() = TCHAR_TO_UTF8(*room);
        return roomInfoPtr;
    }
    return nullptr;
}

void sfBaseWebService::LogJSON(TSharedPtr<FJsonObject> jsonPtr)
{
    if (jsonPtr.IsValid()) {
        FString jsonString;
        TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&jsonString);
        FJsonSerializer::Serialize(jsonPtr.ToSharedRef(), jsonWriter);
        KS::Log::Warning("  " + std::string(TCHAR_TO_UTF8(*jsonString)), LOG_CHANNEL);
    }
}

#undef LOG_CHANNEL
