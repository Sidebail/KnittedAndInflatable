#include "../Public/sfPropertyManager.h"
#include "../Public/SceneFusion.h"
#include "../Public/sfObjectMap.h"
#include "../Public/Consts.h"
#include "../Public/sfLoader.h"
#include "../Public/sfUnrealUtils.h"
#include "../Public/sfPropertyUtil.h"

#include <sfNullProperty.h>
#include <UObject/UnrealType.h>
#include <UObject/EnumProperty.h>
#include <UObject/TextProperty.h>
#include <UObject/CoreRedirects.h>
#include <Engine/Blueprint.h>

#define LOG_CHANNEL "sfPropertyManager"

#define CREATE_TYPE_HANDLER(className, getter, setter) CreateTypeHandler(className::StaticClass(), \
    [this](const sfUPropertyInstance& upropInstance) { return getter(upropInstance); }, \
    [this](const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr) { return setter(upropInstance, propPtr); })

using namespace KS;

sfPropertyManager& sfPropertyManager::Get()
{
    static sfPropertyManager instance;
    return instance;
}

sfPropertyManager::sfPropertyManager() :
    m_syncSubObjects{ false }
{
    
}

sfUPropertyInstance sfPropertyManager::FindUProperty(UObject* uobjPtr, sfProperty::SPtr propPtr)
{
    if (uobjPtr == nullptr || propPtr == nullptr)
    {
        return sfUPropertyInstance();
    }
    // Push property and its ancestors into a stack, so we can then iterate them from the top down
    // We don't need to push the root dictionary into the stack
    std::stack<sfProperty::SPtr> stack;
    while (propPtr->GetDepth() > 0)
    {
        stack.push(propPtr);
        propPtr = propPtr->GetParentProperty();
    }
    UnrealProperty* upropPtr = nullptr;
    UnrealProperty* containerUPropPtr = nullptr;
    void* ptr = nullptr;// pointer to UnrealProperty instance data
    void* defaultPtr = nullptr;// pointer to default UnrealProperty data. Only used for struct subproperties.
    TSharedPtr<FScriptMapHelper> mapPtr = nullptr;
    TSharedPtr<FScriptSetHelper> setPtr = nullptr;
    // Traverse properties from the top down, finding the UnrealProperty at each level until we reach the one we want or
    // don't find what we expect.
    while (stack.size() > 0)
    {
        propPtr = stack.top();
        stack.pop();
        if (upropPtr == nullptr)
        {
            // Get the first property from the object
            upropPtr = uobjPtr->GetClass()->FindPropertyByName(FName(UTF8_TO_TCHAR(propPtr->Key()->c_str())));
            if (upropPtr == nullptr)
            {
                break;
            }
            ptr = upropPtr->ContainerPtrToValuePtr<void>(uobjPtr);
            containerUPropPtr = upropPtr;
            if (upropPtr->GetClass() == UnrealStructProperty::StaticClass())
            {
                // Get the default struct value so we can check if subproperties have their default value
                UObject* defaultObjPtr = GetDefaultObject(uobjPtr);
                if (defaultObjPtr != nullptr)
                {
                    defaultPtr = upropPtr->ContainerPtrToValuePtr<void>(defaultObjPtr);
                }
            }
            continue;
        }
        if (!GetArrayElement(propPtr->Index(), upropPtr, ptr, defaultPtr) &&
            !GetFixedArrayElement(propPtr->Index(), upropPtr, ptr, defaultPtr) &&
            !GetStructField(propPtr->Key(), upropPtr, ptr, defaultPtr) &&
            !GetMapElement(propPtr->Index(), upropPtr, ptr, defaultPtr, mapPtr, stack) &&
            !GetSetElement(propPtr->Index(), upropPtr, ptr, defaultPtr, setPtr))
        {
            // We were expecting the UnrealProperty to be one of the above container types but it was not. Abort.
            upropPtr = nullptr;
            break;
        }
        if (upropPtr == nullptr)
        {
            // We did not find the field or element we were looking for. Abort.
            break;
        }
    }
    if (upropPtr == nullptr)
    {
        KS::Log::Warning("Could not find property " + propPtr->GetPath() + " on " +
            std::string(TCHAR_TO_UTF8(*uobjPtr->GetClass()->GetName())), LOG_CHANNEL);
        return sfUPropertyInstance();
    }
    return sfUPropertyInstance(upropPtr, ptr, defaultPtr, mapPtr, setPtr, containerUPropPtr);
}

bool sfPropertyManager::GetStructField(const sfName& name, UnrealProperty*& upropPtr, void*& ptr, void*& defaultPtr)
{
    UnrealStructProperty* structPropPtr = CastUnrealProperty<UnrealStructProperty>(upropPtr);
    if (structPropPtr == nullptr)
    {
        return false;
    }
    if (!name.IsValid())
    {
        upropPtr = nullptr;
        return true;
    }
    upropPtr = structPropPtr->Struct->FindPropertyByName(FName(UTF8_TO_TCHAR(name->c_str())));
    if (upropPtr != nullptr)
    {
        ptr = upropPtr->ContainerPtrToValuePtr<void>(ptr);
        if (defaultPtr != nullptr)
        {
            defaultPtr = upropPtr->ContainerPtrToValuePtr<void>(defaultPtr);
        }
    }
    return true;
}

bool sfPropertyManager::GetArrayElement(int index, UnrealProperty*& upropPtr, void*& ptr, void*& defaultPtr)
{
    UnrealArrayProperty* arrayPropPtr = CastUnrealProperty<UnrealArrayProperty>(upropPtr);
    if (arrayPropPtr == nullptr || index < 0)
    {
        return false;
    }
    FScriptArrayHelper array(arrayPropPtr, ptr);
    if (index >= array.Num())
    {
        // Expand the array so it contains the index
        array.Resize(index + 1);
    }
    upropPtr = arrayPropPtr->Inner;
    ptr = array.GetRawPtr(index);
    defaultPtr = nullptr;
    return true;
}

bool sfPropertyManager::GetFixedArrayElement(int index, UnrealProperty* upropPtr, void*& ptr, void*& defaultPtr)
{
    if (index < 0 || upropPtr->ArrayDim <= index || upropPtr->ArrayDim <= 1)
    {
        return false;
    }
    ptr = (void*)((uint8_t*)ptr + upropPtr->ElementSize * index);
    if (defaultPtr != nullptr)
    {
        defaultPtr = (void*)((uint8_t*)defaultPtr + upropPtr->ElementSize * index);
    }
    return true;
}

bool sfPropertyManager::GetMapElement(
    int index,
    UnrealProperty*& upropPtr,
    void*& ptr,
    void*& defaultPtr,
    TSharedPtr<FScriptMapHelper>& outMapPtr,
    std::stack<sfProperty::SPtr>& propertyStack)
{
    UnrealMapProperty* mapPropPtr = CastUnrealProperty<UnrealMapProperty>(upropPtr);
    if (mapPropPtr == nullptr)
    {
        return false;
    }
    defaultPtr = nullptr;
    // Because maps are serialized as lists of key values, we expect at least one more property in the stack
    if (propertyStack.size() <= 0)
    {
        upropPtr = nullptr;
        return true;
    }
    outMapPtr = MakeShareable(new FScriptMapHelper(mapPropPtr, ptr));
    if (index < 0 || index >= outMapPtr->Num())
    {
        upropPtr = nullptr;
        return true;
    }
    int sparseIndex = -1;
    while (index >= 0)
    {
        sparseIndex++;
        if (sparseIndex >= outMapPtr->GetMaxIndex())
        {
            upropPtr = nullptr;
            return true;
        }
        if (outMapPtr->IsValidIndex(sparseIndex))
        {
            index--;
        }
    }
    // Get the next property in the stack, and check its index to determine if we want the map key or value.
    sfProperty::SPtr propPtr = propertyStack.top();
    propertyStack.pop();
    if (propPtr->Index() == 0)
    {
        upropPtr = mapPropPtr->KeyProp;
        ptr = outMapPtr->GetKeyPtr(sparseIndex);
    }
    else if (propPtr->Index() == 1)
    {
        upropPtr = mapPropPtr->ValueProp;
        ptr = outMapPtr->GetValuePtr(sparseIndex);
        outMapPtr = nullptr;
    }
    else
    {
        upropPtr = nullptr;
    }
    return true;
}

bool sfPropertyManager::GetSetElement(
    int index,
    UnrealProperty*& upropPtr,
    void*& ptr,
    void*& defaultPtr,
    TSharedPtr<FScriptSetHelper>& outSetPtr)
{
    UnrealSetProperty* setPropPtr = CastUnrealProperty<UnrealSetProperty>(upropPtr);
    if (setPropPtr == nullptr)
    {
        return false;
    }
    defaultPtr = nullptr;
    outSetPtr = MakeShareable(new FScriptSetHelper(setPropPtr, ptr));
    if (index < 0 || index >= outSetPtr->Num())
    {
        upropPtr = nullptr;
        return true;
    }
    int sparseIndex = -1;
    while (index >= 0)
    {
        sparseIndex++;
        if (sparseIndex >= outSetPtr->GetMaxIndex())
        {
            upropPtr = nullptr;
            return true;
        }
        if (outSetPtr->IsValidIndex(sparseIndex))
        {
            index--;
        }
    }
    upropPtr = setPropPtr->ElementProp;
    ptr = outSetPtr->GetElementPtr(sparseIndex);
    return true;
}

sfProperty::SPtr sfPropertyManager::GetValue(UObject* uobjPtr, UnrealProperty* upropPtr)
{
    if (uobjPtr == nullptr || upropPtr == nullptr)
    {
        return nullptr;
    }
    if (m_typeHandlers.size() == 0)
    {
        Initialize();
    }
    auto iter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(upropPtr->GetClass()));
    if (iter == m_typeHandlers.end())
    {
        return nullptr;
    }
    UObject* defaultObjPtr = nullptr;
    if (upropPtr->GetClass() == UnrealStructProperty::StaticClass())
    {
        // Get the default struct value so we can check if subproperties have their default value
        defaultObjPtr = GetDefaultObject(uobjPtr);
    }
    // ArrayDim is the size of the fixed array. It is always 1 for non-fixed arrays.
    if (upropPtr->ArrayDim == 1)
    {
        void* defaultPtr = defaultObjPtr == nullptr ? nullptr : upropPtr->ContainerPtrToValuePtr<void>(defaultObjPtr);
        return iter->second.Get(
            sfUPropertyInstance(upropPtr, upropPtr->ContainerPtrToValuePtr<void>(uobjPtr), defaultPtr));
    }

    sfListProperty::SPtr listPtr = sfListProperty::Create();
    for (int i = 0; i < upropPtr->ArrayDim; i++)
    {
        void* defaultPtr = defaultObjPtr == nullptr ? 
            nullptr : upropPtr->ContainerPtrToValuePtr<void>(defaultObjPtr, i);
        listPtr->Add(iter->second.Get(
            sfUPropertyInstance(upropPtr, upropPtr->ContainerPtrToValuePtr<void>(uobjPtr, i), defaultPtr)));
    }
    return listPtr;
}

bool sfPropertyManager::SetValue(UObject* uobjPtr, const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    if (!upropInstance.IsValid() || propPtr == nullptr)
    {
        return false;
    }
    if (m_typeHandlers.size() == 0)
    {
        Initialize();
    }
    auto iter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(upropInstance.Property()->GetClass()));
    if (iter == m_typeHandlers.end())
    {
        return false;
    }
    bool changed = false;
    // ArrayDim is the size of the fixed array. It is always 1 for non-fixed arrays. If this is a fixed array but the
    // sfproperty is not a list, then we are setting a single element within the fixed array.
    if (upropInstance.Property()->ArrayDim == 1 || propPtr->Type() != sfProperty::LIST)
    {
        changed = iter->second.Set(upropInstance, propPtr);
    }
    else
    {
        sfListProperty::SPtr listPtr = propPtr->AsList();
        int num = FMath::Min(listPtr->Size(), upropInstance.Property()->ArrayDim);
        sfUPropertyInstance current = upropInstance;
        for (int i = 0; i < num; i++)
        {
            if (iter->second.Set(current, listPtr->Get(i)))
            {
                changed = true;
            }
            current.NextElement();
        }
    }
    if (changed)
    {
        MarkHashStale(upropInstance);
        MarkPropertyChanged(uobjPtr, upropInstance.Property(), propPtr);
        return true;
    }
    return false;
}

bool sfPropertyManager::IsDefaultValue(UObject* uobjPtr, UnrealProperty* upropPtr)
{
    if (uobjPtr == nullptr ||
        uobjPtr->HasAnyFlags(RF_ArchetypeObject) ||
        upropPtr == nullptr)
    {
        return false;
    }
    UObject* defaultPtr = GetDefaultObject(uobjPtr);
    if (uobjPtr == defaultPtr)
    {
        return false;
    }
    if (m_typeHandlers.size() == 0)
    {
        Initialize();
    }
    if (m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(upropPtr->GetClass())) != m_typeHandlers.end())
    {
        for (int i = 0; i < upropPtr->ArrayDim; i++)
        {
            if (!upropPtr->Identical_InContainer(uobjPtr, defaultPtr, i))
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

void sfPropertyManager::SetToDefaultValue(UObject* uobjPtr, UnrealProperty* upropPtr)
{
    if (uobjPtr == nullptr || upropPtr == nullptr)
    {
        return;
    }
    if (m_typeHandlers.size() == 0)
    {
        Initialize();
    }
    if (m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(upropPtr->GetClass())) == m_typeHandlers.end())
    {
        return;
    }
    UObject* defaultObjPtr = GetDefaultObject(uobjPtr);
    for (int i = 0; i < upropPtr->ArrayDim; i++)
    {
        if (!upropPtr->Identical_InContainer(uobjPtr, defaultObjPtr, i))
        {
            upropPtr->CopyCompleteValue_InContainer(uobjPtr, defaultObjPtr);
            MarkPropertyChanged(uobjPtr, upropPtr);
            return;
        }
    }
}

void sfPropertyManager::SetStructFieldToDefaultValue(
    UObject* uobjPtr,
    const sfUPropertyInstance& upropInstance,
    const sfName& name)
{
    if (uobjPtr == nullptr || !upropInstance.IsValid() || upropInstance.DefaultData() == nullptr || !name.IsValid())
    {
        return;
    }
    UnrealProperty* upropPtr = upropInstance.Property();
    void* dataPtr = upropInstance.Data();
    void* defaultPtr = upropInstance.DefaultData();
    if (GetStructField(name, upropPtr, dataPtr, defaultPtr) && upropPtr != nullptr)
    {
        upropPtr->CopyCompleteValue(dataPtr, defaultPtr);
    }
}

UObject* sfPropertyManager::GetDefaultObject(UObject* uobjPtr)
{
    // First try get the default sub object from the object's outer
    UObject* defaultObjPtr = nullptr;
    if (uobjPtr->GetOuter() != nullptr)
    {
        defaultObjPtr = uobjPtr->GetOuter()->GetClass()->GetDefaultObject()->GetDefaultSubobjectByName(
            uobjPtr->GetFName());
    }
    // If that fails, get the class default object
    if (defaultObjPtr == nullptr)
    {
        defaultObjPtr = uobjPtr->GetClass()->GetDefaultObject();
    }
    return defaultObjPtr;
}

void sfPropertyManager::CreateProperties(
    UObject* uobjPtr,
    sfDictionaryProperty::SPtr dictPtr,
    const TSet<FString>* const blacklistPtr)
{
    if (uobjPtr == nullptr || dictPtr == nullptr)
    {
        return;
    }
    for (TFieldIterator<UnrealProperty> iter(uobjPtr->GetClass()); iter; ++iter)
    {
        if (IsSyncable(uobjPtr, *iter) && !IsDefaultValue(uobjPtr, *iter))
        {
            FString propertyName = iter->GetName();
            if (blacklistPtr != nullptr && blacklistPtr->Contains(propertyName))
            {
                continue;
            }

            sfProperty::SPtr propPtr = GetValue(uobjPtr, *iter);
            if (propPtr != nullptr)
            {
                std::string name = std::string(TCHAR_TO_UTF8(*propertyName));
                dictPtr->Set(name, propPtr);
            }
        }
    }
}

void sfPropertyManager::ApplyProperties(
    UObject* uobjPtr,
    sfDictionaryProperty::SPtr dictPtr,
    const TSet<FString>* const blacklistPtr)
{
    if (uobjPtr == nullptr || dictPtr == nullptr)
    {
        return;
    }
    UObject* defaultObjPtr = nullptr;
    for (TFieldIterator<UnrealProperty> iter(uobjPtr->GetClass()); iter; ++iter)
    {
        UnrealProperty* upropPtr = *iter;
        if (IsSyncable(uobjPtr, upropPtr))
        {
            FString propertyName = iter->GetName();
            if (blacklistPtr != nullptr && blacklistPtr->Contains(propertyName))
            {
                continue;
            }

            std::string name = std::string(TCHAR_TO_UTF8(*propertyName));
            sfProperty::SPtr propPtr;
            if (!dictPtr->TryGet(name, propPtr))
            {
                SetToDefaultValue(uobjPtr, upropPtr);
            }
            else
            {
                TArray<UObject*> instances;
                sfPropertyManager::GetInstancesWithDefaultValue(uobjPtr, upropPtr, instances);

                void* defaultPtr = nullptr;
                if (upropPtr->GetClass() == UnrealStructProperty::StaticClass())
                {
                    // Get the default struct value so we can check if subproperties have their default value
                    if (defaultObjPtr == nullptr)
                    {
                        defaultObjPtr = GetDefaultObject(uobjPtr);
                    }
                    if (defaultObjPtr != nullptr)
                    {
                        defaultPtr = iter->ContainerPtrToValuePtr<void>(defaultObjPtr);
                    }
                }

                if (SetValue(uobjPtr,
                    sfUPropertyInstance(upropPtr, iter->ContainerPtrToValuePtr<void>(uobjPtr), defaultPtr), propPtr))
                {
                    for (UObject* instance : instances)
                    {
                        upropPtr->CopyCompleteValue_InContainer(instance, uobjPtr);
                        sfPropertyManager::MarkPropertyChanged(instance, upropPtr);
                    }
                }
            }
        }
    }
}

void sfPropertyManager::SendPropertyChanges(
    UObject* uobjPtr,
    sfDictionaryProperty::SPtr dictPtr,
    const TSet<FString>* const blacklistPtr)
{
    if (uobjPtr == nullptr || dictPtr == nullptr)
    {
        return;
    }
    for (TFieldIterator<UnrealProperty> iter(uobjPtr->GetClass()); iter; ++iter)
    {
        if (IsSyncable(uobjPtr, *iter))
        {
            FString propertyName = iter->GetName();
            if (blacklistPtr != nullptr && blacklistPtr->Contains(propertyName))
            {
                continue;
            }

            if (IsDefaultValue(uobjPtr, *iter))
            {
                std::string name = std::string(TCHAR_TO_UTF8(*propertyName));
                dictPtr->Remove(name);
            }
            else
            {
                sfProperty::SPtr propPtr = GetValue(uobjPtr, *iter);
                if (propPtr == nullptr)
                {
                    continue;
                }

                std::string name = std::string(TCHAR_TO_UTF8(*propertyName));
                sfProperty::SPtr oldPropPtr = nullptr;
                if (!dictPtr->TryGet(name, oldPropPtr) || !Copy(oldPropPtr, propPtr))
                {
                    dictPtr->Set(name, propPtr);
                }
            }
        }
    }
}

void sfPropertyManager::SetReferences(UObject* uobjPtr, const std::vector<sfReferenceProperty::SPtr>& references)
{
    for (const sfReferenceProperty::SPtr& referencePtr : references)
    {
        bool handled = false;
        sfProperty::SPtr currentPtr = referencePtr;
        while (currentPtr != nullptr)
        {
            if (currentPtr->Key().IsValid() && currentPtr->Key()->size() > 0 && currentPtr->Key()->at(0) == '#')
            {
                // If the property starts with '#' it is one of or custom properties. Call the object event dispatcher
                // to run the appropriate property change handler.
                SceneFusion::ObjectEventDispatcher->OnPropertyChange(referencePtr);
                handled = true;
                break;
            }
            currentPtr = currentPtr->GetParentProperty();
        }
        if (handled)
        {
            continue;
        }
        
        UObject* referencingObjectPtr = sfObjectMap::GetUObject(referencePtr->GetContainerObject());
        if (referencingObjectPtr == nullptr)
        {
            continue;
        }
        sfUPropertyInstance upropInstance = sfPropertyManager::FindUProperty(referencingObjectPtr, referencePtr);
        if (upropInstance.IsValid())
        {
            if (SetReference(upropInstance, uobjPtr))
            {
                MarkHashStale(upropInstance);
                MarkPropertyChanged(referencingObjectPtr, upropInstance.Property(), referencePtr);
            }
        }
    }
}

bool sfPropertyManager::Copy(sfProperty::SPtr destPtr, sfProperty::SPtr srcPtr)
{
    if (destPtr == nullptr || srcPtr == nullptr || destPtr->Type() != srcPtr->Type())
    {
        return false;
    }
    switch (destPtr->Type())
    {
        case sfProperty::VALUE:
        {
            if (!destPtr->Equals(srcPtr))
            {
                destPtr->AsValue()->SetValue(srcPtr->AsValue()->GetValue());
            }
            break;
        }
        case sfProperty::REFERENCE:
        {
            if (!destPtr->Equals(srcPtr))
            {
                destPtr->AsReference()->SetObjectId(srcPtr->AsReference()->GetObjectId());
            }
            break;
        }
        case sfProperty::LIST:
        {
            CopyList(destPtr->AsList(), srcPtr->AsList());
            break;
        }
        case sfProperty::DICTIONARY:
        {
            CopyDict(destPtr->AsDict(), srcPtr->AsDict());
            break;
        }
    }
    return true;
}

void sfPropertyManager::MarkHashStale(const sfUPropertyInstance& upropInstance)
{
    if (upropInstance.ContainerMap().IsValid())
    {
        m_staleMaps.Add(sfUnrealUtils::GetMap(upropInstance.ContainerMap()), upropInstance.ContainerMap());
    }
    if (upropInstance.ContainerSet().IsValid())
    {
        m_staleSets.Add(upropInstance.ContainerSet()->Set, upropInstance.ContainerSet());
    }
}

void sfPropertyManager::RehashProperties()
{
    for (TPair<void*, TSharedPtr<FScriptMapHelper>>& pair : m_staleMaps)
    {
        pair.Value->Rehash();
    }
    m_staleMaps.Empty();
    for (TPair<FScriptSet*, TSharedPtr<FScriptSetHelper>>& pair : m_staleSets)
    {
        pair.Value->Rehash();
    }
    m_staleSets.Empty();
}

void sfPropertyManager::MarkPropertyChanged(UObject* uobjPtr, UnrealProperty* upropPtr, sfProperty::SPtr propPtr)
{
    if (uobjPtr == nullptr)
    {
        return;
    }
    if (propPtr != nullptr)
    {
        // Get the root uproperty by the name of the sfproperty at depth 1
        int depth = propPtr->GetDepth();
        if (depth > 1)
        {
            do
            {
                propPtr = propPtr->GetParentProperty();
                depth--;
            } while (depth > 1);
            upropPtr = uobjPtr->GetClass()->FindPropertyByName(FName(UTF8_TO_TCHAR(propPtr->Key()->c_str())));
        }
    }
    if (upropPtr == nullptr)
    {
        return;
    }
    SceneFusion::ObjectEventDispatcher->PostPropertyChange(uobjPtr, upropPtr);
    // Workaround for a bug where moving a level will move all the level actors twice.
    // This bug was happening because actor transform changes got applied first, and then when we move the level,
    // Unreal will move the level actors again with an offset. This bug will be fixed if make actor transform relative
    // to the level.
    if (uobjPtr->GetClass()->GetFName() == "WorldTileDetails")
    {
        DisablePropertyChangeHandler();
        FPropertyChangedEvent propertyEvent(upropPtr);
        uobjPtr->PostEditChangeProperty(propertyEvent);
        EnablePropertyChangeHandler();
        return;
    }
    bool alreadyInSet = false;
    m_serverChangedProperties.FindOrAdd(uobjPtr).Add(upropPtr, &alreadyInSet);
    if (alreadyInSet)
    {
        return;
    }
    AActor* actorPtr = Cast<AActor>(uobjPtr->GetOuter());
    if (actorPtr != nullptr)
    {
        m_serverChangedProperties.FindOrAdd(actorPtr).Add(nullptr);
    }
}

void sfPropertyManager::BroadcastChangeEvents()
{
    if (m_serverChangedProperties.Num() <= 0)
    {
        return;
    }
    DisablePropertyChangeHandler();
    SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
    for (TPair<UObject*, TSet<UnrealProperty*>>& pair : m_serverChangedProperties)
    {
        // Check if the uobject is in the sfobjectmap to ensure it is a valid pointer
        if (!sfObjectMap::Contains(pair.Key))
        {
            continue;
        }
        for (UnrealProperty* propPtr : pair.Value)
        {
            FPropertyChangedEvent propertyEvent(propPtr);
            AActor* actorPtr = Cast<AActor>(pair.Key);
            bool oldActorSeamlessTraveled = false;
            if (actorPtr != nullptr)
            {
                oldActorSeamlessTraveled = actorPtr->bActorSeamlessTraveled;

                // Calling PostEditChangeProperty triggers blueprint actors to be reconstructed and will trigger an
                // assertion because the cached transform is different from the real one.
                // Setting bActorSeamlessTraveled to true will prevent this from happening.
                actorPtr->bActorSeamlessTraveled = true;
            }
            pair.Key->PostEditChangeProperty(propertyEvent);
            if (actorPtr != nullptr)
            {
                actorPtr->bActorSeamlessTraveled = oldActorSeamlessTraveled;
            }
        }
    }
    m_serverChangedProperties.Empty();
    SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
    EnablePropertyChangeHandler();
}

void sfPropertyManager::ReplaceUObject(UObject* oldPtr, UObject* newPtr)
{
    TSet<UnrealProperty*>* propertiesPtr = m_localChangedProperties.Find(oldPtr);
    if (propertiesPtr != nullptr)
    {
        m_localChangedProperties.Emplace(newPtr, *propertiesPtr);
        m_localChangedProperties.Remove(oldPtr);
    }

    propertiesPtr = m_serverChangedProperties.Find(oldPtr);
    if (propertiesPtr != nullptr)
    {
        m_serverChangedProperties.Emplace(newPtr, *propertiesPtr);
        m_serverChangedProperties.Remove(oldPtr);
    }
}

void sfPropertyManager::AddPropertyToForceSyncList(FName ownerClassName, FName propertyName)
{
    // Register extra properties to sync
    m_forceSyncList.Add(TPair<FName, FName>(ownerClassName, propertyName));
}

void sfPropertyManager::RemovePropertyFromForceSyncList(FName ownerClassName, FName propertyName)
{
    m_forceSyncList.Remove(TPair<FName, FName>(ownerClassName, propertyName));
}

void sfPropertyManager::AddToBlacklist(FName className, FName propertyName)
{
    m_blacklist.Add(TPair<FName, FName>(className, propertyName));
}

void sfPropertyManager::RemoveFromBlacklist(FName className, FName propertyName)
{
    m_blacklist.Remove(TPair<FName, FName>(className, propertyName));
}

void sfPropertyManager::IgnoreDisableEditOnInstanceFlagForClass(FName className)
{
    m_syncDefaultOnlyList.Add(className);
}

void sfPropertyManager::EnablePropertyChangeHandler()
{
    if (m_onPropertyChangeHandle.IsValid())
    {
        return;
    }
    m_onPropertyChangeHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda(
        [this](UObject* uobjPtr, FPropertyChangedEvent& ev)
    {
        OnUPropertyChange(uobjPtr, ev);
    });
}

void sfPropertyManager::DisablePropertyChangeHandler()
{
    FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(m_onPropertyChangeHandle);
    m_onPropertyChangeHandle.Reset();
}

bool sfPropertyManager::ListeningForPropertyChanges()
{
    return m_onPropertyChangeHandle.IsValid();
}

void sfPropertyManager::SyncProperties()
{
    m_syncSubObjects = true;
    for (TPair<UObject*, TSet<UnrealProperty*>>& pair : m_localChangedProperties)
    {
        UObject* uobjPtr = pair.Key;
        if (uobjPtr->IsPendingKill())
        {
            continue;
        }
        auto iter = m_classToPropertyChangeHandler.find(sfUnrealUtils::GetClassTypeHash(uobjPtr->GetClass()));
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(uobjPtr);
        for (UnrealProperty* propPtr : pair.Value)
        {
            if (iter != m_classToPropertyChangeHandler.end())
            {
                iter->second(uobjPtr, propPtr);
            }
            else if(objPtr != nullptr)
            {
                SyncProperty(objPtr, uobjPtr, propPtr);
            }
        }
    }
    m_syncSubObjects = false;
    m_syncedSubObjects.clear();
    m_localChangedProperties.Empty();
}

void sfPropertyManager::SyncProperty(sfObject::SPtr objPtr, UObject* uobjPtr, const FName& name, bool applyServerValue)
{
    if (uobjPtr == nullptr)
    {
        return;
    }
    UnrealProperty* upropPtr = uobjPtr->GetClass()->FindPropertyByName(name);
    if (upropPtr == nullptr)
    {
        KS::Log::Warning("Could not find property " + std::string(TCHAR_TO_UTF8(*name.ToString())) + " on " +
            std::string(TCHAR_TO_UTF8(*uobjPtr->GetClass()->GetName())), LOG_CHANNEL);
    }
    else
    {
        SyncProperty(objPtr, uobjPtr, upropPtr, applyServerValue);
    }
}

void sfPropertyManager::SyncProperty(sfObject::SPtr objPtr, UObject* uobjPtr, UnrealProperty* upropPtr, bool applyServerValue)
{
    if (upropPtr == nullptr || objPtr == nullptr ||
        objPtr->Property()->Type() != sfProperty::DICTIONARY ||
        SceneFusion::ObjectEventDispatcher->OnUPropertyChange(objPtr, uobjPtr, upropPtr))
    {
        return;
    }

    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    FString path = upropPtr->GetName();
    std::string name = std::string(TCHAR_TO_UTF8(*path));

    if (objPtr->IsLocked() || applyServerValue)
    {
        sfProperty::SPtr propPtr;
        if (propertiesPtr->TryGet(name, propPtr))
        {
            SetValue(uobjPtr, FindUProperty(uobjPtr, propPtr), propPtr);
        }
        else
        {
            SetToDefaultValue(uobjPtr, upropPtr);
        }
    }
    else if (IsDefaultValue(uobjPtr, upropPtr))
    {
        propertiesPtr->Remove(name);
    }
    else
    {
        sfProperty::SPtr propPtr = GetValue(uobjPtr, upropPtr);
        sfProperty::SPtr oldPropPtr;
        if (propPtr == nullptr)
        {
            FString str = upropPtr->GetClass()->GetName() + " is not supported by Scene Fusion. Changes to " +
                upropPtr->GetName() + " will not sync.";
            KS::Log::Warning(TCHAR_TO_UTF8(*str));
        }
        else if (!propertiesPtr->TryGet(name, oldPropPtr) || !Copy(oldPropPtr, propPtr))
        {
            propertiesPtr->Set(name, propPtr);
        }
    }
}

void sfPropertyManager::CleanUp()
{
    RehashProperties();
    BroadcastChangeEvents();
    m_localChangedProperties.Empty();
}

// private functions

void sfPropertyManager::Initialize()
{
    CreateTypeHandler<UnrealBoolProperty>();
    CreateTypeHandler<UnrealFloatProperty>();
    CreateTypeHandler<UnrealIntProperty>();
    CreateTypeHandler<UnrealUInt32Property>();
    CreateTypeHandler<UnrealByteProperty>();
    CreateTypeHandler<UnrealInt64Property>();

    CreateTypeHandler<UnrealInt8Property, uint8_t>();
    CreateTypeHandler<UnrealInt16Property, int>();
    CreateTypeHandler<UnrealUInt16Property, int>();
    CreateTypeHandler<UnrealUInt64Property, int64_t>();

    CREATE_TYPE_HANDLER(UnrealDoubleProperty, GetDouble, SetDouble);
    CREATE_TYPE_HANDLER(UnrealStrProperty, GetFString, SetFString);
    CREATE_TYPE_HANDLER(UnrealTextProperty, GetFText, SetFText);
    CREATE_TYPE_HANDLER(UnrealNameProperty, GetFName, SetFName);
    CREATE_TYPE_HANDLER(UnrealEnumProperty, GetEnum, SetEnum);
    CREATE_TYPE_HANDLER(UnrealArrayProperty, GetArray, SetArray);
    CREATE_TYPE_HANDLER(UnrealMapProperty, GetMap, SetMap);
    CREATE_TYPE_HANDLER(UnrealSetProperty, GetSet, SetSet);
    CREATE_TYPE_HANDLER(UnrealStructProperty, GetStruct, SetStruct);
    CREATE_TYPE_HANDLER(UnrealObjectProperty, GetObject, SetObject);
    CREATE_TYPE_HANDLER(UnrealSoftObjectProperty, GetSoftObject, SetSoftObject);
    CREATE_TYPE_HANDLER(UnrealLazyObjectProperty, GetLazyObject, SetLazyObject);
    CREATE_TYPE_HANDLER(UnrealWeakObjectProperty, GetWeakObject, SetWeakObject);
    CREATE_TYPE_HANDLER(UnrealInterfaceProperty, GetInterface, SetInterface);
    CREATE_TYPE_HANDLER(UnrealClassProperty, GetClass, SetClass);
    CREATE_TYPE_HANDLER(UnrealSoftClassProperty, GetSoftClass, SetSoftClass);
}

void sfPropertyManager::CreateTypeHandler(UClass* typePtr, TypeHandler::Getter getter, TypeHandler::Setter setter)
{
    auto key = sfUnrealUtils::GetClassTypeHash(typePtr);
    if (m_typeHandlers.find(key) != m_typeHandlers.end())
    {
        KS::Log::Warning("Duplicate handler for type " + std::string(TCHAR_TO_UTF8(*typePtr->GetName())), LOG_CHANNEL);
    }
    m_typeHandlers.emplace(key, TypeHandler(getter, setter));
}

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 25
void sfPropertyManager::CreateTypeHandler(FFieldClass* typePtr, TypeHandler::Getter getter, TypeHandler::Setter setter)
{
    auto key = sfUnrealUtils::GetClassTypeHash(typePtr);
    if (m_typeHandlers.find(key) != m_typeHandlers.end())
    {
        KS::Log::Warning("Duplicate handler for type " + std::string(TCHAR_TO_UTF8(*typePtr->GetName())), LOG_CHANNEL);
    }
    m_typeHandlers.emplace(key, TypeHandler(getter, setter));
}
#endif

bool sfPropertyManager::IsSyncable(UObject* uobjPtr, UnrealProperty* upropPtr)
{
    if (IsPropertyInForceSyncList(upropPtr))
    {
        return true;
    }
    FName className = uobjPtr->GetClass()->GetFName();
    if (m_blacklist.Contains(TPair<FName, FName>(className, upropPtr->GetFName())))
    {
        return false;
    }
    uint64_t flags = upropPtr->PropertyFlags;
    return (flags & CPF_Edit) && !(flags & CPF_EditConst) &&
        (!(flags & CPF_DisableEditOnInstance) || uobjPtr->IsTemplate() || m_syncDefaultOnlyList.Contains(className));
}

bool sfPropertyManager::IsPropertyInForceSyncList(UnrealProperty* upropPtr)
{
    UClass* ownerClassPtr = upropPtr->GetOwnerClass();
    if (ownerClassPtr == nullptr)
    {
        return false;
    }
    return m_forceSyncList.Contains(TPair<FName, FName>(ownerClassPtr->GetFName(), upropPtr->GetFName()));
}

void sfPropertyManager::OnUPropertyChange(UObject* uobjPtr, FPropertyChangedEvent& ev)
{
    // Ignore property change event in the following cases
    // - Changed property is null
    // - Changed object is not an instance of the property's owner class
    // - Changed object is in the transient package and has no handler registered for its class
    //
    // Objects in the transient package are non-saved objects we don't sync unless we registered a special handler
    // for a given type in the m_classToPropertyChangeHandler map. If we don't do this check here we will
    // get invalid pointers when transient objects created while merging levels are garbage collected.
    if (ev.MemberProperty == nullptr || !uobjPtr->IsA(ev.MemberProperty->GetOwnerClass()) ||
        (uobjPtr->GetOutermost() == GetTransientPackage() &&
            m_classToPropertyChangeHandler.find(sfUnrealUtils::GetClassTypeHash(uobjPtr->GetClass())) ==
            m_classToPropertyChangeHandler.end()))
    {
        return;
    }
    if (IsSyncable(uobjPtr, ev.MemberProperty))
    {
        // Sliding values in the details panel can generate nearly 1000 change events per second, so to throttle the
        // update rate we queue the property to be processed at most once per tick.
        m_localChangedProperties.FindOrAdd(uobjPtr).Add(ev.MemberProperty);
    }
}

sfProperty::SPtr sfPropertyManager::GetDouble(const sfUPropertyInstance& upropInstance)
{
    return sfValueProperty::Create(ksMultiType(ksMultiType::BYTE_ARRAY, (uint8_t*)upropInstance.Data(), sizeof(double),
        sizeof(double)));
}

bool sfPropertyManager::SetDouble(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    const ksMultiType& value = propPtr->AsValue()->GetValue();
    if (value.GetData().size() != sizeof(double))
    {
        KS::Log::Error("Error setting double property " + 
            std::string(TCHAR_TO_UTF8(*upropInstance.Property()->GetName())) +
            ". Expected " + std::to_string(sizeof(double)) + " bytes, but got " +
            std::to_string(value.GetData().size()) + ".", LOG_CHANNEL);
        return false;
    }
    if (std::memcmp((uint8_t*)upropInstance.Data(), value.GetData().data(), sizeof(double)) != 0)
    {
        std::memcpy((uint8_t*)upropInstance.Data(), value.GetData().data(), sizeof(double));
        return true;
    }
    return false;
}

sfProperty::SPtr sfPropertyManager::GetFString(const sfUPropertyInstance& upropInstance)
{
    return sfPropertyUtil::FromString(*(FString*)upropInstance.Data());
}

bool sfPropertyManager::SetFString(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    FString* strPtr = (FString*)upropInstance.Data();
    FString newValue = sfPropertyUtil::ToString(propPtr);
    if (!strPtr->Equals(newValue))
    {
        *strPtr = newValue;
        return true;
    }
    return false;
}

sfProperty::SPtr sfPropertyManager::GetFText(const sfUPropertyInstance& upropInstance)
{
    FText* textPtr = (FText*)upropInstance.Data();
    if (textPtr->IsEmpty())
    {
        return sfNullProperty::Create();
    }
    sfSession::SPtr sessionPtr = SceneFusion::Service->Session();
    std::vector<uint32_t> stringIds;
    if (textPtr->IsCultureInvariant())
    {
        stringIds.emplace_back(sessionPtr->GetStringTableId(TCHAR_TO_UTF8(*textPtr->ToString())));
    }
    else if (textPtr->IsFromStringTable())
    {
        FName table;
        FString key;
        if (FTextInspector::GetTableIdAndKey(*textPtr, table, key))
        {
            stringIds.emplace_back(sessionPtr->GetStringTableId(TCHAR_TO_UTF8(*table.ToString())));
            stringIds.emplace_back(sessionPtr->GetStringTableId(TCHAR_TO_UTF8(*key)));
        }
    }
    else
    {
        stringIds.emplace_back(sessionPtr->GetStringTableId(
            TCHAR_TO_UTF8(*FTextInspector::GetNamespace(*textPtr).Get(""))));
        stringIds.emplace_back(sessionPtr->GetStringTableId(TCHAR_TO_UTF8(*FTextInspector::GetKey(*textPtr).Get(""))));
        stringIds.emplace_back(sessionPtr->GetStringTableId(TCHAR_TO_UTF8(*textPtr->ToString())));
    }
    if (stringIds.size() == 0)
    {
        return sfNullProperty::Create();
    }
    return sfValueProperty::Create(stringIds);
}

bool sfPropertyManager::SetFText(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    FText* textPtr = (FText*)upropInstance.Data();
    if (propPtr->Type() == sfProperty::NUL)
    {
        if (!textPtr->IsEmpty())
        {
            *textPtr = FText::FromString("");
            return true;
        }
        return false;
    }
    sfSession::SPtr sessionPtr = SceneFusion::Service->Session();
    std::vector<uint32_t> stringIds;
    propPtr->AsValue()->GetValue().GetUIntArray(stringIds);
    if (stringIds.size() == 1)
    {
        // CultureInvariant = true, stringIds = text
        FString text = UTF8_TO_TCHAR(sessionPtr->GetStringFromTable(stringIds[0])->c_str());
        if (!textPtr->IsCultureInvariant() || !textPtr->ToString().Equals(text))
        {
            *textPtr = FText::AsCultureInvariant(text);
            return true;
        }
        return false;
    }
    if (stringIds.size() == 2)
    {
        // stringIds = table, key
        FName table = FName(UTF8_TO_TCHAR(sessionPtr->GetStringFromTable(stringIds[0])->c_str()));
        FString key = UTF8_TO_TCHAR(sessionPtr->GetStringFromTable(stringIds[1])->c_str());
        if (textPtr->IsFromStringTable())
        {
            FName oldTable;
            FString oldKey;
            if (FTextInspector::GetTableIdAndKey(*textPtr, oldTable, oldKey) && oldTable == table && oldKey == key)
            {
                return false;
            }
        }
        *textPtr = FText::FromStringTable(table, key);
        return true;
    }
    else if (stringIds.size() == 3)
    {
        // stringIds = namespace, key, text
        FString nameSpace = UTF8_TO_TCHAR(sessionPtr->GetStringFromTable(stringIds[0])->c_str());
        FString key = UTF8_TO_TCHAR(sessionPtr->GetStringFromTable(stringIds[1])->c_str());
        FString text = UTF8_TO_TCHAR(sessionPtr->GetStringFromTable(stringIds[2])->c_str());
        bool changed = false;
        if (!textPtr->ToString().Equals(text))
        {
            *textPtr = FText::FromString(text);
            changed = true;
        }
        if (nameSpace != FTextInspector::GetNamespace(*textPtr).Get("") || key != FTextInspector::GetKey(*textPtr).Get(""))
        {
            *textPtr = FText::ChangeKey(nameSpace, key, *textPtr);
            changed = true;
        }
        return changed;
    }
    return false;
}

sfProperty::SPtr sfPropertyManager::GetFName(const sfUPropertyInstance& upropInstance)
{
    return sfPropertyUtil::FromString(((FName*)upropInstance.Data())->ToString());
}

bool sfPropertyManager::SetFName(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    FName* namePtr = (FName*)upropInstance.Data();
    FName newValue = *sfPropertyUtil::ToString(propPtr);
    if (*namePtr != newValue)
    {
        *namePtr = newValue;
        return true;
    }
    return false;
}

sfProperty::SPtr sfPropertyManager::GetEnum(const sfUPropertyInstance& upropInstance)
{
    UnrealEnumProperty* tPtr = CastUnrealProperty<UnrealEnumProperty>(upropInstance.Property());
    int64_t value = tPtr->GetUnderlyingProperty()->GetSignedIntPropertyValue(upropInstance.Data());
    if (value >= 0 && value < 256)
    {
        return sfValueProperty::Create((uint8_t)value);
    }
    return sfValueProperty::Create(value);
}

bool sfPropertyManager::SetEnum(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    UnrealEnumProperty* tPtr = CastUnrealProperty<UnrealEnumProperty>(upropInstance.Property());
    int64_t value = propPtr->AsValue()->GetValue();
    if (tPtr->GetUnderlyingProperty()->GetSignedIntPropertyValue(upropInstance.Data()) != value)
    {
        tPtr->GetUnderlyingProperty()->SetIntPropertyValue(upropInstance.Data(), value);
        return true;
    }
    return false;
}

sfProperty::SPtr sfPropertyManager::GetArray(const sfUPropertyInstance& upropInstance)
{
    UnrealArrayProperty* tPtr = CastUnrealProperty<UnrealArrayProperty>(upropInstance.Property());
    auto iter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(tPtr->Inner->GetClass()));
    if (iter == m_typeHandlers.end())
    {
        return nullptr;
    }
    sfListProperty::SPtr listPtr = sfListProperty::Create();
    FScriptArrayHelper array(tPtr, upropInstance.Data());
    for (int i = 0; i < array.Num(); i++)
    {

        sfProperty::SPtr elementPtr = iter->second.Get(sfUPropertyInstance(tPtr->Inner, (void*)array.GetRawPtr(i)));
        if (elementPtr == nullptr)
        {
            return nullptr;
        }
        listPtr->Add(elementPtr);
    }
    return listPtr;
}

bool sfPropertyManager::SetArray(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    UnrealArrayProperty* tPtr = CastUnrealProperty<UnrealArrayProperty>(upropInstance.Property());
    auto iter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(tPtr->Inner->GetClass()));
    if (iter == m_typeHandlers.end())
    {
        return false;
    }
    bool changed = false;
    sfListProperty::SPtr listPtr = propPtr->AsList();
    FScriptArrayHelper array(tPtr, upropInstance.Data());
    if (array.Num() != listPtr->Size())
    {
        array.Resize(listPtr->Size());
        changed = true;
    }
    for (int i = 0; i < listPtr->Size(); i++)
    {
        if (iter->second.Set(sfUPropertyInstance(tPtr->Inner, (void*)array.GetRawPtr(i)), listPtr->Get(i)))
        {
            changed = true;
        }
    }
    return changed;
}

sfProperty::SPtr sfPropertyManager::GetMap(const sfUPropertyInstance& upropInstance)
{
    UnrealMapProperty* tPtr = CastUnrealProperty<UnrealMapProperty>(upropInstance.Property());
    auto keyIter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(tPtr->KeyProp->GetClass()));
    if (keyIter == m_typeHandlers.end())
    {
        return nullptr;
    }
    auto valueIter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(tPtr->ValueProp->GetClass()));
    if (valueIter == m_typeHandlers.end())
    {
        return nullptr;
    }
    sfListProperty::SPtr listPtr = sfListProperty::Create();
    FScriptMapHelper map(tPtr, upropInstance.Data());
    for (int i = 0; i < map.GetMaxIndex(); i++)
    {
        if (!map.IsValidIndex(i))
        {
            continue;
        }
        sfListProperty::SPtr pairPtr = sfListProperty::Create();
        sfProperty::SPtr keyPtr = keyIter->second.Get(sfUPropertyInstance(tPtr->KeyProp, (void*)map.GetKeyPtr(i)));
        if (keyPtr == nullptr)
        {
            return nullptr;
        }
        sfProperty::SPtr valuePtr = valueIter->second.Get(
            sfUPropertyInstance(tPtr->ValueProp, (void*)map.GetValuePtr(i)));
        if (valuePtr == nullptr)
        {
            return nullptr;
        }
        pairPtr->Add(keyPtr);
        pairPtr->Add(valuePtr);
        listPtr->Add(pairPtr);
    }
    return listPtr;
}

bool sfPropertyManager::SetMap(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    UnrealMapProperty* tPtr = CastUnrealProperty<UnrealMapProperty>(upropInstance.Property());
    auto keyIter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(tPtr->KeyProp->GetClass()));
    if (keyIter == m_typeHandlers.end())
    {
        return false;
    }
    auto valueIter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(tPtr->ValueProp->GetClass()));
    if (valueIter == m_typeHandlers.end())
    {
        return false;
    }
    bool changed = false;
    bool changedKey = false;
    sfListProperty::SPtr listPtr = propPtr->AsList();
    FScriptMapHelper map(tPtr, upropInstance.Data());
    if (map.Num() != listPtr->Size())
    {
        changed = true;
        changedKey = true;
        map.EmptyValues(listPtr->Size());
    }
    for (int i = 0; i < listPtr->Size(); i++)
    {
        if (map.Num() < listPtr->Size())
        {
            map.AddDefaultValue_Invalid_NeedsRehash();
        }
        sfListProperty::SPtr pairPtr = listPtr->Get(i)->AsList();
        if (keyIter->second.Set(sfUPropertyInstance(tPtr->KeyProp, (void*)map.GetKeyPtr(i)), pairPtr->Get(0)))
        {
            changed = true;
            changedKey = true;
        }
        if (valueIter->second.Set(sfUPropertyInstance(tPtr->ValueProp, (void*)map.GetValuePtr(i)), pairPtr->Get(1)))
        {
            changed = true;
        }
    }
    if (changedKey)
    {
        map.Rehash();
    }
    return changed;
}

sfProperty::SPtr sfPropertyManager::GetSet(const sfUPropertyInstance& upropInstance)
{
    UnrealSetProperty* tPtr = CastUnrealProperty<UnrealSetProperty>(upropInstance.Property());
    auto iter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(tPtr->ElementProp->GetClass()));
    if (iter == m_typeHandlers.end())
    {
        return nullptr;
    }
    sfListProperty::SPtr listPtr = sfListProperty::Create();
    FScriptSetHelper set(tPtr, upropInstance.Data());
    for (int i = 0; i < set.GetMaxIndex(); i++)
    {
        if (!set.IsValidIndex(i))
        {
            continue;
        }
        sfProperty::SPtr elementPtr = iter->second.Get(
            sfUPropertyInstance(tPtr->ElementProp, (void*)set.GetElementPtr(i)));
        if (elementPtr == nullptr)
        {
            return nullptr;
        }
        listPtr->Add(elementPtr);
    }
    return listPtr;
}

bool sfPropertyManager::SetSet(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    UnrealSetProperty* tPtr = CastUnrealProperty<UnrealSetProperty>(upropInstance.Property());
    auto iter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(tPtr->ElementProp->GetClass()));
    if (iter == m_typeHandlers.end())
    {
        return false;
    }
    bool changed = false;
    sfListProperty::SPtr listPtr = propPtr->AsList();
    FScriptSetHelper set(tPtr, upropInstance.Data());
    if (set.Num() != listPtr->Size())
    {
        changed = true;
        set.EmptyElements(listPtr->Size());
    }
    for (int i = 0; i < listPtr->Size(); i++)
    {
        if (set.Num() < listPtr->Size())
        {
            set.AddDefaultValue_Invalid_NeedsRehash();
        }
        if (iter->second.Set(sfUPropertyInstance(tPtr->ElementProp, (void*)set.GetElementPtr(i)), listPtr->Get(i)))
        {
            changed = true;
        }
    }
    if (changed)
    {
        set.Rehash();
    }
    return changed;
}

sfProperty::SPtr sfPropertyManager::GetStruct(const sfUPropertyInstance& upropInstance)
{
    UnrealStructProperty* tPtr = CastUnrealProperty<UnrealStructProperty>(upropInstance.Property());
    sfDictionaryProperty::SPtr dictPtr = sfDictionaryProperty::Create();
    UnrealProperty* subPropPtr = tPtr->Struct->PropertyLink;
    while (subPropPtr)
    {
        auto iter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(subPropPtr->GetClass()));
        if (iter != m_typeHandlers.end())
        {
            sfProperty::SPtr valuePtr = nullptr;
            // ArrayDim is the size of the fixed array. It is always 1 for non-fixed arrays.
            if (subPropPtr->ArrayDim == 1)
            {
                if (upropInstance.DefaultData() == nullptr ||
                    !subPropPtr->Identical_InContainer(upropInstance.Data(), upropInstance.DefaultData()))
                {
                    valuePtr = iter->second.Get(
                        sfUPropertyInstance(subPropPtr, subPropPtr->ContainerPtrToValuePtr<void>(upropInstance.Data()),
                            upropInstance.DefaultData() == nullptr ?
                            nullptr : subPropPtr->ContainerPtrToValuePtr<void>(upropInstance.DefaultData())));
                }
            }
            else
            {
                sfListProperty::SPtr listPtr = sfListProperty::Create();
                bool isDefault = true;
                for (int i = 0; i < subPropPtr->ArrayDim; i++)
                {
                    if (upropInstance.DefaultData() == nullptr ||
                        !subPropPtr->Identical_InContainer(upropInstance.Data(), upropInstance.DefaultData(), i))
                    {
                        isDefault = false;
                    }
                    listPtr->Add(iter->second.Get(sfUPropertyInstance(
                        subPropPtr, subPropPtr->ContainerPtrToValuePtr<void>(upropInstance.Data(), i),
                        upropInstance.DefaultData() == nullptr ?
                        nullptr : subPropPtr->ContainerPtrToValuePtr<void>(upropInstance.DefaultData(), i))));
                }
                if (!isDefault)
                {
                    valuePtr = listPtr;
                }
            }
            if (valuePtr != nullptr)
            {
                std::string name = TCHAR_TO_UTF8(*subPropPtr->GetName());
                dictPtr->Set(name, valuePtr);
            }
        }
        subPropPtr = subPropPtr->PropertyLinkNext;
    }
    return dictPtr;
}

bool sfPropertyManager::SetStruct(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    UnrealStructProperty* tPtr = CastUnrealProperty<UnrealStructProperty>(upropInstance.Property());
    sfDictionaryProperty::SPtr dictPtr = propPtr->AsDict();
    UnrealProperty* subPropPtr = tPtr->Struct->PropertyLink;
    bool changed = false;
    while (subPropPtr)
    {
        auto iter = m_typeHandlers.find(sfUnrealUtils::GetClassTypeHash(subPropPtr->GetClass()));
        if (iter != m_typeHandlers.end())
        {
            std::string name = TCHAR_TO_UTF8(*subPropPtr->GetName());
            sfProperty::SPtr valuePtr;
            if (dictPtr->TryGet(name, valuePtr))
            {
                // ArrayDim is the size of the fixed array. It is always 1 for non-fixed arrays.
                if (subPropPtr->ArrayDim == 1)
                {
                    if (iter->second.Set(sfUPropertyInstance(subPropPtr,
                        subPropPtr->ContainerPtrToValuePtr<void>(upropInstance.Data())), valuePtr))
                    {
                        changed = true;
                    }
                }
                else
                {
                    sfListProperty::SPtr listPtr = valuePtr->AsList();
                    int num = FMath::Min(listPtr->Size(), subPropPtr->ArrayDim);
                    for (int i = 0; i < num; i++)
                    {
                        if (iter->second.Set(sfUPropertyInstance(subPropPtr,
                            subPropPtr->ContainerPtrToValuePtr<void>(upropInstance.Data(), i)), listPtr->Get(i)))
                        {
                            changed = true;
                        }
                    }
                }
            }
            else if (upropInstance.DefaultData() != nullptr)
            {
                // Set to default value
                subPropPtr->CopyCompleteValue_InContainer(upropInstance.Data(), upropInstance.DefaultData());
            }
        }
        subPropPtr = subPropPtr->PropertyLinkNext;
    }
    return changed;
}

sfProperty::SPtr sfPropertyManager::GetObject(const sfUPropertyInstance& upropInstance)
{
    UnrealObjectProperty* tPtr = CastUnrealProperty<UnrealObjectProperty>(upropInstance.Property());
    UObject* referencePtr = tPtr->GetObjectPropertyValue(upropInstance.Data());
    if (referencePtr == nullptr)
    {
        return sfNullProperty::Create();
    }
    if (referencePtr->IsPendingKill())
    {
        // The object is deleted. Clear the reference.
        tPtr->SetObjectPropertyValue(upropInstance.Data(), nullptr);
        return sfNullProperty::Create();
    }
    return CreatePropertyForObjectReference(upropInstance, referencePtr);
}

bool sfPropertyManager::SetObject(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    UnrealObjectProperty* tPtr = CastUnrealProperty<UnrealObjectProperty>(upropInstance.Property());
    UObject* oldPtr = tPtr->GetObjectPropertyValue(upropInstance.Data());
    if (propPtr->Type() == sfProperty::NUL)
    {
        if (oldPtr == nullptr)
        {
            return false;
        }
        tPtr->SetObjectPropertyValue(upropInstance.Data(), nullptr);
        return true;
    }
    if (propPtr->Type() == sfProperty::REFERENCE)
    {
        // The object is in the level.
        uint32_t objId = propPtr->AsReference()->GetObjectId();
        sfObject::SPtr objPtr = SceneFusion::Service->Session()->GetObject(objId);
        UObject* referencePtr = sfObjectMap::GetUObject(objPtr);
        if (referencePtr != nullptr && referencePtr != oldPtr)
        {
            tPtr->SetObjectPropertyValue(upropInstance.Data(), referencePtr);
            return true;
        }
        else if (m_syncSubObjects && objPtr != nullptr && objPtr->Type() == sfType::UObject &&
            m_syncedSubObjects.insert(objPtr).second)
        {
            // Unreal doesn't fire an event for the change on the sub object, so we have to check for changes
            if (objPtr->IsLocked())
            {
                ApplyProperties(referencePtr, objPtr->Property()->AsDict());
            }
            else
            {
                SendPropertyChanges(referencePtr, objPtr->Property()->AsDict());
            }
        }
        return false;
    }
    // The object is an asset
    FString str = sfPropertyUtil::ToString(propPtr);
    // If str is empty we keep our current value
    if (str.IsEmpty())
    {
        return false;
    }

    FString path, className;
    if (!str.Split(";", &className, &path))
    {
        KS::Log::Warning("Invalid asset string: " + std::string(TCHAR_TO_UTF8(*str)), LOG_CHANNEL);
        return false;
    }

    UObject* assetPtr = sfLoader::Get().LoadFromCache(path);
    if (assetPtr == nullptr || !assetPtr->IsA(tPtr->PropertyClass))
    {
        if (sfLoader::Get().IsUserIdle())
        {
            assetPtr = sfLoader::Get().Load(path, className);
        }
        else
        {
            sfLoader::Get().LoadWhenIdle(propPtr);
        }
    }
    if (assetPtr != nullptr && assetPtr != oldPtr)
    {
        tPtr->SetObjectPropertyValue(upropInstance.Data(), assetPtr);
        return true;
    }
    return false;
}

sfProperty::SPtr sfPropertyManager::GetSoftObject(const sfUPropertyInstance& upropInstance)
{
    FSoftObjectPtr& softObjectPtr = *(FSoftObjectPtr*)upropInstance.Data();
    if (softObjectPtr.IsNull())
    {
        return sfNullProperty::Create();
    }
    UObject* referencePtr = softObjectPtr.Get();
    if (referencePtr != nullptr && !referencePtr->IsPendingKill())
    {
        return CreatePropertyForObjectReference(upropInstance, referencePtr);
    }
    // The object isn't loaded. Get the class name from the asset registry.
    FAssetData asset = UAssetManager::Get().GetAssetRegistry().GetAssetByObjectPath(*softObjectPtr.ToString());
    if (!asset.IsValid())
    {
        // The object is in an unloaded level.
        return sfPropertyUtil::FromString(";" + softObjectPtr.ToString());
    }
    UClass* classPtr = asset.GetClass();
    if (classPtr == nullptr)
    {
        // The class isn't loaded. We have to load the object to get the class.
        GIsSlowTask = true;
        GIsEditorLoadingPackage = true;
        UObject* assetPtr = LoadObject<UObject>(nullptr, *asset.ObjectPath.ToString());
        GIsEditorLoadingPackage = false;
        GIsSlowTask = false;
        if (assetPtr == nullptr)
        {
            KS::Log::Warning("Unable to load soft asset " + std::string(TCHAR_TO_UTF8(*softObjectPtr.ToString())),
                LOG_CHANNEL);
            return sfPropertyUtil::FromString(";" + asset.ObjectPath.ToString());
        }
        classPtr = assetPtr->GetClass();
    }
    return sfPropertyUtil::FromString(sfUnrealUtils::ClassToFString(classPtr) + ";" + asset.ObjectPath.ToString());
}

bool sfPropertyManager::SetSoftObject(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    FSoftObjectPtr& softObjectPtr = *(FSoftObjectPtr*)upropInstance.Data();
    if (propPtr->Type() == sfProperty::NUL)
    {
        if (softObjectPtr.IsNull())
        {
            return false;
        }
        softObjectPtr = nullptr;
        return true;
    }
    if (propPtr->Type() == sfProperty::REFERENCE)
    {
        // The object is in the level.
        UObject* referencePtr = GetReference(propPtr->AsReference());
        if (referencePtr != nullptr && referencePtr != softObjectPtr.Get())
        {
            softObjectPtr = FSoftObjectPath(referencePtr);
            return true;
        }
        return false;
    }
    FString str = sfPropertyUtil::ToString(propPtr);
    // If str is empty we keep our current value
    if (str.IsEmpty())
    {
        return false;
    }
    // The object is an asset or is in another level.
    FString path, className;
    if (!str.Split(";", &className, &path))
    {
        KS::Log::Warning("Invalid soft object string: " + std::string(TCHAR_TO_UTF8(*str)), LOG_CHANNEL);
        return false;
    }
    if (softObjectPtr.ToString() == path)
    {
        return false;
    }
    if (className != "" && !UAssetManager::Get().GetAssetRegistry().GetAssetByObjectPath(*path).IsValid())
    {
        // The asset is missing. Loading it will create a stand-in.
        UObject* standInPtr = sfLoader::Get().Load(path, className);
        if (standInPtr != softObjectPtr.Get())
        {
            softObjectPtr = FSoftObjectPath(standInPtr);
            return true;
        }
        return false;
    }
    softObjectPtr = FSoftObjectPath(path);
    return true;
}

sfProperty::SPtr sfPropertyManager::GetLazyObject(const sfUPropertyInstance& upropInstance)
{
    FLazyObjectPtr& lazyObjectPtr = *(FLazyObjectPtr*)upropInstance.Data();
    if (lazyObjectPtr.IsNull())
    {
        return sfNullProperty::Create();
    }
    UObject* referencePtr = lazyObjectPtr.Get();
    if (referencePtr != nullptr && !referencePtr->IsPendingKill())
    {
        return CreatePropertyForObjectReference(upropInstance, referencePtr);
    }
    // The object isn't loaded. Send the object guid.
    const FGuid& guid = lazyObjectPtr.GetUniqueID().GetGuid();
    std::vector<uint32_t> nums = { guid.A, guid.B, guid.C, guid.D };
    return sfValueProperty::Create(nums);
}

bool sfPropertyManager::SetLazyObject(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    FLazyObjectPtr& lazyObjectPtr = *(FLazyObjectPtr*)upropInstance.Data();
    if (propPtr->Type() == sfProperty::NUL)
    {
        if (lazyObjectPtr.IsNull())
        {
            return false;
        }
        lazyObjectPtr = nullptr;
        return true;
    }

    if (propPtr->Type() == sfProperty::REFERENCE)
    {
        // The object is in the level.
        UObject* referencePtr = GetReference(propPtr->AsReference());
        if (referencePtr != nullptr && referencePtr != lazyObjectPtr.Get())
        {
            lazyObjectPtr = referencePtr;
            return true;
        }
        return false;
    }

    sfValueProperty::SPtr valuePtr = propPtr->AsValue();
    if (valuePtr->GetValue().GetType() == ksMultiType::UINT_ARRAY)
    {
        // Set the object by its guid.
        std::vector<uint32_t> nums;
        valuePtr->GetValue().GetUIntArray(nums);
        FGuid guid{ nums[0], nums[1], nums[2], nums[3] };
        if (lazyObjectPtr.GetUniqueID().GetGuid() == guid)
        {
            return false;
        }
        lazyObjectPtr = FUniqueObjectGuid{ FGuid{ nums[0], nums[1], nums[2], nums[3] } };
        return true;
    }

    // The object is an asset
    FString str = sfPropertyUtil::ToString(propPtr);
    // If str is empty we keep our current value
    if (str.IsEmpty())
    {
        return false;
    }
    FString path, className;
    if (!str.Split(";", &className, &path))
    {
        KS::Log::Warning("Invalid lazy object string: " + std::string(TCHAR_TO_UTF8(*str)), LOG_CHANNEL);
        return false;
    }
    UObject* assetPtr = sfLoader::Get().LoadFromCache(path);
    if (assetPtr == nullptr)
    {
        if (sfLoader::Get().IsUserIdle())
        {
            assetPtr = sfLoader::Get().Load(path, className);
        }
        else
        {
            sfLoader::Get().LoadWhenIdle(propPtr);
        }
    }
    if (assetPtr != nullptr && assetPtr != lazyObjectPtr.Get())
    {
        lazyObjectPtr = assetPtr;
        return true;
    }
    return false;
}

sfProperty::SPtr sfPropertyManager::GetWeakObject(const sfUPropertyInstance& upropInstance)
{
    FWeakObjectPtr& weakObjPtr = *(FWeakObjectPtr*)upropInstance.Data();
    if (!weakObjPtr.IsValid())
    {
        return sfNullProperty::Create();
    }
    UObject* referencePtr = weakObjPtr.Get();
    if (referencePtr != nullptr && !referencePtr->IsPendingKill())
    {
        return CreatePropertyForObjectReference(upropInstance, referencePtr);
    }
    return sfNullProperty::Create();
}

bool sfPropertyManager::SetWeakObject(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    UnrealWeakObjectProperty* tPtr = CastUnrealProperty<UnrealWeakObjectProperty>(upropInstance.Property());
    FWeakObjectPtr& weakObjPtr = *(FWeakObjectPtr*)upropInstance.Data();
    if (propPtr->Type() == sfProperty::NUL)
    {
        if (!weakObjPtr.IsValid())
        {
            return false;
        }
        weakObjPtr.Reset();
        return true;
    }

    if (propPtr->Type() == sfProperty::REFERENCE)
    {
        // The object is in the level.
        UObject* referencePtr = GetReference(propPtr->AsReference());
        if (referencePtr != nullptr && referencePtr != weakObjPtr.Get())
        {
            weakObjPtr = referencePtr;
            return true;
        }
        return false;
    }

    // The object is an asset
    FString str = sfPropertyUtil::ToString(propPtr);
    // If str is empty we keep our current value
    if (str.IsEmpty())
    {
        return false;
    }

    FString path, className;
    if (!str.Split(";", &className, &path))
    {
        KS::Log::Warning("Invalid asset string: " + std::string(TCHAR_TO_UTF8(*str)), LOG_CHANNEL);
        return false;
    }

    UObject* assetPtr = sfLoader::Get().LoadFromCache(path);
    if (assetPtr == nullptr || !assetPtr->IsA(tPtr->PropertyClass))
    {
        if (sfLoader::Get().IsUserIdle())
        {
            assetPtr = sfLoader::Get().Load(path, className);
        }
        else
        {
            sfLoader::Get().LoadWhenIdle(propPtr);
        }
    }
    if (assetPtr != nullptr && assetPtr != weakObjPtr.Get())
    {
        weakObjPtr = assetPtr;
        return true;
    }
    return false;
}

sfProperty::SPtr sfPropertyManager::GetInterface(const sfUPropertyInstance& upropInstance)
{
    UnrealInterfaceProperty* tPtr = CastUnrealProperty<UnrealInterfaceProperty>(upropInstance.Property());
    FScriptInterface inter = tPtr->GetPropertyValue(upropInstance.Data());
    UObject* referencePtr = inter.GetObject();
    if (referencePtr == nullptr)
    {
        return sfNullProperty::Create();
    }
    if (referencePtr->IsPendingKill())
    {
        // The object is deleted. Clear the reference.
        inter.SetInterface(nullptr);
        inter.SetObject(nullptr);
        tPtr->SetPropertyValue(upropInstance.Data(), inter);
        return sfNullProperty::Create();
    }
    return CreatePropertyForObjectReference(upropInstance, referencePtr);
}

bool sfPropertyManager::SetInterface(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    UnrealInterfaceProperty* tPtr = CastUnrealProperty<UnrealInterfaceProperty>(upropInstance.Property());
    FScriptInterface inter = tPtr->GetPropertyValue(upropInstance.Data());
    UObject* oldPtr = inter.GetObject();
    if (propPtr->Type() == sfProperty::NUL)
    {
        if (oldPtr == nullptr)
        {
            return false;
        }
        inter.SetInterface(nullptr);
        inter.SetObject(nullptr);
        tPtr->SetPropertyValue(upropInstance.Data(), inter);
        return true;
    }
    if (propPtr->Type() == sfProperty::REFERENCE)
    {
        // The object is in the level.
        UObject* referencePtr = GetReference(propPtr->AsReference());
        if (referencePtr != nullptr && referencePtr != oldPtr)
        {
            inter.SetInterface(referencePtr);
            inter.SetObject(referencePtr);
            tPtr->SetPropertyValue(upropInstance.Data(), inter);
            return true;
        }
        return false;
    }
    // The object is an asset
    FString str = sfPropertyUtil::ToString(propPtr);
    // If str is empty we keep our current value
    if (str.IsEmpty())
    {
        return false;
    }

    FString path, className;
    if (!str.Split(";", &className, &path))
    {
        KS::Log::Warning("Invalid asset string: " + std::string(TCHAR_TO_UTF8(*str)), LOG_CHANNEL);
        return false;
    }

    UObject* assetPtr = sfLoader::Get().LoadFromCache(path);
    if (assetPtr == nullptr)
    {
        if (sfLoader::Get().IsUserIdle())
        {
            assetPtr = sfLoader::Get().Load(path, className);
        }
        else
        {
            sfLoader::Get().LoadWhenIdle(propPtr);
        }
    }
    if (assetPtr != nullptr && assetPtr != oldPtr)
    {
        inter.SetInterface(assetPtr);
        inter.SetObject(assetPtr);
        tPtr->SetPropertyValue(upropInstance.Data(), inter);
        return true;
    }
    return false;
}

sfProperty::SPtr sfPropertyManager::CreatePropertyForObjectReference(
    const sfUPropertyInstance& upropInstance,
    UObject* referencePtr)
{
    bool isTransient = referencePtr->HasAllFlags(RF_Transient);
    if (!isTransient && referencePtr->GetTypedOuter<ULevel>() != nullptr)
    {
        // The object is in the level. Sync the object if it's not already synced.
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(referencePtr);
        if (objPtr == nullptr)
        {
            objPtr = SceneFusion::ObjectEventDispatcher->Create(referencePtr);
            if (objPtr == nullptr)
            {
                FString str = "Unable to sync reference to " + referencePtr->GetClass()->GetName() + " " +
                    referencePtr->GetName();
                KS::Log::Info(TCHAR_TO_UTF8(*str), LOG_CHANNEL);
                // Empty string means keep your current value
                return sfValueProperty::Create("");
            }
        }
        else if (m_syncSubObjects && objPtr->Type() == sfType::UObject && m_syncedSubObjects.insert(objPtr).second)
        {
            // Unreal doesn't fire an event for the change on the sub object, so we have to check for changes
            if (objPtr->IsLocked())
            {
                ApplyProperties(referencePtr, objPtr->Property()->AsDict());
            }
            else
            {
                SendPropertyChanges(referencePtr, objPtr->Property()->AsDict());
            }
        }
        return sfReferenceProperty::Create(objPtr->Id());
    }

    // The object is an asset
    FString str;
    if (isTransient)
    {
        // This is a stand-in for a missing asset.
        str = sfLoader::Get().GetPathFromStandIn(referencePtr);
        // Try to load the asset from memory
        FString path, className;
        if (str.Split(";", &className, &path))
        {
            if (upropInstance.Property() != nullptr)
            {
                UObject* assetPtr = sfLoader::Get().LoadFromCache(path);
                if (assetPtr != nullptr)
                {
                    // Replace the stand-in with the correct asset
                    SetReference(upropInstance, assetPtr);
                }
            }
        }
        else
        {
            KS::Log::Warning("Reference to transient object " + std::string(TCHAR_TO_UTF8(*referencePtr->GetName())) +
                " will not sync.", LOG_CHANNEL);
        }
    }
    else
    {
        str = sfUnrealUtils::ClassToFString(referencePtr->GetClass()) + ";" + referencePtr->GetPathName();
        m_onGetAssetProperty.Broadcast(referencePtr);

        // Sync the asset if it can be synced and isn't already synced.
        if (sfLoader::Get().IsCreatableAssetType(referencePtr->GetClass()) && !sfObjectMap::Contains(referencePtr))
        {
            SceneFusion::ObjectEventDispatcher->Create(referencePtr);
        }
    }
    return sfPropertyUtil::FromString(str);
}

sfProperty::SPtr sfPropertyManager::FromUObject(UObject* uobjPtr)
{
    sfUPropertyInstance nullPropInstance;
    return CreatePropertyForObjectReference(nullPropInstance, uobjPtr);
}

UObject* sfPropertyManager::GetReference(sfReferenceProperty::SPtr propPtr)
{
    uint32_t objId = propPtr->GetObjectId();
    sfObject::SPtr objPtr = SceneFusion::Service->Session()->GetObject(objId);
    return sfObjectMap::GetUObject(objPtr);
}

bool sfPropertyManager::SetReference(const sfUPropertyInstance& upropInstance, UObject* referencePtr)
{
    if (UnrealObjectProperty* tPtr = CastUnrealProperty<UnrealObjectProperty>(upropInstance.Property()))
    {
        if (referencePtr == tPtr->GetObjectPropertyValue(upropInstance.Data()))
        {
            return false;
        }
        tPtr->SetObjectPropertyValue(upropInstance.Data(), referencePtr);
        return true;
    }
    if (UnrealSoftObjectProperty* tPtr = CastUnrealProperty<UnrealSoftObjectProperty>(upropInstance.Property()))
    {
        if (referencePtr == tPtr->GetObjectPropertyValue(upropInstance.Data()))
        {
            return false;
        }
        tPtr->SetObjectPropertyValue(upropInstance.Data(), referencePtr);
        return true;
    }
    if (UnrealLazyObjectProperty* tPtr = CastUnrealProperty<UnrealLazyObjectProperty>(upropInstance.Property()))
    {
        if (referencePtr == tPtr->GetObjectPropertyValue(upropInstance.Data()))
        {
            return false;
        }
        tPtr->SetObjectPropertyValue(upropInstance.Data(), referencePtr);
        return true;
    }
    if (UnrealWeakObjectProperty* tPtr = CastUnrealProperty<UnrealWeakObjectProperty>(upropInstance.Property()))
    {
        if (referencePtr == tPtr->GetObjectPropertyValue(upropInstance.Data()))
        {
            return false;
        }
        tPtr->SetObjectPropertyValue(upropInstance.Data(), referencePtr);
        return true;
    }
    if (UnrealInterfaceProperty* tPtr = CastUnrealProperty<UnrealInterfaceProperty>(upropInstance.Property()))
    {
        FScriptInterface inter = tPtr->GetPropertyValue(upropInstance.Data());
        if (referencePtr == inter.GetInterface())
        {
            return false;
        }
        inter.SetInterface(referencePtr);
        inter.SetObject(referencePtr);
        tPtr->SetPropertyValue(upropInstance.Data(), inter);
        return true;
    }
    KS::Log::Warning("Expected reference property but found " +
        std::string(TCHAR_TO_UTF8(*upropInstance.Property()->GetClass()->GetName())), LOG_CHANNEL);
    return false;
}

sfProperty::SPtr sfPropertyManager::GetClass(const sfUPropertyInstance& upropInstance)
{
    UnrealClassProperty* tPtr = CastUnrealProperty<UnrealClassProperty>(upropInstance.Property());
    UObject* classPtr = tPtr->GetObjectPropertyValue(upropInstance.Data());
    if (classPtr == nullptr)
    {
        return sfNullProperty::Create();
    }
    return sfPropertyUtil::FromString(sfUnrealUtils::ClassToFString(Cast<UClass>(classPtr)));
}

bool sfPropertyManager::SetClass(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    UnrealObjectProperty* tPtr = CastUnrealProperty<UnrealObjectProperty>(upropInstance.Property());
    UObject* oldPtr = tPtr->GetObjectPropertyValue(upropInstance.Data());
    if (propPtr->Type() == sfProperty::NUL)
    {
        if (oldPtr == nullptr)
        {
            return false;
        }
        tPtr->SetObjectPropertyValue(upropInstance.Data(), nullptr);
        return true;
    }
    UClass* classPtr = sfUnrealUtils::LoadClass(sfPropertyUtil::ToString(propPtr));
    if (oldPtr == classPtr)
    {
        return false;
    }
    tPtr->SetObjectPropertyValue(upropInstance.Data(), classPtr);
    return true;
}

sfProperty::SPtr sfPropertyManager::GetSoftClass(const sfUPropertyInstance& upropInstance)
{
    TSoftClassPtr<UObject>& softClassPtr = *(TSoftClassPtr<UObject>*)upropInstance.Data();
    if (softClassPtr == nullptr)
    {
        return sfNullProperty::Create();
    }
    return sfPropertyUtil::FromString(softClassPtr.ToString());
}

bool sfPropertyManager::SetSoftClass(const sfUPropertyInstance& upropInstance, sfProperty::SPtr propPtr)
{
    TSoftClassPtr<UObject>& softClassPtr = *(TSoftClassPtr<UObject>*)upropInstance.Data();
    if (propPtr->Type() == sfProperty::NUL)
    {
        softClassPtr = nullptr;
    }
    else
    {
        softClassPtr = FSoftObjectPath(sfPropertyUtil::ToString(propPtr));
    }
    return true;
}

// Compares the src list values in lock step with the dest list values. When there is a discrepancy and the list sizes
// are different, we first check for an element removal (Current src value = Next dest value). Next we check for an
// element insertion (Next src value = Current dest value). Finally if neither of the above cases were found, we
// replace the current dest value with the current src value.
void sfPropertyManager::CopyList(sfListProperty::SPtr destPtr, sfListProperty::SPtr srcPtr)
{
    std::vector<sfProperty::SPtr> toAdd;
    for (int i = 0; i < srcPtr->Size(); i++)
    {
        sfProperty::SPtr elementPtr = srcPtr->Get(i);
        if (destPtr->Size() <= i)
        {
            toAdd.push_back(elementPtr);
            continue;
        }
        if (elementPtr->Equals(destPtr->Get(i)))
        {
            continue;
        }
        if (srcPtr->Size() != destPtr->Size())
        {
            // if the current src element matches the next next element, remove the current dest element.
            if (destPtr->Size() > i + 1 && elementPtr->Equals(destPtr->Get(i + 1)))
            {
                destPtr->Remove(i);
                continue;
            }
            // if the current dest element matches the next src element, insert the current src element.
            if (srcPtr->Size() > i + 1 && destPtr->Get(i)->Equals(srcPtr->Get(i + 1)))
            {
                destPtr->Insert(i, elementPtr);
                i++;
                continue;
            }
        }
        if (!Copy(destPtr->Get(i), elementPtr))
        {
            destPtr->Set(i, elementPtr);
        }
    }
    if (toAdd.size() > 0)
    {
        destPtr->AddRange(toAdd);
    }
    else if (destPtr->Size() > srcPtr->Size())
    {
        destPtr->Resize(srcPtr->Size());
    }
}

void sfPropertyManager::CopyDict(sfDictionaryProperty::SPtr destPtr, sfDictionaryProperty::SPtr srcPtr)
{
    std::vector<sfName> toRemove;
    for (const auto& iter : *destPtr)
    {
        if (!srcPtr->HasKey(iter.first))
        {
            toRemove.push_back(iter.second->Key());
        }
    }
    for (const sfName key : toRemove)
    {
        destPtr->Remove(key);
    }
    for (const auto& iter : *srcPtr)
    {
        sfProperty::SPtr destPropPtr;
        if (!destPtr->TryGet(iter.first, destPropPtr) || !Copy(destPropPtr, iter.second))
        {
            destPtr->Set(iter.first, iter.second);
        }
    }
}

void sfPropertyManager::RegisterPropertyChangeHandlerForClass(FName className, PropertyChangeHandler handler)
{
    m_classToPropertyChangeHandler.emplace(sfUnrealUtils::GetNameTypeHash(className), handler);
}

void sfPropertyManager::UnregisterPropertyChangeHandlerForClass(FName className)
{
    m_classToPropertyChangeHandler.erase(sfUnrealUtils::GetNameTypeHash(className));
}

void sfPropertyManager::GetInstances(UObject* defaultObjPtr, TArray<UObject*>& instances)
{
    UObject* outerPtr = defaultObjPtr->GetOuter();
    if (defaultObjPtr->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
    {
        defaultObjPtr->GetArchetypeInstances(instances);
    }
    else if (outerPtr->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
    {
        TArray<UObject*> archetypeInstances;
        outerPtr->GetArchetypeInstances(archetypeInstances);

        for (UnrealProperty* Property = outerPtr->GetClass()->RefLink; Property != nullptr; Property = Property->NextRef)
        {
            if (defaultObjPtr != *Property->ContainerPtrToValuePtr<UObject*>(outerPtr))
            {
                continue;
            }

            for (UObject* archetypeInstance : archetypeInstances)
            {
                if (UObject* componentInstance = *Property->ContainerPtrToValuePtr<UObject*>(archetypeInstance))
                {
                    instances.Add(componentInstance);
                }
            }

            break;
        }
    }
}

void sfPropertyManager::GetInstancesWithDefaultValue(
    UObject* defaultObjPtr,
    UnrealProperty* upropPtr,
    TArray<UObject*>& instances)
{
    GetInstances(defaultObjPtr, instances);
    for (int i = instances.Num() - 1; i >= 0; i--)
    {
        UObject* instance = instances[i];
        if (!upropPtr->Identical_InContainer(instance, defaultObjPtr))
        {
            instances.RemoveAt(i);
        }
    }
}


#undef LOG_CHANNEL
#undef CREATE_TYPE_HANDLER