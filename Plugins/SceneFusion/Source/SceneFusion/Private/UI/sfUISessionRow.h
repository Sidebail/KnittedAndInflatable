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

#include "../../Public/sfSessionInfo.h"

#include <CoreMinimal.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Views/STableRow.h>
#include <Editor/EditorStyle/Public/EditorStyleSet.h>
#include <Runtime/SlateCore/Public/Brushes/SlateColorBrush.h>

/**
 * Handle the generation and events for a row in the sessions table.
 */
class sfUISessionRow : public SMultiColumnTableRow<TSharedPtr<sfSessionInfo>>
{
public:
    DECLARE_DELEGATE_OneParam(OnJoinSession, TSharedPtr<sfSessionInfo>);
    DECLARE_DELEGATE_OneParam(OnStopSession, TSharedPtr<sfSessionInfo>);

    SLATE_BEGIN_ARGS(sfUISessionRow) {}
    SLATE_ARGUMENT(TSharedPtr<sfSessionInfo>, Item)
    SLATE_ARGUMENT(OnJoinSession, OnClickedJoin)
    SLATE_ARGUMENT(OnStopSession, OnClickedStop)
    SLATE_END_ARGS()

public:
    /**
     * Construct a multi-column table row and track slate arguments
     *
     * @param   const FArguments& - slate arguments
     * @param   const TSharedRef<STableViewBase>& - table view that owns this row
     */
    void Construct(const FArguments& inArgs, const TSharedRef<STableViewBase>& owner)
    {
        Item = inArgs._Item;
        OnClickedJoin = inArgs._OnClickedJoin;
        OnClickedStop = inArgs._OnClickedStop;
        SMultiColumnTableRow<TSharedPtr<sfSessionInfo>>::Construct(FSuperRowType::FArguments(), owner);

        // Convert time from UTC to local time
        FTimespan adj = FDateTime::Now() - FDateTime::UtcNow();
        FDateTime localTime = Item->StartTime + adj;

        FString tooltip = "ID: " + FString::FromInt(Item->RoomInfoPtr->Id()) +
            "\nStart time: " + localTime.ToString(TEXT("%Y-%m-%d %H:%M:%S")) +
            "\nScene Fusion " + Item->RequiredVersion +
            "\n" + Item->LaunchApplication;

        FString applicationString = FString::Printf(TEXT("Unreal %d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION);
        if (!Item->LaunchApplication.StartsWith(applicationString)) {
            tooltip += "\nIncompatible Unreal versions.";
        }
        if (!Item->CanStop)
        {
            tooltip += FString("\nYou do not have permission to stop this session.");
        }
        SetToolTipText(FText::FromString(tooltip));
    }

    /**
     * Generate a widget for the column name.
     *
     * @param   const FName& - column name
     * @param   TSharedRef<SWidget>
     */
    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& columnName) override
    {
        // iterate struct properties and generate a widget for it
        if (columnName == "Project")
        {
            return TextWidget(Item->UnrealProjectName);
        }

        if (columnName == "Level")
        {
            return TextWidget(Item->LevelName);
        }

        if (columnName == "Creator")
        {
            return TextWidget(Item->Creator);
        }

        if (columnName == "Action")
        {
            return SNew(SBorder)
                .Padding(FMargin(5, 0))
                .HAlign(HAlign_Right).VAlign(VAlign_Center)
                .BorderImage(new FSlateColorBrush(FLinearColor(1.0f, 1.0f, 1.0f)))
                .BorderBackgroundColor(FColor::Black)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .HAlign(HAlign_Center)
                        .Text(FText::FromString("Join"))
                        .IsEnabled_Lambda([this]() -> bool {
                            FString applicationString = FString::Printf(
                                TEXT("Unreal %d.%d"),
                                ENGINE_MAJOR_VERSION,
                                ENGINE_MINOR_VERSION);
                            return Item->LaunchApplication.StartsWith(applicationString);})
                        .OnClicked(this, &sfUISessionRow::HandleOnClickedJoin)
                    ]
                    + SHorizontalBox::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .HAlign(HAlign_Center)
                        .Text(FText::FromString("Stop"))
                        .IsEnabled_Lambda([this]() -> bool { return Item->CanStop;})
                        .OnClicked(this, &sfUISessionRow::HandleOnClickedStop)
                    ]
                ];
        }

        // default to null widget if property cannot be found
        return SNullWidget::NullWidget;
    }

    /**
     * Create a text widget
     *
     * @param   const FName& - text value
     */
    TSharedRef<SWidget> TextWidget(const FString& value)
    {
        return SNew(SBorder)
            .Padding(FMargin(5, 0))
            .VAlign(VAlign_Center)
            .BorderImage(new FSlateColorBrush(FLinearColor(1.0f, 1.0f, 1.0f)))
            .BorderBackgroundColor(FColor::Black)
            [
                SNew(STextBlock).Text(FText::FromString(value))
            ];
    }

    /**
     * Handle the click event for the join button
     *
     * @param   FReply - event state
     */
    FReply HandleOnClickedJoin()
    {
        OnClickedJoin.Execute(Item); 
        return FReply::Handled();
    }

    /**
     * Handle the click event for the stop button
     *
     * @param   FReply - event state
     */
    FReply HandleOnClickedStop()
    {
        OnClickedStop.Execute(Item);
        return FReply::Handled();
    }

protected:
    TSharedPtr<sfSessionInfo> Item;
    OnJoinSession OnClickedJoin;
    OnStopSession OnClickedStop;
};