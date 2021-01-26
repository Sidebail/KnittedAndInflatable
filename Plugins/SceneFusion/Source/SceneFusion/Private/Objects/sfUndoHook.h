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
#include "UObject/NoExportTypes.h"
#include "sfUndoHook.generated.h"

/**
 * This is part of a hack to run code after an undo transaction but before the objects in the transaction have
 * PostEditUndo called.
 */
UCLASS()
class UsfUndoHook : public UObject
{
	GENERATED_BODY()
	
public:
    /**
     * Called after undoing a transaction that this object was hacked into. Rebuilds BSP.
     */
    virtual void PostEditUndo() override;
};
