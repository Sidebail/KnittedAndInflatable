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
#include <Widgets/Views/STableRow.h>

/**
 * Panel that shows the paths of missing assets.
 */
class sfMissingAssetsPanel : public sfUIPanel
{
public:
    /**
     * Constructor
     */
    sfMissingAssetsPanel();

    /**
     * @return  int number of missing assets.
     */
    int NumMissingAssets();

    /**
     * Adds a missing asset path to display.
     *
     * @param   const FString& path
     */
    void AddMissingPath(const FString& path);

    /**
     * Removes a missing asset path from the display.
     *
     * @param   const FString& path
     */
    void RemoveMissingPath(const FString& path);

    /**
     * Refreshes the message box displaying the number of missing assets.
     */
    void RefreshMessageBox();

    /**
     * Clears all missing asset paths.
     */
    void Clear();

private:
    TSharedPtr<SListView<TSharedPtr<FString>>> m_missingAssetsListPtr;
    TArray<TSharedPtr<FString>> m_missingAssets;
    TSharedPtr<sfUIMessageBox> m_messageBoxPtr;

};