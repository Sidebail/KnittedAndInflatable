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

#include "sfBaseActivity.h"

#include <CoreMinimal.h>

/**
 * Randomly performs configurable activities.
 */
class sfMonkey
{
public:
    /**
     * Constructor.
     */
    sfMonkey();

    /**
     * Destructor
     */
    ~sfMonkey();

    /**
     * Is the monkey running?
     */
    bool IsRunning();

    /**
     * Restores the monkey to its default configuration.
     */
    void UseDefaults();

    /**
     * Sets all activity weights to 0 and clears all activity configuration.
     */
    void Reset();

    /**
     * Gets the activity with the given name.
     *
     * @param   const FString& name of activity to get. Case insensitive.
     * @return  TSharedPtr<sfBaseActivity> activity with the given name, or invalid pointer if none exists.
     */
    TSharedPtr<sfBaseActivity> GetActivity(const FString& name);

    /**
     * Starts randomly adding assets to the level. Does nothing if the monkey does not have a valid path with assets.
     */
    void Start();

    /**
     * Stops randomly adding assets to the level.
     */
    void Stop();

private:
    bool m_isRunning;
    TArray<TSharedPtr<sfBaseActivity>> m_activities;
    TSharedPtr<sfBaseActivity> m_currentActivity;
    FDelegateHandle m_tickHandle;
    FDelegateHandle m_onActorDeletedHandle;

    /**
     * Adds a random asset to the level.
     *
     * @param   float deltaTime since the last tick.
     * @return  bool true to keep the tick function registered.
     */
    bool Tick(float deltaTime);

    /**
     * Called when an actor is deleted. Calls OnActorDeleted on the current activity.
     *
     * @param   AActor* actorPtr
     */
    void OnActorDeleted(AActor* actorPtr);
};