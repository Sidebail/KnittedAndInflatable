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

#include <CoreMinimal.h>
#include "sfAction.h"

/**
 * Executes actions at scheduled times.
 */
class sfTimer
{
public:
    /**
     * Constructor.
     */
    sfTimer();

    /**
     * Destructor
     */
    ~sfTimer();

    /**
     * Executes the given action at the given time.
     *
     * @param   sfAction::Action action to execute
     * @param   const TArray<FString>& args of action
     * @param   FDateTime date time to execute the action
     */
    FDelegateHandle StartTimer(sfAction::Action action, const TArray<FString>& args, FDateTime dateTime);

    /**
     * Stops excuting action of the given handle.
     *
     * @param   FDelegateHandle handle
     */
    void StopTimer(FDelegateHandle handle);

    /**
     * Stops all actions.
     */
    void StopAll();

private:
    TSet<FDelegateHandle> m_handles;
};