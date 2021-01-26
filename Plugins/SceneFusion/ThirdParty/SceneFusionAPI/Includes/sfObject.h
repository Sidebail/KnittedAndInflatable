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
#include <memory>
#include <string>
#include <sstream>
#include <Exports.h>
#include <ksHierarchyObject.h>
#include "sfUser.h"
#include "sfProperty.h"
#include "sfName.h"

namespace KS {
namespace SceneFusion2 {
    class sfSession;

    /**
     * Objects are synced between server and clients. Objects have an id, a user-defined string type, and a property
     * which may be a collection of properties. Objects are arranged in a hierarchy and can be locked by users. When
     * objects are constructed they are not automatically synced. To begin syncing an object, call
     * sfSession.Create(sfObject::SPtr).
     */
    class EXTERNAL sfObject :
        public ksHierarchyObject<sfObject>
    {
    public:
        typedef std::shared_ptr<sfObject> SPtr;

        /**
         * Object flags.
         *
         * OptionalChildren: Clients will not receive children for this object unless they subscribe to it using
         *                   sfSession::SubscribeToChildren
         * Transient:        The object will be deleted when the user who created it leaves the session.
         */
        enum ObjectFlags : uint8_t
        {
            NoFlags = 0,
            OptionalChildren = 1 << 1,
            Transient = 1 << 2
        };

        /**
         * Shared pointer constructor.
         *
         * @param   const sfName& - type of object.
         * @param   sfProperty::SPtr - property for the object.
         * @param   ObjectFlags - flags.
         */
        static SPtr Create(const sfName& type, sfProperty::SPtr propertyPtr = nullptr, ObjectFlags flags = NoFlags);

        /**
         * Destructor.
         */
        virtual ~sfObject() {};

        /**
         * String representation of this object.
         *
         * @return  std::string
         */
        virtual std::string ToString() = 0;

        /**
         * Object id.
         *
         * @return  const uint32_t&
         */
        virtual const uint32_t& Id() const = 0;

        /**
         * User-defined object type.
         *
         * @return  const sfName&
         */
        virtual const sfName& Type() const = 0;

        /**
         * Object flags.
         *
         * @return  ObjectFlags
         */
        virtual const ObjectFlags Flags() const = 0;

        /**
         * Object property.
         *
         * @return  sfProperty::SPtr
         */
        virtual sfProperty::SPtr Property() = 0;

        /**
         * Set the property used by this object.
         *
         * @param   sfProperty::SPtr - property
         */
        virtual void SetProperty(sfProperty::SPtr propertyPtr) = 0;

        /**
         * The session the object belongs to.
         *
         * @return  std::shared_ptr<sfSession>
         */
        virtual std::shared_ptr<sfSession> Session() = 0;

        /**
         * Are changes to the object being synced? To begin syncing an object, call sfSession.Create(sfObject).
         *
         * @return  bool
         */
        virtual bool IsSyncing() = 0;

        /**
         * Is the object created on the server? This differs slightly from IsSyncing. When we send a create request,
         * IsSyncing becomes true, but IsCreated becomes true when the server acknowledges the creation. Similarly when
         * we send a delete request, IsSyncing becomes false but IsCreated becomes false when the server acknowledges
         * the deletion.
         *
         * @return  bool
         */
        virtual bool IsCreated() = 0;

        /**
         * Are we waiting for the server to create this object?
         *
         * @return  bool
         */
        virtual bool IsCreatePending() = 0;

        /**
         * Are we waiting for the server to unsubscribe the local player from this object's children?
         *
         * @return  bool
         */
        virtual bool IsUnsubscriptionPending() = 0;

        /**
         * Are we waiting for the server to delete this object?
         *
         * @return  bool
         */
        virtual bool IsDeletePending() = 0;

        /**
         * The user who owns the lock on this object, either directly or indirectly. A user will indirectly own a lock
         * if they directly lock an ancestor of the object. Null if no user owns a lock on this object. The object may
         * still be indirectly locked without a lock owner if one or more users own locks on descendants of the object.
         *
         * @return  sfUser::SPtr
         */
        virtual sfUser::SPtr LockOwner() = 0;

        /**
         * Can we edit the object? Objects cannot be edited if they are locked by another user, if they are deleted
         * locally and we're waiting for the server to resolve the delete request, or while applying changes from the
         * server.
         *
         * @return  bool
         */
        virtual bool CanEdit() = 0;

        /**
         * Can we edit the children of this object? Objects cannot have their children edited if they are fully locked
         * by another user (meaning that user directly locked the object or one of its ancestors), if they are deleted
         * locally and we're waiting for the server to resolve the delete request, or while applying changes from the
         * server.
         *
         * @return  bool
         */
        virtual bool CanEditChildren() = 0;

        /**
         * Is the object locked directly or indirectly by another user?
         *
         * @return  bool
         */
        virtual bool IsLocked() = 0;

        /**
         * Are we waiting to acquire a lock on this object?
         *
         * @return  bool
         */
        virtual bool IsLockPending() = 0;

        /**
         * Is the object fully locked? Locked objects are either fully locked or partially locked. Partially locked
         * objects can have their children edited and full locked objects cannot. An object is fully locked if it or
         * one of its ancestors is locked directly. Otherwise if a descendant is locked directly, the object is
         * partially locked.
         *
         * @return  bool
         */
        virtual bool IsFullyLocked() = 0;

        /**
         * Is the object partially locked? Locked objects are either fully locked or partially locked. Partially locked
         * objects can have their children edited and fully locked objects cannot. An object is fully locked if it or
         * one of its ancestors is locked directly. Otherwise if a descendant is locked directly, the object is
         * partially locked.
         *
         * @return  bool
         */
        virtual bool IsPartiallyLocked() = 0;

        /**
         * Is this object locked directly? An object is locked directly if another user called RequestLock() on the
         * object.
         *
         * @return  bool
         */
        virtual bool IsLockedDirectly() = 0;

        /**
         * Requests a lock on this object. If the object is locked by another user, tries to acquire the lock when it
         * becomes unlocked. If the object is not syncing, call this before calling sfSession.Create to create the
         * object with a lock.
         */
        virtual void RequestLock() = 0;

        /**
         * Releases the lock on this object, or stops trying to acquire the lock if we do not own the lock on this object.
         */
        virtual void ReleaseLock() = 0;

        /**
         * Sets the child index, changing the object's placement in its parent's child list. Does nothing if the object
         * has no parent, the object is locked, the index is out of bounds, or the child is already at the given index.
         *
         * @param   int - index to move child to.
         * @return  bool - true if the child was moved successfully. Note that this does not wait for a response from the
         *          server, so it may return true and be reverted later if another user gets a lock that prevents the
         *          child move. If the child was already at the given index, returns false.
         */
        virtual bool SetChildIndex(int index) = 0;
    };
} // SceneFusion2
} // KS