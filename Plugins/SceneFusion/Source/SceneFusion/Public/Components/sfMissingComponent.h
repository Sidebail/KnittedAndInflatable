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

#include "../sfMissingObject.h"
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "sfMissingComponent.generated.h"


/**
 * Component class to use as a stand-in for missing non-scene component classes.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UsfMissingComponent : public UActorComponent, public IsfMissingObject
{
	GENERATED_BODY()

public:	
    // Name of the missing component class
    UPROPERTY(VisibleAnywhere)
    FString ClassName;
	
    /**
     * @return  FString& name of the missing component class or blueprint path.
     */
    virtual FString& MissingClass() override;

    /**
     * Replaces this componet with a new one using the data from this component's sfObject.
     */
    virtual void Reload() override;

    /**
     * Called before destroying the object. Removes the component from the missing object manager.
     */
    virtual void BeginDestroy() override;
};
