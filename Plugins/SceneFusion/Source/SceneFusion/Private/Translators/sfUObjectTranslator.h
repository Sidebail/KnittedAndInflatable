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
#include <sfSession.h>
#include <list>

using namespace KS::SceneFusion2;

/**
 * Manages syncing of uobjects.
 */
class sfUObjectTranslator : public sfBaseUObjectTranslator
{
    friend class sfActorTranslator;
    friend class sfComponentTranslator;
    friend class sfLevelTranslator;
    friend class sfBaseUObjectTranslator;

public:

    /**
     * Callback to call when a uobject is modified.
     *
     * @param   sfObject::SPtr
     * @param   UObject*
     */
    typedef std::function<void(sfObject::SPtr, UObject*)> OnModifyHandler;

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
     * Creates an sfObject for a uobject.
     *
     * @param   UObject* uobjPtr to create sfObject for.
     * @param   sfObject::SPtr* outObjPtr created for the uobject.
     * @return  bool true if the uobject was handled by this translator.
     */
    virtual bool Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr) override;

    /**
     * Called when a uobject is created by another user.
     *
     * @param   sfObject::SPtr objPtr that was created.
     * @param   int childIndex of new object. -1 if object is a root.
     */
    virtual void OnCreate(sfObject::SPtr objPtr, int childIndex) override;

    virtual void OnConfirmDelete(sfObject::SPtr objPtr) override;

    /**
     * Called when an object is modified by an undo or redo transaction.
     *
     * @param   sfObject::SPtr objPtr for the uobject that was modified. nullptr if the uobjPtr is not synced.
     * @param   UObject* uobjPtr that was modified.
     * @return  bool true if event was handled and need not be processed by other handlers.
     */
    virtual bool OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr) override;

private:
    sfSession::SPtr m_sessionPtr;
    std::list<sfObject::SPtr> m_pendingDeletions;
    FDelegateHandle m_tickHandle;

    /**
     * Updates the uobject translator.
     *
     * @param   float deltaTime in seconds since last tick.
     */
    void Tick(float deltaTime);

    /**
     * Adds an object to the list of referenced objects with deletion pending. If the object's parent is later
     * recreated, we will reuse this object in the create request to keep its id the same and preserve references.
     *
     * @param   sfObject::SPtr objPtr to add to pending delete list.
     */
    void AddPendingDeletion(sfObject::SPtr objPtr);

    /**
     * Removes uobjects from the pending delete list that belong to the given level.
     *
     * @param   ULevel* levelPtr to remove pending delete uobjects for.
     */
    void RemovePendingDeletionsInLevel(ULevel* levelPtr);

    /**
     * Attaches an sfObject to its uobject's outer's sfObject. Creates the sfObject for the outer if it does not
     * exist. Initializes the sfObject's properties if the parent is initialized.
     *
     * @param   sfObject::SPtr objPtr to find or create parent for.
     * @param   UObject* uobjPtr for the sfObject.
     */
    void FindOrCreateParent(sfObject::SPtr objPtr, UObject* uobjPtr);

    /**
     * Initializes properties for a uobject's sfObject. If the uobject is nullptr or pending kill, detaches the
     * sfObject from its parent.
     *
     * @param   sfObject::SPtr objPtr to initialize.
     * @param   UObject* uobjPtr for the sfObject.
     */
    void InitializeObjectProperties(sfObject::SPtr objPtr, UObject* uobjPtr);

    /**
     * Creates or finds a uobject for an sfObject and initializes it with server values. Recursively initializes child
     * uobjects for child sfObjects.
     *
     * @param   UObject* outerPtr for the object.
     * @param   sfObject::SPtr objPtr to initialize uobject for.
     */
    void InitializeUObject(UObject* outerPtr, sfObject::SPtr objPtr);
};