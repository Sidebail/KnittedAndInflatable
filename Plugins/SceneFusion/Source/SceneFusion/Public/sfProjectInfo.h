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

#include "sfSessionInfo.h"
#include <CoreMinimal.h>

/**
 * Scene Fusion project information.  Includes limits and a list of running sessions.
 */
struct sfProjectInfo
{
public:
    int Id;
    FString Name;
    int CompanyId;
    FString CompanyName;
    FString SubscriptionId;
    FString SubscriptionName;
    int SessionLimit;
    int UserLimit;
    int ObjectLimit;
    int SessionCount;
    int UserCount;
    FDateTime SubscriptionExpiry;
    TArray<TSharedPtr<sfSessionInfo>> Sessions;
    bool LANEnabled;
    bool IsManager;
    bool IsOwner;
    TArray<FString> Users;

    /**
     * Constructor
     */
    sfProjectInfo() :
        Id{ -1 },
        Name{ "" },
        CompanyId{ -1 },
        CompanyName{ "" },
        SubscriptionId{ "" },
        SubscriptionName{ "" },
        SessionLimit{ -1 },
        UserLimit{ 2 },
        ObjectLimit{ 0 },
        SessionCount{ 0 },
        UserCount{ 0 },
        LANEnabled{ false },
        IsManager{ false },
        IsOwner{ false }
    {
        SubscriptionExpiry = FDateTime(0);
    }

    /**
     * Overload the == operator.
     *
     * @param   const sfProjectInfo& projectInfo
     * @return  bool
     */
    bool operator==(const sfProjectInfo& projectInfo) {
        if (Id != projectInfo.Id ||
            Name != projectInfo.Name ||
            CompanyId != projectInfo.CompanyId ||
            CompanyName != projectInfo.CompanyName ||
            SubscriptionId != projectInfo.SubscriptionId ||
            SubscriptionName != projectInfo.SubscriptionName || 
            SessionLimit != projectInfo.SessionLimit ||
            UserLimit != projectInfo.UserLimit ||
            ObjectLimit != projectInfo.ObjectLimit ||
            SessionCount != projectInfo.SessionCount ||
            UserCount != projectInfo.UserCount ||
            SubscriptionExpiry != projectInfo.SubscriptionExpiry ||
            Sessions.Num() != projectInfo.Sessions.Num() ||
            LANEnabled != projectInfo.LANEnabled ||
            IsManager != projectInfo.IsManager ||
            IsOwner != projectInfo.IsOwner ||
            Users.Num() != projectInfo.Users.Num() ||
            Sessions.Num() != projectInfo.Sessions.Num())
        {
            return false;
        }

        for (int i = 0; i < Users.Num(); i++)
        {
            if (Users[i] != projectInfo.Users[i])
            {
                return false;
            }
        }

        for (int i = 0; i<Sessions.Num(); i++)
        {
            if (*Sessions[i] != *projectInfo.Sessions[i])
            {
                return false;
            }
        }
        return true;
    }

    /**
     * Overload the != operator.
     *
     * @param   const sfProjectInfo& projectInfo
     * @return  bool
     */
    bool operator!=(const sfProjectInfo& projectInfo) {
        return !(*this == projectInfo);
    }
};

typedef TMap<int, TSharedPtr<sfProjectInfo>> ProjectMap;
