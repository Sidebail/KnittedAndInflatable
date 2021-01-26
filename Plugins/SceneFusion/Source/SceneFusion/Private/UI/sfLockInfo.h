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
#include <Widgets/SBoxPanel.h>

#include "../../Public/Translators/sfActorTranslator.h"

/**
 * Lock info.
 */
class sfLockInfo
{
public:
    sfActorTranslator::LockType LockType;
    sfUser::SPtr LockOwner;
    TSharedPtr<SHorizontalBox> Icon;

    /**
     * Constructor.
     */
    sfLockInfo();

private:
    /**
     * Gets lock icon for the given actor.
     *
     * @return  const FSlateBrush*
     */
    const FSlateBrush* GetLockIcon() const;

    /**
     * Gets lock color for the given actor.
     *
     * @return  FSlateColor
     */
    FSlateColor GetLockColor() const;

    /**
     * Gets lock tool tip for the given actor.
     *
     * @return  FText
     */
    FText GetLockTooltip() const;
};
