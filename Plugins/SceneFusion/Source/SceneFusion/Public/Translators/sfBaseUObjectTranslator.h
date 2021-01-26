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
#include "sfBaseTranslator.h"
#include "../sfUPropertyInstance.h"
#include "../sfUnrealUtils.h"
#include <CoreUObject.h>
#include <unordered_map>

/**
 * Base class for translators that sync UObject properties using reflection.
 */
class sfBaseUObjectTranslator : public sfBaseTranslator
{
public:
    /**
     * Handler for an sfProperty change event.
     *
     * @param   UObject* the changed property belongs to.
     * @param   sfProperty::SPtr new property value. nullptr if the property was set to the default value.
     * @return  bool if false, the default property handler will be called.
     */
    typedef std::function<bool(UObject*, sfProperty::SPtr)> PropertyChangeHandler;

    /**
     * Handler for a post property change event.
     *
     * @param   UObject* whose property changed.
     */
    typedef std::function<void(UObject*)> PostPropertyChangeHandler;

    /**
     * Handler for a remove elements event. Called before the elements are removed.
     *
     * @param   UObject* whose property had elements removed.
     * @param   int index of removal.
     * @param   int count - number of elements removed.
     */
    typedef std::function<void(UObject*, int index, int count)> RemoveElementsHandler;

    /**
     * Callback to call when a uobject is modified.
     *
     * @param   sfObject::SPtr
     * @param   UObject*
     */
    typedef std::function<void(sfObject::SPtr, UObject*)> OnModifyHandler;

    /**
     * Callback to set property value on the given UObject.
     *
     * @param   UObject* to set property on.
     * @param   const sfUPropertyInstance& upropInstance
     * @param   sfProperty::SPtr new property value.
     */
    typedef std::function<void(UObject*, const sfUPropertyInstance&, sfProperty::SPtr)> SetPropertyCallback;

    /**
     * Registers a post property change handler for objects of type T, called after a property is changed by another
     * user.
     *
     * @param   const FName& propertyName to register handler for.
     * @param   PostPropertyChangeHandler handler to register.
     */
    template<typename T>
    void RegisterPostPropertyChangeHandler(const FName& propertyName, PostPropertyChangeHandler handler)
    {
        RegisterPostPropertyChangeHandler(T::StaticClass()->GetFName(), propertyName, handler);
    }

    /**
     * Unregisters a post property change handler for objects of type T.
     *
     * @param   const FName& propertyName to unregister handler for.
     */
    template<typename T>
    void UnregisterPostPropertyChangeHandler(const FName& propertyName)
    {
        UnregisterPostPropertyChangeHandler(T::StaticClass()->GetFName(), propertyName);
    }

    /**
     * Registers a post property change handler for a class and property, called after a property is changed by another
     * user.
     *
     * @param   const FName& className to register handler for.
     * @param   const FName& propertyName to register handler for.
     * @param   PostPropertyChangeHandler handler to register.
     */
    void RegisterPostPropertyChangeHandler(
        const FName& className,
        const FName& propertyName,
        PostPropertyChangeHandler handler);

    /**
     * Unregisters a post property change handler.
     *
     * @param   const FName& className to unregister handler for.
     * @param   const FName& propertyName to unregister handler for.
     */
    void UnregisterPostPropertyChangeHandler(const FName& className, const FName& propertyName);

    /**
     * Registers a remove elements handler for objects of type T, called when array elements are removed by another
     * user.
     *
     * @param   const FName& propertyName to register handler for.
     * @param   RemoveElementsHandler handler to register.
     */
    template<typename T>
    void RegisterRemoveElementsHandler(const FName& propertyName, RemoveElementsHandler handler)
    {
        RegisterRemoveElementsHandler(T::StaticClass()->GetFName(), propertyName, handler);
    }

    /**
     * Unregisters a remove elements handler for objects of type T.
     *
     * @param   const FName& propertyName to unregister handler for.
     */
    template<typename T>
    void UnregisterRemoveElementsHandler(const FName& propertyName)
    {
        UnregisterRemoveElementsHandler(T::StaticClass()->GetFName(), propertyName);
    }

    /**
     * Registers a remove elements handler for a class and property, called when array elements are removed by another
     * user.
     *
     * @param   const FName& className to register handler for.
     * @param   const FName& propertyName to register handler for.
     * @param   RemoveElementsHandler handler to register.
     */
    void RegisterRemoveElementsHandler(
        const FName& className,
        const FName& propertyName,
        RemoveElementsHandler handler);
    /**
     * Unregisters a remove elements handler.
     *
     * @param   const FName& className to unregister handler for.
     * @param   const FName& propertyName to unregister handler for.
     */

    void UnregisterRemoveElementsHandler(const FName& className, const FName& propertyName);

    /**
     * Registers a handler to call when a uobject of type T is modified.
     *
     * @param   OnModifyHandler handler
     */
    template<typename T>
    void RegisterOnModifyHandler(OnModifyHandler handler)
    {
        m_onModifyHandlers.emplace(sfUnrealUtils::GetClassTypeHash(T::StaticClass()), handler);
    }

    /**
     * Unregisters the on modify handler for uobjects of type T.
     */
    template<typename T>
    void UnregisterOnModifyHandler()
    {
        m_onModifyHandlers.erase(sfUnrealUtils::GetClassTypeHash(T::StaticClass()));
    }

    /**
     * Registers a handler to call when a uobject with the given class name is modified.
     *
     * @param   FName className
     * @param   OnModifyHandler handler
     */
    void RegisterOnModifyHandler(FName className, OnModifyHandler handler);

    /**
     * Unregisters the on modify handler for uobjects with the given class name.
     *
     * @param   FName className
     */
    void UnregisterOnModifyHandler(FName className);

protected:
    // Map of property names to custom sfProperty change event handlers
    std::unordered_map<sfName, PropertyChangeHandler> m_propertyChangeHandlers;
    // Map of class name & property name (ids combined into a uint64) to post property change event handlers
    std::unordered_map<uint64_t, PostPropertyChangeHandler> m_postPropertyChangeHandlers;
    // Map of class name & property name (ids combined into a uint64) to remove element event handlers
    std::unordered_map<uint64_t, RemoveElementsHandler> m_removeElementsHandlers;
    // Map of class names ids to modify event handlers
    std::unordered_map<TypeHash, OnModifyHandler> m_onModifyHandlers;

    /**
     * Creates a unique uint64 key from a class name and property name.
     *
     * @param   const FName& className
     * @param   const FName& propertyName
     * @return  uint64_t key
     */
    uint64_t CreateKey(const FName& className, const FName& propertyName);

    /**
     * Tries to insert elements from an sfListProperty into an array using reflection. Returns false if the UnrealProperty
     * is not an array property. Expands the array if the insertion index is greater than the size of the array.
     *
     * @param   UObject* uobjPtr the property belongs to.
     * @param   const sfUPropertyInstance& upropInstance to try inserting array elements for. Returns false if this is
     *          not an array property.
     * @param   sfListProperty::SPtr listPtr with elements to insert.
     * @param   int index of first element to insert, and the index to insert at.
     * @param   int count - number of elements to insert.
     * @return  bool true if the UnrealProperty was an array property.
     */
    bool ArrayInsert(
        UObject* uobjPtr,
        const sfUPropertyInstance& upropInstance,
        sfListProperty::SPtr listPtr,
        int index,
        int count);

    /**
     * Tries to remove elements from an array using reflection. Returns false if the UnrealProperty is not an array
     * property.
     *
     * @param   UObject* uobjPtr the property belongs to.
     * @param   const sfUPropertyInstance& upropInstance to try removing array elements from. Returns false if this is
     *          not an array property.
     * @param   int index of first element to remove.
     * @param   int count - number of elements to remove.
     * @return  bool true if the UnrealProperty was an array property.
     */
    bool ArrayRemove(UObject* uobjPtr, const sfUPropertyInstance& upropInstance, int index, int count);

    /**
     * Tries to insert elements from an sfListProperty into a set using reflection. Returns false if the UnrealProperty is
     * not a set property.
     *
     * @param   UObject* uobjPtr the property belongs to.
     * @param   const sfUPropertyInstance& upropInstance to try inserting set elements for. Returns false if this is
     *          not a set property.
     * @param   sfListProperty::SPtr listPtr with elements to insert.
     * @param   int index of first element to insert, and the index to insert at.
     * @param   int count - number of elements to insert.
     * @return  bool true if the UnrealProperty was a set property.
     */
    bool SetInsert(
        UObject* uobjPtr,
        const sfUPropertyInstance& upropInstance,
        sfListProperty::SPtr listPtr,
        int index,
        int count);

    /**
     * Tries to remove elements from a set using reflection. Returns false if the UnrealProperty is not a set property.
     *
     * @param   const sfUPropertyInstance& upropInstance to try removing set elements from. Returns false if this is
     *          not a set property.
     * @param   int index of first element to remove.
     * @param   int count - number of elements to remove.
     * @return  bool true if the UnrealProperty was a set property.
     */
    bool SetRemove(const sfUPropertyInstance& upropInstance, int index, int count);

    /**
     * Tries to insert elements from an sfListProperty into a map using reflection. Returns false if the UnrealProperty is
     * not a map property.
     *
     * @param   UObject* uobjPtr the property belongs to.
     * @param   const sfUPropertyInstance& upropInstance to try inserting map elements for. Returns false if this is
     *          not a map property.
     * @param   sfListProperty::SPtr listPtr with elements to insert. Each element is another sfListProperty with two
                elements: a key followed by a value.
     * @param   int index of first element to insert, and the index to insert at.
     * @param   int count - number of elements to insert.
     * @return  bool true if the UnrealProperty was a map property.
     */
    bool MapInsert(
        UObject* uobjPtr,
        const sfUPropertyInstance& upropInstance,
        sfListProperty::SPtr listPtr,
        int index,
        int count);

    /**
     * Tries to remove elements from a map using reflection. Returns false if the UnrealProperty is not a map property.
     *
     * @param   const sfUPropertyInstance& upropInstance to try removing map elements from. Returns false if this is
     *          not a map property.
     * @param   int index of first element to remove.
     * @param   int count - number of elements to remove.
     * @return  bool true if the UnrealProperty was a map property.
     */
    bool MapRemove(const sfUPropertyInstance& upropInstance, int index, int count);

    /**
     * Called when an object property changes.
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
     * Called when one or more elements are added to a list property.
     *
     * @param   sfListProperty::SPtr listPtr that elements were added to.
     * @param   int index elements were inserted at.
     * @param   int count - number of elements added.
     */
    virtual void OnListAdd(sfListProperty::SPtr listPtr, int index, int count) override;

    /**
     * Called when one or more elements are removed from a list property.
     *
     * @param   sfListProperty::SPtr listPtr that elements were removed from.
     * @param   int index elements were removed from.
     * @param   int count - number of elements removed.
     */
    virtual void OnListRemove(sfListProperty::SPtr listPtr, int index, int count) override;

    /**
     * Called after a uproperty is changed by another user.
     *
     * @param   UObject* uobjPtr whose property changed.
     * @param   UnrealProperty* upropPtr that changed.
     */
    virtual void PostPropertyChange(UObject* uobjPtr, UnrealProperty* upropPtr) override;

    /**
     * Called when a uobject is modified.
     *
     * @param   sfObject::SPtr objPtr for the uobject that was modified.
     * @param   UObject* uobjPtr that was modified.
     */
    virtual void OnUObjectModified(sfObject::SPtr objPtr, UObject* uobjPtr) override;

    /**
     * Gets the uobject for an sfObject, or nullptr if the sfObject has no uobject.
     *
     * @param   sfObject::SPtr objPtr to get uobject for.
     * @return  UObject* uobject for the sfObject.
     */
    virtual UObject* GetUObject(sfObject::SPtr objPtr);

    /**
     * Sets property value on the given object. If the object is a default object, apply the changes to all instances
     * which have the default value.
     *
     * @param   UObject* uobjPtr to set property on
     * @param   sfProperty::SPtr propertyPtr - new property value
     * @param   SetPropertyCallback setProperty - callback to set property value
     */
    virtual void SetPropertyOnObjectAndAllInstances(
        UObject* uobjPtr,
        sfProperty::SPtr propertyPtr,
        SetPropertyCallback setProperty);

    /**
     * Initializes the uobject-type children of an sfobject.
     *
     * @param   sfObject::SPtr objPtr to initialize children for.
     */
    void InitializeChildren(sfObject::SPtr objPtr);

    /**
     * Calls the property change handler for an sfProperty.
     *
     * @param   UObject* uobjPtr whose property changed.
     * @param   sfProperty::SPtr propertyPtr that changed.
     * @return  bool true if the change event was handled.
     */
    bool CallPropertyHandler(UObject* uobjPtr, sfProperty::SPtr propertyPtr);
};