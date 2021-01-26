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

#include "sfUIStyles.h"

#include <Widgets/Text/STextBlock.h>
#include <Widgets/Images/SImage.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Input/SHyperlink.h>

/**
 * Display a message box with an error, warning, or info icon.
 */
class sfUIMessageBox : public SBorder
{
public:
    SLATE_BEGIN_ARGS(sfUIMessageBox) :
        _Pad(10),
        _SmallIcon(false),
        _BackgroundColor(255, 255, 255, 32)
    {}
    SLATE_ARGUMENT(float, Pad)
    SLATE_ARGUMENT(bool, SmallIcon)
    SLATE_ARGUMENT(FColor, BackgroundColor)
    SLATE_END_ARGS()

public:
    enum Icon { INFO, WARNING, ERROR };

    DECLARE_DELEGATE(OnClickDelegate);

    /**
     * Construct the widget
     *
     * @param   const FArguments& - slate arguments
     */
    void Construct(const FArguments& inArgs)
    {
        m_smallIcon = inArgs._SmallIcon;
        float pad = inArgs._Pad;
        float space = fmax(5, pad);
        SBorder::Construct(SBorder::FArguments());
        SetBorderImage(new FSlateColorBrush(FLinearColor(inArgs._BackgroundColor)));
        m_widgetPtr = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(pad, pad, space, pad).AutoWidth()
        [
            SAssignNew(m_imagePtr, SImage)
        ]
        + SHorizontalBox::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Center).Padding(0, pad, pad, pad).AutoWidth()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Center).Padding(0).AutoHeight()
            [
                SAssignNew(m_textPtr, STextBlock)
                .WrapTextAt(300)
                .Justification(ETextJustify::Left)
                .Text(FText::FromString("Test Message"))
            ]
            + SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0).AutoHeight()
            [
                SAssignNew(m_linkPtr, SHyperlink)
                .Visibility(EVisibility::Collapsed)
                .Text_Lambda([this]()-> FText { return m_linkText; })
                .OnNavigate(FSimpleDelegate::CreateLambda([this]()->void
                {
                    m_onClick.ExecuteIfBound();
                }))
            ]
        ];
    }

    /**
     * Display a message or hide the widget if the message is blank.
     *
     * @param   const FString& - message
     * @param   MessageType
     * @param   OnClickDelegate onClick - if provided, the message will be a clickable link that will call this
     *          function when clicked.
     */
    void SetMessage(const FString& message, Icon type, OnClickDelegate onClick = nullptr)
    {
        if (!onClick.IsBound())
        {
            SetMessage(message, "", type, onClick);
        }
        else
        {
            SetMessage("", message, type, onClick);
        }
    }

    /**
     * Display a message or hide the widget if the message is blank.
     *
     * @param   const FString& - message to display.
     * @param   const FString& clickable link to display under the message.
     * @param   MessageType
     * @param   OnClickDelegate onClick callback to call if the link is clicked.
     */
    void SetMessage(const FString& message, const FString& link, Icon type, OnClickDelegate onClick)
    {
        if (message.IsEmpty() && link.IsEmpty()) {
            SetVisibility(EVisibility::Collapsed);
        }
        else {
            SetVisibility(EVisibility::Visible);
            SetContent(m_widgetPtr.ToSharedRef());
            FText text = FText::FromString(message);
            m_linkText = FText::FromString(link);
            m_onClick = onClick;
            m_textPtr->SetText(text);
            m_textPtr->SetVisibility(message.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
            m_linkPtr->SetVisibility(link.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
            if (m_smallIcon)
            {
                switch (type)
                {
                    case WARNING: m_imagePtr->SetImage(sfUIStyles::Get().GetBrush("SceneFusion.Warning.small")); break;
                    case ERROR: m_imagePtr->SetImage(sfUIStyles::Get().GetBrush("SceneFusion.Error.small")); break;
                    default: m_imagePtr->SetImage(sfUIStyles::Get().GetBrush("SceneFusion.Info.small")); break;
                }
            }
            else
            {
                switch (type)
                {
                    case WARNING: m_imagePtr->SetImage(sfUIStyles::Get().GetBrush("SceneFusion.Warning")); break;
                    case ERROR: m_imagePtr->SetImage(sfUIStyles::Get().GetBrush("SceneFusion.Error")); break;
                    default: m_imagePtr->SetImage(sfUIStyles::Get().GetBrush("SceneFusion.Info")); break;
                }
            }
        }
    }

private:
    TSharedPtr<SImage> m_imagePtr;
    TSharedPtr<STextBlock> m_textPtr;
    TSharedPtr<SWidget> m_widgetPtr;
    TSharedPtr<SHyperlink> m_linkPtr;
    OnClickDelegate m_onClick;
    FText m_linkText;
    bool m_smallIcon;
};