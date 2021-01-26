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
#include <sfObject.h>
#include <sfDictionaryProperty.h>
#include <sfListProperty.h>
#include <CoreMinimal.h>
#include "..\sfUPropertyTypes.h"

using namespace KS::SceneFusion2;

/**
 * Base class for handling object events.
 */
class sfBaseTranslator
{
public:
    friend class sfObjectEventDispatcher;

    /**
     * Constructor
     */
    sfBaseTranslator() {}

    /**
     * Destructor
     */
    virtual ~sfBaseTranslator() {}

protected:
    /**
     * Initialization. Called after connecting to a session.
     */
    virtual void Initialize() {}

    /**
     * Deinitialization. Called after disconnecting from a session.
     */
    virtual void CleanUp() {}

    /**
     * Creates an sfObject for a uobject.
     *
     * @param   UObject* uobjPtr to create sfObject for.
     * @param   sfObject::SPtr* outObjPtr created for the uobject.
     * @return  bool true if the uobject was handled by this translator.
     */
    virtual bool Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr) { return false; }

    /**
     * Called when an object is created by another user.
     *
     * @param   sfObject::SPtr objPtr that was created.
     * @param   int childIndex of new object. -1 if object is a root.
     */
    virtual void OnCreate(sfObject::SPtr objPtr, int childIndex) {}

    /**
     * Called when an object is deleted by another user.
     *
     * @param   sfObject::SPtr objPtr that was deleted.
     */
    virtual void OnDelete(sfObject::SPtr objPtr) {}

    /**
     * Called when a locally-deleted object is confirmed as deleted.
     *
     * @param   sfObject::SPtr objPtr that was confirmed as deleted.
     */
    virtual void OnConfirmDelete(sfObject::SPtr objPtr) {}

    /**
     * Called when an object is locked by another user.
     *
     * @param   sfObject::SPtr objPtr that was locked.
     */
    virtual void OnLock(sfObject::SPtr objPtr) {}

    /**
     * Called when an object is unlocked by another user.
     *
     * @param   sfObject::SPtr objPtr that was unlocked.
     */
    virtual void OnUnlock(sfObject::SPtr objPtr) {}

    /**
     * Called when an object's lock owner changes.
     *
     * @param   sfObject::SPtr objPtr whose lock owner changed.
     */
    virtual void OnLockOwnerChange(sfObject::SPtr objPtr) {}

    /**
     * Called when an object's direct lock owner changes.
     *
     * @param   sfObject::SPtr objPtr whose direct lock owner changed.
     */
    virtual void OnDirectLockChange(sfObject::SPtr objPtr) {}

    /**
     * Called when an object's parent is changed by another user.
     *
     * @param   sfObject::SPtr objPtr whose parent changed.
     * @param   int childIndex of the object. -1 if the object is a root.
     */
    virtual void OnParentChange(sfObject::SPtr objPtr, int childIndex) {}

    /**
     * Called when an object property changes.
     *
     * @param   sfProperty::SPtr propertyPtr that changed.
     */
    virtual void OnPropertyChange(sfProperty::SPtr propertyPtr) {}

    /**
     * Called when a field is removed from a dictionary property.
     *
     * @param   sfDictionaryProperty::SPtr dictPtr the field was removed from.
     * @param   const sfName& name of removed field.
     */
    virtual void OnRemoveField(sfDictionaryProperty::SPtr dictPtr, const sfName& name) {}

    /**
     * Called when one or more elements are added to a list property.
     *
     * @param   sfListProperty::SPtr listPtr that elements were added to.
     * @param   int index elements were inserted at.
     * @param   int count - number of elements added.
     */
    virtual void OnListAdd(sfListProperty::SPtr listPtr, int index, int count) {}

    /**
     * Called when one or more elements are removed from a list property.
     *
     * @param   sfListProperty::SPtr listPtr that elements were removed from.
     * @param   int index elements were removed from.
     * @param   int count - number of elements removed.
     */
    virtual void OnListRemove(sfListProperty::SPtr listPtr, int index, int count) {}

    /**
     * Called when a property on a uobject changes.
     *
     * @param   sfObject::SPtr objPtr for the uobject whose property changed.
     * @param   UObject* uobjPtr whose property changed.
     * @param   UnrealProperty* upropPtr that changed.
     * @return  bool false if the property change event should be handled by the default handler.
     */
    virtual bool OnUPropertyChange(sfObject::SPtr objPtr, UObject* uobjPtr, UnrealProperty* upropPtr) { return false; }

    /**
     * Called after a uproperty is changed by another user.
     *
     * @param   UObject* uobjPtr whose property changed.
     * @param   UnrealProperty* upropPtr that changed.
     */
    virtual void PostPropertyChange(UObject* uobjPtr, UnrealProperty* upropPtr) {};

    /**
     * Called when a uobject is modified.
     *
     * @param   sfObject::SPtr objPtr for the uobject that was modified.
     * @param   UObject* uobjPtr that was modified.
     */
    virtual void OnUObjectModified(sfObject::SPtr objPtr, UObject* uobjPtr) {}

    /**
     * Called when an object is modified by and undo or redo transaction.
     *
     * @param   sfObject::SPtr objPtr for the uobject that was modified. nullptr if the uobjPtr is not synced.
     * @param   UObject* uobjPtr that was modified.
     * @return  bool true if event was handled and need not be processed by other handlers.
     */
    virtual bool OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr) { return false; }
};