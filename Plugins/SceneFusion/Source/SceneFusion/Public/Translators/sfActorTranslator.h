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

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>
#include <sfObject.h>
#include <sfSession.h>
#include <sfValueProperty.h>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <Editor/UnrealEd/Classes/Editor/TransBuffer.h>
#include <Layers/Layer.h>
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 23
#include <Layers/ILayers.h>
#else
#include <Layers/LayersSubsystem.h>
#endif

#include "sfBaseUObjectTranslator.h"
#include "../sfUPropertyInstance.h"
#include "../Consts.h"

using namespace KS::SceneFusion2;
using namespace KS;

/**
 * Manages actor syncing.
 */
class sfActorTranslator : public sfBaseUObjectTranslator
{
public:
    friend class sfBlueprintTranslator;
    friend class sfLevelTranslator;
    friend class sfComponentTranslator;
    friend class sfUObjectTranslator;
    friend class sfModelTranslator;
    friend class AsfMissingActor;
    friend class sfUndoManager;
    friend class SceneFusion;

    /**
     * Types of lock.
     */
    enum LockType
    {
        NotSynced,
        Unlocked,
        PartiallyLocked,
        FullyLocked
    };

    /**
     * Callback to initialize an actor or its sfObject.
     *
     * @param   sfObject::SPtr
     * @param   AActor*
     */
    typedef std::function<void(sfObject::SPtr, AActor*)> Initializer;

    /**
     * Delegate for lock state change.
     *
     * @param   AActor* - pointer to the actor whose lock state changed
     * @param   LockType - lock type
     * @param   sfUser::SPtr - lock owner
     */
    DECLARE_MULTICAST_DELEGATE_ThreeParams(OnLockStateChangeDelegate, AActor*, LockType, sfUser::SPtr);

    /**
     * Lock state change event handler.
     */
    OnLockStateChangeDelegate OnLockStateChange;

    /**
     * Delegate for when the actor limit is reached.
     */
    DECLARE_MULTICAST_DELEGATE(OnReachActorLimitDelegate);

    /**
     * Invoked when the actor limit is first reached.
     */
    OnReachActorLimitDelegate OnReachActorLimit;

    /**
     * Constructor
     */
    sfActorTranslator();

    /**
     * Destructor
     */
    virtual ~sfActorTranslator();

    /**
     * Initialization. Called after connecting to a session.
     */
    virtual void Initialize() override;

    /**
     * Deinitialization. Called after disconnecting from a session.
     */
    virtual void CleanUp() override;

    /**
     * Checks if an actor can be synced.
     *
     * @param   AActor* actorPtr
     * @param   bool allowPendingKill - if true, actors that are pending kill are considered syncable.
     * @return  bool true if the actor can be synced.
     */
    bool IsSyncable(AActor* actorPtr, bool allowPendingKill = false);

    /**
     * @return  bool true if the actor is listed in the world outliner.
     */
    inline bool IsListedinWorldOutliner(AActor* actorPtr);

    /**
     * Checks if an actor is spawned by the UChildComponent.
     *
     * @param   AActor* actorPtr
     * @return  bool true if the actor is a child actor.
     */
    bool IsChildActor(AActor* actorPtr);

    /**
     * Registers a function to initialize actors of type T.
     *
     * @param   Initializer initializer to register.
     */
    template<typename T>
    void RegisterActorInitializer(Initializer initializer)
    {
        UnregisterActorInitializer<T>();
        m_actorInitializers.emplace(T::StaticClass(), initializer);
    }

    /**
     * Unregisters the actor initializer for actors of type T.
     */
    template<typename T>
    void UnregisterActorInitializer()
    {
        m_actorInitializers.erase(T::StaticClass());
    }

    /**
     * Registers a function to call when creating sfObjects for actors of type T.
     *
     * @param   Initializer initializer funcion to register.
     */
    template<typename T>
    void RegisterObjectInitializer(Initializer initializer)
    {
        UnregisterObjectInitializer<T>();
        m_objectInitializers.emplace(T::StaticClass(), initializer);
    }

    /**
     * Unregisters the object initializer for actors of type T.
     */
    template<typename T>
    void UnregisterObjectInitializer()
    {
        m_objectInitializers.erase(T::StaticClass());
    }

    /**
     * Registers an actor type to sync hidden instances for. By default hidden actors are not synced, but hidden actors
     * of this type will be synced.
     */
    template<typename T>
    void RegisterHiddenSyncType()
    {
        m_hiddenSyncTypes.Add(T::StaticClass());
    }

    /**
     * Unregisters an actor type to sync hidden instances for.
     */
    template<typename T>
    void UnregisterHiddenSyncType()
    {
        m_hiddenSyncTypes.Remove(T::StaticClass());
    }

    /**
     * Checks the selected actors for lock location property changes.
     */
    void CheckLockLocationChanges();

    /**
     * Called when a layer is modified. Marks the actors whose layers changed as dirty so that the translator can
     * send the changes to the server in the next tick. It is done this way because the event may be triggered
     * multiple times in the same frame.
     *
     * @param   const ELayersAction::Type action
     * @param   const TWeakObjectPtr<ULayer>& changedLayer
     * @param   const FName& changedProperty
     */
    void OnLayersChanged(
        const ELayersAction::Type action,
        const TWeakObjectPtr<ULayer>& changedLayer,
        const FName& changedProperty);

private:
    FDelegateHandle m_onActorAddedHandle;
    FDelegateHandle m_onActorDeletedHandle;
    FDelegateHandle m_onActorAttachedHandle;
    FDelegateHandle m_onActorDetachedHandle;
    FDelegateHandle m_onFolderChangeHandle;
    FDelegateHandle m_onLabelChangeHandle;
    FDelegateHandle m_onSelectHandle;
    FDelegateHandle m_onDeselectHandle;
    FDelegateHandle m_tickHandle;

    TArray<AActor*> m_uploadList;
    std::unordered_set<sfObject::SPtr> m_recreateSet;
    TQueue<AActor*> m_revertFolderQueue;
    TArray<AActor*> m_syncParentList;
    TArray<FString> m_foldersToCheck;
    TSet<AActor*> m_actorsToCheckLayerChange;
    TMap<AActor*, sfListProperty::SPtr> m_actorsToApplyLayerChange;

    std::unordered_map<UClass*, Initializer> m_actorInitializers;
    std::unordered_map<UClass*, Initializer> m_objectInitializers;
    TSet<UClass*> m_hiddenSyncTypes;
    sfSession::SPtr m_sessionPtr;
    bool m_collectGarbage;
    bool m_reachedActorLimit;
    float m_bspRebuildDelay;

#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 23
    TSharedPtr<ILayers> m_worldLayersPtr;
#else
    ULayersSubsystem* m_worldLayersPtr;
#endif

    /**
     * Updates the actor translator.
     *
     * @param   float deltaTime in seconds since last tick.
     */
    void Tick(float deltaTime);

    /**
     * Checks if we should sync hidden actors of the given class.
     *
     * @param   UClass* classPtr to check.
     * @return  bool true if hidden actors of the given class should be synced.
     */
    bool IsHiddenSyncType(UClass* classPtr);

    /**
     * Destroys an actor.
     *
     * @param   AActor* actorPtr to destroy.
     */
    void DestroyActor(AActor* actorPtr);

    /**
     * Destroys actors that don't exist on the server in the given level.
     *
     * @param   ULevel* levelPtr - level to check
     */
    void DestroyUnsyncedActorsInLevel(ULevel* levelPtr);

    /**
     * Destroys components of an actor that don't exist on the server. Moves components that belong to another actor on
     * the server back to the server's actor.
     *
     * @param   AActor* actorPtr to destroy unsynced components for.
     */
    void DestroyUnsyncedComponents(AActor* actorPtr);

    /**
     * Reverts folders to server values for actors whose folder changed while locked.
     */
    void RevertLockedFolders();

    /**
     * Recreates actors that were deleted while locked.
     */
    void RecreateLockedActors();

    /**
     * Deletes folders that were emptied by other users.
     */
    void DeleteEmptyFolders();

    /**
     * Recursively removes child components of a deleted actor from the sfObjectMap. Optionally removes child actors or
     * adds them as children of a level object. Optionally removes child uobjects or stores them for reuse if their
     * outer is recreated.
     *
     * @param   sfObject::SPtr objPtr to recursively cleanup children for.
     * @param   sfObject::SPtr levelObjPtr to add actor child objects to. If provided, child uobjects are stored to be
     *          resused if their outers are recreated. Not used if recurseChildActors is true.
     * @param   bool recurseChildActors - if true, child actors and their descendants will also be removed from the
     *          sfObjectMap.
     */
    void CleanUpChildrenOfDeletedObject(
        sfObject::SPtr objPtr,
        sfObject::SPtr levelObjPtr = nullptr,
        bool recurseChildActors = false);

    /**
     * Calls the actor initializer for the actor's class, if one is registered.
     *
     * @param   sfObject::SPtr objPtr for the actor.
     * @param   AActor* actorPtr
     */
    void CallActorInitializer(sfObject::SPtr objPtr, AActor* actorPtr);

    /**
     * Calls the object initializer for the actor's class, if one is registered.
     *
     * @param   sfObject::SPtr objPtr for the actor.
     * @param   AActor* actorPtr
     */
    void CallObjectInitializer(sfObject::SPtr objPtr, AActor* actorPtr);

    /**
     * Called when an actor is added to the level.
     *
     * @param   AActor* actorPtr
     */
    void OnActorAdded(AActor* actorPtr);

    /**
     * Called when an actor is deleted from the level.
     *
     * @param   AActor* actorPtr
     */
    void OnActorDeleted(AActor* actorPtr);

    /**
     * Called when an actor is attached to or detached from another actor.
     *
     * @param   AActor* actorPtr
     * @param   const AActor* parentPtr
     */
    void OnAttachDetach(AActor* actorPtr, const AActor* parentPtr);

    /**
     * Called when an actor's folder changes.
     *
     * @param   const AActor* actorPtr
     * @param   FName oldFolder
     */
    void OnFolderChange(const AActor* actorPtr, FName oldFolder);

    /**
     * Called when an actor's label changes.
     *
     * @param   const AActor* actorPtr
     */
    void OnLabelChanged(AActor* actorPtr);

    /**
     * Called when an actor is selected. Sends a lock request for the actor.
     *
     * @param   AActor* actorPtr that was selected.
     */
    void OnSelect(AActor* actorPtr);

    /**
     * Called when an actor is deselected. Releases the lock on the actor.
     *
     * @param   AActor actorPtr that was deselected.
     */
    void OnDeselect(AActor* actorPtr);

    /**
     * Called for each actor in an undo delete transaction, or redo create transaction. Recreates the actor on the
     * server, or deletes the actor if another actor with the same name already exists.
     *
     * @param   AActor* actorPtr
     */
    void OnUndoDelete(AActor* actorPtr);

    /**
     * Adds an sfObject to the set of objects to recreate actors for.
     *
     * @param   sfObject::SPtr objPtr
     */
    void RecreateActor(sfObject::SPtr objPtr);

    /**
     * Sends new label and name values to the server, or reverts to the server values if the actor is locked.
     *
     * @param   AActor* actorPtr to sync label and name for.
     * @param   sfObject::SPtr objPtr for the actor.
     * @param   sfDictionaryProperty::SPtr propertiesPtr for the actor.
     */
    void SyncLabelAndName(AActor* actorPtr, sfObject::SPtr objPtr, sfDictionaryProperty::SPtr propertiesPtr);

    /**
     * Sends a new folder value to the server, or reverts to the server value if the actor is locked.
     *
     * @param   AActor* actorPtr to sync folder for.
     * @param   sfObject::SPtr objPtr for the actor.
     * @param   sfDictionaryProperty::SPtr propertiesPtr for the actor.
     */
    void SyncFolder(AActor* actorPtr, sfObject::SPtr objPtr, sfDictionaryProperty::SPtr propertiesPtr);

    /**
     * Sends a new parent value to the server, or reverts to the server value if the actor or new parent are locked.
     *
     * @param   AActor* actorPtr to sync parent for.
     * @param   sfObject::SPtr objPtr for the actor.
     */
    void SyncParent(AActor* actorPtr, sfObject::SPtr objPtr);

    /**
     * Creates actor objects on the server.
     *
     * @param   TArray<AActor*>& actors to upload. Actors will be removed from the list, unless we could not upload the
     *          actor because it was deleted and recreated (using undo) and the server hasn't acknowledged the delete
     *          yet.
     */
    void UploadActors(TArray<AActor*>& actors);

    /**
     * Uploads actor objects to the server, or deletes them if creating them exceeds the actor limit.
     *
     * @param   std::list<sfObject::SPtr> objects to upload.
     * @param   int numActors being uploaded.
     * @param   uint32_t limit - number of actors allowed on the server.
     * @param   sfObject::SPtr parentPtr for the objects.
     * @param   ULevel* levelPtr the actors are in.
     */
    void Upload(
        std::list<sfObject::SPtr>& objects,
        int numActors, uint32_t limit,
        sfObject::SPtr parentPtr,
        ULevel* levelPtr);

    /**
     * Creates an sfObject for a uobject. Does not upload or create properties for the object.
     *
     * @param   UObject* uobjPtr to create sfObject for.
     * @param   sfObject::SPtr* outObjPtr created for the uobject.
     * @return  bool true if the uobject was handled by this translator.
     */
    virtual bool Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr) override;

    /**
     * Recursively creates actor objects for an actor and its children.
     *
     * @param   AActor* actorPtr to create object for.
     * @return  sfObject::SPtr object for the actor.
     */
    sfObject::SPtr CreateObject(AActor* actorPtr);

    /**
     * Creates or finds an actor for an object and initializes it with server values. Recursively initializes child
     * actors for child objects.
     *
     * @param   sfObject::SPtr objPtr to initialize actor for.
     * @param   ULevel* levelPtr the actor belongs to.
     * @return  AActor* actor for the object.
     */
    AActor* InitializeActor(sfObject::SPtr objPtr, ULevel* levelPtr);

    /**
     * Iterates a list of objects and their descendants, looking for child actors whose objects are not attached and
     * attaches those objects.
     *
     * @param   const std::list<sfObject::SPtr>& objects
     */
    void FindAndAttachChildren(const std::list<sfObject::SPtr>& objects);

    /**
     * Registers property change handlers for server events.
     */
    void RegisterPropertyChangeHandlers();

    /**
     * Locks an actor.
     * 
     * @param   AActor* actorPtr
     * @param   sfObject::SPtr objPtr for the actor.
     */
    void Lock(AActor* actorPtr, sfObject::SPtr objPtr);

    /**
     * Unlocks an actor.
     *
     * @param   AActor* actorPtr
     */
    void Unlock(AActor* actorPtr);

    /**
     * Called when an actor is created by another user.
     *
     * @param   sfObject::SPtr objPtr that was created.
     * @param   int childIndex of new object. -1 if object is a root
     */
    virtual void OnCreate(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Called when an actor is deleted by another user.
     *
     * @param   sfObject::SPtr objPtr that was deleted.
     */
    virtual void OnDelete(sfObject::SPtr objPtr) override;

    /**
     * Called when a locally-deleted actor is confirmed as deleted.
     *
     * @param   sfObject::SPtr objPtr that was confirmed as deleted.
     */
    virtual void OnConfirmDelete(sfObject::SPtr objPtr) override;

    /**
     * Called when an actor is locked by another user.
     *
     * @param   sfObject::SPtr objPtr that was locked.
     */
    virtual void OnLock(sfObject::SPtr objPtr) override;

    /**
     * Called when an actor is unlocked by another user.
     *
     * @param   sfObject::SPtr objPtr that was unlocked.
     */
    virtual void OnUnlock(sfObject::SPtr objPtr) override;

    /**
     * Called when an actor's lock owner changes.
     *
     * @param   sfObject::SPtr objPtr whose lock owner changed.
     */
    virtual void OnLockOwnerChange(sfObject::SPtr objPtr) override;

    /**
     * Called when an actor's parent is changed by another user.
     *
     * @param   sfObject::SPtr objPtr whose parent changed.
     * @param   int childIndex of the object. -1 if the object is a root.
     */
    virtual void OnParentChange(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Called when an object is modified by an undo or redo transaction.
     *
     * @param   sfObject::SPtr objPtr for the uobject that was modified. nullptr if the uobjPtr is not synced.
     * @param   UObject* uobjPtr that was modified.
     * @return  bool true if event was handled and need not be processed by other handlers.
     */
    virtual bool OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr) override;

    /**
     * Enables the actor attached and dettached event handler.
     */
    void EnableParentChangeHandler();

    /**
     * Disables the actor attached and dettached event handler.
     */
    void DisableParentChangeHandler();

    /**
     * Calls OnLockStateChange event handlers.
     *
     * @param   sfObject::SPtr objPtr whose lock state changed
     * @param   AActor* actorPtr
     */
    void InvokeOnLockStateChange(sfObject::SPtr objPtr, AActor* actorPtr);

    /**
     * Clears collections of actors.
     */
    void ClearActorCollections();

    /**
     * Deletes all actors in the given level.
     *
     * @param   sfObject::SPtr levelObjPtr
     * @param   ULevel* levelPtr
     */
    void OnRemoveLevel(sfObject::SPtr levelObjPtr, ULevel* levelPtr);

    /**
     * Calls OnCreate on every child of the given level sfObject. Destroys all unsynced actors after.
     *
     * @param   sfObject::SPtr sfLevelObjPtr
     * @param   ULevel* levelPtr
     */
    void OnSFLevelObjectCreate(sfObject::SPtr sfLevelObjPtr, ULevel* levelPtr);

    /**
     * Detaches the given actor from its parent if the given sfObject's parent is a level object and returns true.
     * Otherwise, returns false.
     *
     * @param   sfObject::SPtr objPtr
     * @param   AActor* actorPtr
     * @return  bool
     */
    bool DetachIfParentIsLevel(sfObject::SPtr objPtr, AActor* actorPtr);

    /**
     * Logs out an error that the given sfObject has no parent and then leaves the session.
     *
     * @param   sfObject::SPtr objPtr
     */
    void LogNoParentErrorAndDisconnect(sfObject::SPtr objPtr);

    /**
     * Sends layer changes to the server.
     */
    void SendLayersChange();

    /**
     * Applies server layer changes to all actors whose layers changed on the server.
     * Since the OnPropertyChange event get triggered on every list element but we want to apply all the changes at
     * the same time, so we queue the actors whose layer changed on the server and apply the changes in the next tick.
     */
    void ApplyLayersChange();

    /**
     * Applies server layer changes on the given actor.
     *
     * @param   AActor* actorPtr to apply layer changes on
     * @param   sfListProperty::SPtr layersPropPtr
     */
    void SetLayers(AActor* actorPtr, sfListProperty::SPtr layersPropPtr);
};
