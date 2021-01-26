#include "sfUI.h"
#include "sfUIStyles.h"
#include "sfUICommands.h"
#include "sfDetailsPanelManager.h"
#include "../Web/sfBaseWebService.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfConfig.h"
#include "../../Public/Translators/sfActorTranslator.h"
#include "../../Public/sfLoader.h"
#include "sfGettingStartedPanel.h"

#include <iostream>

#include <sfService.h>
#include <ksRoomInfo.h>
#include <LevelEditor.h>
#include <Framework/MultiBox/MultiBoxBuilder.h>
#include <Runtime/Core/Public/Misc/MessageDialog.h>
#include <Widgets/Docking/SDockTab.h>
#include <Runtime/Launch/Resources/Version.h>

#define LOG_CHANNEL "sfUI"

using namespace KS;
using namespace KS::SceneFusion2;

static const FName SceneFusionTabName("Scene Fusion");
static const FName MissingAssetsTabName("SF Missing Assets");
static const FName GettingStartedTabName("SF Getting Started");

void sfUI::Initialize()
{
    KS::Log::Info("Scene Fusion intializing UI.", LOG_CHANNEL);

    InitializeStyles();
    InitializeCommands();
    ExtendToolBar();
    RegisterSFTab();
    RegisterSFHandlers();

    m_missingAssetsPanel.RefreshMessageBox();

    m_outlinerManagerPtr = MakeShareable(new sfOutlinerManager);
    TSharedPtr<sfActorTranslator> actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(
        sfType::Actor);
    if (actorTranslatorPtr.IsValid())
    {
        actorTranslatorPtr->OnLockStateChange.AddRaw(m_outlinerManagerPtr.Get(), &sfOutlinerManager::SetLockState);
    }
}

void sfUI::Cleanup()
{
    KS::Log::Info("Scene Fusion cleanup UI.", LOG_CHANNEL);
    m_activeWidget = nullptr;
    m_panelSwitcherPtr.Reset();
    m_loginPanel.Hide();
    m_projectsPanel.Hide();
    m_sessionsPanel.Hide();
    m_onlinePanel.Hide();

    sfUIStyles::Shutdown();
    sfUICommands::Unregister();
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SceneFusionTabName);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MissingAssetsTabName);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GettingStartedTabName);
}

void sfUI::InitializeStyles()
{
    sfUIStyles::Initialize();
    sfUIStyles::ReloadTextures();
}

void sfUI::InitializeCommands()
{
    sfUICommands::Register();
    m_UICommandListPtr = MakeShareable(new FUICommandList);

    // Map Scene Fusion actions to UI commands
    m_UICommandListPtr->MapAction(
        sfUICommands::Get().ToolBarClickPtr,
        FExecuteAction::CreateLambda([this]() { FGlobalTabmanager::Get()->InvokeTab(SceneFusionTabName); }),
        FCanExecuteAction()
    );
    m_UICommandListPtr->MapAction(
        sfUICommands::Get().MissingAssetsPtr,
        FExecuteAction::CreateLambda([this]() { FGlobalTabmanager::Get()->InvokeTab(MissingAssetsTabName); }),
        FCanExecuteAction()
    );
    m_UICommandListPtr->MapAction(
        sfUICommands::Get().GettingStartedPtr,
        FExecuteAction::CreateLambda([this]() { FGlobalTabmanager::Get()->InvokeTab(GettingStartedTabName); }),
        FCanExecuteAction()
    );

    // Map the 'Web Console' menu entry to the action that opens the user's browser to the web console.
    m_UICommandListPtr->MapAction(
        sfUICommands::Get().WebConsolePtr,
        FExecuteAction::CreateLambda([this]()
        {
            FString consoleUrl = sfConfig::Get().WebURL;
            FString error = "";
            FPlatformProcess::LaunchURL(*consoleUrl, nullptr, &error);
            if (!error.IsEmpty())
            {
                KS::Log::Error(sfUnrealUtils::FToStdString(error), LOG_CHANNEL);
            }
        }),
        FCanExecuteAction()
    );

    // Map the 'Documentation' menu entry to the action that opens the user's browser to
    // the Scene Fusion documentation.
    m_UICommandListPtr->MapAction(
        sfUICommands::Get().DocumentPtr,
        FExecuteAction::CreateLambda([this]()
        {
            FString error = "";
            FPlatformProcess::LaunchURL(
                *FString("https://docs.kinematicsoup.com/SceneFusion/index.html"), nullptr, &error);
            if (!error.IsEmpty())
            {
                KS::Log::Error(sfUnrealUtils::FToStdString(error), LOG_CHANNEL);
            }
        }),
        FCanExecuteAction()
    );

    // Map the 'Email Support' menu entry to the action that opens a email form to
    // the Scene Fusion support email.
    m_UICommandListPtr->MapAction(
        sfUICommands::Get().EmailSupportPtr,
        FExecuteAction::CreateLambda([this]()
        {
            FString error = "";
            FPlatformProcess::LaunchURL(
                *FString("mailto:support@kinematicsoup.com"), nullptr, &error);
            if (!error.IsEmpty())
            {
                KS::Log::Error(sfUnrealUtils::FToStdString(error), LOG_CHANNEL);
            }
        }),
        FCanExecuteAction()
    );
}

// Add the tool bar button using a ui extender
void sfUI::ExtendToolBar()
{
    FLevelEditorModule& module = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    TSharedPtr<FExtender> extenderPtr = MakeShareable(new FExtender);
    extenderPtr->AddToolBarExtension(
        "Content",
        EExtensionHook::Position::After,
        m_UICommandListPtr,
        FToolBarExtensionDelegate::CreateRaw(this, &sfUI::OnExtendToolBar)
    );
    module.GetToolBarExtensibilityManager()->AddExtender(extenderPtr);
}

void sfUI::RegisterSFTab()
{
    FTabSpawnerEntry& tabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        SceneFusionTabName,
        FOnSpawnTab::CreateRaw(this, &sfUI::OnCreateSFTab)
    );
    tabSpawner.SetDisplayName(FText::FromName(SceneFusionTabName));
    tabSpawner.SetMenuType(ETabSpawnerMenuType::Hidden);

    FTabSpawnerEntry& tabSpawner2 = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        MissingAssetsTabName,
        FOnSpawnTab::CreateRaw(this, &sfUI::OnCreateMissingAssetsTab)
    );
    tabSpawner2.SetDisplayName(FText::FromName(MissingAssetsTabName));
    tabSpawner2.SetMenuType(ETabSpawnerMenuType::Hidden);

    FTabSpawnerEntry& tabSpawner3 = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        GettingStartedTabName,
        FOnSpawnTab::CreateRaw(this, &sfUI::OnCreateGettingStartedTab)
    );
    tabSpawner3.SetDisplayName(FText::FromName(GettingStartedTabName));
    tabSpawner3.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void sfUI::RegisterSFHandlers()
{
    m_connectEventPtr = SceneFusion::Service->RegisterOnConnectHandler(
        [this](sfSession::SPtr& sessionPtr, const std::string& errorMessage)
    {
        OnConnectComplete(sessionPtr, errorMessage);
    });
    m_disconnectEventPtr = SceneFusion::Service->RegisterOnDisconnectHandler(
        [this](sfSession::SPtr& sessionPtr, const std::string& errorMessage)
    {
        OnDisconnect(sessionPtr, errorMessage);
    });
}

void sfUI::RegisterUIHandlers()
{
    m_loginPanel.OnLogin.BindLambda([this]() {
        if (sfConfig::Get().ProjectId == -1) {
            ShowProjectsPanel();
        }
        else {
            ShowSessionsPanel();
        }
    });

    m_projectsPanel.OnLogout.BindLambda([this]() {
        SceneFusion::WebService->Logout(sfBaseWebService::OnLogoutDelegate::CreateLambda([this]() {
            ShowLoginPanel();
        }));
    });

    m_projectsPanel.OnSelectProject.BindRaw(this, &sfUI::ShowSessionsPanel);

    m_sessionsPanel.OnLogout.BindLambda([this]() {
        SceneFusion::WebService->Logout(sfBaseWebService::OnLogoutDelegate::CreateLambda([this]() {
            ShowLoginPanel(); 
        }));
    });

    m_sessionsPanel.OnSwitchProject.BindRaw(this, &sfUI::ShowProjectsPanel);
    
    m_sessionsPanel.OnStartSession.BindLambda([this](TSharedPtr<sfSessionInfo> sessionInfoPtr)
    {
        SceneFusion::IsSessionCreator = true;
        JoinSession(sessionInfoPtr);
    });

    m_sessionsPanel.OnJoinSession.BindLambda([this](TSharedPtr<sfSessionInfo> sessionInfoPtr)
    {
        SceneFusion::IsSessionCreator = false;
        JoinSession(sessionInfoPtr);
    });

    m_onlinePanel.OnLeaveSession.BindLambda([this]()
    {
        SceneFusion::Service->LeaveSession();
    });

    FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this](float time)->bool {
            if (SceneFusion::WebService->IsLoggedIn()) {
                SceneFusion::WebService->RefreshToken(
                    sfBaseWebService::OnRefreshTokenDelegate::CreateLambda([this](const FString& error) {
                        if (!error.IsEmpty()) {
                            KS::Log::Error("Error refreshing Scene Fusion authentication token: " +
                                std::string(TCHAR_TO_UTF8(*error)), LOG_CHANNEL);
                            m_sessionsPanel.OnLogout.Execute();
                        }
                    })
                );
            }
            return true; // returning true will reschedule this delegate.
        }),
        30.0f // delay period
     );
}

void sfUI::ShowLoginPanel()
{   
    m_loginPanel.Show();
    m_projectsPanel.Hide();
    m_sessionsPanel.Hide();
    m_onlinePanel.Hide();
    m_activeWidget = m_loginPanel.Widget();
    m_panelSwitcherPtr->SetActiveWidget(m_loginPanel.Widget());
}

void sfUI::ShowProjectsPanel()
{
    m_loginPanel.Hide();
    m_projectsPanel.Show();
    m_sessionsPanel.Hide();
    m_onlinePanel.Hide();
    m_activeWidget = m_projectsPanel.Widget();
    m_panelSwitcherPtr->SetActiveWidget(m_projectsPanel.Widget());
}

void sfUI::ShowSessionsPanel()
{
    m_loginPanel.Hide();
    m_projectsPanel.Hide();
    m_sessionsPanel.Show();
    m_onlinePanel.Hide();
    m_activeWidget = m_sessionsPanel.Widget();
    m_panelSwitcherPtr->SetActiveWidget(m_sessionsPanel.Widget());
}

void sfUI::ShowOnlinePanel()
{
    m_loginPanel.Hide();
    m_projectsPanel.Hide();
    m_sessionsPanel.Hide();
    m_onlinePanel.Show();
    m_activeWidget = m_onlinePanel.Widget();
    m_panelSwitcherPtr->SetActiveWidget(m_onlinePanel.Widget());
    m_onlinePanel.DisplayLog("", sfUIMessageBox::Icon::INFO);
    m_onlinePanel.HideUpgradeLink();
}

void sfUI::JoinSession(TSharedPtr<sfSessionInfo> sessionInfoPtr)
{
    FString version = "Unreal Engine " + FString(ENGINE_VERSION_STRING);
    std::string token(TCHAR_TO_UTF8(*sfConfig::Get().SFToken));
    std::string username(TCHAR_TO_UTF8(*sfConfig::Get().Name));
    std::string application(TCHAR_TO_UTF8(*version));
    SceneFusion::Service->JoinSession(sessionInfoPtr->RoomInfoPtr, token, username, application);
}

void sfUI::OnConnectComplete(sfSession::SPtr sessionPtr, const std::string& errorMessage)
{
    m_sessionsPanel.Enable();
    if (sessionPtr != nullptr)
    {
        // UI Session Event Registration
        m_userJoinEventPtr = sessionPtr->RegisterOnUserJoinHandler(
            [this](sfUser::SPtr value) { m_onlinePanel.AddUser(std::move(value)); }
        );
        
        m_userLeaveEventPtr = sessionPtr->RegisterOnUserLeaveHandler(
            [this](sfUser::SPtr value) { m_onlinePanel.RemoveUser(std::move(value)); }
        );

        m_userColorChangeEventPtr = sessionPtr->RegisterOnUserColorChangeHandler(
            [this](sfUser::SPtr value) { m_onlinePanel.UpdateUserColor(std::move(value)); }
        );

        m_onCreateStandInHandle = sfLoader::Get().OnCreateStandIn.AddLambda(
            [this](const FString& path, UObject* standInPtr)
        {
            m_missingAssetsPanel.AddMissingPath(path);
            RefreshMissingAssetsWarning();
        });

        m_onReplaceStandInHandle = sfLoader::Get().OnReplaceStandIn.AddLambda(
            [this](const FString& path, UObject* standInPtr)
        {
            m_missingAssetsPanel.RemoveMissingPath(path);
            RefreshMissingAssetsWarning();
        });

        m_onMissingAssetHandle = SceneFusion::MissingObjectManager->OnMissingAsset.AddLambda(
            [this](const FString& path)
        {
            m_missingAssetsPanel.AddMissingPath(path);
            RefreshMissingAssetsWarning();
        });

        m_onFoundAssetHandle = SceneFusion::MissingObjectManager->OnFoundMissingAsset.AddLambda(
            [this](const FString& path)
        {
            m_missingAssetsPanel.RemoveMissingPath(path);
            RefreshMissingAssetsWarning();
        });

        ShowOnlinePanel();
        SceneFusion::Get().OnConnect();
        m_outlinerManagerPtr->Initialize();
        sfDetailsPanelManager::Get().Initialize();
        m_missingAssetsPanel.Clear();
        RefreshMissingAssetsWarning();
    }
    else if (!errorMessage.empty())
    {
        m_sessionsPanel.DisplayMessage(FString(UTF8_TO_TCHAR(errorMessage.c_str())), sfUIMessageBox::ERROR);
    }
}

void sfUI::OnDisconnect(sfSession::SPtr sessionPtr, const std::string& errorMessage)
{
    sessionPtr->UnregisterOnUserJoinHandler(m_userJoinEventPtr);
    sessionPtr->UnregisterOnUserLeaveHandler(m_userLeaveEventPtr);
    sessionPtr->UnregisterOnUserColorChangeHandler(m_userColorChangeEventPtr);
    sfLoader::Get().OnCreateStandIn.Remove(m_onCreateStandInHandle);
    sfLoader::Get().OnReplaceStandIn.Remove(m_onReplaceStandInHandle);
    SceneFusion::MissingObjectManager->OnMissingAsset.Remove(m_onMissingAssetHandle);
    SceneFusion::MissingObjectManager->OnFoundMissingAsset.Remove(m_onFoundAssetHandle);

    ShowSessionsPanel();
    m_onlinePanel.ClearUsers();
    if (!errorMessage.empty())
    {
        m_sessionsPanel.DisplayMessage(FString(UTF8_TO_TCHAR(errorMessage.c_str())), sfUIMessageBox::ERROR);
    }
    SceneFusion::Get().CleanUp();
    m_outlinerManagerPtr->CleanUp();
    sfDetailsPanelManager::Get().CleanUp();
}

void sfUI::RefreshMissingAssetsWarning()
{
    if (m_missingAssetsPanel.NumMissingAssets() == 0)
    {
        m_onlinePanel.MissingAssetMessage()->SetVisibility(EVisibility::Collapsed);
    }
    else
    {
        FString str;
        if (m_missingAssetsPanel.NumMissingAssets() == 1)
        {
            str = "1 missing asset.";
        }
        else
        {
            str = FString::FromInt(m_missingAssetsPanel.NumMissingAssets()) + " missing assets.";
        }
        m_onlinePanel.MissingAssetMessage()->SetMessage(str, sfUIMessageBox::Icon::WARNING, 
            sfUIMessageBox::OnClickDelegate::CreateLambda([this]()
        {
            FGlobalTabmanager::Get()->InvokeTab(MissingAssetsTabName);
        }));
    }
}

void sfUI::HandleLog(KS::LogLevel level, const char* channel, const char* message)
{
    if (m_activeWidget == m_onlinePanel.Widget() && (IsInGameThread() || IsInSlateThread()))
    {
        FString str(UTF8_TO_TCHAR(message));
        switch (level)
        {
            case KS::LogLevel::LOG_WARNING:
            {
                m_onlinePanel.DisplayLog(str, sfUIMessageBox::Icon::WARNING);
                break;
            }
            case KS::LogLevel::LOG_ERROR:
            {
                m_onlinePanel.DisplayLog(str, sfUIMessageBox::Icon::ERROR);
                break;
            }
        }
    }
}

void sfUI::OnExtendToolBar(FToolBarBuilder& builder)
{
    builder.AddToolBarButton(sfUICommands::Get().ToolBarClickPtr);

    // TODO: Add drop down menu with other SF links 
    TAttribute<FText> label;
    label.Set(FText::FromString(TEXT("Scene Fusion Options")));
    builder.AddComboButton(
        FUIAction(),
        FOnGetContent::CreateRaw(this, &sfUI::OnCreateToolBarMenu),
        label,
        label,
        FSlateIcon(),
        true
    );
}

TSharedRef<SWidget> sfUI::OnCreateToolBarMenu()
{
    TSharedPtr<FExtender> extender = MakeShareable(new FExtender);
    FMenuBuilder builder(true, m_UICommandListPtr, extender);

    // Add menu bar commands
    builder.AddMenuEntry(
        sfUICommands::Get().MissingAssetsPtr,
        NAME_None,
        TAttribute<FText>(),
        TAttribute<FText>(),
        FSlateIcon()
    );
    builder.AddMenuEntry(
        sfUICommands::Get().GettingStartedPtr,
        NAME_None,
        TAttribute<FText>(),
        TAttribute<FText>(),
        FSlateIcon()
    );
    builder.AddMenuEntry(
        sfUICommands::Get().WebConsolePtr,
        NAME_None,
        TAttribute<FText>(),
        TAttribute<FText>(),
        FSlateIcon()
    );
    builder.AddMenuEntry(
        sfUICommands::Get().DocumentPtr,
        NAME_None,
        TAttribute<FText>(),
        TAttribute<FText>(),
        FSlateIcon()
    );
    builder.AddMenuEntry(
        sfUICommands::Get().EmailSupportPtr,
        NAME_None,
        TAttribute<FText>(),
        TAttribute<FText>(),
        FSlateIcon()
    );

    return builder.MakeWidget();
}

TSharedRef<SDockTab> sfUI::OnCreateSFTab(const FSpawnTabArgs& args)
{
    auto tab = SNew(SDockTab)
        .Icon(sfUIStyles::Get().GetBrush("SceneFusion.TabIcon"))
        .TabRole(NomadTab)
        [
            SAssignNew(m_panelSwitcherPtr, SWidgetSwitcher)
        ];
    RegisterUIHandlers();

    m_panelSwitcherPtr->AddSlot(0).AttachWidget(m_loginPanel.Widget());
    m_panelSwitcherPtr->AddSlot(1).AttachWidget(m_projectsPanel.Widget());
    m_panelSwitcherPtr->AddSlot(2).AttachWidget(m_sessionsPanel.Widget());
    m_panelSwitcherPtr->AddSlot(3).AttachWidget(m_onlinePanel.Widget());

    if (m_activeWidget.IsValid()) {
        m_panelSwitcherPtr->SetActiveWidget(m_activeWidget.ToSharedRef());
    }
    else {
        ShowLoginPanel();
        m_loginPanel.Authenticate();
    }

    return tab;
}

TSharedRef<SDockTab> sfUI::OnCreateMissingAssetsTab(const FSpawnTabArgs& args)
{
    auto tab = SNew(SDockTab)
        .Icon(sfUIStyles::Get().GetBrush("SceneFusion.TabIcon"))
        .TabRole(NomadTab)
        [
            SAssignNew(m_missingAssetsPtr, SWidgetSwitcher)
        ];

    m_missingAssetsPtr->AddSlot(0).AttachWidget(m_missingAssetsPanel.Widget());
    m_missingAssetsPtr->SetActiveWidget(m_missingAssetsPanel.Widget());
    return tab;
}

TSharedRef<SDockTab> sfUI::OnCreateGettingStartedTab(const FSpawnTabArgs& args)
{
    return sfGettingStartedPanel::CreateGettingStartedTab();
}


sfUIOnlinePanel::OnGoToDelegate& sfUI::OnGoToUser()
{
    return m_onlinePanel.OnGoTo;
}

sfUIOnlinePanel::OnFollowDelegate& sfUI::OnFollowUser()
{
    return m_onlinePanel.OnFollow;
}

void sfUI::UnfollowCamera()
{
    m_onlinePanel.UnfollowCamera();
}

void sfUI::ShowUpgradeLink(SceneFusion::RestrictedFeature feature)
{
    m_onlinePanel.ShowUpgradeLink(feature);
}

#undef LOG_CHANNEL