#include "../../Public/Translators/sfBaseUObjectTranslator.h"
#include "sfUObjectTranslator.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfUnrealUtils.h"

using namespace KS::SceneFusion2;

void sfBaseUObjectTranslator::RegisterOnModifyHandler(FName className, sfBaseUObjectTranslator::OnModifyHandler handler)
{
    m_onModifyHandlers.emplace(sfUnrealUtils::GetNameTypeHash(className), handler);
}

void sfBaseUObjectTranslator::UnregisterOnModifyHandler(FName className)
{
    m_onModifyHandlers.erase(sfUnrealUtils::GetNameTypeHash(className));
}

void sfBaseUObjectTranslator::RegisterPostPropertyChangeHandler(
    const FName& className,
    const FName& propertyName,
    sfBaseUObjectTranslator::PostPropertyChangeHandler handler)
{
    m_postPropertyChangeHandlers.emplace(CreateKey(className, propertyName), handler);
}

void sfBaseUObjectTranslator::UnregisterPostPropertyChangeHandler(const FName& className, const FName& propertyName)
{
    m_postPropertyChangeHandlers.erase(CreateKey(className, propertyName));
}

void sfBaseUObjectTranslator::RegisterRemoveElementsHandler(
    const FName& className,
    const FName& propertyName,
    sfBaseUObjectTranslator::RemoveElementsHandler handler)
{
    m_removeElementsHandlers.emplace(CreateKey(className, propertyName), handler);
}

void sfBaseUObjectTranslator::UnregisterRemoveElementsHandler(const FName& className, const FName& propertyName)
{
    m_removeElementsHandlers.erase(CreateKey(className, propertyName));
}

uint64_t sfBaseUObjectTranslator::CreateKey(const FName& className, const FName& propertyName)
{
    return ((uint64_t)sfUnrealUtils::GetNameTypeHash(className) << 32) | sfUnrealUtils::GetNameTypeHash(propertyName);
}

void sfBaseUObjectTranslator::OnPropertyChange(sfProperty::SPtr propertyPtr)
{
    UObject* uobjPtr = GetUObject(propertyPtr->GetContainerObject());
    if (uobjPtr == nullptr || CallPropertyHandler(uobjPtr, propertyPtr))
    {
        return;
    }

    SetPropertyOnObjectAndAllInstances(uobjPtr, propertyPtr, 
        [](UObject* uobjPtr, const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
    {
        sfPropertyManager::Get().SetValue(uobjPtr, upropInstance, propPtr);
    });
}

void sfBaseUObjectTranslator::OnRemoveField(sfDictionaryProperty::SPtr dictPtr, const sfName& name)
{
    UObject* uobjPtr = GetUObject(dictPtr->GetContainerObject());
    if (uobjPtr == nullptr)
    {
        return;
    }

    if (dictPtr->GetDepth() != 0)
    {
        // This is a field of a struct
        if (!CallPropertyHandler(uobjPtr, dictPtr))
        {
            sfUPropertyInstance upropInstance = sfPropertyManager::Get().FindUProperty(uobjPtr, dictPtr);
            sfPropertyManager::Get().SetStructFieldToDefaultValue(uobjPtr, upropInstance, name);
        }
        return;
    }

    auto handlerIter = m_propertyChangeHandlers.find(name);
    if (handlerIter != m_propertyChangeHandlers.end())
    {
        // Call property-specific handler
        bool handled = false;
        sfUnrealUtils::PreserveUndoStack([handlerIter, uobjPtr, &handled]()
        {
            handled = handlerIter->second(uobjPtr, nullptr);
        });
        if (handled)
        {
            // Event was handled by the property-specific handler
            return;
        }
    }

    UnrealProperty* upropPtr = uobjPtr->GetClass()->FindPropertyByName(FName(UTF8_TO_TCHAR(name->c_str())));
    if (upropPtr != nullptr)
    {
        // If this is an array property and the default value is an empty array, call the remove elements handler for
        // all elements
        UnrealArrayProperty* arrayPropPtr = CastUnrealProperty<UnrealArrayProperty>(upropPtr);
        if (arrayPropPtr != nullptr)
        {
            UObject* defaultPtr = sfPropertyManager::Get().GetDefaultObject(uobjPtr);
            FScriptArrayHelper defaultArray{ arrayPropPtr, arrayPropPtr->ContainerPtrToValuePtr<void>(defaultPtr) };
            if (defaultArray.Num() == 0)
            {
                FScriptArrayHelper array{ arrayPropPtr, arrayPropPtr->ContainerPtrToValuePtr<void>(uobjPtr) };
                if (array.Num() > 0)
                {
                    uint64_t key = CreateKey(arrayPropPtr->GetOwnerStruct()->GetFName(), arrayPropPtr->GetFName());
                    auto iter = m_removeElementsHandlers.find(key);
                    if (iter != m_removeElementsHandlers.end())
                    {
                        iter->second(uobjPtr, 0, array.Num());
                    }
                }
            }
        }
        sfPropertyManager::Get().SetToDefaultValue(uobjPtr, upropPtr);
    }
}

void sfBaseUObjectTranslator::OnListAdd(sfListProperty::SPtr listPtr, int index, int count)
{
    UObject* uobjPtr = GetUObject(listPtr->GetContainerObject());
    if (uobjPtr == nullptr || CallPropertyHandler(uobjPtr, listPtr))
    {
        return;
    }

    SetPropertyOnObjectAndAllInstances(
        uobjPtr,
        listPtr,
        [this, index, count](UObject* uobjPtr, const sfUPropertyInstance& upropInstance, sfProperty::SPtr listPtr)
        {
            ArrayInsert(uobjPtr, upropInstance, listPtr->AsList(), index, count) ||
            SetInsert(uobjPtr, upropInstance, listPtr->AsList(), index, count) ||
            MapInsert(uobjPtr, upropInstance, listPtr->AsList(), index, count);
            sfPropertyManager::Get().MarkPropertyChanged(uobjPtr, upropInstance.Property(), listPtr);
        });
}

bool sfBaseUObjectTranslator::ArrayInsert(
    UObject* uobjPtr,
    const sfUPropertyInstance& upropInstance,
    sfListProperty::SPtr listPtr,
    int index,
    int count)
{
    UnrealArrayProperty* arrayPropPtr = CastUnrealProperty<UnrealArrayProperty>(upropInstance.Property());
    if (arrayPropPtr == nullptr)
    {
        return false;
    }
    FScriptArrayHelper array(arrayPropPtr, upropInstance.Data());
    if (array.Num() < index)
    {
        // Expand the array to the index we want to insert at.
        array.Resize(index);
    }
    array.InsertValues(index, count);
    for (int i = index; i < index + count; i++)
    {
        sfPropertyManager::Get().SetValue(uobjPtr, sfUPropertyInstance(arrayPropPtr->Inner, array.GetRawPtr(i)),
            listPtr->Get(i));
    }
    return true;
}

bool sfBaseUObjectTranslator::SetInsert(
    UObject* uobjPtr,
    const sfUPropertyInstance& upropInstance,
    sfListProperty::SPtr listPtr,
    int index,
    int count)
{
    UnrealSetProperty* setPropPtr = CastUnrealProperty<UnrealSetProperty>(upropInstance.Property());
    if (setPropPtr == nullptr)
    {
        return false;
    }
    TSharedPtr<FScriptSetHelper> setPtr = MakeShareable(new FScriptSetHelper(setPropPtr, upropInstance.Data()));
    int firstInsertIndex = setPtr->GetMaxIndex();
    int lastInsertIndex = 0;
    for (int i = 0; i < count; i++)
    {
        int insertIndex = setPtr->AddDefaultValue_Invalid_NeedsRehash();
        firstInsertIndex = FMath::Min(firstInsertIndex, insertIndex);
        lastInsertIndex = FMath::Max(lastInsertIndex, insertIndex);
    }
    int listIndex = -1;
    for (int i = 0; i < setPtr->GetMaxIndex(); i++)
    {
        if (!setPtr->IsValidIndex(i))
        {
            continue;
        }
        listIndex++;
        if (listIndex < index && i < firstInsertIndex)
        {
            continue;
        }
        sfPropertyManager::Get().SetValue(uobjPtr, sfUPropertyInstance(setPropPtr->ElementProp, setPtr->GetElementPtr(i)),
            listPtr->Get(listIndex));
        if (listIndex >= index + count - 1 && i >= lastInsertIndex)
        {
            break;
        }
    }
    sfPropertyManager::Get().MarkHashStale(upropInstance);
    return true;
}

bool sfBaseUObjectTranslator::MapInsert(
    UObject* uobjPtr,
    const sfUPropertyInstance& upropInstance,
    sfListProperty::SPtr listPtr,
    int index,
    int count)
{
    UnrealMapProperty* mapPropPtr = CastUnrealProperty<UnrealMapProperty>(upropInstance.Property());
    if (mapPropPtr == nullptr)
    {
        return false;
    }
    TSharedPtr<FScriptMapHelper> mapPtr = MakeShareable(new FScriptMapHelper(mapPropPtr, upropInstance.Data()));
    int firstInsertIndex = mapPtr->GetMaxIndex();
    int lastInsertIndex = 0;
    for (int i = 0; i < count; i++)
    {
        int insertIndex = mapPtr->AddDefaultValue_Invalid_NeedsRehash();
        firstInsertIndex = FMath::Min(firstInsertIndex, insertIndex);
        lastInsertIndex = FMath::Max(lastInsertIndex, insertIndex);
    }
    int listIndex = -1;
    for (int i = 0; i < mapPtr->GetMaxIndex(); i++)
    {
        if (!mapPtr->IsValidIndex(i))
        {
            continue;
        }
        listIndex++;
        if (listIndex < index && i < firstInsertIndex)
        {
            continue;
        }
        sfListProperty::SPtr pairPtr = listPtr->Get(listIndex)->AsList();
        sfPropertyManager::Get().SetValue(uobjPtr, sfUPropertyInstance(mapPropPtr->KeyProp, mapPtr->GetKeyPtr(i)),
            pairPtr->Get(0));
        sfPropertyManager::Get().SetValue(uobjPtr, sfUPropertyInstance(mapPropPtr->ValueProp, mapPtr->GetValuePtr(i)),
            pairPtr->Get(1));
        if (listIndex >= index + count - 1 && i >= lastInsertIndex)
        {
            break;
        }
    }
    sfPropertyManager::Get().MarkHashStale(upropInstance);
    return true;
}

void sfBaseUObjectTranslator::OnListRemove(sfListProperty::SPtr listPtr, int index, int count)
{
    UObject* uobjPtr = GetUObject(listPtr->GetContainerObject());
    if (uobjPtr == nullptr || CallPropertyHandler(uobjPtr, listPtr))
    {
        return;
    }

    SetPropertyOnObjectAndAllInstances(
        uobjPtr,
        listPtr,
        [this, index, count](UObject* uobjPtr, const sfUPropertyInstance& upropInstance, sfProperty::SPtr listPtr)
        {
            ArrayRemove(uobjPtr, upropInstance, index, count) || SetRemove(upropInstance, index, count) ||
            MapRemove(upropInstance, index, count);
            sfPropertyManager::Get().MarkPropertyChanged(uobjPtr, upropInstance.Property(), listPtr);
        });
}

bool sfBaseUObjectTranslator::ArrayRemove(UObject* uobjPtr, const sfUPropertyInstance& upropInstance, int index, int count)
{
    UnrealArrayProperty* arrayPropPtr = CastUnrealProperty<UnrealArrayProperty>(upropInstance.Property());
    if (arrayPropPtr == nullptr)
    {
        return false;
    }

    uint64_t key = CreateKey(arrayPropPtr->GetOwnerStruct()->GetFName(), arrayPropPtr->GetFName());
    auto iter = m_removeElementsHandlers.find(key);
    if (iter != m_removeElementsHandlers.end())
    {
        iter->second(uobjPtr, index, count);
    }

    FScriptArrayHelper array(arrayPropPtr, upropInstance.Data());
    array.RemoveValues(index, count);
    return true;
}

bool sfBaseUObjectTranslator::SetRemove(const sfUPropertyInstance& upropInstance, int index, int count)
{
    UnrealSetProperty* setPropPtr = CastUnrealProperty<UnrealSetProperty>(upropInstance.Property());
    if (setPropPtr == nullptr)
    {
        return false;
    }
    FScriptSetHelper set(setPropPtr, upropInstance.Data());
    int i = 0;
    for (; i < set.GetMaxIndex(); i++)
    {
        if (set.IsValidIndex(i))
        {
            index--;
            if (index < 0)
            {
                break;
            }
        }
    }
    set.RemoveAt(i, count);
    return true;
}

bool sfBaseUObjectTranslator::MapRemove(const sfUPropertyInstance& upropInstance, int index, int count)
{
    UnrealMapProperty* mapPropPtr = CastUnrealProperty<UnrealMapProperty>(upropInstance.Property());
    if (mapPropPtr == nullptr)
    {
        return false;
    }
    FScriptMapHelper map(mapPropPtr, upropInstance.Data());
    int i = 0;
    for (; i < map.GetMaxIndex(); i++)
    {
        if (map.IsValidIndex(i))
        {
            index--;
            if (index < 0)
            {
                break;
            }
        }
    }
    map.RemoveAt(i, count);
    return true;
}

void sfBaseUObjectTranslator::PostPropertyChange(UObject* uobjPtr, UnrealProperty* upropPtr)
{
    uint64_t key = CreateKey(upropPtr->GetOwnerStruct()->GetFName(), upropPtr->GetFName());
    auto iter = m_postPropertyChangeHandlers.find(key);
    if (iter != m_postPropertyChangeHandlers.end())
    {
        iter->second(uobjPtr);
    }
}

void sfBaseUObjectTranslator::OnUObjectModified(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    UClass* classPtr = uobjPtr->GetClass();
    while (classPtr != nullptr)
    {
        auto iter = m_onModifyHandlers.find(sfUnrealUtils::GetClassTypeHash(classPtr));
        if (iter != m_onModifyHandlers.end())
        {
            iter->second(objPtr, uobjPtr);
        }
        classPtr = classPtr->GetSuperClass();
    }
}

UObject* sfBaseUObjectTranslator::GetUObject(sfObject::SPtr objPtr)
{
    return sfObjectMap::GetUObject(objPtr);
}

void sfBaseUObjectTranslator::SetPropertyOnObjectAndAllInstances(
    UObject* uobjPtr,
    sfProperty::SPtr propertyPtr,
    SetPropertyCallback setProperty)
{
    sfUPropertyInstance upropInstance = sfPropertyManager::Get().FindUProperty(uobjPtr, propertyPtr);
    if (upropInstance.IsValid())
    {
        TArray<UObject*> instances;
        UnrealProperty* upropPtr = nullptr;
        if (upropInstance.ContainerProperty() != nullptr)
        {
            upropPtr = upropInstance.ContainerProperty();
        }
        else
        {
            upropPtr = upropInstance.Property();
        }

        sfPropertyManager::Get().GetInstancesWithDefaultValue(uobjPtr, upropPtr, instances);
        setProperty(uobjPtr, upropInstance, propertyPtr);

        for (UObject* instance : instances)
        {
            upropPtr->CopyCompleteValue_InContainer(instance, uobjPtr);
            sfPropertyManager::Get().MarkPropertyChanged(instance, upropPtr);
        }
    }
}

void sfBaseUObjectTranslator::InitializeChildren(sfObject::SPtr objPtr)
{
    TSharedPtr<sfUObjectTranslator> uobjectTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfUObjectTranslator>(sfType::UObject);
    if (!uobjectTranslatorPtr.IsValid())
    {
        return;
    }
    auto iter = objPtr->Children().begin();
    while (iter != objPtr->Children().end())
    {
        sfObject::SPtr childPtr = *iter;
        // Increment iterator before calling InitializeObjectProperties to prevent an invalid iterator if the child
        // gets detached.
        ++iter;
        if (childPtr->Type() == sfType::UObject)
        {
            uobjectTranslatorPtr->InitializeObjectProperties(childPtr, sfObjectMap::GetUObject(childPtr));
        }
    }
}

bool sfBaseUObjectTranslator::CallPropertyHandler(UObject* uobjPtr, sfProperty::SPtr propertyPtr)
{
    // Get property at depth 1
    int depth = propertyPtr->GetDepth();
    sfProperty::SPtr currentPtr = propertyPtr;
    while (depth > 1)
    {
        currentPtr = currentPtr->GetParentProperty();
        depth--;
    }
    bool handled = false;
    auto handlerIter = m_propertyChangeHandlers.find(currentPtr->Key());
    if (handlerIter != m_propertyChangeHandlers.end())
    {
        // Call property-specific handler
        sfUnrealUtils::PreserveUndoStack([handlerIter, uobjPtr, currentPtr, &handled]()
        {
            handled = handlerIter->second(uobjPtr, currentPtr);
        });
    }
    return handled;
}