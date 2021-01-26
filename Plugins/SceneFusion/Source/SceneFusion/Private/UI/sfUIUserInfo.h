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

#include "../Includes/sfUser.h"
#include "sfUIToggleButton.h"
#include <CoreMinimal.h>
#include <Runtime/SlateCore/Public/Brushes/SlateColorBrush.h>

using namespace KS::SceneFusion2;

/**
 * Wrapper for a Scene Fusion user object that contains additional UI information.
 */
class sfUIUserInfo
{
public:
    /**
     * Constructor
     *
     * @param   sfUser::SPtr - Scene Fusion User shared pointer
     */
    sfUIUserInfo(sfUser::SPtr sfUserPtr) :
        m_sfUserPtr{ sfUserPtr },
        m_isFollowed { false }
    {
        m_iconBrushPtr = sfUIStyles::CreateIcon("User128.png", FVector2D{ 16.0f, 16.0f });
        m_iconBrushPtr->TintColor = Color();
    }

    /**
     * Destructor. Clean up icon brush pointer.
     */
    ~sfUIUserInfo() {
        if (m_iconBrushPtr) {
            delete m_iconBrushPtr;
        }
    }

    /**
     * Is this the local user.
     *
     * @return  bool
     */
    bool IsLocal()
    {
        return (m_sfUserPtr) ? m_sfUserPtr->IsLocal() : false;
    }

    /**
     * User ID
     *
     * @return  uint32
     */
    uint32 Id()
    {
        return (m_sfUserPtr) ? m_sfUserPtr->Id() : 0;
    }

    /**
     * Convert the sfUser name to an FString
     *
     * @return  FString
     */
    FString Name()
    {
        return FString(UTF8_TO_TCHAR(m_sfUserPtr->Name().c_str()));
    }

    /**
     * Convert the sfUser color to a FLinearColor
     *
     * @return  FLinearColor
     */
    FLinearColor Color()
    {
        return FLinearColor(
            m_sfUserPtr->Color().R(),
            m_sfUserPtr->Color().G(),
            m_sfUserPtr->Color().B()
        );
    }

    /**
     * Refresh UI information.
     * Set the icon brush tint to the user color.
     */
    void Refresh()
    {
        m_iconBrushPtr->TintColor = Color();
    }

    /**
     * Get the icon brush.
     *
     * @return  const FSlateImageBrush&
     */
    const FSlateImageBrush& IconBrush()
    {
        return *m_iconBrushPtr;
    }

    /**
     * Is this user being followed by local camera?
     *
     * @param   bool isFollowed - is the user followed by the local camera
     */
    void SetIsFollowed(bool isFollowed)
    {
        if (m_isFollowed != isFollowed)
        {
            m_isFollowed = isFollowed;
            if (FollowButton.IsValid())
            {
                FollowButton->SetIsPressed(m_isFollowed, m_isFollowed ? "Unfollow" : "Follow");
            }
        }
    }

    TSharedPtr<sfUIToggleButton> FollowButton;

private:
    sfUser::SPtr m_sfUserPtr;
    FSlateImageBrush* m_iconBrushPtr;
    bool m_isFollowed;
};