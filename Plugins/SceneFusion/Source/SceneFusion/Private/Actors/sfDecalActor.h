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
#include <Engine/DecalActor.h>
#include <Components/BillboardComponent.h>
#include <Components/ArrowComponent.h>
#include "sfDecalActor.generated.h"

/**
 * A decal actor that cannot be selected in the editor.
 */
UCLASS()
class AsfDecalActor : public ADecalActor
{
    GENERATED_BODY()
public:

    /**
     * Constructor
     *
     * @param   const FObjectInitializer& objectInitializer
     */
    AsfDecalActor(const FObjectInitializer& objectInitializer);

    /**
     * Overrides IsSelectable to always return false.
     *
     * @return  bool false
     */
    virtual bool IsSelectable() const override;
};