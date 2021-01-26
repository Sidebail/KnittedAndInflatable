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

#include "../../Public/sfProjectInfo.h"

#include <CoreMinimal.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Views/STableRow.h>
#include <Editor/EditorStyle/Public/EditorStyleSet.h>
#include <Runtime/SlateCore/Public/Brushes/SlateColorBrush.h>

/**
 * Handle the generation and events for a row in the SF projects panel.
 */
class sfUIProjectRow : public STableRow<TSharedPtr<sfProjectInfo>>
{
public:
    DECLARE_DELEGATE_OneParam(OnSelectProject, TSharedPtr<sfProjectInfo>);

    SLATE_BEGIN_ARGS(sfUIProjectRow) {}
    SLATE_ARGUMENT(TSharedPtr<sfProjectInfo>, Item)
        SLATE_ARGUMENT(OnSelectProject, OnClickedProject)
        SLATE_END_ARGS()

public:
    typedef typename STableRow<TSharedPtr<sfProjectInfo>>::FArguments FTableRowArgs;

    /**
     * Construct the UI to display a summary of a sfProjectInfo object.
     *
     * @param   const FArguments& - slate arguments
     * @param   const TSharedRef<STableViewBase>& - table view that owns this row
     */
    void Construct(const FArguments& inArgs, const TSharedRef<STableViewBase>& owner)
    {
        m_item = inArgs._Item;
        m_onClickedProject = inArgs._OnClickedProject;

        FString userList = "";
        for (auto& user : m_item->Users) {
            userList += user + "\n";
        }
        userList.RemoveFromEnd("\n");

        STableRow<TSharedPtr<sfProjectInfo>>::Construct(FTableRowArgs()
            .Padding(FMargin(0, 5))
            .Content()
            [
                 SNew(SBorder)
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Center)
                .Padding(FMargin(5, 5))
                .BorderImage(new FSlateColorBrush(FLinearColor(FColor(0, 0, 0, 128))))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoWidth()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(m_item->Name))
                            .Font(sfUIStyles::GetDefaultFontStyle("Bold", 10))
                        ]
                        + SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoWidth()
                        [
                            SNew(STextBlock).Text(FText::FromString(" (" + m_item->CompanyName + ")"))
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Center).Padding(FMargin(10, 0))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(m_item->SubscriptionName))
                        ]
                        + SHorizontalBox::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)
                        [
                            SNew(SBox).WidthOverride(75)
                            [
                                SNew(SButton).HAlign(HAlign_Center)
                                .Text(FText::FromString("Upgrade"))
                                .IsEnabled(m_item->IsOwner)
                                .OnClicked(this, &sfUIProjectRow::UpgradeSubscription)
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Center).Padding(FMargin(10, 0))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(FString::FromInt(m_item->Users.Num()) + " Team Members"))
                            .ToolTipText(FText::FromString(userList))
                        ]
                        + SHorizontalBox::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)
                        [
                            SNew(SBox).WidthOverride(75)
                            [
                                SNew(SButton).HAlign(HAlign_Center)
                                .Text(FText::FromString("Invite Users"))
                                .IsEnabled(m_item->IsManager)
                                .OnClicked(this, &sfUIProjectRow::InviteUsers)
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Center).Padding(FMargin(10, 0))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(FString::FromInt(m_item->Sessions.Num()) + " Sessions"))
                        ]
                    + SHorizontalBox::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)
                        [
                            SNew(SBox).WidthOverride(75)
                            [
                                SNew(SButton).HAlign(HAlign_Center)
                                .Text(FText::FromString("Sessions"))
                                .OnClicked(this, &sfUIProjectRow::ShowSessions)
                            ]
                        ]
                    ]
                ]
            ], 
        owner);
    }

    /**
     * Execute the delegate responsible for selecting a project and showing sessions.
     *
     * @return  FReply;
     */
    FReply ShowSessions()
    {
        m_onClickedProject.Execute(m_item);
        return FReply::Handled();
    }

    /**
     * Open a remote browser to the project manangment page for invites.
     *
     * @return  FReply;
     */
    FReply InviteUsers()
    {
        FString url = "https://console.kinematicsoup.com" + 
            FString("?tabId=projects&projectId=") + FString::FromInt(m_item->Id) + 
            FString("&utm_source=Plugin&utm_medium=Unreal&utm_campaign=Invite");
        FPlatformProcess::LaunchURL(*url, nullptr, nullptr);
        return FReply::Handled();
    }

    /**
     * Open a remote browser to the SF subscription selection and pricing page.
     *
     * @return  FReply;
     */
    FReply UpgradeSubscription()
    {
        FString url = "https://www.kinematicsoup.com/scene-fusion/pricing" + 
            FString("?utm_source=Plugin&utm_medium=Unreal&utm_campaign=Upgrade");
        FPlatformProcess::LaunchURL(*url, nullptr, nullptr);
        return FReply::Handled();
    }
protected:
    TSharedPtr<sfProjectInfo> m_item;
    OnSelectProject m_onClickedProject;
    TSharedPtr<SHorizontalBox> m_container;
};