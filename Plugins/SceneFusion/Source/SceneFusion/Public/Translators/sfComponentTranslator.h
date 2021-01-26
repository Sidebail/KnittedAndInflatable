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
#include <sfSession.h>
#include <unordered_map>
#include "sfBaseUObjectTranslator.h"
#include "sfActorTranslator.h"

using namespace KS::SceneFusion2;

class sfComponentTranslator : public sfBaseUObjectTranslator
{
public:
    friend class sfActorTranslator;
    friend class sfUndoManager;
    friend class UsfMissingComponent;
    friend class UsfMissingSceneComponent;

    /**
     * Callback to initialize a component or sfObject.
     *
     * @param   sfObject::SPtr
     * @param   UActorComponent*
     */
    typedef std::function<void(sfObject::SPtr, UActorComponent*)> Initializer;

    /**
     * Callback for when a component is deleted by another user.
     *
     * @param   sfObject::SPtr for the component. May be nullptr.
     * @param   UActorComponent* that was deleted.
     */
    typedef std::function<void(sfObject::SPtr, UActorComponent*)> DeleteHandler;

    /**
     * Callback for when a component is moved from one actor to another.
     *
     * @param   sfObject::SPtr for the component.
     * @param   USceneComponent* whose owner changed.
     * @param   AActor* new owner for the component.
     */
    typedef std::function<void(sfObject::SPtr, USceneComponent*, AActor*)> OwnerChangeHandler;

    /**
     * Callback for when a component's transform changes.
     *
     * @param   sfObject::SPtr for the component.
     * @param   USceneComponent* whose transform changed.
     */
    typedef std::function<void(sfObject::SPtr, USceneComponent*)> TransformChangeHandler;

    /**
     * Constructor
     */
    sfComponentTranslator();

    /**
     * Destructor
     */
    virtual ~sfComponentTranslator();

    /**
     * Initialization. Called after connecting to a session.
     */
    virtual void Initialize() override;

    /**
     * Deinitialization. Called after disconnecting from a session.
     */
    virtual void CleanUp() override;

    /**
     * Checks if a component is syncable.
     *
     * @param   bool allowPendingKill - if true, components that are pending kill are considered syncable.
     * @return  bool true if the component is syncable.
     */
    bool IsSyncable(UActorComponent* componentPtr, bool allowPendingKill = false);

    /**
     * Sends a new transform value to the server, or applies the server value to the component if the object is locked.
     *
     * @param   USceneComponent* componentPtr to sync transform for.
     * @param   bool applyServerValues - if true, applies server values to the component even if the object is
     *          unlocked.
     */
    void SyncTransform(USceneComponent* componentPtr, bool applyServerValues = false);

    /**
     * Checks for and sends transform changes for all components in an actor.
     *
     * @param   AActor* actorPtr to send transform update for.
     */
    void SyncComponentTransforms(AActor* actorPtr);

    /**
     * Checks for new, deleted, renamed, and reparented components and sends changes to the server, or reverts to the
     * server state if the actor is locked.
     *
     * @param   AActor* actorPtr to sync components for.
     */
    void SyncComponents(AActor* actorPtr);

    /**
     * Registers a function to initialize components of type T.
     *
     * @param   Initializer initializer to register.
     */
    template<typename T>
    void RegisterComponentInitializer(Initializer initializer)
    {
        m_componentInitializers.emplace(T::StaticClass(), initializer);
    }

    /**
     * Unregisters the actor initializer for components of type T.
     */
    template<typename T>
    void UnregisterComponentInitializer()
    {
        m_componentInitializers.erase(T::StaticClass());
    }

    /**
     * Registers a function to call when creating sfObjects for components of type T.
     *
     * @param   Initializer initializer to register.
     */
    template<typename T>
    void RegisterObjectInitializer(Initializer initializer)
    {
        m_objectInitializers.emplace(T::StaticClass(), initializer);
    }

    /**
     * Unregisters the object initializer for components of type T.
     */
    template<typename T>
    void UnregisterObjectInitializer()
    {
        m_objectInitializers.erase(T::StaticClass());
    }

    /**
     * Registers a function to call when components of type T are deleted by other users.
     *
     * @param   DeleteHandler handler to register.
     */
    template<typename T>
    void RegisterDeleteHandler(DeleteHandler handler)
    {
        m_deleteHandlers.emplace(T::StaticClass(), handler);
    }

    /**
     * Unregisters the delete handler for components of type T.
     */
    template<typename T>
    void UnregisterDeleteHandler()
    {
        m_deleteHandlers.erase(T::StaticClass());
    }

    /**
     * Registers a function to call when components of type T are destroyed locally.
     *
     * @param   DestroyHandler handler to register.
     */
    template<typename T>
    void RegisterDestroyHandler(DeleteHandler handler)
    {
        m_destroyHandlers.emplace(T::StaticClass(), handler);
    }

    /**
     * Unregisters the destroy handler for components of type T.
     */
    template<typename T>
    void UnregisterDestroyHandler()
    {
        m_destroyHandlers.erase(T::StaticClass());
    }

    /**
     * Registers a function to call before components of type T have their owner changed by another user.
     *
     * @param   OwnerChangeHandler handler to register.
     */
    template<typename T>
    void RegisterPreOwnerChangeHandler(OwnerChangeHandler handler)
    {
        m_preOwnerChangeHandlers.emplace(T::StaticClass(), handler);
    }

    /**
     * Unregisters the pre owner change handler for components of type T.
     */
    template<typename T>
    void UnregisterPreOwnerChangeHandler()
    {
        m_preOwnerChangeHandlers.erase(T::StaticClass());
    }

    /**
     * Registers a function to call after components of type T have their owner changed by another user.
     *
     * @param   OwnerChangeHandler handler to register.
     */
    template<typename T>
    void RegisterPostOwnerChangeHandler(OwnerChangeHandler handler)
    {
        m_postOwnerChangeHandlers.emplace(T::StaticClass(), handler);
    }

    /**
     * Unregisters the pre owner change handler for components of type T.
     */
    template<typename T>
    void UnregisterPostOwnerChangeHandler()
    {
        m_postOwnerChangeHandlers.erase(T::StaticClass());
    }

    /**
     * Registers a function to call when components of type T have their transform changed by other users.
     *
     * @param   TransformChangeHandler handler to register.
     */
    template<typename T>
    void RegisterTransformChangeHandler(TransformChangeHandler handler)
    {
        m_transformChangeHandlers.emplace(T::StaticClass(), handler);
    }

    /**
     * Unregisters the transform change handler for components of type T.
     */
    template<typename T>
    void UnregisterTransformChangeHandler()
    {
        m_transformChangeHandlers.erase(T::StaticClass());
    }

private:
    sfSession::SPtr m_sessionPtr;
    TSharedPtr<sfActorTranslator> m_actorTranslatorPtr;
    FDelegateHandle m_onApplyObjectToActorHandle;
    FDelegateHandle m_tickHandle;
    FDelegateHandle m_onDeselectHandle;
    FDelegateHandle m_onMoveHandle;
    std::unordered_map<UClass*, Initializer> m_componentInitializers;
    std::unordered_map<UClass*, Initializer> m_objectInitializers;
    std::unordered_map<UClass*, DeleteHandler> m_deleteHandlers;
    std::unordered_map<UClass*, DeleteHandler> m_destroyHandlers;
    std::unordered_map<UClass*, OwnerChangeHandler> m_preOwnerChangeHandlers;
    std::unordered_map<UClass*, OwnerChangeHandler> m_postOwnerChangeHandlers;
    std::unordered_map<UClass*, TransformChangeHandler> m_transformChangeHandlers;
    TSet<AActor*> m_syncComponentsList;
    TSet<USceneComponent*> m_transformChangedSet;

    /**
     * Updates the component translator.
     *
     * @param   float deltaTime in seconds since last tick.
     */
    void Tick(float deltaTime);

    /**
     * Creates an sfObject for a component and uploads it to the server.
     *
     * @param UActorComponent* componentPtr
     */
    void Upload(UActorComponent* componentPtr);

    /**
     * Creates an sfObject for a uobject. Does not upload or create properties for the object.
     *
     * @param   UObject* uobjPtr to create sfObject for.
     * @param   sfObject::SPtr* outObjPtr created for the uobject.
     * @return  bool true if the uobject was handled by this translator.
     */
    virtual bool Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr) override;

    /**
     * Recursively creates sfObjects for a component and its children.
     *
     * @param   UActorComponent* componentPtr to create object for.
     * @return  sfObject::SPtr object for the actor.
     */
    sfObject::SPtr CreateObject(UActorComponent* componentPtr);

    /**
     * Creates or finds a component for an object and initializes it with server values. Recursively initializes
     * children for child objects.
     *
     * @param   AActor* actorPtr to add component to.
     * @param   sfObject::SPtr objPtr to initialize component for.
     * @return  UActorComponent* component for the object.
     */
    UActorComponent* InitializeComponent(AActor* actorPtr, sfObject::SPtr objPtr);

    /**
     * Iterates an object and its descendents, looking for children whose objects are not attached and
     * attaches those objects.
     *
     * @param   const std::list<sfObject::SPtr>& objects
     */
    void FindAndAttachChildren(sfObject::SPtr objPtr);

    /**
     * Registers property change handlers for server events.
     */
    void RegisterPropertyChangeHandlers();

    /**
     * Recursively iterates the component children of an object, and recreates destroyed components.
     *
     * @param sfObject::SPtr objPtr to restore deleted component children for.
     */
    void RestoreDeletedComponents(sfObject::SPtr objPtr);

    /**
     * Recursively iterates the component children of an object, looking for destroyed components and deletes their
     * corresponding objects.
     *
     * @param   sfObject::SPtr objPtr to check for deleted child components.
     */
    void FindDeletedComponents(sfObject::SPtr objPtr);

    /**
     * Calls the delete handler for a component's class, if one is registered.
     *
     * @param   sfObject::SPtr objPtr for the deleted component. Null if the component was not synced.
     * @param   UActorComponent* componentPtr that was deleted.
     */
    void CallDeleteHandler(sfObject::SPtr objPtr, UActorComponent* componentPtr);

    /**
     * Calls the destroy handler for a component's class, if one is registered.
     *
     * @param   sfObject::SPtr objPtr for the destroyed component.
     * @param   UActorComponent* componentPtr that was destroyed.
     */
    void CallDestroyHandler(sfObject::SPtr objPtr, UActorComponent* componentPtr);

    /**
     * Sets the parent of a component on the server, or moves the component back to it's server parent if the component
     * is locked.
     *
     * @param   AActor* actorPtr the component belongs to.
     * @param   UActorComponent* componentPtr to sync parent for.
     * @param   sfObject::SPtr objPtr for the component.
     */
    void SyncParent(AActor* actorPtr, UActorComponent* componentPtr, sfObject::SPtr objPtr);

    /**
     * Called when a component is created by another user.
     *
     * @param   sfObject::SPtr objPtr that was created.
     * @param   int childIndex of new object. -1 if object is a root.
     */
    virtual void OnCreate(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Called when a component is deleted by another user.
     *
     * @param   sfObject::SPtr objPtr that was deleted.
     */
    virtual void OnDelete(sfObject::SPtr objPtr) override;

    /**
     * Called when a component's parent is changed by another user.
     *
     * @param   sfObject::SPtr objPtr whose parent changed.
     * @param   int childIndex of the object. -1 if the object is a root.
     */
    virtual void OnParentChange(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Called when a component property changes.
     *
     * @param   sfProperty::SPtr propertyPtr that changed.
     */
    virtual void OnPropertyChange(sfProperty::SPtr propertyPtr) override;

    /**
     * Called when a field is removed from a dictionary property.
     *
     * @param   sfDictionaryProperty::SPtr dictPtr the field was removed from.
     * @param   const sfName& name of removed field.
     */
    virtual void OnRemoveField(sfDictionaryProperty::SPtr dictPtr, const sfName& name) override;

    /**
     * Called when a property on an component changes.
     *
     * @param   sfObject::SPtr objPtr for the component whose property changed.
     * @param   UObject* uobjPtr whose property changed.
     * @param   UnrealProperty* upropPtr that changed.
     * @return  bool false if the property change event should be handled by the default handler.
     */
    virtual bool OnUPropertyChange(sfObject::SPtr objPtr, UObject* uobjPtr, UnrealProperty* upropPtr) override;

    /**
     * Called when an object is modified by an undo or redo transaction.
     *
     * @param   sfObject::SPtr objPtr for the uobject that was modified. nullptr if the uobjPtr is not synced.
     * @param   UObject* uobjPtr that was modified.
     * @return  bool true if event was handled and need not be processed by other handlers.
     */
    virtual bool OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr) override;

    /**
     * Called when attempting to apply an object to an actor via drag drop. Sends components material changes
     * to the server, or reverts it to the server value if the actor is locked.
     *
     * @param   UObject* uobjPtr
     * @param   AActor* actorPtr
     */
    void OnApplyObjectToActor(UObject* uobjPtr, AActor* actorPtr);
};
