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

#include "sfUIPanel.h"
#include "sfUIUserRow.h"

#include "sfUser.h"

#include <Runtime/Slate/Public/Framework/SlateDelegates.h>

using namespace KS::SceneFusion2;

class sfUIOnlinePanel : public sfUIPanel
{
public:
    /**
     * Delegate for UI leave session
     */
    DECLARE_DELEGATE(OnLeaveSessionDelegate);

    /**
     * Delegate for go to camera.
     *
     * @param   uint32_t userId
     */
    DECLARE_DELEGATE_OneParam(OnGoToDelegate, uint32_t);

    /**
     * Delegate for follow camera.
     *
     * @param   uint32_t userId
     * @return  uint32_t id of user it is following. Return 0 if it is not following any user.
     */
    DECLARE_DELEGATE_RetVal_OneParam(uint32_t, OnFollowDelegate, uint32_t);

    /**
     * Constructor
     */
    sfUIOnlinePanel();

    /**
     * Leave session event handler
     */
    OnLeaveSessionDelegate OnLeaveSession;

    /**
     * Goto button click event handler.
     */
    OnGoToDelegate OnGoTo;

    /**
     * Follow button click event handler.
     */
    OnFollowDelegate OnFollow;

    /**
     * Add a user to the user list
     *
     * @param   sfUser::SPtr - user pointer
     */
    void AddUser(sfUser::SPtr userPtr);

    /**
     * Remove a user from the user list
     *
     * @param   sfUser::SPtr - user pointer
     */
    void RemoveUser(sfUser::SPtr userPtr);

    /**
     * Update the UI color for a user
     *
     * @param   sfUser::SPtr - user pointer
     */
    void UpdateUserColor(sfUser::SPtr userPtr);

    /**
     * Clear the user list.
     */
    void ClearUsers();

    /**
     * Unfollow camera.
     */
    void UnfollowCamera();

    /**
     * Displays a log message with a button to open the log console.
     *
     * @param   const FString& message to display.
     * @param   sfUIMessageBox::Icon icon to display.
     */
    void DisplayLog(const FString& message, sfUIMessageBox::Icon icon);

    /**
     * @return TSharedPtr<sfUIMessageBox> message box for displaying the number of missing assets.
     */
    TSharedPtr<sfUIMessageBox> MissingAssetMessage();

    /**
     * Shows a link to upgrade the user's tier.
     *
     * @param   SceneFusion::RestrictedFeature feature the user tried to use that was restricted.
     */
    void ShowUpgradeLink(SceneFusion::RestrictedFeature feature);

    /**
     * Hides the link to upgrade the user's tier.
     */
    void HideUpgradeLink();

private:
    TSharedPtr<SListView<TSharedPtr<sfUIUserInfo>>> m_userListPtr;
    TSharedPtr<sfUIMessageBox> m_logPtr;
    TSharedPtr<sfUIMessageBox> m_missingAssetMessagePtr;
    TSharedPtr<sfUIMessageBox> m_upgradePtr;
    TMap<uint32, TSharedPtr<sfUIUserInfo>> m_userMap;
    TSharedPtr<sfUIUserInfo> m_localUserPtr;
    TArray<TSharedPtr<sfUIUserInfo>> m_users;
    SceneFusion::RestrictedFeature m_restrictedFeature;
    bool m_showAvatar;

    /**
     * UI for session info.
     */
    void InfoArea();

    /**
     * UI for session preferences.
     */
    void PreferenceArea();

    /**
     * UI for session users.
     */
    void UserArea();

    /**
     * Goto another user's camera
     *
     * @param   TSharedPtr<sfSessionInfo> sessionPtr
     */
    void Goto(TSharedPtr<sfUIUserInfo> userInfoPtr);

    /**
     * Follow another user's camera
     *
     * @param   TSharedPtr<sfSessionInfo> sessionPtr
     */
    void Follow(TSharedPtr<sfUIUserInfo> userInfoPtr);

    /**
     * Handle a user color change action.
     *
     * @param   TSharedPtr<sfUserInfo> - UI user info
     * @param   FLinearColor - new color
     */
    void SetUserColor(TSharedPtr<sfUIUserInfo> userInfoPtr, FLinearColor color);

    /**
     * Handle the reply from the webserver for setting user color.
     *
     * @param   FLinearColor - new color
     * @param   FString - error message
     */
    void SetUserColorReply(FLinearColor color, const FString& error);

    /**
     * Handles show avatars checkbox change.
     *
     * @param   ECheckBoxState newCheckedState
     */
    void OnShowAvatarsCheckboxChanged(ECheckBoxState newCheckedState);
};