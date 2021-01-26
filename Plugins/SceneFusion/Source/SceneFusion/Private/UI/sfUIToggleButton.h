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

#include <Widgets/Input/SButton.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Images/SImage.h>
#include <EditorStyleSet.h>

/**
 * Toggle button.
 */
class sfUIToggleButton : public SButton
{
public:
    /**
     * Sets toggle button state and text.
     *
     * @param   bool isPressed
     * @param   FString text on button
     */
    void SetIsPressed(bool isPressed, FString text)
    {
        if (isPressed)
        {
            NormalImage = &FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("FlatButton").Pressed;
            HoverImage = &FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("FlatButton").Hovered;
            StaticCastSharedRef<STextBlock>(GetContent())->SetText(FText::FromString(text));
        }
        else
        {
            NormalImage = &FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Button").Normal;
            HoverImage = &FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Button").Hovered;
            StaticCastSharedRef<STextBlock>(GetContent())->SetText(FText::FromString(text));
        }
    }
};