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
#include <CoreMinimal.h>
#include <UObject/UObjectArray.h>
#include <Launch/Resources/Version.h>
#include <unordered_map>
#include <functional>

using namespace KS::SceneFusion2;

/**
 * Maps sfObjects to uobjects and vice versa.
 */
class sfObjectMap
{
public:
    typedef std::function<void(sfObject::SPtr objPtr, UObject* uobjPtr)> RemoveHandler;

    /**
     * Enables the delete listener to remove objects from the map when uobjects are garbage collected.
     */
    static void EnableDeleteListener();

    /**
     * Disables the delete listener.
     */
    static void DisableDeleteListener();

    /**
     * Checks if a uobject is in the map.
     *
     * @param   const UObject* uobjPtr
     * @return  bool true if the uobject is in the map.
     */
    static bool Contains(const UObject* uobjPtr);

    /**
     * Checks if an sfObject is in the map.
     *
     * @param   sfObject::SPtr objPtr
     * @return  bool true if the object is in the map.
     */
    static bool Contains(sfObject::SPtr objPtr);
    
    /**
     * Gets the sfObject for a uobject, or nullptr if the uobject has no sfObject.
     *
     * @param   const UObject* uobjPtr
     * @return  sfObject::SPtr sfObject for the uobject, or nullptr if none was found.
     */
    static sfObject::SPtr GetSFObject(const UObject* uobjPtr);

    /**
     * Gets the sfObject for a uobject, or creates one with an empty dictionary property and adds it to the map if none
     * was found.
     *
     * @param   UObject* uobjPtr
     * @param   const sfName& type of object to create.
     * @param   sfObject::ObjectFlags flags to create object with.
     * @return  sfObject::SPtr sfObject for the uobject.
     */
    static sfObject::SPtr GetOrCreateSFObject(UObject* uobjPtr, const sfName& type, 
        sfObject::ObjectFlags flags = sfObject::ObjectFlags::NoFlags);

    /**
     * Gets the uobject for an sfObject, or nullptr if the sfObject has no uobject.
     *
     * @param   sfObject::SPtr objPtr to get uobject for.
     * @return  UObject* uobject for the sfObject.
     */
    static UObject* GetUObject(sfObject::SPtr objPtr);

    /**
     * Adds a mapping between a uobject and an sfObject.
     *
     * @param   sfObject::SPtr objPtr
     * @param   UObject* uobjPtr
     */
    static void Add(sfObject::SPtr objPtr, UObject* uobjPtr);

    /**
     * Removes a uobject and its sfObject from the map.
     *
     * @param   const UObject* uobjPtr to remove.
     * @return  sfObject::SPtr that was removed, or nullptr if the uobject was not in the map.
     */
    static sfObject::SPtr Remove(const UObject* uobjPtr);

    /**
     * Removes an sfObject and its uobject from the map.
     *
     * @param   sfObject::SPtr objPtr to remove.
     * @return  UObject* that was removed, or nullptr if the sfObject was not in the map.
     */
    static UObject* Remove(sfObject::SPtr objPtr);

    /**
     * Clears the map.
     */
    static void Clear();

    /**
     * @return  std::unordered_map<sfObject::SPtr, UObject*>::iterator pointing to the first pair in the map.
     */
    static std::unordered_map<sfObject::SPtr, UObject*>::iterator Begin();

    /**
     * @return  std::unordered_map<sfObject::SPtr, UObject*>::iterator pointing past the last pair in the map.
     */
    static std::unordered_map<sfObject::SPtr, UObject*>::iterator End();

    /**
     * Gets the uobject for an sfObject cast to T*.
     *
     * @param   const sfObject::SPtr objPtr
     * @return  T* uobject for the sfObject, or nullptr not found or not of type T.
     */
    template<typename T>
    static T* Get(const sfObject::SPtr objPtr)
    {
        return Cast<T>(GetUObject(objPtr));
    }

    /**
     * Registers a handler to call when uobjects of type T are removed from the map.
     *
     * @param   RemoveHandler handler
     */
    template<typename T>
    static void RegisterRemoveHandler(RemoveHandler handler)
    {
        m_removeHandlers.emplace(T::StaticClass(), handler);
    }

    /**
     * Unregisters the remove handler for uobjects of type T.
     */
    template<typename T>
    static void UnregisterRemoveHandler()
    {
        m_removeHandlers.erase(T::StaticClass());
    }

private:
    /**
     * Delete event listener called when uobjects are garbage collected.
     */
    class DeleteEventListener : public FUObjectArray::FUObjectDeleteListener
    {
    public:
        /**
         * Called when a uobject is garbage collected. Removes it from the sfObjectMap.
         *
         * @param   const class UOjectBase* uobjPtr that was garbage collected.
         * @param   int index of uobject in global uobject array.
         */
        virtual void NotifyUObjectDeleted(const class UObjectBase *uobjPtr, int index) override;

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
        /**
         * Called when UObject Array is being shut down, this is where all listeners should be removed from it
         */
        virtual void OnUObjectArrayShutdown() override;
#endif
    };

    static TMap<const UObject*, sfObject::SPtr> m_uToSFObjectMap;
    static std::unordered_map<sfObject::SPtr, UObject*> m_sfToUObjectMap;
    static std::unordered_map<UClass*, RemoveHandler> m_removeHandlers;
    static DeleteEventListener m_deleteListener;

    /**
     * Calls the remove handler for an object.
     *
     * @param   sfObject::SPtr objPtr to call remove handler for.
     * @param   UObject* uobjPtr to call remove handler for.
     */
    static void CallRemoveHandler(sfObject::SPtr objPtr, UObject* uobjPtr);
};