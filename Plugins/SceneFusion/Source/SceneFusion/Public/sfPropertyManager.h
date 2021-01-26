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

#include "sfValueProperty.h"
#include "sfSession.h"
#include "sfUPropertyInstance.h"
#include "sfUnrealUtils.h"

#include <CoreMinimal.h>
#include <unordered_set>

using namespace KS;
using namespace KS::SceneFusion2;

#define LOG_CHANNEL "sfPropertyManager"

/**
 * Manages syncing of uproperties.
 */
class sfPropertyManager
{
public:
    /**
     * @return  sfPropertyManager& static singleton instance
     */
    static sfPropertyManager& Get();
    
    /**
     * Constructor
     */
    sfPropertyManager();

    /**
     * Syncs property change.
     *
     * @param   UObject* uobjPtr
     * @param   UnrealProperty* upropPtr
     */
    typedef std::function<void(UObject* uobjPtr, UnrealProperty* upropPtr)> PropertyChangeHandler;

    /**
     * On get asset property event.
     *
     * @param   UObject* assetPtr
     */
    DECLARE_EVENT_OneParam(sfPropertyManager, OnGetAssetPropertyEvent, UObject*);

    /**
     * Invoked when getting the value of an asset reference property.
     *
     * @return  OnGetAssetPropertyEvent&
     */
    OnGetAssetPropertyEvent& OnGetAssetProperty()
    {
        return m_onGetAssetProperty;
    }

    /**
     * Finds a uproperty of a uobject corresponding to an sfproperty.
     *
     * @param   UObject* uobjPtr to find property on.
     * @param   sfProperty::SPtr propPtr to find corresponding uproperty for.
     * @return  sfUPropertyInstance
     */
    sfUPropertyInstance FindUProperty(UObject* uobjPtr, sfProperty::SPtr propPtr);

    /**
     * Converts a UnrealProperty to an sfProperty using reflection.
     *
     * @param   UObject* uobjPtr to get property from.
     * @param   UnrealProperty* upropPtr
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetValue(UObject* uobjPtr, UnrealProperty* upropPtr);

    /**
     * Sets a UnrealProperty using reflection to a value from an sfProperty.
     *
     * @param   UObject* uobjPtr to set property on.
     * @param   const sfUPropertyInstance& upropInstance
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetValue(UObject* uobjPtr, const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Checks if an object has the default value for a property using reflection. Returns false if the property cannot
     * be synced by Scene Fusion.
     *
     * @param   UObject* uobjPtr to check property on.
     * @param   UnrealProperty* upropPtr to check.
     * @return  bool false if the property does not have it's default value, or if the property type cannot be synced
     *          by Scene Fusion.
     */
    bool IsDefaultValue(UObject* uobjPtr, UnrealProperty* upropPtr);

    /**
     * Sets a property on an object to the default value using reflection. Does nothing if the property cannot be
     * synced by Scene Fusion.
     *
     * @param   UObject* objPtr to set property on.
     * @param   UnrealProperty* upropPtr to set.
     */
    void SetToDefaultValue(UObject* uobjPtr, UnrealProperty* upropPtr);

    /**
     * Sets a struct field on an object to the default value using reflection.
     *
     * @param   UObject* uobjPtr to set struct field on.
     * @param   const sfUPropertyInstance& upropInstance for the struct.
     * @param   const sfName& name of the struct field.
     */
    void SetStructFieldToDefaultValue(UObject* uobjPtr, const sfUPropertyInstance& upropInstance, const sfName& name);

    /**
     * Gets the default object for an object.
     *
     * @param   UObject* uobjPtr to get default object for.
     * @return  UObject* default object
     */
    UObject* GetDefaultObject(UObject* uobjPtr);

    /**
     * Adds a property into the force to sync list.
     *
     * @param   FName ownerClassName
     * @param   FName propertyName
     */
    void AddPropertyToForceSyncList(FName ownerClassName, FName propertyName);

    /**
     * Removes a property from the force to sync list.
     *
     * @param   FName ownerClassName
     * @param   FName propertyName
     */
    void RemovePropertyFromForceSyncList(FName ownerClassName, FName propertyName);

    /**
     * Adds a property to the blacklist.
     *
     * @param   FName ownerClassName
     * @param   FName propertyName
     */
    void AddToBlacklist(FName ownerClassName, FName propertyName);

    /**
     * Removes a property from the blacklist.
     *
     * @param   FName ownerClassName
     * @param   FName propertyName
     */
    void RemoveFromBlacklist(FName ownerClassName, FName propertyName);

    /**
     * Ignore the CPF_DisableEditOnInstance flag for the given class when deciding if a property is syncable.
     *
     * @param   FName className
     */
    void IgnoreDisableEditOnInstanceFlagForClass(FName className);

    /**
     * Checks if a property is syncable.
     * A UnrealProperty can be synced if it is in the force sync set or if the CPF_Edit flag is set,
     * the CPF_DisableEditOnInstance flag is not set and the CPF_EditConst flag is not set.
     *
     * @param   UObject* uobjPtr to check.
     * @param   UnrealProperty* upropPtr to check.
     * @return  bool true if the property is syncable.
     */
    bool IsSyncable(UObject* uobjPtr, UnrealProperty* upropPtr);

    /**
     * Iterates all properties of an object using reflection and creates sfProperties for properties with non-default
     * values as fields in an sfDictionaryProperty.
     *
     * @param   UObject* uobjPtr to create properties for.
     * @param   sfDictionaryProperty::SPtr dictPtr to add properties to.
     * @param   const TArray<FString>* const blacklistPtr - if the property name is in this list, ignore the property.
     */
    void CreateProperties(
        UObject* uobjPtr,
        sfDictionaryProperty::SPtr dictPtr,
        const TSet<FString>* const blacklistPtr = nullptr);

    /**
     * Applies property values from an sfDictionaryProperty to an object using reflection.
     *
     * @param   UObject* uobjPtr to apply property values to.
     * @param   sfDictionaryProperty::SPtr dictPtr to get property values from. If a value for a property is not in the
     *          dictionary, sets the property to its default value.
     * @param   const TArray<FString>* const blacklistPtr - if the property name is in this list, ignore the property.
     */
    void ApplyProperties(
        UObject* uobjPtr,
        sfDictionaryProperty::SPtr dictPtr,
        const TSet<FString>* const blacklistPtr = nullptr);

    /**
     * Iterates all properties of an object using reflection and updates an sfDictionaryProperty when its values are
     * different from those on the object. Removes fields from the dictionary for properties that have their default
     * value.
     *
     * @param   UObject* uobjPtr to iterate properties on.
     * @param   sfDictionaryProperty::SPtr dictPtr to update.
     * @param   const TArray<FString>* const blacklistPtr - if the property name is in this list, ignore the property.
     */
    void SendPropertyChanges(
        UObject* uobjPtr,
        sfDictionaryProperty::SPtr dictPtr,
        const TSet<FString>* const blacklistPtr = nullptr);

    /**
     * Sets a list of references to the given uobject using reflection.
     *
     * @param   UObject* uobjPtr to set references to.
     * @param   const std::vector<sfReferenceProperty::SPtr>& references to set to the uobject.
     */
    void SetReferences(UObject* uobjPtr, const std::vector<sfReferenceProperty::SPtr>& references);

    /**
     * Creates a property from a uobject reference.
     *
     * @param   UObject* uobjPtr
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr FromUObject(UObject* uobjPtr);

    /**
     * Finds or loads a uobject of type T from a property.
     * 
     * @param   sfProperty::SPtr propPtr
     * @param   T* currentPtr to return if the uobject cannot be found or loaded.
     * @return  T* uobject the property refers to.
     */
    template<typename T>
    T* To(sfProperty::SPtr propPtr, T* currentPtr = nullptr)
    {
        if (propPtr->Type() == sfProperty::NUL)
        {
            return nullptr;
        }
        if (propPtr->Type() == sfProperty::REFERENCE)
        {
            // The object is in the level.
            uint32_t objId = propPtr->AsReference()->GetObjectId();
            sfObject::SPtr objPtr = SceneFusion::Service->Session()->GetObject(objId);
            return sfObjectMap::Get<T>(objPtr);
        }
        // The object is an asset
        FString str = sfPropertyUtil::ToString(propPtr);
        // If str is empty we keep our current value
        if (str.IsEmpty())
        {
            return currentPtr;
        }

        FString path, className;
        if (!str.Split(";", &className, &path))
        {
            KS::Log::Warning("Invalid asset string: " + std::string(TCHAR_TO_UTF8(*str)), LOG_CHANNEL);
            return currentPtr;
        }

        T* assetPtr = Cast<T>(sfLoader::Get().LoadFromCache(path));
        if (assetPtr == nullptr)
        {
            assetPtr = Cast<T>(sfLoader::Get().Load(path, className));
        }
        return assetPtr == nullptr ? currentPtr : assetPtr;
    }

    /**
     * Copies the data from one property into another if they are the same property type.
     *
     * @param   sfProperty::SPtr destPtr to copy into.
     * @param   sfProperty::SPtr srcPtr to copy from.
     * @return  bool false if the properties were not the same type.
     */
    bool Copy(sfProperty::SPtr destPtr, sfProperty::SPtr srcPtr);

    /**
     * Adds a uproperty instance's containing hash to the set of hashes that need rehashing. Does nothing if the
     * uproperty instance is not a key in a hash.
     *
     * @param   const sfUPropertyInstance& upropInstance
     */
    void MarkHashStale(const sfUPropertyInstance& upropInstance);

    /**
     * Marks a property as being changed so we will call the appropriate change events for it when BroadcastChangeEvents
     * is called.
     *
     * @param   UObject* uobjPtr the property belongs to.
     * @param   UnrealProperty* upropPtr - uproperty that changed.
     * @param   sfProperty::SPtr propPtr - property that changed. If provided, will look for a uproperty with the name
     *          of the sfProperty at depth 1.
     */
    void MarkPropertyChanged(UObject* uobjPtr, UnrealProperty* upropPtr, sfProperty::SPtr propPtr = nullptr);

    /**
     * Rehashes property containers whose keys were changed by other users.
     */
    void RehashProperties();

    /**
     * Invokes property change events for properties that were changed by the functions in this class.
     */
    void BroadcastChangeEvents();

    /**
     * Replaces a uobject with another uobject in the local and server changed property maps.
     *
     * @param   UObject* oldPtr to be replaced.
     * @param   UObject* newPtr to replace with.
     */
    void ReplaceUObject(UObject* oldPtr, UObject* newPtr);

    /**
     * Enables the property change event handler that syncs property changes.
     */
    void EnablePropertyChangeHandler();

    /**
     * Disables the property change event handler that syncs property changes.
     */
    void DisablePropertyChangeHandler();

    /**
     * Returns true if it is listening for property changes.
     *
     * @return  bool
     */
    bool ListeningForPropertyChanges();

    /**
     * Sends new property values to the server for properties that were changed locally, or reverts them to the server
     * value if they belong to a locked object.
     */
    void SyncProperties();

    /**
     * Sends a new property value to the server, or reverts it to the server value if its object is locked.
     *
     * @param   sfObject::SPtr objPtr the property belongs to.
     * @param   UObject* uobjPtr the property belongs to.
     * @param   UnrealProperty* upropPtr to sync.
     * @param   bool applyServerValue - if true, will apply the server value even if the object is unlocked.
     */
    void SyncProperty(
        sfObject::SPtr objPtr,
        UObject* uobjPtr,
        UnrealProperty* upropPtr,
        bool applyServerValue = false);

    /**
     * Sends a new property value to the server, or reverts it to the server value if its object is locked.
     *
     * @param   sfObject::SPtr objPtr the property belongs to.
     * @param   UObject* uobjPtr the property belongs to.
     * @param   const FName& name of property to sync.
     * @param   bool applyServerValue - if true, will apply the server value even if the object is unlocked.
     */
    void SyncProperty(
        sfObject::SPtr objPtr,
        UObject* uobjPtr,
        const FName& name,
        bool applyServerValue = false);

    /**
     * Rehashes stale properties and broadcasts change events. Clears state.
     */
    void CleanUp();

    /**
     * Registers UnrealProperty change handler for the given class.
     *
     * @param   FName classname
     * @param   PropertyChangeHandler handler
     */
    void RegisterPropertyChangeHandlerForClass(FName className, PropertyChangeHandler handler);

    /**
     * Unregisters UnrealProperty change handler for the given class.
     *
     * @param   FName classname
     */
    void UnregisterPropertyChangeHandlerForClass(FName className);

    /**
     * Gets all instances of the given default object.
     *
     * @param   UObject* defaultObjPtr - pointer to the default object
     * @param   TArray<UObject*>& instances
     */
    void GetInstances(UObject* defaultObjPtr, TArray<UObject*>& instances);

    /**
     * Gets all instances with default property value of the given default object.
     *
     * @param   UObject* defaultObjPtr - pointer to the default object
     * @param   UnrealProperty* upropPtr - property changed
     * @param   TArray<UObject*>& instances
     */
    void GetInstancesWithDefaultValue(UObject* defaultObjPtr, UnrealProperty* upropPtr, TArray<UObject*>& instances);

    /**
     * Adds, removes, and/or sets elements in a destination list to make it the same as a source list.
     *
     * @param   sfListProperty::SPtr destPtr to modify.
     * @param   sfListProperty::SPtr srcPtr to make destPtr a copy of.
     */
    void CopyList(sfListProperty::SPtr destPtr, sfListProperty::SPtr srcPtr);

    /**
     * Adds, removes, and/or sets fields in a destination dictionary so to make it the same as a source dictionary.
     *
     * @param   sfDictionaryProperty::SPtr destPtr to modify.
     * @param   sfDictionaryProperty::SPtr srcPtr to make destPtr a copy of.
     */
    void CopyDict(sfDictionaryProperty::SPtr destPtr, sfDictionaryProperty::SPtr srcPtr);

private:
    /**
     * Holds getter and setter delegates for converting between a UnrealProperty type and sfValueProperty.
     */
    struct TypeHandler
    {
    public:
        /**
         * Gets a UnrealProperty value using reflection and converts it to an sfProperty.
         *
         * @param   const sfUPropertyInstance& to get value for.
         * @return  sfProperty::SPtr
         */
        typedef std::function<sfProperty::SPtr(const sfUPropertyInstance&)> Getter;

        /**
         * Sets a UnrealProperty value using reflection to a value from an sfProperty.
         *
         * @param   const sfUPropertyInstance& to set value for.
         * @param   sfProperty::SPtr to get value from.
         * @return  bool true if the value changed.
         */
        typedef std::function<bool(const sfUPropertyInstance&, sfProperty::SPtr)> Setter;

        /**
         * Getter
         */
        Getter Get;

        /**
         * Setter
         */
        Setter Set;

        /**
         * Constructor
         *
         * @param   Getter getter
         * @param   Setter setter
         */
        TypeHandler(Getter getter, Setter setter) :
            Get{ getter },
            Set{ setter }
        {

        }
    };

    // TMaps seem buggy and I don't trust them. Dereferencing the pointer returned by TMap.find causes an access
    // violation, so we use std::unordered_map which works fine.
    // Keys are UnrealProperty class name ids.
    std::unordered_map<TypeHash, TypeHandler> m_typeHandlers;
    TMap<void*, TSharedPtr<FScriptMapHelper>> m_staleMaps;// maps that need rehashing
    TMap<FScriptSet*, TSharedPtr<FScriptSetHelper>> m_staleSets;// sets that need rehashing
    // properties changed by the server we need to fire events for
    TMap<UObject*, TSet<UnrealProperty*>> m_serverChangedProperties;
    // properties changed locally we need to process
    TMap<UObject*, TSet<UnrealProperty*>> m_localChangedProperties;
    // we don't call property change handlers on non-editable properties unless they're in the force sync list
    TSet<TPair<FName, FName>> m_forceSyncList;// key: owner class name, value: property name
    TSet<TPair<FName, FName>> m_blacklist;// key: class name, value: property name
    FDelegateHandle m_onPropertyChangeHandle;
    // key: class fname type hash
    std::unordered_map<TypeHash, PropertyChangeHandler> m_classToPropertyChangeHandler;
    TSet<FName> m_syncDefaultOnlyList;// Sync default only properties for types in this list
    std::unordered_set<sfObject::SPtr> m_syncedSubObjects;
    bool m_syncSubObjects;
    OnGetAssetPropertyEvent m_onGetAssetProperty;

    /**
     * Registers UnrealProperty type handlers.
     */
    void Initialize();

    /**
     * Creates a property type handler.
     *
     * @param   UClass* typePtr to create handler for.
     * @param   TypeHandler::Getter getter for getting properties of the given type.
     * @param   TypeHandler::Setter setter for setting properties of the given type.
     */
    void CreateTypeHandler(UClass* typePtr, TypeHandler::Getter getter, TypeHandler::Setter setter);

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 25
    /**
     * Creates a property type handler.
     *
     * @param   FFieldClass* typePtr to create handler for.
     * @param   TypeHandler::Getter getter for getting properties of the given type.
     * @param   TypeHandler::Setter setter for setting properties of the given type.
     */
    void CreateTypeHandler(FFieldClass* typePtr, TypeHandler::Getter getter, TypeHandler::Setter setter);
#endif

    /**
     * Returns true if the given UnrealProperty is in the force to sync list.
     *
     * @param   UObject* uobjPtr to check
     * @param   UnrealProperty* upropPtr to check
     * @return  bool
     */
    bool IsPropertyInForceSyncList(UnrealProperty* upropPtr);

    /**
     * Called when a property is changed through the details panel.
     *
     * @param   UObject* uobjPtr whose property changed.
     * @param   FPropertyChangedEvent& ev with information on what property changed.
     */
    void OnUPropertyChange(UObject* uobjPtr, FPropertyChangedEvent& ev);

    /**
     * Gets a double property value using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetDouble(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a double property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetDouble(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets a string property value using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetFString(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a string property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetFString(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets a text property value using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetFText(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a text property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetFText(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets a name property value using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetFName(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a name property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetFName(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets an enum property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetEnum(const sfUPropertyInstance& upropInstance);

    /**
     * Sets an enum property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetEnum(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets an array property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtrr
     */
    sfProperty::SPtr GetArray(const sfUPropertyInstance& upropInstance);

    /**
     * Sets an array property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetArray(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets a map property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetMap(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a map property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetMap(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets a set property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetSet(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a set property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetSet(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets a struct property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetStruct(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a struct property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetStruct(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets an object property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetObject(const sfUPropertyInstance& upropInstance);

    /**
     * Sets an object property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetObject(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets a soft object property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetSoftObject(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a soft object property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetSoftObject(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets a lazy object property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetLazyObject(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a lazy object property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetLazyObject(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets a weak object property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetWeakObject(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a weak object property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetWeakObject(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets an interface property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetInterface(const sfUPropertyInstance& upropInstance);

     /**
      * Sets an interface property value using reflection.
      *
      * @param   const sfUPropertyInstance& upropInstance to set.
      * @param   sfProperty::SPtr propPtr to get value from.
      * @return  bool true if the value changed.
      */
    bool SetInterface(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Creates property for the given object reference.
     *
     * @param   const sfUPropertyInstance& upropInstance
     * @param   UObject* referencePtr
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr CreatePropertyForObjectReference(
        const sfUPropertyInstance& upropInstance,
        UObject* referencePtr);

    /**
     * Gets the uobject referenced by a reference property.
     *
     * @param   sfReferenceProperty::SPtr propPtr to get referenced uobject for.
     * @return  UObject* referenced object
     */
    UObject* GetReference(sfReferenceProperty::SPtr propPtr);

    /**
     * Sets a reference property.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   UObject* referencePtr to point to.
     * @return  bool true if the property changed.
     */
    bool SetReference(const sfUPropertyInstance& upropInstance, UObject* referencePtr);

    /**
     * Gets a class property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetClass(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a class property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetClass(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Gets a soft class property value from an object using reflection converted to an sfProperty.
     *
     * @param   const sfUPropertyInstance& upropInstance to get.
     * @return  sfProperty::SPtr
     */
    sfProperty::SPtr GetSoftClass(const sfUPropertyInstance& upropInstance);

    /**
     * Sets a soft class property value using reflection.
     *
     * @param   const sfUPropertyInstance& upropInstance to set.
     * @param   sfProperty::SPtr propPtr to get value from.
     * @return  bool true if the value changed.
     */
    bool SetSoftClass(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr);

    /**
     * Takes a pointer to a struct and sets it to point at a field of the struct using reflection. Returns false if the
     * uproperty is not a struct property.
     *
     * @param   const sfName& name of field to get.
     * @param   UnrealProperty*& upropPtr - if not a struct property, returns false. Otherwise this is updated to point to
     *          the field property. If the field is not found, this is set to nullptr.
     * @param   void*& ptr to the struct data. Will be updated to point to the field data, if found.
     * @param   void*& defaultPtr to the default struct data. Will be updated to point to the default field data, if
     *          found. May be nullptr.
     * @return  bool true if upropPtr is a struct property.
     */
    bool GetStructField(const sfName& name, UnrealProperty*& upropPtr, void*& ptr, void*& defaultPtr);

    /**
     * Takes a pointer to an array and sets it to point at an element of the array using reflection. Returns false if
     * the uproperty is not an array property. If the index is outside the bounds of the array, expands the array so it
     * contains the index.
     *
     * @param   index of element to get.
     * @param   UnrealProperty*& upropPtr - if not an array property, returns false. Otherwise this is updated to point to
     *          the element property. If the element is not found, this is set to nullptr.
     * @param   void*& ptr to the array data. Will be updated to point to the element data, if found.
     * @param   void*& defaultPtr to the default array data. Will be set to nullptr.
     * @return  bool true if upropPtr is an array property.
     */
    bool GetArrayElement(int index, UnrealProperty*& upropPtr, void*& ptr, void*& defaultPtr);

    /**
     * Takes a pointer to a fixed array and sets it to point at an element of the array using reflection. Returns false if
     * the uproperty is not a fixed array property.
     *
     * @param   index of element to get.
     * @param   UnrealProperty* upropPtr - if not a fixed array property, returns false.
     * @param   void*& ptr to the array data. Will be updated to point to the element data, if found.
     * @param   void*& defaultPtr to the default array data. Will be updated to point to the default element data, if
     *          found. May be nullptr.
     * @return  bool true if upropPtr is a fixed array property.
     */
    bool GetFixedArrayElement(int index, UnrealProperty* upropPtr, void*& ptr, void*& defaultPtr);

    /**
     * Takes a pointer to a map and sets it to point at a key or value of the map using reflection. Returns false if
     * the uproperty is not a map property.
     *
     * @param   index of element to get.
     * @param   UnrealProperty*& upropPtr - if not a map property, returns false. Otherwise this is updated to point to the
     *          element property. If the element is not found, this is set to nullptr.
     * @param   void*& ptr to the map data. Will be updated to point to the element data, if found.
     * @param   void*& defaultPtr to the default map data. Will be set to nullptr.
     * @param   TSharedPtr<FScriptMapHelper> outMapPtr - will point to the map if the element we are getting is a key.
     * @param   std::stack<sfProperty::SPtr>& propertyStack containing the sub properties to look for next. If the
     *          uproperty is a map and the index is within its bounds, the top property will be popped off the stack.
     *          If it's index is 0, we'll get the key, If 1, we'll get the value.
     * @return  bool true if upropPtr is a map property.
     */
    bool GetMapElement(
        int index,
        UnrealProperty*& upropPtr,
        void*& ptr,
        void*& defaultPtr,
        TSharedPtr<FScriptMapHelper>& outMapPtr,
        std::stack<sfProperty::SPtr>& propertyStack);

    /**
     * Takes a pointer to a set and sets it to point at an element of the set using reflection. Returns false if the
     * uproperty is not a set property.
     *
     * @param   index of element to get.
     * @param   UnrealProperty*& upropPtr - if not a set property, returns false. Otherwise this is updated to point to the
     *          element property. If the element is not found, this is set to nullptr.
     * @param   void*& ptr to the set data. Will be updated to point to the element data, if found.
     * @param   void*& defaultPtr to the default set data. Will be set to nullptr.
     * @param   TSharedPtr<FScriptMapHelper> outSetPtr - will point to the set.
     * @return  bool true if upropPtr is a map property.
     */
    bool GetSetElement(
        int index,
        UnrealProperty*& upropPtr,
        void*& ptr,
        void*& defaultPtr,
        TSharedPtr<FScriptSetHelper>& outSetPtr);

    /**
     * Creates a property handler for type T.
     */
    template<typename T>
    void CreateTypeHandler()
    {
        CreateTypeHandler(T::StaticClass(),
            [](const sfUPropertyInstance& upropInstance)
            {
                T* tPtr = CastUnrealProperty<T>(upropInstance.Property());
                return sfValueProperty::Create(tPtr->GetPropertyValue(upropInstance.Data()));
            },
            [](const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
            {
                T* tPtr = CastUnrealProperty<T>(upropInstance.Property());
                if (!propPtr->Equals(sfValueProperty::Create(tPtr->GetPropertyValue(upropInstance.Data()))))
                {
                    tPtr->SetPropertyValue(upropInstance.Data(), propPtr->AsValue()->GetValue());
                    return true;
                }
                return false;
            }
        );
    }

    /**
     * Creates a property handler for type T that casts the value to U, where U is a type supported by ksMultiType.
     */
    template<typename T, typename U>
    void CreateTypeHandler()
    {
        CreateTypeHandler(T::StaticClass(),
            [](const sfUPropertyInstance& upropInstance)
            {
                T* tPtr = CastUnrealProperty<T>(upropInstance.Property());
                return sfValueProperty::Create((U)tPtr->GetPropertyValue(upropInstance.Data()));
            },
            [](const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
            {
                T* tPtr = CastUnrealProperty<T>(upropInstance.Property());
                U value = propPtr->AsValue()->GetValue();
                if ((U)tPtr->GetPropertyValue(upropInstance.Data()) != value)
                {
                    tPtr->SetPropertyValue(upropInstance.Data(), value);
                    return true;
                }
                return false;
            }
        );
    }
};

#undef LOG_CHANNEL