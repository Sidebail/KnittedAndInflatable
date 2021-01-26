#include "sfWebService.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfConfig.h"

#include <Runtime/Online/HTTP/Public/Http.h>
#include <Runtime/Launch/Resources/Version.h>
#include <HAL/Platform.h>

#define LOG_CHANNEL "sfWebService"

sfWebService::sfWebService() : sfBaseWebService{}
{
}

void sfWebService::Logout(OnLogoutDelegate onLogout)
{
    if (!m_isLoggedIn)
    {
        return;
    }

    // Create JSON request data
    sfConfig& settings = sfConfig::Get();
    TSharedPtr<FJsonObject> jsonObjectPtr = MakeShareable(new FJsonObject);
    jsonObjectPtr->SetStringField("email", settings.Email);
    
    Send(settings.ServiceURL / "v2/logout", "POST", jsonObjectPtr,
        OnResponseDelegate::CreateLambda(
            [this, onLogout](TSharedPtr<FJsonObject> resultPtr, const FString& error) {
                HandleLogoutResponse(error, onLogout);
            }
        )
    );
}

void sfWebService::Login(const FString& email, const FString& pass, OnLoginDelegate onLogin)
{
    if (m_isLoggedIn || m_isLoggingIn)
    {
        return;
    }
    
    // Create JSON request data
    sfConfig& settings = sfConfig::Get();
    TSharedPtr<FJsonObject> jsonObjectPtr = MakeShareable(new FJsonObject);
    jsonObjectPtr->SetStringField("email", email);
    jsonObjectPtr->SetStringField("password", pass);
    m_isLoggingIn = true;

    Send(settings.ServiceURL / "v2/login", "POST", jsonObjectPtr,
        OnResponseDelegate::CreateLambda(
            [this, email, onLogin](TSharedPtr<FJsonObject> resultPtr, const FString& error) {
                if (!error.IsEmpty()) {
                    HandleLoginResponse(email, "", resultPtr, error, onLogin);
                }
                else {
                    FetchSFToken(email, resultPtr->GetStringField("token"), onLogin);
                }
            }
        )
    );
}

void sfWebService::Authenticate(OnLoginDelegate onLogin)
{
    if (m_isLoggedIn || m_isLoggingIn) {
        return;
    }

    sfConfig& settings = sfConfig::Get();
    m_isLoggingIn = true;

    if (settings.Email.IsEmpty() || settings.Token.IsEmpty()) {
        HandleLoginResponse(settings.Email, settings.Token, nullptr, "Missing email and/or token.", onLogin);
    }
    else {
        FetchSFToken(settings.Email, settings.Token, onLogin);
    }
}

void sfWebService::GetSessions(OnGetSessionsDelegate onGetSessions)
{
    if (!m_isLoggedIn || m_isFetchingSessions) {
        return;
    }

    sfConfig& settings = sfConfig::Get();
    TSharedPtr<FJsonObject> jsonObjectPtr = MakeShareable(new FJsonObject);
    jsonObjectPtr->SetStringField("token", settings.SFToken);
    jsonObjectPtr->SetStringField("version", SceneFusion::Version());
    m_isFetchingSessions = true;

    Send(settings.ServiceURL / "v1/getSessions", "POST", jsonObjectPtr,
        OnResponseDelegate::CreateLambda(
            [this, onGetSessions](TSharedPtr<FJsonObject> resultPtr, const FString& error) {
                //LogJSON(resultPtr);
                HandleGetSessionsResponse(resultPtr, error, onGetSessions);
            }
        )
    );
}


void sfWebService::StartSession(int projectId, const FString& level, OnStartSessionDelegate onStartSession)
{
    if (!m_isLoggedIn || m_isStartingSession) {
        return;
    }

    sfConfig& settings = sfConfig::Get();
    TSharedPtr<FJsonObject> jsonObjectPtr = MakeShareable(new FJsonObject);
    jsonObjectPtr->SetStringField("token", settings.SFToken);
    jsonObjectPtr->SetStringField("version", SceneFusion::Version());
    jsonObjectPtr->SetNumberField("projectId", projectId);
    jsonObjectPtr->SetStringField("scene", level);
    m_isStartingSession = true;

    Send(settings.ServiceURL / "v1/startsession", "POST", jsonObjectPtr,
        OnResponseDelegate::CreateLambda(
            [this, onStartSession](TSharedPtr<FJsonObject> resultPtr, const FString& error) {
                //LogJSON(resultPtr);
                HandleStartSessionResponse(resultPtr, error, onStartSession);
            }
        )
    );
}


void sfWebService::StopSession(TSharedPtr<sfSessionInfo> sessionInfoPtr, OnStopSessionDelegate onStopSession)
{
    if (!m_isLoggedIn || m_isStoppingSession) {
        return;
    }

    sfConfig& settings = sfConfig::Get();
    TSharedPtr<FJsonObject> jsonObjectPtr = MakeShareable(new FJsonObject);
    jsonObjectPtr->SetStringField("token", settings.SFToken);
    jsonObjectPtr->SetNumberField("projectId", sessionInfoPtr->ProjectId);
    jsonObjectPtr->SetNumberField("session", sessionInfoPtr->RoomInfoPtr->Id());
    m_isStoppingSession = true;

    Send(settings.ServiceURL / "v1/stopSession", "POST", jsonObjectPtr,
        OnResponseDelegate::CreateLambda(
            [this, sessionInfoPtr, onStopSession](TSharedPtr<FJsonObject> resultPtr, FString& error) {
                //LogJSON(resultPtr);
                if (error.IsEmpty() && resultPtr.IsValid() && resultPtr->GetStringField("result") != "Success")
                {
                    error = "Failure";
                }
                HandleStopSessionResponse(sessionInfoPtr, error, onStopSession);
            }
        )
    );
}


void sfWebService::SetUserColor(int projectId, const FLinearColor& color, OnSetUserColorDelegate onSetUserColor)
{
    if (!m_isLoggedIn && m_isSettingUserColor) {
        return;
    }

    sfConfig& settings = sfConfig::Get();
    TSharedPtr<FJsonObject> jsonObjectPtr = MakeShareable(new FJsonObject);
    jsonObjectPtr->SetStringField("token", settings.SFToken);
    jsonObjectPtr->SetNumberField("projectId", projectId);
    jsonObjectPtr->SetStringField("newColor", color.ToFColor(true).ToHex());   
    m_isSettingUserColor = true;

    Send(settings.ServiceURL / "v1/setColor", "POST", jsonObjectPtr,
        OnResponseDelegate::CreateLambda(
            [this, projectId, color, onSetUserColor](TSharedPtr<FJsonObject> resultPtr, const FString& error) {
                //LogJSON(resultPtr);
                HandleSetUserColorResponse(projectId, color, error, onSetUserColor);
            }
        )
    );
}

void sfWebService::RefreshToken(OnRefreshTokenDelegate onRefreshToken)
{
    if (!m_isLoggedIn && m_isRefreshingToken) {
        return;
    }

    sfConfig& settings = sfConfig::Get();
    TSharedPtr<FJsonObject> jsonObjectPtr = MakeShareable(new FJsonObject);
    jsonObjectPtr->SetStringField("token", settings.SFToken);

    m_isRefreshingToken = true;
    Send(settings.ServiceURL / "v1/refreshToken", "POST", jsonObjectPtr,
        OnResponseDelegate::CreateLambda(
            [this, onRefreshToken](TSharedPtr<FJsonObject> resultPtr, const FString& error) {
                //LogJSON(resultPtr);
                HandleRefreshTokenResponse(error, onRefreshToken);
            }
        )
    );
}

void sfWebService::DownloadLANServer(
    TSharedPtr<sfProjectInfo> project,
    OnDownloadLANServerDelegate onDownLoadLANServer)
{
    if (!m_isLoggedIn || m_isDownloadingLANServer) {
        return;
    }

    sfConfig& settings = sfConfig::Get();
    TSharedPtr<FJsonObject> jsonObjectPtr = MakeShareable(new FJsonObject);
    jsonObjectPtr->SetStringField("token", settings.SFToken);
    jsonObjectPtr->SetNumberField("projectId", project->Id);
    jsonObjectPtr->SetStringField("version", SceneFusion::Version());
#if PLATFORM_MAC
    jsonObjectPtr->SetStringField("os", "osx");
#elif PLATFORM_LINUX
    jsonObjectPtr->SetStringField("os", "linux");
#else
    jsonObjectPtr->SetStringField("os", "win");
#endif
    m_isDownloadingLANServer = true;

    jsonObjectPtr->SetStringField("application", FString::Printf(TEXT("Unreal Editor %d.%d.%d"),
        ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION));
    FString jsonString;
    TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&jsonString);
    FJsonSerializer::Serialize(jsonObjectPtr.ToSharedRef(), jsonWriter);
    //UE_LOG(LogSceneFusion, Log, TEXT("URL: %s\n%s"), *url, *jsonString);

    // HTTPS Post Request
    TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();
    request->SetURL(settings.ServiceURL / "v1/downloadLANServer");
    request->SetVerb("POST");
    request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
    request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
    request->SetContentAsString(jsonString);
    request->OnProcessRequestComplete().BindLambda(
        [this, onDownLoadLANServer](FHttpRequestPtr requestPtr, FHttpResponsePtr responsePtr, bool wasSuccessful) {
            FString error = "";
            TArray<uint8> data;

            if (!wasSuccessful || !responsePtr.IsValid()) {
                error = "HTTP Request failed";
            }
            else if (!EHttpResponseCodes::IsOk(responsePtr->GetResponseCode())) {
                if (responsePtr->GetResponseCode() == 404) {
                    error = "A LAN server could not be found for this Scene Fusion version";
                }
                else {
                    error = "HTTP Request failed: ";
                    error.AppendInt(responsePtr->GetResponseCode());
                }
            }
            else {
                // Check response data
                FString contentType = responsePtr->GetHeader("content-type");
                if (contentType.Contains("application/zip")) {
                    data = responsePtr->GetContent();
                }
                else if (contentType.Contains("application/json")) {
                    TSharedPtr<FJsonObject> resultPtr = nullptr;
                    ParseJSON(responsePtr->GetContent(), resultPtr, error);
                    if (error.IsEmpty()) {
                        error = "Unexpected JSON response";
                    }
                }
                else {
                    error = "Unexpected content type: " + contentType;
                }
            }

            HandleDownloadLANServerResponse(data, error, onDownLoadLANServer);
        }
    );
    request->ProcessRequest();
}

void sfWebService::RegisterInstall()
{
    if (!sfConfig::Get().InstallToken.IsEmpty()) {
        return;
    }

    sfConfig& settings = sfConfig::Get();
    TSharedPtr<FJsonObject> jsonObjectPtr = MakeShareable(new FJsonObject);
    jsonObjectPtr->SetStringField("version", SceneFusion::Version());

    Send(settings.ServiceURL / "v2/registerInstall", "POST", jsonObjectPtr,
        OnResponseDelegate::CreateLambda(
            [this](TSharedPtr<FJsonObject> resultPtr, const FString& error) {
                //LogJSON(resultPtr);
                HandleRegisterInstallResponse(resultPtr, error);
            }
        )
    );
}

void sfWebService::FetchSFToken(const FString& email, const FString& token, OnLoginDelegate onLogin)
{
    sfConfig& settings = sfConfig::Get();
    TSharedPtr<FJsonObject> jsonObjectPtr = MakeShareable(new FJsonObject);
    jsonObjectPtr->SetStringField("email", email);
    jsonObjectPtr->SetStringField("token", token);
    jsonObjectPtr->SetStringField("version", SceneFusion::Version());
    jsonObjectPtr->SetStringField("installToken", settings.InstallToken);

    Send(settings.ServiceURL / "v2/SceneFusionLogin", "POST", jsonObjectPtr,
        OnResponseDelegate::CreateLambda(
            [this, email, token, onLogin](TSharedPtr<FJsonObject> resultPtr, const FString& error) {
                //LogJSON(resultPtr);
                HandleLoginResponse(email, token, resultPtr, error, onLogin);
            }
        )
    );
}

void sfWebService::Send(
    const FString& url, 
    const FString& verb, 
    TSharedPtr<FJsonObject> jsonDataPtr, 
    OnResponseDelegate onComplete)
{
    jsonDataPtr->SetStringField("application", FString::Printf(TEXT("Unreal %d.%d.%d"),
        ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION));
    FString jsonString;
    TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&jsonString);
    FJsonSerializer::Serialize(jsonDataPtr.ToSharedRef(), jsonWriter);
    //UE_LOG(LogSceneFusion, Log, TEXT("URL: %s\n%s"), *url, *jsonString);

    // HTTPS Post Request
    TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();
    request->SetURL(url);
    request->SetVerb(verb);
    request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
    request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
    request->SetContentAsString(jsonString);
    request->OnProcessRequestComplete().BindLambda(
        [onComplete](FHttpRequestPtr requestPtr, FHttpResponsePtr responsePtr, bool wasSuccessful) {
            FString error = "";
            TSharedPtr<FJsonObject> resultPtr = nullptr;

            if (!wasSuccessful || !responsePtr.IsValid()) {
                error = "HTTP Request failed";
            }
            else if (!EHttpResponseCodes::IsOk(responsePtr->GetResponseCode())) {
                error = "HTTP Request failed: ";
                error.AppendInt(responsePtr->GetResponseCode());
            }
            else {
                ParseJSON(responsePtr->GetContent(), resultPtr, error);
            }

            onComplete.Execute(resultPtr, error);
        }
    );
    request->ProcessRequest();
}

void sfWebService::ParseJSON(TArray<uint8> rawData, TSharedPtr<FJsonObject>& outJSONPtr, FString& error)
{
    TSharedPtr<FJsonObject> jsonObjectPtr = MakeShareable(new FJsonObject);
    FUTF8ToTCHAR TCHARData(reinterpret_cast<const ANSICHAR*>(rawData.GetData()), rawData.Num());
    FString jsonString(TCHARData.Length(), TCHARData.Get());
    TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(jsonString);
    FJsonSerializer::Deserialize(jsonReader, jsonObjectPtr);

    if (jsonObjectPtr->HasField("err")) {
        error = jsonObjectPtr->GetStringField("err");
    }
    else if (jsonObjectPtr->HasField("msg")) {
        outJSONPtr = jsonObjectPtr->GetObjectField("msg");
    }
    else {
        KS::Log::Error("Response missing 'msg' or 'err' field:\n "
            + std::string(TCHAR_TO_UTF8(*jsonString)), LOG_CHANNEL);
        error = "Unexpected response format, check output logs.";
     }
}

#undef LOG_CHANNEL
