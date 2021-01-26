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

#include "sfMonkey.h"
#include "sfTimer.h"
#include "sfAction.h"

#include <CoreMinimal.h>
#include <Editor.h>

/**
 * Testing utilities
 */
class sfTestUtil
{
public:
    /**
     * Registers test console commands.
     */
    static void RegisterCommands();

    /**
     * Performs cleanup.
     */
    static void CleanUp();

private:
    static TSharedPtr<sfMonkey> m_monkeyPtr;
    static IConsoleCommand* m_monkeyCommandPtr;
    static TSharedPtr<sfTimer> m_timerPtr;
    static IConsoleCommand* m_timerCommandPtr;
    static TSharedPtr<sfAction> m_actionPtr;
    static IConsoleCommand* m_logDebugCommandPtr;

    /**
     * Monkey console command. If no arguments are given, toggles the monkey on or off. If at least one argument is
     * given, starts the monkey.
     *
     * @param   const TArray<FString>& args
     */
    static void Monkey(const TArray<FString>& args);

    /**
     * Run console command. Runs given action.
     *
     * @param   const TArray<FString>& args
     */
    static void Run(const TArray<FString>& args);

    /**
     * Enables or disables debug logging.
     *
     * @param   const TArray<FString>& args
     */
    static void LogDebug(const TArray<FString>& args);

    /**
     * Combines quoted arguments into single arguments, because Unreal does not do this for us. We have no way of
     * knowing if there were multiple consecuitive spaces in the quoted text, so consecuitive spaces will be collapsed
     * into one space.
     *
     * @param   const TArray<FString>& inArgs to parse.
     * @param   TArray<FString>& outArgs with quoted arguments combined.
     */
    static void CombineQuotedArgs(const TArray<FString>& inArgs, TArray<FString>& outArgs);
};