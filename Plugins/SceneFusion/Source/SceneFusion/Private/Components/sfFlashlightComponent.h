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

#include "CoreMinimal.h"
#include "Components/SpotLightComponent.h"
#include "sfFlashlightComponent.generated.h"

/**
 * Flashlight component that has no icon shown in the viewport.
 */
UCLASS()
class UsfFlashlightComponent : public USpotLightComponent
{
	GENERATED_BODY()
	
public:
    /**
     * Constructor
     */
    UsfFlashlightComponent()
    {
        //Hide icon
#if WITH_EDITORONLY_DATA
        DynamicEditorTextureScale = 0.0f;
#endif
    }
};
