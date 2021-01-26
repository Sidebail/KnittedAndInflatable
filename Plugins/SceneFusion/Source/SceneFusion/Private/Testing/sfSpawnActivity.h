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

#include "sfBaseActivity.h"

/**
 * Activity for randomly placing assets into the level. Currently only static meshes, skeletal meshes, and particle
 * systems are supported.
 */
class sfSpawnActivity : public sfBaseActivity
{
public:
    /**
     * Constructor
     *
     * @param   const FString& name of activity.
     * @param   float weight the determines how likely this activity is to occur.
     */
    sfSpawnActivity(const FString& name, float weight);

    /**
     * Handles command arguments. The arguments are either paths relative to /Game/ that determine where we look for
     * assets, or any of the following options:
     *     -r | -reset: clears all paths used to look for assets.
     *
     * @param   const TArray<FString>& args
     * @param   int index of first arg in array to process.
     */
    virtual void HandleArgs(const TArray<FString>& args, int index) override;

    /**
     * Adds a random asset to the level.
     */
    virtual void Start() override;

private:
    TArray<FString> m_assetPaths;
};