#include "sfUISessionsPanel.h"
#include "sfUISessionRow.h"
#include "../Web/sfBaseWebService.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfConfig.h"
#include "../../Public/Translators/sfActorTranslator.h"

#include <Editor.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SHyperlink.h>
#include <Widgets/Layout/SExpandableArea.h>
#include <HAL/PlatformFilemanager.h>
#include <HAL/PlatformProcess.h>
#include <Misc/FileHelper.h>
#include <HAL/Platform.h>
#if PLATFORM_WINDOWS
#include <Windows/WindowsPlatformMisc.h>
#endif

#define LOG_CHANNEL "sfUISessionsPanel"
#define SF_EXTERNAL "KinematicSoup/SceneFusion/"
#define LAN_SERVER_FOLDER SF_EXTERNAL "server/"
#define LAN_SERVER_EXECUTEBLE "Reactor.exe"
#define LAN_CONFIG "config.json"
#define CONFIG_JSON "sf_config.json"
#define SCENE_JSON "sf_scene.json"

sfUISessionsPanel::sfUISessionsPanel() : sfUIPanel("Sessions")
{
    m_socket = nullptr;
    m_socketReceiverPtr = nullptr;
    m_lanServerState = MISSING;

    InitializeUDP();
    AccountInfoArea();
    OnlineArea();
    LANArea();
    
    // Refresh session list periodically
    FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this](float time)->bool {
            RedrawSessionsList();
            if (m_widgetPtr.IsValid() && m_widgetPtr->GetVisibility() == EVisibility::Visible) {
                SceneFusion::WebService->GetSessions(
                    sfBaseWebService::OnGetSessionsDelegate::CreateRaw(this, &sfUISessionsPanel::GetSessionsReply)
                );

                m_lanServerState = ServerCheck();
            }
            return true; // returning true will reschedule this delegate.
        }),
        3.0f // delay in seconds
    );
}

sfUISessionsPanel::~sfUISessionsPanel()
{
    CleanupUDP();
}

void sfUISessionsPanel::AccountInfoArea()
{
    m_contentPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 2).AutoHeight()
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 0).AutoWidth()
        [
            SNew(SButton).HAlign(HAlign_Center)
            .Text(FText::FromString("Logout"))
            .OnClicked(FOnClicked::CreateLambda([this]()->FReply {
                OnLogout.Execute();
                return FReply::Handled();
            }))
        ]
        + SHorizontalBox::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 0).AutoWidth()
        [
            SNew(SButton)
            .Text(FText::FromString("Switch Project"))
            .OnClicked(FOnClicked::CreateLambda([this]()->FReply {
                OnSwitchProject.ExecuteIfBound();
                return FReply::Handled();
            }))
        ]
    ];
}

void sfUISessionsPanel::OnlineArea()
{
    // New version download
    m_contentPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 10, 5, 5).AutoHeight()
    [
        SAssignNew(m_newVersionPtr, sfUIMessageBox)
    ];

    m_contentPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 10, 5, 5).AutoHeight()
    [
        SAssignNew(m_limitMessagePtr, sfUIMessageBox)
    ];
    
    m_contentPtr->AddSlot().HAlign(HAlign_Fill).VAlign(VAlign_Center).Padding(5, 10, 5, 5).AutoHeight()
    [
        // Online Session Section
        SNew(SExpandableArea)
        .AreaTitle_Lambda([this]() -> FText {
            return FText::FromString(FString::Printf(TEXT("Online Sessions (%d)"), m_sessions.Num()));
        })
        .InitiallyCollapsed(false)
        .BodyContent()
        [
            // Start Online Session Button
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
            [
                SNew(SBox)
                .HAlign(HAlign_Left)
                .WidthOverride(150)
                [
                    SNew(SButton)
                    .HAlign(HAlign_Center)
                    .Text(FText::FromString("Start Online Session"))
                    .OnClicked(FOnClicked::CreateRaw(this, &sfUISessionsPanel::StartSession))
                ]
            ]

            // Online Sessions List
            + SVerticalBox::Slot().AutoHeight()
            [
                SAssignNew(m_sessionListPtr, SListView<TSharedPtr<sfSessionInfo>>)
                .ItemHeight(24)
                .ListItemsSource(&m_sessions)
                .OnGenerateRow(TSlateDelegates<TSharedPtr<sfSessionInfo>>::FOnGenerateRow::CreateLambda(
                    [this](TSharedPtr<sfSessionInfo> sessionInfoPtr,
                        const TSharedRef<STableViewBase>& owner)->TSharedRef<ITableRow> {
                            return SNew(sfUISessionRow, owner)
                            .Item(sessionInfoPtr)
                            .IsEnabled_Lambda([this, sessionInfoPtr]() -> bool {
                            return !m_stoppingRoomIds.Contains(sessionInfoPtr->RoomInfoPtr->Id()); })
                            .OnClickedJoin(
                                sfUISessionRow::OnJoinSession::CreateRaw(this, &sfUISessionsPanel::JoinSession))
                            .OnClickedStop(
                                sfUISessionRow::OnStopSession::CreateRaw(this, &sfUISessionsPanel::StopSession));
                        }
                    ))
                .SelectionMode(ESelectionMode::None)
                .HeaderRow
                (
                    SNew(SHeaderRow)
                    + SHeaderRow::Column("Project").DefaultLabel(FText::FromString("Project"))
                    + SHeaderRow::Column("Level").DefaultLabel(FText::FromString("Level"))
                    + SHeaderRow::Column("Creator").DefaultLabel(FText::FromString("Creator"))
                    + SHeaderRow::Column("Action").DefaultLabel(FText::FromString(""))
                )
            ]
        ]
    ];
}

void sfUISessionsPanel::LANArea()
{
    // LAN Session Section
    m_contentPtr->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(5, 5, 5, 10).AutoHeight()
    [
        SNew(SBox)
        .Visibility_Lambda([this]() -> EVisibility {
            return (m_projectMap.Contains(sfConfig::Get().ProjectId) && m_projectMap[sfConfig::Get().ProjectId]->LANEnabled) ?
                EVisibility::Collapsed :
                EVisibility::Visible;
        })
        [
            SNew(SHyperlink)
            .Text(FText::FromString("Contact support to learn how to enable LAN servers for this project"))
            .OnNavigate(FSimpleDelegate::CreateLambda([]() {
                FString error = "";
                FPlatformProcess::LaunchURL(
                    *FString("https://www.kinematicsoup.com/contact-us-ent"), nullptr, &error);
            }))
        ]
    ];
    m_contentPtr->AddSlot().HAlign(HAlign_Fill).VAlign(VAlign_Center).Padding(5, 10, 5, 5).AutoHeight()
    [
        SNew(SExpandableArea)
        .Visibility_Lambda([this]() -> EVisibility {
            return (m_projectMap.Contains(sfConfig::Get().ProjectId) && m_projectMap[sfConfig::Get().ProjectId]->LANEnabled) ?
                EVisibility::Visible :
                EVisibility::Collapsed;
        })
        .AreaTitle_Lambda([this]() -> FText {
            return FText::FromString(FString::Printf(TEXT("LAN Sessions (%d)"), m_lanSessions.Num()));
        })
        .InitiallyCollapsed(false)
        .BodyContent()
        [
            SNew(SVerticalBox)

            // Install LAN Server Button
            + SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)
            [
                SNew(SBox)
                .Visibility_Lambda([this]() -> EVisibility {
                    return m_lanServerState != OK ? EVisibility::Visible : EVisibility::Collapsed;
                })
                .WidthOverride(200)
                [
                    SNew(SButton)
                    .HAlign(HAlign_Center)
                    .Text(FText::FromString("Install Scene Fusion LAN Server"))
                    .OnClicked(FOnClicked::CreateRaw(this, &sfUISessionsPanel::InstallLANServer))
                ]
            ]

            // Start LAN Session Button
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
            [
                SNew(SHorizontalBox)
                .Visibility_Lambda([this]() -> EVisibility {
                    return m_lanServerState == OK ? EVisibility::Visible : EVisibility::Collapsed;
                })
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox)
                    .WidthOverride(120)
                    [
                        SNew(SButton)
                        .HAlign(HAlign_Center)
                        .Text(FText::FromString("Start LAN Session"))
                        .OnClicked(FOnClicked::CreateRaw(this, &sfUISessionsPanel::StartLANSession))
                    ]
                ]
                + SHorizontalBox::Slot().Padding(10, 0).VAlign(VAlign_Center).AutoWidth()
                [
                   SNew(STextBlock)
                    .Text(FText::FromString("Port"))
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox)
                    .WidthOverride(50)
                    [
                        SAssignNew(m_portPtr, SEditableTextBox)
                        .Text(FText::FromString("8000"))
                    ]
                ]
            ]

            // LAN Sessions List
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
            [
                SNew(SBox)
                [
                    SAssignNew(m_lanSessionListPtr, SListView<TSharedPtr<sfSessionInfo>>)
                    .ItemHeight(24)
                    .ListItemsSource(&m_lanSessions)
                    .OnGenerateRow(TSlateDelegates<TSharedPtr<sfSessionInfo>>::FOnGenerateRow::CreateLambda(
                        [this](TSharedPtr<sfSessionInfo> sessionInfoPtr,
                            const TSharedRef<STableViewBase>& owner)->TSharedRef<ITableRow> {
                                return SNew(sfUISessionRow, owner)
                                .Item(sessionInfoPtr)
                                .IsEnabled_Lambda([this, sessionInfoPtr]() -> bool {
                                return !m_stoppingRoomIds.Contains(sessionInfoPtr->RoomInfoPtr->Id()); })
                                .OnClickedJoin(
                                    sfUISessionRow::OnJoinSession::CreateRaw(this, &sfUISessionsPanel::JoinSession))
                                .OnClickedStop(
                                    sfUISessionRow::OnStopSession::CreateRaw(this, &sfUISessionsPanel::StopSession));
                            }
                        ))
                    .SelectionMode(ESelectionMode::None)
                    .HeaderRow
                    (
                        SNew(SHeaderRow)
                        + SHeaderRow::Column("Project").DefaultLabel(FText::FromString("Project"))
                        + SHeaderRow::Column("Level").DefaultLabel(FText::FromString("Level"))
                        + SHeaderRow::Column("Creator").DefaultLabel(FText::FromString("Creator"))
                        + SHeaderRow::Column("Action").DefaultLabel(FText::FromString(""))
                    )
                ]
            ]
            
            // Manual LAN Connect
            + SVerticalBox::Slot()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(120)
                    [
                        SNew(SButton)
                        .HAlign(HAlign_Center)
                        .Text(FText::FromString("Manual Connect"))
                        .OnClicked(FOnClicked::CreateRaw(this, &sfUISessionsPanel::ManualConnect))
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(10, 0).HAlign(HAlign_Left).VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(50)
                    [
                        SNew(STextBlock).Text(FText::FromString("Address"))
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(100)
                    [
                        SAssignNew(m_manualAddressPtr, SEditableTextBox).Text(FText::FromString(""))
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(10, 0).HAlign(HAlign_Left).VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .HAlign(HAlign_Center)
                    .WidthOverride(30)
                    [
                        SNew(STextBlock).Text(FText::FromString("Port"))
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(50)
                    [
                        SAssignNew(m_manualPortPtr, SEditableTextBox).Text(FText::FromString("8000"))
                    ]
                ]
            ]
        ]
    ];
}

void sfUISessionsPanel::Show()
{
    Enable();
    if (m_widgetPtr.IsValid()) {
        if (m_projects.Num() == 0) {
            DisplayMessage("Fetching sessions...", sfUIMessageBox::INFO);
        }
        else {
            DisplayMessage("", sfUIMessageBox::INFO);
        }
        m_widgetPtr->SetVisibility(EVisibility::Visible);

        m_lanServerState = ServerCheck();
        m_limitMessagePtr->SetVisibility(EVisibility::Collapsed);
        
        FString version = SceneFusion::Get().WebService->LatestVersion();
        if (!version.IsEmpty() && version != SceneFusion::Version()) {
            ShowDownloadLink();
        }
        else {
            HideDownloadLink();
        }

        SceneFusion::WebService->GetSessions(
            sfBaseWebService::OnGetSessionsDelegate::CreateRaw(this, &sfUISessionsPanel::GetSessionsReply)
        );
    }
}

void sfUISessionsPanel::Hide()
{
    if (m_widgetPtr.IsValid()) {
        m_widgetPtr->SetVisibility(EVisibility::Hidden);
    }
}

void sfUISessionsPanel::ShowDownloadLink()
{
    if (m_newVersionPtr.IsValid()) {
        m_newVersionPtr->SetMessage(
            "Get Scene Fusion Version " + SceneFusion::Get().WebService->LatestVersion(),
            sfUIMessageBox::Icon::INFO, 
            sfUIMessageBox::OnClickDelegate::CreateLambda([this]() {
                FPlatformProcess::LaunchURL(
                *FString("https://www.kinematicsoup.com/scene-fusion/download"), nullptr, nullptr);
            })
        );
        m_newVersionPtr->SetVisibility(EVisibility::Visible);
    }
}

void sfUISessionsPanel::HideDownloadLink()
{
    if (m_newVersionPtr.IsValid()) {
        m_newVersionPtr->SetVisibility(EVisibility::Collapsed);
    }
}

void sfUISessionsPanel::JoinSession(TSharedPtr<sfSessionInfo> sessionInfoPtr)
{
    if (sessionInfoPtr.IsValid()) {
        DisplayMessage("Joining session...", sfUIMessageBox::INFO);
        Disable();
        OnJoinSession.ExecuteIfBound(sessionInfoPtr);
    }
}

FReply sfUISessionsPanel::StartSession()
{
    int objectLimit = m_projectMap.Contains(sfConfig::Get().ProjectId) ?
        m_projectMap[sfConfig::Get().ProjectId]->ObjectLimit : 0;
    if (objectLimit >= 0)
    {
        // Check if there are multiple levels
        if (GEditor->GetEditorWorldContext().World()->GetLevels().Num() > 1)
        {
            m_limitMessagePtr->SetMessage("You have multiple levels loaded which is not allowed on your account.",
                "Upgrade your account to enable multi-level sessions.", sfUIMessageBox::ERROR,
                sfUIMessageBox::OnClickDelegate::CreateLambda([this]()
            {
                FString error = "";
                FPlatformProcess::LaunchURL(
                    *FString("https://www.kinematicsoup.com/scene-fusion/pricing"), nullptr, &error);
            }));
            return FReply::Handled();
        }
        // Check the actor limit
        int numActors = 0;
        TSharedPtr<sfActorTranslator> translatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(
            sfType::Actor);
        for (AActor* actorPtr : GEditor->GetEditorWorldContext().World()->PersistentLevel->Actors)
        {
            if (translatorPtr->IsSyncable(actorPtr))
            {
                numActors++;
            }
        }
        if (numActors > objectLimit)
        {
            m_limitMessagePtr->SetMessage("Your level has " + FString::FromInt(numActors) + 
                " actors which exceeds the " + FString::FromInt(objectLimit) +
                " actor limit.", "Upgrade your account to remove this limit.", sfUIMessageBox::ERROR,
                sfUIMessageBox::OnClickDelegate::CreateLambda([this]()
            {
                FString error = "";
                FPlatformProcess::LaunchURL(
                    *FString("https://www.kinematicsoup.com/scene-fusion/pricing"), nullptr, &error);
            }));
            return FReply::Handled();
        }
    }
    m_limitMessagePtr->SetVisibility(EVisibility::Collapsed);

    DisplayMessage("Starting session...", sfUIMessageBox::INFO);
    Disable();
    FString sessionName = FString(FApp::GetProjectName()) + ";" +
        GEditor->GetEditorWorldContext().World()->GetMapName();
    // TODO: Add custom session name
    // TODO: Make sure it is less than 255 characters
    SceneFusion::WebService->StartSession(
        sfConfig::Get().ProjectId,
        sessionName,
        sfBaseWebService::OnStartSessionDelegate::CreateRaw(this, &sfUISessionsPanel::StartSessionReply)
    );
    return FReply::Handled();
}

void sfUISessionsPanel::StartSessionReply(TSharedPtr<sfSessionInfo> sessionInfoPtr, const FString& error)
{
    Enable();
    if (!error.IsEmpty()) {
        DisplayMessage(error, sfUIMessageBox::ERROR);
        return;
    }

    if (sessionInfoPtr.IsValid())
    {
        OnStartSession.ExecuteIfBound(sessionInfoPtr);
    }
}

void sfUISessionsPanel::StopSession(TSharedPtr<sfSessionInfo> sessionInfoPtr)
{
    if (sessionInfoPtr.IsValid()) {
        FText title = FText::FromString("Confirm Stop Session");
        FText message = FText::FromString(
            "Stop the session " + sessionInfoPtr->LevelName + " launched by " + sessionInfoPtr->Creator + "?");
        EAppReturnType::Type result = FMessageDialog::Open(EAppMsgType::Type::OkCancel, message, &title);
        if (result == EAppReturnType::Ok)
        {
            DisplayMessage("Stoping session...", sfUIMessageBox::INFO);
            m_stoppingRoomIds.Add(sessionInfoPtr->RoomInfoPtr->Id());
            SceneFusion::WebService->StopSession(sessionInfoPtr,
                sfBaseWebService::OnStopSessionDelegate::CreateRaw(this, &sfUISessionsPanel::StopSessionReply));
        }
    }
}

void sfUISessionsPanel::StopSessionReply(TSharedPtr<sfSessionInfo> sessionInfoPtr, const FString& errorMessage)
{
    m_stoppingRoomIds.Remove(sessionInfoPtr->RoomInfoPtr->Id());
    if (!errorMessage.IsEmpty())
    {
        DisplayMessage(errorMessage, sfUIMessageBox::ERROR);
    }
    else
    {
        ClearMessage();
        TSharedPtr<sfProjectInfo> projectInfoPtr = nullptr;
        if (m_projectMap.Contains(sfConfig::Get().ProjectId))
        {
            projectInfoPtr = m_projectMap[sfConfig::Get().ProjectId];
        }

        if (projectInfoPtr.IsValid() && projectInfoPtr->Sessions == m_sessions) // check that our data isn't stale
        {
            TArray<TSharedPtr<sfSessionInfo>> sessionList(m_sessions);
            for (int i = 0; i < sessionList.Num(); i++)
            {
                if (sessionList[i]->RoomInfoPtr->Id() == sessionInfoPtr->RoomInfoPtr->Id())
                {
                    sessionList.RemoveAt(i);
                    projectInfoPtr->Sessions = sessionList;

                    // Update the sessions list
                    RedrawSessionsList();
                    break;
                }
            }
        }
    }
}

void sfUISessionsPanel::GetSessionsReply(ProjectMap& projectMap, const FString& error)
{
    if (!error.IsEmpty()) {
        DisplayMessage(error, sfUIMessageBox::WARNING);
        if (error == "Unexpected get sessions error.")
        {
            DisplayMessage("Logging out...", sfUIMessageBox::INFO);
            Disable();
            OnLogout.Execute();
        }
        return;
    }

    bool changed = false;
    if (m_projectMap.Num() != projectMap.Num())
    {
        changed = true;
    }
    else
    {
        for (auto& pair : m_projectMap) {
            if (!projectMap.Contains(pair.Key) || !(*projectMap[pair.Key] == *pair.Value))
            {
                changed = true;
                break;
            }
        }
    }

    if (changed)
    {
        // Replace the project map
        std::swap(m_projectMap, projectMap);

        // Clear the selected project
        int projectId = sfConfig::Get().ProjectId;
        if (projectId == -1 || !m_projectMap.Contains(projectId)) {
            OnSwitchProject.ExecuteIfBound();
            return;
        }

        // Rebuild the project list
        m_projects.Empty();
        if (m_projectMap.Num() > 0) {
            for (auto& pair : m_projectMap) {
                m_projects.Add(pair.Value);
            }
        }

        // Update the sessions list
        RedrawSessionsList();
    }
}

void sfUISessionsPanel::RedrawSessionsList()
{
    m_sessions.Empty();
    int projectId = sfConfig::Get().ProjectId;
    if (projectId != -1 && m_projectMap.Num() > 0  && m_projectMap.Contains(projectId)) {
        m_sessions.Append(m_projectMap[projectId]->Sessions);
    }

    m_lanSessions.Empty();
    TArray<uint32> removals;
    for (auto& lanSessionPair : m_lanSessionMap)
    {
        if (lanSessionPair.Value->Time < FDateTime::Now() - FTimespan::FromSeconds(5))
        {
            m_lanSessions.Add(lanSessionPair.Value);
            removals.Add(lanSessionPair.Key);
        }
    }
    for (auto& key : removals)
    {
        m_lanSessionMap.Remove(key);
    }

    m_sessionListPtr->RequestListRefresh();
    m_lanSessionListPtr->RequestListRefresh();
}

void sfUISessionsPanel::InitializeUDP()
{
    // BUFFER SIZE
    m_socket = FUdpSocketBuilder(TEXT("SF2 Socket"))
        .AsNonBlocking()
        .AsReusable()
        .WithBroadcast()
        .BoundToAddress(FIPv4Address::Any)
        .BoundToEndpoint(FIPv4Endpoint::Any)
        .BoundToPort(8000)
        .WithReceiveBufferSize(1024);

    m_socketReceiverPtr = new FUdpSocketReceiver(m_socket, FTimespan::FromMilliseconds(1000), TEXT("SF2 Receiver"));
    m_socketReceiverPtr->OnDataReceived().BindRaw(this, &sfUISessionsPanel::OnBroadcastRecieved);
    m_socketReceiverPtr->Start();
}

void sfUISessionsPanel::CleanupUDP()
{
    if (m_socketReceiverPtr)
    {
        m_socketReceiverPtr->Stop();
        delete m_socketReceiverPtr;
        m_socketReceiverPtr = nullptr;
    }

    if (m_socket)
    {
        m_socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(m_socket);
    }
}

void sfUISessionsPanel::OnBroadcastRecieved(const FArrayReaderPtr& ArrayReaderPtr, const FIPv4Endpoint& EndPt)
{
    std::string temp;
    uint8* data = ArrayReaderPtr->GetData();

    TSharedPtr<sfSessionInfo> info = MakeShareable(new sfSessionInfo);
    info->RoomInfoPtr = ksRoomInfo::Create();
    info->RoomInfoPtr->Id() = *reinterpret_cast<uint32*>(ArrayReaderPtr->GetData());
    info->RoomInfoPtr->Port() = *reinterpret_cast<uint16*>(ArrayReaderPtr->GetData() + 4);
    info->RoomInfoPtr->Scene() = "Scene Fusion";
    info->RoomInfoPtr->Host() = std::string(TCHAR_TO_UTF8(*EndPt.Address.ToString()));
    info->RoomInfoPtr->Type() = "Scene Fusion";

    int offset = 6;
    uint8 len = ArrayReaderPtr->GetData()[offset++];
    temp.assign(reinterpret_cast<char const*>(data + offset), (size_t)len);
    info->ProjectName = FString(temp.c_str());
    offset += len;

    len = ArrayReaderPtr->GetData()[offset++];
    temp.assign(reinterpret_cast<char const*>(data + offset), (size_t)len);
    FString str = FString(temp.c_str());
    TArray<FString> strArray;
    if (str.ParseIntoArray(strArray, TEXT(";"), true) > 1)
    {
        info->UnrealProjectName = FString(strArray[0]);
        info->LevelName = FString(strArray[1]);
    }
    else
    {
        info->LevelName = str;
    }
    offset += len;

    len = ArrayReaderPtr->GetData()[offset++];
    temp.assign(reinterpret_cast<char const*>(data + offset), (size_t)len);
    info->Creator = FString(temp.c_str());
    offset += len;

    len = ArrayReaderPtr->GetData()[offset++];
    temp.assign(reinterpret_cast<char const*>(data + offset), (size_t)len);
    info->RequiredVersion = FString(temp.c_str());
    offset += len;
    
    len = ArrayReaderPtr->GetData()[offset++];
    temp.assign(reinterpret_cast<char const*>(data + offset), (size_t)len);
    info->LaunchApplication = FString(temp.c_str());
    offset += len;

    // Update session list
    if (m_lanSessionMap.Contains(info->RoomInfoPtr->Id())) {
        m_lanSessionMap[info->RoomInfoPtr->Id()] = info;
    }
    else {
        m_lanSessionMap.Add(info->RoomInfoPtr->Id(), info);
    }
}

FReply sfUISessionsPanel::StartLANSession()
{
    DisplayMessage("Starting LAN session...", sfUIMessageBox::INFO);
    Disable();

    // Construct paths
    FString serverPath = FPaths::ProjectDir() + LAN_SERVER_FOLDER;
    FString gameJsonPath = CONFIG_JSON;
    FString sceneJsonPath = SCENE_JSON;
    FString logFile = "sf_" + FDateTime::UtcNow().ToString() + ".log";
    if (!FPaths::FileExists(serverPath + LAN_SERVER_EXECUTEBLE))
    {
        KS::Log::Error("Failed to start LAN server. Could not find '"
            + std::string(TCHAR_TO_UTF8(*serverPath)) + LAN_SERVER_EXECUTEBLE + "'", LOG_CHANNEL);
        return FReply::Handled();
    }
    if (!FPaths::FileExists(serverPath + gameJsonPath) || !FPaths::FileExists(serverPath + sceneJsonPath))
    {
        KS::Log::Error("Failed to start LAN server. Could not find '" +
            std::string(TCHAR_TO_UTF8(*gameJsonPath)) +
            "' or '" + std::string(TCHAR_TO_UTF8(*sceneJsonPath)) + "'", LOG_CHANNEL);
        return FReply::Handled();
    }

    // Construct session info
    ksRoomInfo::SPtr roomInfoPtr = ksRoomInfo::Create();
    roomInfoPtr->Host() = "localhost";
    roomInfoPtr->Port() = (uint16_t)FCString::Atoi(*m_portPtr->GetText().ToString());
    roomInfoPtr->Scene() = "Scene Fusion";
    roomInfoPtr->Id() = (uint32_t)FDateTime::Now().GetTicks();

    int projectId = sfConfig::Get().ProjectId;

    TSharedPtr<sfSessionInfo> sessionInfoPtr = MakeShareable(new sfSessionInfo());
    sessionInfoPtr->RoomInfoPtr = roomInfoPtr;
    sessionInfoPtr->ProjectId = projectId;
    if (m_projectMap.Contains(projectId))
    {
        sessionInfoPtr->ProjectName = m_projectMap[projectId]->Name;
    }
    sessionInfoPtr->UnrealProjectName = FString(FApp::GetProjectName());
    sessionInfoPtr->LevelName = GEditor->GetEditorWorldContext().World()->GetMapName();
    sessionInfoPtr->Creator = FString(sfConfig::Get().Name);

    // Write LAN config
    TSharedPtr<FJsonObject> jsonPtr = MakeShareable(new FJsonObject);
    jsonPtr->SetNumberField("projectId", projectId);
    jsonPtr->SetStringField("projectName", sessionInfoPtr->ProjectName);
    jsonPtr->SetStringField("scene", sessionInfoPtr->UnrealProjectName + ";" + sessionInfoPtr->LevelName);
    jsonPtr->SetStringField("creator", sessionInfoPtr->Creator);
    jsonPtr->SetStringField("version", sessionInfoPtr->RequiredVersion);
    jsonPtr->SetNumberField("port", roomInfoPtr->Port());
    jsonPtr->SetStringField("token", sfConfig::Get().SFToken);
    jsonPtr->SetStringField("application", FString::Printf(TEXT("Unreal %d.%d.%d"),
        ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION));

    FString jsonString;
    TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&jsonString);
    FJsonSerializer::Serialize(jsonPtr.ToSharedRef(), jsonWriter);
    FFileHelper::SaveStringToFile(jsonString, *(serverPath + LAN_CONFIG));

    // Server a detached server process
    FString args = "";
    args = " -id="+ FString(std::to_string(roomInfoPtr->Id()).c_str());
    args += " -t=SceneFusion";
    args += " -tcp=" + FString::FromInt(roomInfoPtr->Port());
    args += " -l=\"" + logFile + "\"";
    args += " \"" + gameJsonPath + "\"";
    args += " \"" + sceneJsonPath + "\"";

    uint32_t processId = 0;
    m_processHandle = FPlatformProcess::CreateProc(
        *(serverPath + LAN_SERVER_EXECUTEBLE),  // executable name
        *args,  // command line arguments
        false,  // if false, the new process will be detached
        false,  // if true, the new process will not have a window
        false,  // if true, the process won't be there at all
        &processId,  // if non-NULL, this will be filled in with the ProcessId
        0,  // 2 idle, -1 low, 0 normal, 1 high, 2 higher
        *serverPath,  // Directory to start in when running the program, or NULL to use the current working directory
        nullptr); // Optional HANDLE to pipe for redirecting output

    if (m_processHandle.IsValid())
    {
        KS::Log::Info("Launching Scene Fusion server (PID: " + std::to_string(processId) + ")" , LOG_CHANNEL);
        KS::Log::Info(std::string(TCHAR_TO_ANSI(*args)), LOG_CHANNEL);
        FTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([this, sessionInfoPtr](float time)->bool {
                OnStartSession.ExecuteIfBound(sessionInfoPtr);
                return false; // returning true will reschedule this delegate.
            }),
            1.0f // delay period
        );
    }
    else
    {
        KS::Log::Error("Failed to start LAN server.", LOG_CHANNEL);
    }

    return FReply::Handled();
}

FReply sfUISessionsPanel::ManualConnect()
{
    ksRoomInfo::SPtr roomInfoPtr = ksRoomInfo::Create();
    roomInfoPtr->Host() = std::string(TCHAR_TO_UTF8(*m_manualAddressPtr->GetText().ToString()));
    roomInfoPtr->Port() = (uint16_t)FCString::Atoi(*m_manualPortPtr->GetText().ToString());
    roomInfoPtr->Scene() = "Scene Fusion";
    roomInfoPtr->Type() = "Scene Fusion";
    roomInfoPtr->Id() = 1;


    TSharedPtr<sfSessionInfo> sessionInfoPtr = MakeShareable(new sfSessionInfo());
    sessionInfoPtr->RoomInfoPtr = roomInfoPtr;
    sessionInfoPtr->ProjectId = sfConfig::Get().ProjectId;
    sessionInfoPtr->UnrealProjectName = FString(FApp::GetProjectName());
    sessionInfoPtr->LevelName = GEditor->GetEditorWorldContext().World()->GetMapName();
    sessionInfoPtr->Creator = "Unknown";

    KS::Log::Info("Attempting a manual connection to a Scene Fusion server. (Address = " + 
        roomInfoPtr->Host() + ", Port = " + std::to_string(roomInfoPtr->Port()) + ")", LOG_CHANNEL);
    DisplayMessage("Joining session...", sfUIMessageBox::INFO);
    Disable();
    OnJoinSession.ExecuteIfBound(sessionInfoPtr);
    return FReply::Handled();
}

FReply sfUISessionsPanel::InstallLANServer()
{
    // Display progress bar
    GWarn->BeginSlowTask(
        NSLOCTEXT("SceneFusion", "SceneFusion_InstallLANServer", "Install SceneFusion LAN server"), true);
    GWarn->StatusUpdate(1, 3,
        NSLOCTEXT("SceneFusion", "SceneFusion_InstallLANServer", "Downloading SceneFusion LAN server"));

    ClearMessage();
    SceneFusion::WebService->DownloadLANServer(
        m_projectMap[sfConfig::Get().ProjectId],
        sfBaseWebService::OnDownloadLANServerDelegate::CreateRaw(this, &sfUISessionsPanel::DownloadLANServerReply));

    return FReply::Handled();
}

void sfUISessionsPanel::DownloadLANServerReply(TArray<uint8> data, FString& error)
{
    try
    {
        if (error.IsEmpty() && data.Num() == 0)
        {
            error = "Failed to download server";
        }

        // Report error and exit
        if (!error.IsEmpty())
        {
            GWarn->EndSlowTask();
            DisplayMessage(error, sfUIMessageBox::WARNING);
            return;
        }

        GWarn->StatusUpdate(2, 3,
            NSLOCTEXT("SceneFusion", "SceneFusion_InstallLANServer", "Installing SceneFusion LAN server"));

        FString fromRelative = SF_EXTERNAL "SceneFusionServer.zip";
        FString toRelative = LAN_SERVER_FOLDER;
        FString fromFull = FPaths::ProjectDir() + fromRelative;
        FString toFull = FPaths::ProjectDir() + toRelative;

        // Delete the old server
        IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (platformFile.FileExists(*fromFull))
        {
            platformFile.DeleteFile(*fromFull);
        }
        if (platformFile.DirectoryExists(*toFull))
        {
            platformFile.DeleteDirectory(*toFull);
        }

        // Save zip archive
        FFileHelper::SaveArrayToFile(data, *fromFull);

        // Extract archive using powershell
        FString args = " Expand-Archive -Path '" + fromRelative + "' -DestinationPath '" + toRelative + "' -Force";
        FProcHandle process = FPlatformProcess::CreateProc(
            TEXT("powershell.exe"),
            *args,
            false,
            true,
            true,
            nullptr,
            0,
            *FPaths::ProjectDir(),
            nullptr);
        if (!process.IsValid())
        {
            KS::Log::Error("Failed to unzip LAN server.", LOG_CHANNEL);
        }
        else
        {
            while (FPlatformProcess::IsProcRunning(process));
        }

        GWarn->StatusUpdate(2, 3,
            NSLOCTEXT("SceneFusion", "SceneFusion_InstallLANServer", "Validating SceneFusion LAN server."));
        m_lanServerState = ServerCheck();
        GWarn->EndSlowTask();
        if (m_lanServerState == MISSING)
        {
            DisplayMessage(
                "Error extracting '" + FPaths::ConvertRelativePathToFull(fromFull) +
                "' to '" + FPaths::ConvertRelativePathToFull(toFull) + "'",
                sfUIMessageBox::WARNING);
        }
        else
        {
            if (platformFile.FileExists(*fromFull))
            {
                platformFile.DeleteFile(*fromFull);
            }

            if (m_lanServerState == INCOMPATIBLE)
            {
                DisplayMessage(
                    "LAN server version is incompatible with Scene Fusion version.",
                    sfUIMessageBox::WARNING);
            }
        }
    }
    catch (std::exception ex)
    {
        KS::Log::Error("Unexpected error installing LAN server.\n" + std::string(ex.what()));
    }
}

sfUISessionsPanel::ServerState sfUISessionsPanel::ServerCheck()
{
    FString reactorPath = FPaths::ProjectDir() + LAN_SERVER_FOLDER + "Reactor.exe";
    FString runtimePath = FPaths::ProjectDir() + LAN_SERVER_FOLDER + "KSServerRuntime.dll";

    // Check that the server and runtime are available
    IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!platformFile.FileExists(*reactorPath) || !platformFile.FileExists(*runtimePath))
    {
        return MISSING;
    }

    FString productVersion = "";
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 23
    // On lower version of Unreal, FWindowsPlatformMisc::GetFileVersion does not exist.
    // So we skip the version check and just return OK.
    return OK;
#else
#if PLATFORM_WINDOWS
    uint64 version = FWindowsPlatformMisc::GetFileVersion(runtimePath);
    productVersion = FString::Printf(TEXT("%d.%d.%d"),
        (version >> 48) & 0xffff,
        (version >> 32) & 0xffff,
        (version >> 16) & 0xffff);
#endif
    if (SceneFusion::Version() == productVersion)
    {
        return OK;
    }
#endif
    return INCOMPATIBLE;
}

#undef LOG_CHANNEL
#undef LAN_SERVER_FOLDER
#undef LAN_SERVER_EXECUTEBLE
#undef CONFIG_JSON
#undef SCENE_JSON