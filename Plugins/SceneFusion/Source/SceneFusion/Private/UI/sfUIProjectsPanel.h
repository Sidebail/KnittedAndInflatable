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
#include "../../Public/sfProjectInfo.h"
#include <Runtime/Slate/Public/Framework/SlateDelegates.h>
#include <Widgets/Input/SComboBox.h>
#include <Widgets/Input/SEditableTextBox.h>

/**
 * Scene Fusion sessions list
 */
class sfUIProjectsPanel : public sfUIPanel
{
public:
    /**
     * Delegate for UI logout
     */
    DECLARE_DELEGATE(OnLogoutDelegate);

    /**
     * Delegate for join/start sessions
     */
    DECLARE_DELEGATE(OnSelectProjectDelegate);

    /**
     * Constructor
     */
    sfUIProjectsPanel();

    /**
     * Destructor
     */
    virtual ~sfUIProjectsPanel();

    /**
     * Show the UI widget
     */
    virtual void Show() override;

    /**
     * Hide the UI widget
     */
    virtual void Hide() override;

    /**
     * Logout event handler
     */
    OnLogoutDelegate OnLogout;
    OnSelectProjectDelegate OnSelectProject;

private:
    // UI widgets
    TSharedPtr<SComboBox<TSharedPtr<FString>>> m_accountComboPtr;
    TSharedPtr<SListView<TSharedPtr<sfProjectInfo>>> m_projectsListPtr;
    TSharedPtr<STextBlock> m_selectedAccountPtr;
    TSharedPtr<sfUIMessageBox> m_newVersionPtr;

    // State
    TArray<TSharedPtr<FString>> m_accounts;
    TArray<TSharedPtr<sfProjectInfo>> m_projects;
    ProjectMap m_projectMap;

    /**
     * Shows the download link for the newest version of Scene Fusion.
     */
    void ShowDownloadLink();

    /**
     * Hides the download link for the newest version of Scene Fusion.
     */
    void HideDownloadLink();

    /**
     * Draw the project list
     */
    void DrawProjectList();

    /**
     * Handle a select project UI action
     *
     * @param   TSharedPtr<sfProjectInfo> - project information
     */
    void SelectProject(TSharedPtr<sfProjectInfo> valuePtr);

    /**
     * Handle the reply from a GetSessions web request
     *
     * @param   ProjectMap& - project information
     * @param   const FString& - error message
     */
    void GetProjectsReply(ProjectMap& projectMap, const FString& error);
};
