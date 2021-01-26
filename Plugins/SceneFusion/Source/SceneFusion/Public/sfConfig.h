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

#include "../Public/SceneFusion.h"
#include <Runtime/Core/Public/Misc/Paths.h>
#include <Runtime/Core/Public/Misc/FileHelper.h>

/**
 * Scene Fusion configuration class
 */
class sfConfig
{
private:
    sfConfig() :
        Name(""),
        Email(""),
        Token(""),
        InstallToken(""),
        SFToken(""),
        ProjectId(-1),
        ServiceURL("https://matchmaker-console.kinematicsoup.com/api"),
        WebURL("https://console.kinematicsoup.com"),
        MockWebServerAddress(""),
        MockWebServerPort(""),
        ShowAvatar(true),
        SyncBlueprint(false),
        SyncLandscape(true),
        LogDebug(false),
        IdleTime(0.5),
        MaxCreateTimeMS(40),
        ShowGettingStartedScreen(true),
        LastSFVersion("")
    {}

public:
    /**
     * Get a reference to a static instance of the configuration.
     *
     * @return sfConfig&
     */
    static sfConfig& Get() {
        static sfConfig config;
        return config;
    }

    FString Name;
    FString Email;
    FString Token;
    FString InstallToken;
    FString SFToken;
    int ProjectId;
    FString ServiceURL;
    FString WebURL;
    FString MockWebServerAddress;
    FString MockWebServerPort;
    bool ShowAvatar;
    bool SyncBlueprint;
    bool SyncLandscape;
    bool LogDebug;
    float IdleTime;
    int MaxCreateTimeMS;
    bool ShowGettingStartedScreen;
    FString LastSFVersion;

    /**
     * Relative Path to the Scene Fusion configuration file.
     *
     * @return  FString
     */
    static FString Path()
    {
        KS::Log::Info(TCHAR_TO_UTF8(*FPaths::EngineSavedDir()));
        return FPaths::EngineSavedDir() + "/Config/Windows/SceneFusion.ini";
    }

    /**
     * Save configuration data to the user saved directory to prevent configs from being distributed with the plugin
     */
    void Save()
    {
        TArray<FString> configs;
        configs.Add("Email=" + Email);
        configs.Add("Token=" + Token);
        configs.Add("SFToken=" + SFToken);
        configs.Add("InstallToken=" + InstallToken);
        configs.Add("ServiceURL=" + ServiceURL);
        configs.Add("ProjectId=" + FString::FromInt(ProjectId));
        configs.Add("WebURL=" + WebURL);
        configs.Add("MockWebServerAddress=" + MockWebServerAddress);
        configs.Add("MockWebServerPort=" + MockWebServerPort);
        configs.Add("ShowAvatar=" + FString(ShowAvatar ? "true" : "false"));
        configs.Add("SyncBlueprint=" + FString(SyncBlueprint ? "true" : "false"));
        configs.Add("SyncLandscape=" + FString(SyncLandscape ? "true" : "false"));
        configs.Add("LogDebug=" + FString(LogDebug ? "true" : "false"));
        configs.Add("IdleTime=" + FString::SanitizeFloat(IdleTime));
        configs.Add("MaxCreateTimeMS=" + FString::FromInt(MaxCreateTimeMS));
        configs.Add("ShowGettingStartedScreen=" + FString(ShowGettingStartedScreen ? "true" : "false"));
        configs.Add("LastSFVersion=" + LastSFVersion);
        FFileHelper::SaveStringArrayToFile(configs, *Path());
    }

    /**
     * Load configuration settings.
     */
    void Load()
    {
        try {
            TArray<FString> configs;
            FFileHelper::LoadFileToStringArray(configs, *Path());

            FString key;
            FString value;
            for (FString& line : configs)
            {
                if (line.Split("=", &key, &value))
                {
                    if (key.Equals("Email"))
                    {
                        Email = value;
                        continue;
                    }
                    if (key.Equals("Token"))
                    {
                        Token = value;
                        continue;
                    }
                    if (key.Equals("SFToken"))
                    {
                        SFToken = value;
                        continue;
                    }
                    if (key.Equals("InstallToken"))
                    {
                        InstallToken = value;
                        continue;
                    }
                    if (key.Equals("ServiceURL"))
                    {
                        ServiceURL = value;
                        continue;
                    }
                    if (key.Equals("ProjectId"))
                    {
                        ProjectId = FCString::Atoi(*value);
                        continue;
                    }
                    if (key.Equals("WebURL"))
                    {
                        WebURL = value;
                        continue;
                    }
                    if (key.Equals("MockWebServerAddress"))
                    {
                        MockWebServerAddress = value;
                        continue;
                    }
                    if (key.Equals("MockWebServerPort"))
                    {
                        MockWebServerPort = value;
                        continue;
                    }
                    if (key.Equals("ShowAvatar"))
                    {
                        ShowAvatar = value == "true";
                        continue;
                    }
                    if (key.Equals("SyncBlueprint"))
                    {
                        SyncBlueprint = value == "true";
                        continue;
                    }
                    if (key.Equals("SyncLandscape"))
                    {
                        SyncLandscape = value == "true";
                        continue;
                    }
                    if (key.Equals("LogDebug"))
                    {
                        LogDebug = value == "true";
                        continue;
                    }
                    if (key.Equals("IdleTime"))
                    {
                        IdleTime = FCString::Atof(*value);
                        continue;
                    }
                    if (key.Equals("MaxCreateTimeMS"))
                    {
                        MaxCreateTimeMS = FCString::Atoi(*value);
                        continue;
                    }
                    if (key.Equals("ShowGettingStartedScreen"))
                    {
                        ShowGettingStartedScreen = value == "true";
                        continue;
                    }
                    if (key.Equals("LastSFVersion"))
                    {
                        LastSFVersion = value;
                        continue;
                    }
                }
            }
        }
        catch (std::exception ex)
        {
            KS::Log::Info("Unable to load existing Scene Fusion configs.");
        }
    }
};
