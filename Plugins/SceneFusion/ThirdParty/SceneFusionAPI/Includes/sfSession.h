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
#include <string>
#include <memory>
#include <ksEvent.h>
#include <Exports.h>
#include "sfUser.h"
#include "sfObject.h"
#include "sfProperty.h"
#include "sfDictionaryProperty.h"
#include "sfListProperty.h"
#include "sfReferenceProperty.h"

namespace KS {
namespace SceneFusion2 {

    /**
     * Provides functions for accessing and manipulating synced objects and session data for a Scene Fusion session.
     */
    class EXTERNAL sfSession : 
        public std::enable_shared_from_this<sfSession>
    {
    public:
        typedef std::shared_ptr<sfSession> SPtr;

        /**
         * Create event handler.
         *
         * @param   sfObject::SPtr - object that was created.
         * @param   int - child index the object was inserted at. -1 if this is a root object.
         */
        typedef std::function<void(sfObject::SPtr objPtr, int childIndex)> CreateHandler;

        /**
         * Delete event handler.
         *
         * @param   sfObject::SPtr - object that was deleted.
         */
        typedef std::function<void(sfObject::SPtr objPtr)> DeleteHandler;

        /**
         * Confirm delete event handler.
         *
         * @param   sfObject::SPtr - object whose deletion was confirmed.
         */
        typedef std::function<void(sfObject::SPtr objPtr)> ConfirmDeleteHandler;

        /**
         * Lock event handler.
         *
         * @param   sfObject::SPtr - object that became locked.
         */
        typedef std::function<void(sfObject::SPtr objPtr)> LockHandler;

        /**
         * Unlock event handler.
         *
         * @param   sfObject::SPtr - object that became unlocked.
         */
        typedef std::function<void(sfObject::SPtr objPtr)> UnlockHandler;

        /**
         * Lock owner change event handler.
         *
         * @param   sfObject::SPtr - object whose lock owner changed.
         */
        typedef std::function<void(sfObject::SPtr objPtr)> LockOwnerChangeHandler;

        /**
         * Direct lock change event handler.
         *
         * @param   sfObject::SPtr - object whose direct lock owner changed.
         */
        typedef std::function<void(sfObject::SPtr objPtr)> DirectLockChangeHandler;

        /**
         * Remove handler for a dictionary property.
         *
         * @param   sfDictionaryProperty::SPtr - dict the field was removed from.
         * @param   const sfName& - key of removed field.
         */
        typedef std::function<void(sfDictionaryProperty::SPtr dictPtr, const sfName& key)> DictionaryRemoveHandler;

        /**
         * Parent change event handler.
         *
         * @param   sfObject::SPtr - object whose parent changed.
         * @param   int - childIndex of the object. -1 if the object has no parent.
         */
        typedef std::function<void(sfObject::SPtr objPtr, int childIndex)> ParentChangeHandler;

        /**
         * Property change event handler.
         *
         * @param   sfProperty::SPtr - property that changed.
         */
        typedef std::function<void(sfProperty::SPtr propertyPtr)> PropertyChangeHandler;

        /**
         * List add event handler.
         *
         * @param   sfListProperty::SPtr - list that elements were added to.
         * @param   int - index elements were inserted at.
         * @param   int - count - number of elements added.
         */
        typedef std::function<void(sfListProperty::SPtr listPtr, int index, int count)> ListAddHandler;

        /**
         * List remove event handler.
         *
         * @param   sfListProperty::SPtr - list that elements were removed from.
         * @param   int - index elements were removed from.
         * @param   int - count - number of elements removed.
         */
        typedef std::function<void(sfListProperty::SPtr listPtr, int index, int count)> ListRemoveHandler;

        /**
         * User join event handler.
         *
         * @param   sfUser::SPtr - user who joined the session.
         */
        typedef std::function<void(sfUser::SPtr userPtr)> UserJoinHandler;

        /**
         * User leave event handler.
         *
         * @param   sfUser::SPtr - user who left the session.
         */
        typedef std::function<void(sfUser::SPtr userPtr)> UserLeaveHandler;

        /**
         * User color change event handler.
         *
         * @param   sfUser::SPtr - user whose color changed.
         */
        typedef std::function<void(sfUser::SPtr userPtr)> UserColorChangeHandler;

        /**
         * Acknowledge subscription event handler.
         *
         * @param   bool isSubscription - if true, it was subscription. Otherwise, it was unsubscription.
         * @param   sfObject::SPtr - parent of children that the user subscribed to
         */
        typedef std::function<void(bool, sfObject::SPtr objPtr)> AcknowledgeSubscriptionHandler;

        /**
         * Object event.
         *
         * @param   sfObject::SPtr& - object
         */
        typedef ksEvent<sfObject::SPtr&>::SPtr ObjectEventHandle;

        /**
         * Object event that affected index in parent.
         *
         * @param   sfObject::SPtr& - object
         * @param   sfListProperty::SPtr& - index of object in its parent
         */
        typedef ksEvent<sfObject::SPtr&, int&>::SPtr ObjectIndexEventHandle;

        /**
         * User event.
         *
         * @param   sfUser::SPtr& - user
         */
        typedef ksEvent<sfUser::SPtr&>::SPtr UserEventHandle;

        /**
         * Property event.
         *
         * @param   sfListProperty::SPtr& - property
         */
        typedef ksEvent<sfProperty::SPtr&>::SPtr PropertyEventHandle;

        /**
         * List property event.
         *
         * @param   sfListProperty::SPtr& - property
         * @param   int& - index of the first modified property
         * @param   int& - number of modified properties
         */
        typedef ksEvent<sfListProperty::SPtr&, int&, int&>::SPtr ListPropertyEventHandle;
        
        /**
         * Dictionary property event.
         *
         * @param   sfDictionaryProperty::SPtr& - property
         * @param   sfName& - key of the modified property
         */
        typedef ksEvent<sfDictionaryProperty::SPtr&, sfName&>::SPtr DictionaryPropertyEventHandle;

        /**
         * Acknowledge subscription event.
         *
         * @param   bool& isSubscription - if true, it was subscription. Otherwise, it was unsubscription.
         * @param   sfObject::SPtr& - object
         */
        typedef ksEvent<bool&, sfObject::SPtr&>::SPtr AcknowledgeSubscriptionEventHandle;

        /**
         * Destructor
         */
        virtual ~sfSession() {}

        /**
         * Registers an on create event handler that is invoked when an object is created by another user,
         * or when an object we attempted to delete could not be deleted. When an object with children is created,
         * this is not invoked for the children.
         *
         * @param   CreateHandler
         * @return  ObjectIndexEventHandle
         */
        virtual ObjectIndexEventHandle RegisterOnCreateHandler(CreateHandler handler) = 0;

        /**
         * Unregisters an on create event handler.
         *
         * @param   ObjectIndexEventHandle
         */
        virtual void UnregisterOnCreateHandler(ObjectIndexEventHandle eventPtr) = 0;

        /**
         * Registers an on delete event handler that is invoked when an object is deleted by another user,
         * or when an object we attempted to create could not be created. When an object with children is deleted,
         * this is not invoked for the children.
         *
         * @param   DeleteHandler
         * @return  ObjectEventHandle
         */
        virtual ObjectEventHandle RegisterOnDeleteHandler(DeleteHandler handler) = 0;

        /**
         * Unregisters an on delete event handler.
         *
         * @param   ObjectEventHandle
         */
        virtual void UnregisterOnDeleteHandler(ObjectEventHandle eventPtr) = 0;

        /**
         * Registers an on confirm delete event handler that is invoked when an object deleted by the local user is
         * confirmed as deleted by the server.
         *
         * @param   ConfirmDeleteHandler
         * @return  ObjectEventHandle
         */
        virtual ObjectEventHandle RegisterOnConfirmDeleteHandler(ConfirmDeleteHandler handler) = 0;

        /**
         * Unregisters an on confirm delete event handler.
         *
         * @param   ObjectEventHandle
         */
        virtual void UnregisterOnConfirmDeleteHandler(ObjectEventHandle eventPtr) = 0;

        /**
         * Registers an on lock event handler that is invoked when an object becomes locked by another user,
         * directly or indirectly.
         *
         * @param   LockHandler
         * @return  ObjectEventHandle
         */
        virtual ObjectEventHandle RegisterOnLockHandler(LockHandler handler) = 0;

        /**
         * Unregisters an on lock event handler.
         *
         * @param   ObjectEventHandle
         */
        virtual void UnregisterOnLockHandler(ObjectEventHandle eventPtr) = 0;

        /**
         * Registers an on unlock event handler that is invoked when an object locked by another user,
         * directly or indirectly, becomes unlocked.
         *
         * @param   UnlockHandler
         * @return  ObjectEventHandle
         */
        virtual ObjectEventHandle RegisterOnUnlockHandler(UnlockHandler handler) = 0;

        /**
         * Unregisters an on unlock event handler.
         *
         * @param   ObjectEventHandle
         */
        virtual void UnregisterOnUnlockHandler(ObjectEventHandle eventPtr) = 0;

        /**
         * Registers an on lock owner change event handler that is invoked when the lock owner on a locked object
         * changes. If the lock owner is null, the object is partially locked. Partially locked objects can have their
         * children edited, but are still locked indirectly by one or more descendant locks.
         *
         * @param   LockOwnerChangeHandler
         * @return  ObjectEventHandle
         */
        virtual ObjectEventHandle RegisterOnLockOwnerChangeHandler(LockOwnerChangeHandler handler) = 0;

        /**
         * Unregisters an on lock owner change event handler.
         *
         * @param   ObjectEventHandle
         */
        virtual void UnregisterOnLockOwnerChangeHandler(ObjectEventHandle eventPtr) = 0;

        /**
         * Registers an on direct lock change event handler that is invoked when the direct lock owner changes.
         *
         * @param   DirectLockChangeHandler
         * @return  ObjectEventHandle
         */
        virtual ObjectEventHandle RegisterOnDirectLockChangeHandler(DirectLockChangeHandler handler) = 0;

        /**
         * Unregisters an on direct lock change lock event handler.
         *
         * @param   ObjectEventHandle
         */
        virtual void UnregisterOnDirectLockChangeHandler(ObjectEventHandle eventPtr) = 0;

        /**
         * Registers an on parent change event handler that is invoked when an object's parent or child index is
         * changed by another user, or when we attempt to change the parent or child index but could not because the
         * child, old parent, or new parent became locked.
         *
         * @param   ParentChangeHandler
         * @return  ObjectIndexEventHandle
         */
        virtual ObjectIndexEventHandle RegisterOnParentChangeHandler(ParentChangeHandler handler) = 0;

        /**
         * Unregisters an on parent change event handler.
         *
         * @param   ObjectIndexEventHandle
         */
        virtual void UnregisterOnParentChangeHandler(ObjectIndexEventHandle eventPtr) = 0;

        /**
         * Registers an on property change event handler that is invoked when a property of an object is changed by
         * another user, or when we attempted to change a property on an object that becomes locked.
         *
         * @param   PropertyChangeHandler
         * @return  PropertyEventHandle
         */
        virtual PropertyEventHandle RegisterOnPropertyChangeHandler(PropertyChangeHandler handler) = 0;

        /**
         * Unregisters an on property change event handler.
         *
         * @param   PropertyEventHandle
         */
        virtual void UnregisterOnPropertyChangeHandler(PropertyEventHandle eventPtr) = 0;

        /**
         * Registers an on remove field event handler that is invoked when a field of a dictionary property is removed
         * by another user, or when we attempt to set a new field on an object that becomes locked. The event is called
         * before the field is removed.
         *
         * @param   DictionaryRemoveHandler
         * @return  DictionaryPropertyEventHandle
         */
        virtual DictionaryPropertyEventHandle RegisterOnDictionaryRemoveHandler(DictionaryRemoveHandler handler) = 0;

        /**
         * Unregisters an on remove field event handler.
         *
         * @param   DictionaryPropertyEventHandle
         */
        virtual void UnregisterOnDictionaryRemoveHandler(DictionaryPropertyEventHandle eventPtr) = 0;

        /**
         * Registers an on list add event handler that is invoked when one or more elements are added to a list
         * property by another user, or when we attempt to remove elements on an object that becomes locked.
         *
         * @param   ListAddHandler
         * @return  ListPropertyEventHandle
         */
        virtual ListPropertyEventHandle RegisterOnListAddHandler(ListAddHandler handler) = 0;

        /**
         * Unregisters an on list add event handler.
         *
         * @param   ListPropertyEventHandle
         */
        virtual void UnregisterOnListAddHandler(ListPropertyEventHandle eventPtr) = 0;

        /**
         * Registers an on list remove event handler that is invoked when one or more elements are removed from a list
         * property by another user, or when we attempt to add elements on an object that becomes locked. The event is
         * called before the elements are removed.
         *
         * @param   ListRemoveHandler
         * @return  ListPropertyEventHandle
         */
        virtual ListPropertyEventHandle RegisterOnListRemoveHandler(ListRemoveHandler handler) = 0;

        /**
         * Unregisters an on list remove event handler.
         *
         * @param   ListPropertyEventHandle eventPtr to unregister.
         */
        virtual void UnregisterOnListRemoveHandler(ListPropertyEventHandle eventPtr) = 0;

        /**
         * Registers an on user join event handler that is invoked when a user joins the session. 
         * Invoked once for each user already in the session when the local user connects.
         *
         * @param   UserJoinHandler
         * @return  UserEventHandle
         */
        virtual UserEventHandle RegisterOnUserJoinHandler(UserJoinHandler handler) = 0;

        /**
         * Unregisters an on user join event handler.
         *
         * @param   UserEventHandle
         */
        virtual void UnregisterOnUserJoinHandler(UserEventHandle eventPtr) = 0;

        /**
         * Registers an on user leave event handler that is invoked when a user leaves the session.
         * Invoked once for each user in the session when the local user disconnects.
         *
         * @param   UserLeaveHandler
         * @return  UserEventHandle
         */
        virtual UserEventHandle RegisterOnUserLeaveHandler(UserLeaveHandler handler) = 0;

        /**
         * Unregisters an on user leave event handler.
         *
         * @param   UserEventHandle
         */
        virtual void UnregisterOnUserLeaveHandler(UserEventHandle eventPtr) = 0;

        /**
         * Registers an on user color change event handler that is invoked when a user's color changes.
         *
         * @param   UserColorChangeHandler handler
         * @return  UserEventHandle
         */
        virtual UserEventHandle RegisterOnUserColorChangeHandler(UserColorChangeHandler handler) = 0;

        /**
         * Unregisters an on user color change event handler.
         *
         * @param   UserEventHandle eventPtr to unregister.
         */
        virtual void UnregisterOnUserColorChangeHandler(UserEventHandle eventPtr) = 0;

        /**
         * Registers an on acknowledge subscription event handler that is invoked when the server acknowledges
         * the children subscription.
         *
         * @param   AcknowledgeSubscriptionHandler
         * @return  AcknowledgeSubscriptionEventHandle
         */
        virtual AcknowledgeSubscriptionEventHandle RegisterOnAcknowledgeSubscriptionHandler(
            AcknowledgeSubscriptionHandler handler) = 0;

        /**
         * Unregisters an on acknowledge subscription event handler.
         *
         * @param   AcknowledgeSubscriptionEventHandle
         */
        virtual void UnregisterOnAcknowledgeSubscriptionHandler(AcknowledgeSubscriptionEventHandle eventPtr) = 0;

        /**
         * Are we connected to the session?
         *
         * @return  bool
         */
        virtual bool IsConnected() const = 0;

        /**
         * The local user.
         *
         * @return  sfUser::SPtr
         */
        virtual sfUser::SPtr LocalUser() const = 0;

        /**
         * The local user id.
         *
         * @return  uint32_t
         */
        virtual uint32_t LocalUserId() = 0;

        /**
         * If returns true, the local user cannot edit objects in this session.
         * This is true while applying edits from other users.
         *
         * @return  bool
         */
        virtual bool EditsDisabled() const = 0;

        /**
         * The number of objects in the session.
         *
         * @return  size_t
         */
        virtual size_t NumObjects() const = 0;

        /**
         * Gets an object by id.
         *
         * @param   uint32_t - id of object to get.
         * @return  sfObject::SPtr - object with the given id, or null if none was found.
         */
        virtual sfObject::SPtr GetObject(uint32_t id) const = 0;

        /**
         * Get a list of root objects. This list may not have the same order across clients.
         *
         * @return  const std::list<sfObject::SPtr>&
         */
        virtual const std::list<sfObject::SPtr>& GetRootObjects() const = 0;

        /**
         * Gets a user by id.
         *
         * @param   uint32_t - id of user to get.
         * @return  sfUser::SPtr - with the given id, or null if none was found.
         */
        virtual sfUser::SPtr GetUser(uint32_t id) const = 0;

        /**
         * Get a list of connected users.
         *
         * @return  const std::list<sfUser::SPtr>&
         */
        virtual const std::list<sfUser::SPtr>& GetUsers() const = 0;

        /**
         * Gets the number of objects of the given type, not including objects whose creation is pending.
         *
         * @param   const sfName& type
         * @return  uint32_t number of objects of the given type.
         */
        virtual uint32_t GetObjectCount(const sfName& type) const = 0;

        /**
         * Gets the maximum number of objects of the given type.
         *
         * @param   const sfName& type.
         * @return  uint32_t maximum number of objects of the given type. UINT32_MAX for unlimited.
         */
        virtual uint32_t GetObjectLimit(const sfName& type) const = 0;

        /**
         * Gets all reference properties that reference the given object.
         *
         * @param   sfObject::SPtr objPtr to get references for.
         * @return  std::vector<sfReferenceProperty::SPtr> references to the object. The first in each pair is the
         *          object containing the reference, and the second is the reference property.
         */
        virtual std::vector<sfReferenceProperty::SPtr> GetReferences(
            sfObject::SPtr objPtr) const = 0;

        /**
         * Sends a create request for an object and its descendants, and puts them in the syncing state. Does nothing
         * if the object is already synced or locked, or not a root object.
         *
         * @param   sfObject::SPtr - object to create.
         */
        virtual void Create(sfObject::SPtr objPtr) = 0;

        /**
         * Sends a create request for an object and its descendants, puts them in the syncing state, and inserts the
         * object as a child of a syncing parent. Does nothing if the object is already synced or locked, or not a root
         * object.
         *
         * @param   sfObject::SPtr - object to create.
         * @param   sfObject::SPtr - parent to add obj to. Must be syncing.
         * @param   int - child index to insert obj at.
         */
        virtual void Create(sfObject::SPtr objPtr, sfObject::SPtr parentPtr, int childIndex) = 0;

        /**
         * Sends a create request for a list of objects and their descendants, and puts them in the syncing state. Does
         * not create objects that are already synced or locked, or not root objects.  Objects that are not created 
         * will be set to null pointers.
         *
         * @param   std::list<sfObject::SPtr>& - objects to create.
         */
        virtual void Create(std::list<sfObject::SPtr>& objectList) = 0;

        /**
         * Sends a create request for a list of objects and their descendants, puts them in the syncing state, and
         * inserts them as children of a syncing parent. Does not create objects that are already synced or locked, or
         * not root objects. Objects that are not created will be set to null pointers.
         *
         * @param   std::list<sfObject::SPtr>& - objects to create.
         * @param   sfObject::SPtr - parent to add obj to. Must be syncing.
         * @param   int - child index to insert objects at.
         */
        virtual void Create(std::list<sfObject::SPtr>& objectList, sfObject::SPtr parentPtr, int childIndex) = 0;

        /**
         * Deletes an object and its descendants from the server and stops syncing it.
         *
         * @param   sfObject::SPtr - object to delete.
         */
        virtual void Delete(sfObject::SPtr objPtr) = 0;

        /**
         * Gets the id for a name string. The ids and names are shared across the session so any user can use the id to
         * retrieve the same name.
         *
         * @param   const sfName& str
         * @return  uint32_t name id
         */
        virtual uint32_t GetStringTableId(const sfName& str) = 0;

        /**
         * Gets a name string by id. The ids and names are shared across the session so any user can use the id to
         * retrieve the same name. If the id is not in the table, logs an error and returns empty string.
         *
         * @param   uint32_t id of name to get.
         * @return  sfName name with the given id
         */
        virtual sfName GetStringFromTable(uint32_t id) = 0;

        /**
         * Gets a name string by id. The ids and names are shared across the session so any user can use the id to
         * retrieve the same name. If the id is not in the table, return an invalid sfName.
         *
         * @param   uint32_t id of name to get.
         * @return  sfName name with the given id
         */
        virtual sfName TryGetStringFromTable(uint32_t id) = 0;

        /**
         * Set a new color for the connected user.
         *
         * @param   float - red value [0 to 1]
         * @param   float - green value [0 to 1]
         * @param   float - blue value [0 to 1]
         */
        virtual void SetUserColor(float r, float g, float b) = 0;

        /**
         * Subscribe to an object's children on the server.
         *
         * @param   sfObject::SPtr objPtr
         */
        virtual void SubscribeToChildren(sfObject::SPtr objPtr) = 0;

        /**
         * Unsubscribe from an object's children on the server.
         *
         * @param   sfObject::SPtr objPtr
         */
        virtual void UnsubscribeFromChildren(sfObject::SPtr objPtr) = 0;
    };
} // SceneFusion2
} // KS