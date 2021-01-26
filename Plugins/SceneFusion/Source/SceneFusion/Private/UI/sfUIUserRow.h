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

#include "sfUIUserInfo.h"
#include "sfUIToggleButton.h"

#include <CoreMinimal.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SCheckBox.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Views/STableRow.h>
#include <Widgets/Images/SImage.h>
#include <Runtime/AppFramework/Public/Widgets/Colors/SColorPicker.h>

#include <Editor/EditorStyle/Public/EditorStyleSet.h>
#include <Runtime/SlateCore/Public/Brushes/SlateColorBrush.h>
#include <Runtime/SlateCore/Public/Brushes/SlateBoxBrush.h>

/**
 * Handle the generation and events for a row in the sessions table.
 */
class sfUIUserRow : public SMultiColumnTableRow<TSharedPtr<sfUIUserInfo>>
{
public:
    DECLARE_DELEGATE_OneParam(OnGoto, TSharedPtr<sfUIUserInfo>);
    DECLARE_DELEGATE_OneParam(OnFollow, TSharedPtr<sfUIUserInfo>);
    DECLARE_DELEGATE_TwoParams(OnChangeColor, TSharedPtr<sfUIUserInfo>, FLinearColor);

    SLATE_BEGIN_ARGS(sfUIUserRow) {}
    SLATE_ARGUMENT(TSharedPtr<sfUIUserInfo>, Item)
    SLATE_ARGUMENT(OnGoto, OnGoto)
    SLATE_ARGUMENT(OnFollow, OnFollow)
    SLATE_ARGUMENT(OnChangeColor, OnChangeColor)
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
        m_sfUserInfoPtr = inArgs._Item;
        m_onGoto = inArgs._OnGoto;
        m_onFollow = inArgs._OnFollow;
        m_onChangeColor = inArgs._OnChangeColor;

        m_tempColor = m_sfUserInfoPtr->Color();
        m_colorPickerArgs.OnColorCommitted.BindRaw(this, &sfUIUserRow::ChangeColor);
        m_colorPickerArgs.OnColorPickerWindowClosed.BindRaw(this, &sfUIUserRow::CommitColor);
        m_colorPickerArgs.OnColorPickerCancelled.BindRaw(this, &sfUIUserRow::ChangeColorCancelled);

        SMultiColumnTableRow<TSharedPtr<sfUIUserInfo>>::Construct(FSuperRowType::FArguments(), owner);
    }

    /**
     * Generate a widget for the column name.
     *
     * @param   const FName& - column name
     * @return  TSharedRef<SWidget>
     */
    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& columnName) override
    {
        // iterate struct properties and generate a widget for it
        if (columnName == "Icon")
        {
            return SNew(SBorder)
                .BorderImage(new FSlateColorBrush(FColor::Black))
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(SImage)
                    .Image(&(m_sfUserInfoPtr->IconBrush()))
                    .OnMouseButtonDown(FPointerEventHandler::CreateRaw(this, &sfUIUserRow::SelectColor))
                ];
        }

        if (columnName == "User")
        {
            return Label(m_sfUserInfoPtr->Name());
        }

        if (columnName == "Goto" && !m_sfUserInfoPtr->IsLocal())
        {
            return ButtonWidget("Go To", FOnClicked::CreateRaw(this, &sfUIUserRow::HandleOnGoto));
        }

        if (columnName == "Follow" && !m_sfUserInfoPtr->IsLocal())
        {
            return ToggleButtonWidget("Follow", FOnClicked::CreateRaw(this, &sfUIUserRow::HandleOnFollow));
        }

        // default to null widget if property cannot be found
        return SNew(SBorder)
            .Padding(FMargin(0, 0))
            .VAlign(VAlign_Fill)
            .BorderImage(new FSlateColorBrush(FColor::Black));
    }

    /**
     * Create a text label
     *
     * @param   const FString& - text
     * @return  TSharedRef<SWidget>
     */
    TSharedRef<SWidget> Label(const FString& text)
    {
        return SNew(SBorder)
            .Padding(FMargin(5, 0))
            .VAlign(VAlign_Center)
            .BorderImage(new FSlateColorBrush(FColor::Black))
            [
                SNew(STextBlock).Text(FText::FromString(text))
            ];
    }

    /**
     * Create a button widget
     *
     * @param   const FString& text - button text
     * @return  TSharedRef<SWidget>
     */
    TSharedRef<SWidget> ButtonWidget(const FString& text, FOnClicked onClicked)
    {
        return SNew(SBorder)
            .Padding(FMargin(2, 2))
            .HAlign(HAlign_Fill).VAlign(VAlign_Center)
            .BorderImage(new FSlateColorBrush(FColor::Black))
            [
                SNew(SButton)
                .HAlign(HAlign_Center)
                .Text(FText::FromString(text))
                .OnClicked(onClicked)
            ];
    }

    /**
     * Create a button widget
     *
     * @param   const FString& text - button text
     * @return  TSharedRef<SWidget>
     */
    TSharedRef<SWidget> ToggleButtonWidget(const FString& text, FOnClicked onClicked)
    {
        return SNew(SBorder)
            .Padding(FMargin(2, 2))
            .HAlign(HAlign_Fill).VAlign(VAlign_Center)
            .BorderImage(new FSlateColorBrush(FColor::Black))
            [
                SAssignNew(m_sfUserInfoPtr->FollowButton, sfUIToggleButton)
                .HAlign(HAlign_Center)
                .Text(FText::FromString(text))
                .OnClicked(onClicked)
            ];
    }

    /**
     * Open a color picker and initialize it with the current user color.
     *
     * @param   const FGeometry& - pointer position
     * @param   const FPointerEvent& - pointer event
     * @param   FReply - event state
     */
    FReply SelectColor(const FGeometry& geometry, const FPointerEvent& event)
    {
        if (m_sfUserInfoPtr->IsLocal()) {
            m_colorPickerArgs.InitialColorOverride = m_sfUserInfoPtr->Color();
            OpenColorPicker(m_colorPickerArgs);
        }
        return FReply::Handled();
    }

    /**
     * Store a temporary color from a color picker
     *
     * @param   FLinearColor - color returned from a color picker
     */
    void ChangeColor(FLinearColor color)
    {
        m_tempColor = color;
    }

    /**
     * When the color change is cancelled we set the temp color to the
     * current user color.  This stops CommitColor from updating the
     * user color.
     *
     * @param   FLinearColor - color returned from a color picker
     */
    void ChangeColorCancelled(FLinearColor color)
    {
        m_tempColor = m_sfUserInfoPtr->Color();
    }

    /**
     * Handle the color change selection from the SColorPicker
     *
     * @param   const TSharedRef<SWindow>& - color picker window
     */
    void CommitColor(const TSharedRef<SWindow>& windowPtr)
    {
        if (m_tempColor != m_sfUserInfoPtr->Color()) {
            m_onChangeColor.ExecuteIfBound(m_sfUserInfoPtr, m_tempColor);
        }
    }

    /**
     * Handle the click event for the 'Go To' button
     *
     * @return   FReply - event state
     */
    FReply HandleOnGoto()
    {
        m_onFollow.ExecuteIfBound(nullptr);
        m_onGoto.ExecuteIfBound(m_sfUserInfoPtr);
        return FReply::Handled();
    }

    /**
     * Handle the click event for the 'Follow' button
     *
     * @return   FReply - event state
     */
    FReply HandleOnFollow()
    {
        m_onFollow.ExecuteIfBound(m_sfUserInfoPtr);
        return FReply::Handled();
    }

protected:
    TSharedPtr<sfUIUserInfo> m_sfUserInfoPtr;
    TSharedPtr<SImage> m_colorPtr;

    OnGoto m_onGoto;
    OnFollow m_onFollow;
    OnChangeColor m_onChangeColor;

    FColorPickerArgs m_colorPickerArgs;
    FLinearColor m_tempColor;
};