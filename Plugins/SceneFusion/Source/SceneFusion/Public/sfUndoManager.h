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

#include "Objects/sfUndoHook.h"
#include <CoreMinimal.h>
#include <Editor/UnrealEd/Classes/Editor/TransBuffer.h>
#include <Runtime/Launch/Resources/Version.h>
#include <functional>
#include <unordered_map>
#include <sfObject.h>

using namespace KS::SceneFusion2;

/**
 * Interface for receiving a post undo event.
 */
class sfIPostUndoHandler
{
public:
    /**
     * Called after an undo or redo transaction.
     */
    virtual void PostUndo() = 0;
};

/**
 * Manager for handling undo events.
 */
class sfUndoManager
{
public:
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 21
    typedef const FTransactionContext& TransactionContext;
#else
    typedef FUndoSessionContext TransactionContext;
#endif

    /**
     * Handle to call for objects in an undo or redo transaction.
     *
     * @param   sfObject::SPtr for the uobject. May be nullptr.
     * @param   UObject* uobject in the undo or redo transaction.
     */
    typedef std::function<void(sfObject::SPtr, UObject*)> UndoHandler;

    /**
     * @return  sfUndoManager& static singleton instance
     */
    static sfUndoManager& Get();

    /**
     * Constructor
     */
    sfUndoManager();

    /**
     * Destructor
     */
    ~sfUndoManager();

    /**
     * Initialization. Sets up event handlers.
     */
    void Initialize();

    /**
     * Clean up
     */
    void CleanUp();

    /**
     * Gets the unlocalized string for the current transaction. Empty string if we aren't in a transaction.
     *
     * @return  FString& unlocalized undo text for the current transaction.
     */
    const FString& GetUndoText();

    /**
     * @return  bool true if we are applying an undo or redo transaction.
     */
    bool InUndoRedo();

    /**
     * Adds a handler to be called after the current undo or redo transaction. If a handler is added multiple times it
     * will only be called once.
     *
     * @param   sfIPostUndoHandler* handler to call after the current undo or redo transaction.
     */
    void AddPostUndoHandler(sfIPostUndoHandler* handler);

    /**
     * Registers an undo handler for objects of type T.
     *
     * @param   UndoHandler handler
     */
    template<typename T>
    void RegisterUndoHandler(UndoHandler handler)
    {
        m_undoHandlers.emplace(T::StaticClass(), handler);
    }

    /**
     * Removes the undo handler for objects of type T.
     */
    template<typename T>
    void UnregisterUndoHandler()
    {
        m_undoHandlers.erase(T::StaticClass());
    }

    /**
     * Registers an pre undo handler for objects of type T.
     *
     * @param   UndoHandler handler
     */
    template<typename T>
    void RegisterPreUndoHandler(UndoHandler handler)
    {
        m_preUndoHandlers.emplace(T::StaticClass(), handler);
    }

    /**
     * Removes the pre undo handler for objects of type T.
     */
    template<typename T>
    void UnregisterPreUndoHandler()
    {
        m_preUndoHandlers.erase(T::StaticClass());
    }

private:
    /**
     * This is a hack that allows us to add objects to a transaction, allowing us to run PostEditUndo code from our
     * objects before the PostEditUndo code from other objects in the transaction.
     */
    class TransactionHack : public FTransaction
    {
    public:
        /**
         * Adds an object to the changed object map of a transaction. The actual objects changed by the transaction are
         * not added until the transaction is applied, so any objects we add before applying the transaction will have
         * PostEditUndo called prior to the objects that are actually in the transaction.
         *
         * @param   UObject* uobjPtr
         */
        void AddChangedObject(UObject* uobjPtr)
        {
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 20
            FTransaction::FChangedObjectValue changedObjectValue(0, nullptr);
            ChangedObjects.Add(uobjPtr, changedObjectValue);
#else
            ChangedObjects.Add(uobjPtr);
#endif
        };
    };

    std::unordered_map<UClass*, UndoHandler> m_undoHandlers;
    std::unordered_map<UClass*, UndoHandler> m_preUndoHandlers;
    TSet<sfIPostUndoHandler*> m_postUndoHandlers;
    TSet<USceneComponent*> m_childrenToCheck;
    TSet<USceneComponent*> m_parentsToCheck;
    TSet<AActor*> m_destroyedActorsToCheck;
    UTransBuffer* m_undoBufferPtr;
    UsfUndoHook* m_undoHookPtr;
    FDelegateHandle m_onUndoHandle;
    FDelegateHandle m_onRedoHandle;
    FDelegateHandle m_beforeUndoRedoHandle;
    FString m_undoText;
    bool m_inUndoRedo;

    /**
     * Called when a transaction is undone.
     *
     * @param   TransactionContext context
     * @param   bool success
     */
    void OnUndo(TransactionContext context, bool success);

    /**
     * Called when a transaction is redone.
     *
     * @param   TransactionContext context
     * @param   bool success
     */
    void OnRedo(TransactionContext context, bool success);

    /**
     * Called when a transaction is undone or redone.
     *
     * @param   TransactionContext context
     */
    void BeforeUndoRedo(TransactionContext context);

    /**
     * Unreal can get in a bad state if another user changed the children of a component after the transaction was
     * recorded and we undo or redo the transaction, causing a component's child list to be incorrect. We call this
     * before a transaction to store the components of a transaction and their children in private member arrays we
     * can use to correct the bad state after the transaction. Unreal can also partially recreate actors in a
     * transaction that were deleted by another user, so this records the actors in the transaction that are deleted
     * so we can redelete them after the transaction.
     *
     * @param   const FTransaction* transactionPtr
     */
    void RecordPreTransactionState(const FTransaction* transactionPtr);

    /**
     * Checks for and corrects bad state in the child lists of components affected by a transaction.
     */
    void FixTransactedComponentChildren();

    /**
     * Destroys actors that were recreated by a transaction but aren't showing in the world outliner. This happens when
     * an actor in the transaction was destroyed by another user.
     *
     * @param   const TArray<UObject*>& objects in the transaction.
     */
    void DestroyUnwantedActors(const TArray<UObject*>& objects);

    /**
     * Called when a transaction is undone or redone. Sends changes made by the transaction to the server, or reverts
     * changed values to server values for locked objects.
     *
     * @param   bool isUndo
     */
    void OnUndoRedo(bool isUndo);
};