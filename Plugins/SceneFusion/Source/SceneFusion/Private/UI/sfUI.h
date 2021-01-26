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

#include "sfUIProjectsPanel.h"
#include "sfUISessionsPanel.h"
#include "sfUILoginPanel.h"
#include "sfUIOnlinePanel.h"
#include "sfMissingAssetsPanel.h"
#include "sfOutlinerManager.h"
#include "../../Public/sfSessionInfo.h"
#include <sfSession.h>
#include <ksEvent.h>
#include <CoreMinimal.h>
#include <Widgets/Layout/SWidgetSwitcher.h>

using namespace KS::SceneFusion2;

/**
 * Scene Fusion User Interface
 */
class sfUI : public TSharedFromThis<sfUI>
{
public:
    /**
     * Initialize the styles and UI components used by Scene Fusion.
     */
    void Initialize();

    /**
     * Clean up the styles and UI components used by Scene Fusion.
     */
    void Cleanup();

    /**
     * Gets delegate for go to camera.
     *
     * @return sfUIOnlinePanel::OnGoToDelegate&
     */
    sfUIOnlinePanel::OnGoToDelegate& OnGoToUser();
    
    /**
     * Gets delegate for follow camera.
     *
     * @return sfUIOnlinePanel::OnFollowDelegate&
     */
    sfUIOnlinePanel::OnFollowDelegate& OnFollowUser();

    /**
     * Unfollows camera.
     */
    void UnfollowCamera();

    /**
     * Shows a link in the UI to upgrade the user's account.
     *
     * @param   SceneFusion::RestrictedFeature feature the user tried to use that was restricted.
     */
    void ShowUpgradeLink(SceneFusion::RestrictedFeature feature);

    /**
     * Connects to a session.
     *
     * @param   TSharedPtr<sfSessionInfo> sessionInfoPtr - determines where to connect.
     */
    void JoinSession(TSharedPtr<sfSessionInfo> sessionInfoPtr);

    /**
     * Displays a log in the online panel.
     *
     * @param   KS::LogLevel level
     * @param   const char* channel that was logged to.
     * @param   const char* message
     */
    void HandleLog(KS::LogLevel level, const char* channel, const char* message);

private:
    // Commands
    TSharedPtr<class FUICommandList> m_UICommandListPtr;

    // UI components
    TSharedPtr<SWidgetSwitcher> m_panelSwitcherPtr;
    TSharedPtr<SWidget> m_activeWidget;
    TSharedPtr<SWidgetSwitcher> m_missingAssetsPtr;
    sfUIProjectsPanel m_projectsPanel;
    sfUISessionsPanel m_sessionsPanel;
    sfUIOnlinePanel m_onlinePanel;
    sfUILoginPanel m_loginPanel;
    sfMissingAssetsPanel m_missingAssetsPanel;

    // Event pointers
    KS::ksEvent<sfSession::SPtr&, const std::string&>::SPtr m_connectEventPtr;
    KS::ksEvent<sfSession::SPtr&, const std::string&>::SPtr m_disconnectEventPtr;
    KS::ksEvent<sfUser::SPtr&>::SPtr m_userJoinEventPtr;
    KS::ksEvent<sfUser::SPtr&>::SPtr m_userLeaveEventPtr;
    KS::ksEvent<sfUser::SPtr&>::SPtr m_userColorChangeEventPtr;
    FDelegateHandle m_onCreateStandInHandle;
    FDelegateHandle m_onReplaceStandInHandle;
    FDelegateHandle m_onMissingAssetHandle;
    FDelegateHandle m_onFoundAssetHandle;

    TSharedPtr<sfOutlinerManager> m_outlinerManagerPtr;

    /**
     * Initialize styles.
     */
    void InitializeStyles();

    /**
     * Initialise commands.
     */
    void InitializeCommands();

    /**
     * Extend the level editor tool bar with a SF button
     */
    void ExtendToolBar();

    /**
     * Register a SF Tab panel with a tab spawner.
     */
    void RegisterSFTab();

    /**
     * Register Scene Fusion event handlers.
     */
    void RegisterSFHandlers();

    /**
     * Register UI event handlers
     */
    void RegisterUIHandlers();

    /**
     * Show the login panel, and hide other panels.
     */
    void ShowLoginPanel();

    /**
     * Show the projects panel, and hide other panels.
     */
    void ShowProjectsPanel();

    /**
     * Show the sessions panel, and hide other panels.
     */
    void ShowSessionsPanel();

    /**
     * Show the online panel, and hide other panels.
     */
    void ShowOnlinePanel();

    /**
     * Called when a connection attempt completes.
     *
     * @param   sfSession::SPtr sessionPtr we connected to. nullptr if the connection failed.
     * @param   const std::string& errorMessage. Empty string if the connection was successful.
     */
    void OnConnectComplete(sfSession::SPtr sessionPtr, const std::string& errorMessage);

    /**
     * Called when we disconnect from a session.
     *
     * @param   sfSession::SPtr sessionPtr we disconnected from.
     * @param   const std::string& errorMessage
     */
    void OnDisconnect(sfSession::SPtr sessionPtr, const std::string& errorMessage);

    /**
     * Create the widgets used in the toolbar.
     *
     * @param   FToolBarBuilder& - toolbar builder
     */
    void OnExtendToolBar(FToolBarBuilder& builder);

    /**
     * Updates the missing assets warning in the online panel with the number of missing assets, or hides the message
     * if there are no missing assets.
     */
    void RefreshMissingAssetsWarning();

    /**
     * Create tool bar menu.
     * 
     * @return  TSharedRef<SWidget>
     */
    TSharedRef<SWidget> OnCreateToolBarMenu();

    /**
     * Create the Scene Fusion tab.
     * 
     * @param   const FSpawnTabArgs& - tab spawning arguments
     * @return  TSharedRef<SDockTab>
     */
    TSharedRef<SDockTab> OnCreateSFTab(const FSpawnTabArgs& args);

    /**
     * Create the Missing Assets tab.
     *
     * @param   const FSpawnTabArgs& - tab spawning arguments
     * @return  TSharedRef<SDockTab>
     */
    TSharedRef<SDockTab> OnCreateMissingAssetsTab(const FSpawnTabArgs& args);

    /**
     * Create the Getting Started tab.
     *
     * @param   const FSpawnTabArgs& - tab spawning arguments
     * @return  TSharedRef<SDockTab>
     */
    TSharedRef<SDockTab> OnCreateGettingStartedTab(const FSpawnTabArgs& args);
};