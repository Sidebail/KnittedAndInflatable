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

#include "../../Public/SceneFusion.h"

#include <Log.h>
#include <CoreMinimal.h>
#include <GameFramework/Actor.h>
#include <Editor.h>

/**
 * Interface for activities that can be performed by sfMonkey.
 */
class sfBaseActivity
{
public:
    /**
     * Constructor
     *
     * @param   const FString& name of the activity.
     * @param   float weight of the activity, effecting it's likelyhood of occurring.
     */
    sfBaseActivity(const FString& name, float weight);

    /**
     * Destructor
     */
    virtual ~sfBaseActivity();

    /**
     * @return  const FString& activity name
     */
    const FString& Name() const { return m_name; }

    /**
     * @return  float& activity duration in seconds.
     */
    float& Duration() { return m_duration; }

    /**
     * @return  float activity duration in seconds.
     */
    float Duration() const { return m_duration; }

    /**
     * @return  float& activity weight. Higher numbers increase likelyhood of the activity occurring. Setting to 0 will
     *          disable the activity.
     */
    float& Weight() { return m_weight; }

    /**
     * @return  float activity weight. Higher numbers increase likelyhood of the activity occurring. Setting to 0 will
     *          disable the activity.
     */
    float Weight() const { return m_weight; }

    /**
     * Handles command arguments.
     *
     * @param   const TArray<FString>& args
     * @param   int index of first argument in array to process.
     */
    virtual void HandleArgs(const TArray<FString>& args, int index) {};

    /**
     * Called when the activity starts.
     */
    virtual void Start() {};

    /**
     * Called every tick while the activity is active.
     *
     * @param   float deltaTime since the last tick in seconds.
     */
    virtual void Tick(float deltaTime) {};

    /**
     * Called when the activity finishes.
     */
    virtual void Finish() {};

    /**
     * Called when an actor is deleted.
     *
     * @param   AActor* actorPtr that was deleted.
     */
    virtual void OnActorDeleted(AActor* actorPtr) {};

protected:

    /**
     * Fills an array with actors randomly selected from the level.
     *
     * @param   TArray<AActor*>& actors array to fill with actors.
     */
    virtual void RandomActors(TArray<AActor*>& actors);

    /**
     * Returns a random actor from the level. May return nullptr.
     *
     * @return  AActor* random actor
     */
    virtual AActor* RandomActor();

private:
    FString m_name;
    float m_duration;
    float m_weight;
};