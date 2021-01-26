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

#include "../../Public/Translators/sfBaseUObjectTranslator.h"
#include <CoreMinimal.h>

class sfAssetTranslator : public sfBaseUObjectTranslator
{
protected:
    /**
     * Initialization. Called after connecting to a session.
     */
    virtual void Initialize() override;

    /**
     * Deinitialization. Called after disconnecting from a session.
     */
    virtual void CleanUp() override;

    /**
     * Creates an sfObject for an asset.
     *
     * @param   UObject* uobjPtr to create sfObject for.
     * @param   sfObject::SPtr* outObjPtr created for the uobject.
     * @return  bool true if the uobject was handled by this translator.
     */
    virtual bool Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr) override;

    /**
     * Called when an asset is created by another user.
     *
     * @param   sfObject::SPtr objPtr that was created.
     * @param   int childIndex of new object. -1 if object is a root.
     */
    virtual void OnCreate(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Called when an asset is modified by an undo or redo transaction.
     *
     * @param   sfObject::SPtr objPtr for the uobject that was modified. nullptr if the uobjPtr is not synced.
     * @param   UObject* uobjPtr that was modified.
     * @return  bool true if event was handled and need not be processed by other handlers.
     */
    virtual bool OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr) override;

    /**
     * Called after a uproperty is changed by another user.
     *
     * @param   UObject* uobjPtr whose property changed.
     * @param   UnrealProperty* upropPtr that changed.
     */
    virtual void PostPropertyChange(UObject* uobjPtr, UnrealProperty* upropPtr) override;

private:
    TSet<UObject*> m_conflictingAssets;
    // Maps paths to sfObjects for synced assets that were deleted locally
    TMap<FString, sfObject::SPtr> m_deletedAssets;
    FDelegateHandle m_onDeleteHandle;
    FDelegateHandle m_onCreateHandle;

    /**
     * Called when an asset is deleted.
     *
     * @param   const FAssetData& assetData for the deleted asset.
     */
    void OnDeleteAsset(const FAssetData& assetData);

    /**
     * Called when we create an asset that was missing locally.
     *
     * @param   UObject* assetPtr that was created.
     */
    void OnCreateMissingAsset(UObject* assetPtr);

    /**
     * Computes a checksum from a dictionary property.
     *
     * @param   sfDictionaryProperty::SPtr dictPtr to compute checksum for.
     * @return  uint64_t checksum
     */
    uint64_t Checksum(sfDictionaryProperty::SPtr dictPtr);
};