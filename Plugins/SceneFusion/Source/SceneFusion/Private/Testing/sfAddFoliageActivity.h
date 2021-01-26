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
#include <InstancedFoliageActor.h>

/**
 * Activity that adds foliage to random mesh components.
 */
class sfAddFoliageActivity : public sfBaseActivity
{
public:
    /**
     * Constructor
     *
     * @param   const FString& name of activity.
     * @param   float weight the determines how likely this activity is to occur.
     */
    sfAddFoliageActivity(const FString& name, float weight);

    /**
     * Selects random bounds to add foliage in.
     */
    virtual void Start() override;

    /**
     * Called every tick to add foliage.
     *
     * @param   float deltaTime in seconds since the last tick.
     */
    virtual void Tick(float deltaTime) override;

    /**
     * Called if this activity is active when an actor is deleted.
     *
     * @param   AActor* actorPtr that was deleted.
     */
    virtual void OnActorDeleted(AActor* actorPtr) override;

private:
    AInstancedFoliageActor* m_actorPtr;
    UFoliageType* m_typePtr;
    FBox m_bounds;

    /**
     * Checks if a component can have foliage attached. We do not attach foliage to model components because it doesn't
     * work properly.
     *
     * @param   const UPrimitiveComponent* componentPtr
     * @return  bool true if the component is not a model component.
     */
    static bool Filter(const UPrimitiveComponent* componentPtr);
};