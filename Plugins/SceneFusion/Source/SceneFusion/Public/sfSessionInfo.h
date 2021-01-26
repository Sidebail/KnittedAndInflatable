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

#include "ksRoomInfo.h"
#include <CoreMinimal.h>
#include <Misc/DateTime.h>

using namespace KS::Reactor;

/**
 * Session data that is available without connecting to a room. Consists of room info and level creator.
 */
struct sfSessionInfo
{
public:
    int ProjectId;
    FString ProjectName;
    FString Creator;
    FString UnrealProjectName;
    FString LevelName;
    FString LaunchApplication;
    bool CanStop;
    FString RequiredVersion;
    ksRoomInfo::SPtr RoomInfoPtr;
    FDateTime Time;
    FDateTime StartTime;

    /**
     * Constructor
     */
    sfSessionInfo();

    /**
     * Overload the == operator.
     *
     * @param   const sfSessionInfo& sessionInfo
     * @return  bool
     */
    bool operator==(const sfSessionInfo& sessionInfo) {
        return ProjectId == sessionInfo.ProjectId &&
            ProjectName == sessionInfo.ProjectName &&
            Creator == sessionInfo.Creator &&
            LevelName == sessionInfo.LevelName &&
            LaunchApplication == sessionInfo.LaunchApplication &&
            CanStop == sessionInfo.CanStop &&
            RequiredVersion == sessionInfo.RequiredVersion &&
            *RoomInfoPtr == *sessionInfo.RoomInfoPtr;
    }

    /**
     * Overload the != operator.
     *
     * @param   const sfSessionInfo& sessionInfo
     * @return  bool
     */
    bool operator!=(const sfSessionInfo& sessionInfo) {
        return !(*this == sessionInfo);
    }
};