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

#include "sfUPropertyTypes.h"

#include <CoreMinimal.h>
#include <Runtime/CoreUObject/Public/UObject/UnrealType.h>

/**
 * Stores pointers to an Unreal property and to the data for an instance of that property.
 */
class sfUPropertyInstance
{
public:
    /**
     * Constructor for an invalid property instance.
     */
    sfUPropertyInstance() :
        m_propertyPtr{ nullptr },
        m_dataPtr{ nullptr },
        m_defaultDataPtr{ nullptr },
        m_mapPtr{ nullptr },
        m_setPtr{ nullptr },
        m_containerProperty { nullptr }
    {

    }

    /**
     * Constructor
     *
     * @param   UnrealProperty* propertyPtr
     * @param   void* dataPtr
     */
    sfUPropertyInstance(UnrealProperty* propertyPtr, void* dataPtr) :
        m_propertyPtr{ propertyPtr },
        m_dataPtr{ dataPtr },
        m_defaultDataPtr{ nullptr },
        m_mapPtr{ nullptr },
        m_setPtr{ nullptr },
        m_containerProperty{ nullptr }
    {

    }

    /**
     * @param   UnrealProperty* propertyPtr
     * @param   void* dataPtr
     * @param   void* defaultDataPtr - pointer to default property data.
     */
    sfUPropertyInstance(UnrealProperty* propertyPtr, void* dataPtr, void* defaultDataPtr) :
        m_propertyPtr{ propertyPtr },
        m_dataPtr{ dataPtr },
        m_defaultDataPtr{ defaultDataPtr },
        m_mapPtr{ nullptr },
        m_setPtr{ nullptr },
        m_containerProperty{ nullptr }
    {

    }

    /**
     * Constructor
     *
     * @param   UnrealProperty* propertyPtr
     * @param   void* dataPtr
     * @param   void* defaultDataPtr - pointer to default property data.
     * @param   TSharedPtr<FScriptMapHelper> mapPtr - container for this property, needed only if this property is a
     *          key in a hash.
     * @param   TSharedPtr<FScriptSetHelper> setPtr - container for this property, needed only if this property is a
     *          key in a hash.
     * @param   UnrealProperty* containerProperty
     */
    sfUPropertyInstance(
        UnrealProperty* propertyPtr,
        void* dataPtr,
        void* defaultDataPtr,
        TSharedPtr<FScriptMapHelper> mapPtr,
        TSharedPtr<FScriptSetHelper> setPtr,
        UnrealProperty* containerProperty)
        :
        m_propertyPtr{ propertyPtr },
        m_dataPtr{ dataPtr },
        m_defaultDataPtr{ defaultDataPtr },
        m_mapPtr{ mapPtr },
        m_setPtr{ setPtr },
        m_containerProperty{ containerProperty }
    {

    }        

    /**
     * Checks if the property is valid.
     *
     * @return  bool true if the property pointer and data pointer are not null.
     */
    bool IsValid() const { return m_propertyPtr != nullptr && m_dataPtr != nullptr; }

    /**
     * Unreal property pointer.
     *
     * @return  UnrealProperty*
     */
    UnrealProperty* Property() const { return m_propertyPtr; }

    /**
     * Pointer to property instance data.
     *
     * @return  void*
     */
    void* Data() const { return m_dataPtr; }

    /**
     * Pointer to default property data.
     *
     * @return  void*
     */
    void* DefaultData() const { return m_defaultDataPtr; }

    /**
     * If this property is a key in a map, this points to the map.
     *
     * @return  const TSharedPtr<FScriptMapHelper>&
     */
    const TSharedPtr<FScriptMapHelper>& ContainerMap() const { return m_mapPtr; }

    /**
     * If this property is a key in a set, this points to the set.
     *
     * @return  const TSharedPtr<FScriptSetHelper>&
     */
    const TSharedPtr<FScriptSetHelper>& ContainerSet() const { return m_setPtr; }

    /**
     * Returns the container property.
     *
     * @return  UnrealProperty*
     */
    UnrealProperty* ContainerProperty() const { return m_containerProperty; }

    /**
     * Advances the data pointer to the next element in the array. Does not do any bounds checking.
     */
    void NextElement()
    {
        m_dataPtr = (void*)((uint8_t*)m_dataPtr + m_propertyPtr->ElementSize);
        if (m_defaultDataPtr != nullptr)
        {
            m_defaultDataPtr = (void*)((uint8_t*)m_defaultDataPtr + m_propertyPtr->ElementSize);
        }
    }

private:
    UnrealProperty* m_propertyPtr;
    void* m_dataPtr;
    void* m_defaultDataPtr;
    // If this property is a key in a hash, we need a pointer to the hash container so it can be rehashed if we set the
    // property.
    TSharedPtr<FScriptMapHelper> m_mapPtr;
    TSharedPtr<FScriptSetHelper> m_setPtr;
    UnrealProperty* m_containerProperty;
};