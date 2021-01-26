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

#include "sfMissingObject.h"
#include <Runtime/Engine/Classes/Engine/AssetManager.h>

/**
 * Maps missing class/blueprint names to stand-in objects and replaces the stand-ins with the correct class/blueprint
 * when they become available.
 */
class sfMissingObjectManager
{
public:
    /**
     * Delegate for on new stand-in.
     *
     * @param   const FString& path to missing asset.
     * @param   UObject* stand-in
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(OnMissingAssetDelegate, const FString&);

    /**
     * Delegate for on replace missing asset.
     *
     * @param   const FString& path to asset that used to be missing.
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(OnFoundMissingAssetDelegate, const FString&);

    /**
     * Invoked when an asset cannot be found. This is invoked once per asset path.
     */
    OnMissingAssetDelegate OnMissingAsset;

    /**
     * Invoked when a missing asset is found.
     */
    OnFoundMissingAssetDelegate OnFoundMissingAsset;

    /**
     * Initialization. Adds event handlers that listen for new assets.
     */
    void Initialize();

    /**
     * Clean up. Removes event handlers and clears the stand-in map.
     */
    void CleanUp();

    /**
     * Adds a stand-in to the stand-in map.
     *
     * @param   IsfMissingObject* standInPtr to add.
     */
    void AddStandIn(IsfMissingObject* standInPtr);

    /**
     * Removes a stand-in from the stand-in map.
     *
     * @param   IsfMissingObject* standInPtr to remove.
     */
    void RemoveStandIn(IsfMissingObject* standInPtr);

    /**
     * Adds an sfObject for an asset that is missing. When the asset is found, the create event will be dispatched
     * again for the sfObject.
     *
     * @param   const FString& path to missing asset.
     * @param   sfObject::SPtr objPtr for the missing asset.
     */
    void AddMissingObject(const FString& path, sfObject::SPtr objPtr);

private:
    // Maps missing class/blueprint names/paths to stand-in objects.
    TMap<FString, TArray<IsfMissingObject*>> m_standInMap;
    TMap<FString, sfObject::SPtr> m_missingObjects;
    FDelegateHandle m_onHotReloadHandle;
    FDelegateHandle m_onNewAssetHandle;

    /**
     * Called after game code is compiled. Checks if missing classes are now available and replaces stand-ins with
     * instances of the newly available classes.
     *
     * @param   bool automatic -  true if the recompile was triggered automatically.
     */
    void OnHotReload(bool automatic);

    /**
     * Called when a new asset is created. If the asset is a blueprint, replaces any stand-ins for that blueprint with
     * instances of the blueprint.
     *
     * @param   const FAssetData& assetData for the new asset.
     */
    void OnNewAsset(const FAssetData& assetData);
};