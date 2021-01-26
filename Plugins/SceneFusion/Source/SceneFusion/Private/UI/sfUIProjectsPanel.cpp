#include "sfUIProjectsPanel.h"
#include "sfUIProjectRow.h"
#include "../Web/sfBaseWebService.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfConfig.h"
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SHyperlink.h>
#include <Widgets/Layout/SExpandableArea.h>

sfUIProjectsPanel::sfUIProjectsPanel() : sfUIPanel("Projects")
{
    m_contentPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 2).AutoHeight()
    [
        SNew(SButton).HAlign(HAlign_Center)
        .Text(FText::FromString("Logout"))
        .OnClicked(FOnClicked::CreateLambda([this]()->FReply {
            OnLogout.Execute();
            return FReply::Handled();
        }))
    ];

    DrawProjectList();

    // Refresh session list periodically
    FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this](float time)->bool {
            if (m_widgetPtr.IsValid() && m_widgetPtr->GetVisibility() == EVisibility::Visible) {
                SceneFusion::WebService->GetSessions(
                    sfBaseWebService::OnGetSessionsDelegate::CreateRaw(this, &sfUIProjectsPanel::GetProjectsReply)
                );
            }
            return true; // returning true will reschedule this delegate.
        }),
        10.0f // delay in seconds
    );
}

sfUIProjectsPanel::~sfUIProjectsPanel()
{
}

void sfUIProjectsPanel::DrawProjectList() {
    // New version download
    m_contentPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 10, 5, 5).AutoHeight()
    [
        SAssignNew(m_newVersionPtr, sfUIMessageBox)
    ];

    m_contentPtr->AddSlot().HAlign(HAlign_Fill).VAlign(VAlign_Center).Padding(5, 10, 5, 5).AutoHeight()
    [
        SAssignNew(m_projectsListPtr, SListView<TSharedPtr<sfProjectInfo>>)
        .ItemHeight(24)
        .ListItemsSource(&m_projects)
        .OnGenerateRow(TSlateDelegates<TSharedPtr<sfProjectInfo>>::FOnGenerateRow::CreateLambda(
            [this](TSharedPtr<sfProjectInfo> projectInfoPtr,
                const TSharedRef<STableViewBase>& owner)->TSharedRef<ITableRow> {
                    return SNew(sfUIProjectRow, owner)
                    .Item(projectInfoPtr)
                    .OnClickedProject(sfUIProjectRow::OnSelectProject::CreateRaw(this, &sfUIProjectsPanel::SelectProject));
                }
            ))
        .SelectionMode(ESelectionMode::None)
    ];
}

void sfUIProjectsPanel::Show()
{
    Enable();
    if (m_widgetPtr.IsValid()) {
        if (m_projects.Num() == 0) {
            DisplayMessage("Fetching projects...", sfUIMessageBox::INFO);
        }
        else {
            DisplayMessage("", sfUIMessageBox::INFO);
        }
        m_widgetPtr->SetVisibility(EVisibility::Visible);

        sfConfig::Get().ProjectId = -1;
        sfConfig::Get().Save();
        FString version = SceneFusion::Get().WebService->LatestVersion();
        if (!version.IsEmpty() && version != SceneFusion::Version()) {
            ShowDownloadLink();
        }
        else {
            HideDownloadLink();
        }

        SceneFusion::WebService->GetSessions(
            sfBaseWebService::OnGetSessionsDelegate::CreateRaw(this, &sfUIProjectsPanel::GetProjectsReply)
        );
    }
}

void sfUIProjectsPanel::Hide()
{
    if (m_widgetPtr.IsValid()) {
        m_widgetPtr->SetVisibility(EVisibility::Hidden);
    }
}

void sfUIProjectsPanel::ShowDownloadLink()
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

void sfUIProjectsPanel::HideDownloadLink()
{
    if (m_newVersionPtr.IsValid()) {
        m_newVersionPtr->SetVisibility(EVisibility::Collapsed);
    }
}

void sfUIProjectsPanel::SelectProject(TSharedPtr<sfProjectInfo> valuePtr)
{
    if (!valuePtr.IsValid())
    {
        return;
    }
    sfConfig::Get().ProjectId = valuePtr->Id;
    sfConfig::Get().Save();
    OnSelectProject.ExecuteIfBound();
}

void sfUIProjectsPanel::GetProjectsReply(ProjectMap& projectMap, const FString& error)
{
    if (!error.IsEmpty()) {
        DisplayMessage(error, sfUIMessageBox::WARNING);
        return;
    }

    std::swap(m_projectMap, projectMap);
    m_projects.Empty();
    if (m_projectMap.Num() > 0) {
        for (auto& pair : m_projectMap) {
            m_projects.Add(pair.Value);
        }
    }
    m_projectsListPtr->RequestListRefresh();

    if (m_projects.Num() == 0) {
        DisplayMessage(
            "You are not a member of any project. Wait for a project invitation email or setup your own project.",
            sfUIMessageBox::INFO
        );
    }
    else {
        DisplayMessage("", sfUIMessageBox::INFO);
    }
}