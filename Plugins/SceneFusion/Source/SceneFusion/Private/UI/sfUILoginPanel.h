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

#include <Runtime/Slate/Public/Framework/SlateDelegates.h>
#include <Widgets/Input/SEditableTextBox.h>

/**
 * Scene Fusion login panel
 */
class sfUILoginPanel : public sfUIPanel
{
public:
    /**
     * Delegate for UI login
     */
    DECLARE_DELEGATE(OnLoginDelegate);

    /**
     * Constructor
     */
    sfUILoginPanel();  

    /**
     * Authenticate
     */
    void Authenticate();

    /**
     * Login event handler
     */
    OnLoginDelegate OnLogin;
private:
    // UI Widgets
    TSharedPtr<SEditableTextBox> m_emailPtr;
    TSharedPtr<SEditableTextBox> m_passPtr;

    /**
     * Login if enter was pressed in a text box.
     *
     * @param   const FText& text
     * @param   ETextCommit::Type commitType
     */
    void OnCommit(const FText& text, ETextCommit::Type commitType);

    /**
     * Send a web request to login
     *
     * @param   FReply - UI event reply
     */
    FReply Login();

    /**
     * Handle the reply from a Login web request
     *
     * @param   const FString& - error message
     */
    void LoginReply(const FString& error);

    /**
     * Handle the reply from an automatic login web request
     *
     * @param   const FString& - error message
     */
    void AuthenticateReply(const FString& error);
};