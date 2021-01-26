#include "sfFoliageTranslator.h"
#include "../../Public/Translators/sfLevelTranslator.h"
#include "../../Public/Translators/sfActorTranslator.h"
#include "../../Public/Translators/sfComponentTranslator.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/sfPropertyUtil.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/sfBufferArchive.h"
#include "../../Public/sfUnrealUtils.h"
#include "../../Public/sfLoader.h"
#include <Engine/World.h>
#include <Editor.h>
#include <EdMode.h>
#include <EditorModeManager.h>
#include <LandscapeComponent.h>
#include <LandscapeHeightfieldCollisionComponent.h>

#define LOG_CHANNEL "sfFoliageTranslator"

sfFoliageTranslator::sfFoliageTranslator() :
    m_uiStale{ false }
{
    // Don't sync PerInstanceSMData on UFoiliageInstancedStaticMeshComponents as the data can be generated from the
    // foliage instance data we sync.
    sfPropertyManager::Get().AddToBlacklist(UFoliageInstancedStaticMeshComponent::StaticClass()->GetFName(),
        "PerInstanceSMData");
}

void sfFoliageTranslator::Initialize()
{
    m_preTickHandle = SceneFusion::Get().PreTick.AddRaw(this, &sfFoliageTranslator::PreTick);
    m_showedDisabledMessage = false;
    if (SceneFusion::Get().Service->Session()->GetObjectLimit(sfType::Foliage) == 0)
    {
        return;
    }

    sfLoader::Get().RegisterCreatableAssetType<UFoliageType>();

    TSharedPtr<sfActorTranslator> actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(
        sfType::Actor);
    // Sync AInstancedFoliageActors even though they aren't shown in the world outliner
    actorTranslatorPtr->RegisterHiddenSyncType<AInstancedFoliageActor>();
    actorTranslatorPtr->RegisterObjectInitializer<AInstancedFoliageActor>(
        [this](sfObject::SPtr objPtr, AActor* actorPtr)
    {
        // Create the child sfObject containing foliage data 
        sfObject::SPtr childPtr = CreateObject(Cast<AInstancedFoliageActor>(actorPtr));
        if (childPtr != nullptr)
        {
            objPtr->AddChild(childPtr);
        }
    });
    actorTranslatorPtr->RegisterActorInitializer<AInstancedFoliageActor>(
        [this](sfObject::SPtr objPtr, AActor* actorPtr)
    {
        // Set the level's pointer to its foliage actor.
        AInstancedFoliageActor* foliagePtr = Cast<AInstancedFoliageActor>(actorPtr);
        foliagePtr->GetLevel()->InstancedFoliageActor = foliagePtr;
    });
    actorTranslatorPtr->RegisterOnModifyHandler<AInstancedFoliageActor>([this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        m_modifiedFoliageActors.Add(Cast<AInstancedFoliageActor>(uobjPtr));
    });

    TSharedPtr<sfComponentTranslator> componentTranslatorPtr = SceneFusion::Get()
        .GetTranslator<sfComponentTranslator>(sfType::Component);
    componentTranslatorPtr->RegisterObjectInitializer<UFoliageInstancedStaticMeshComponent>(
        [this](sfObject::SPtr objPtr, UActorComponent* componentPtr)
    {
        // Add component to the modified set so we will sync its instances on the next tick
        sfObject::SPtr parentPtr = sfObjectMap::GetSFObject(componentPtr->GetOwner());
        if (parentPtr != nullptr && sfUnrealUtils::FindChildByType(parentPtr, sfType::Foliage) != nullptr)
        {
            m_modifiedComponents.Add(Cast<UFoliageInstancedStaticMeshComponent>(componentPtr));
        }
    });
    componentTranslatorPtr->RegisterOnModifyHandler(UFoliageInstancedStaticMeshComponent::StaticClass()->GetFName(),
        [this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        m_modifiedComponents.Add(Cast<UFoliageInstancedStaticMeshComponent>(uobjPtr));
    });
    componentTranslatorPtr->RegisterDestroyHandler<UFoliageInstancedStaticMeshComponent>(
        [this](sfObject::SPtr objPtr, UActorComponent* componentPtr)
    {
        m_modifiedComponents.Add(Cast<UFoliageInstancedStaticMeshComponent>(componentPtr));
    });

    sfObjectMap::RegisterRemoveHandler<AInstancedFoliageActor>([this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        m_modifiedFoliageActors.Remove(Cast<AInstancedFoliageActor>(uobjPtr));
    });
    sfObjectMap::RegisterRemoveHandler<UFoliageInstancedStaticMeshComponent>(
        [this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        m_modifiedComponents.Remove(Cast<UFoliageInstancedStaticMeshComponent>(uobjPtr));
    });
}

void sfFoliageTranslator::CleanUp()
{
    sfLoader::Get().UnregisterCreatableAssetType<UFoliageType>();
    SceneFusion::Get().PreTick.Remove(m_preTickHandle);
    SceneFusion::Get().GetTranslator<sfActorTranslator>(
        sfType::Actor)->UnregisterHiddenSyncType<AInstancedFoliageActor>();
    m_modifiedFoliageActors.Empty();
    m_modifiedComponents.Empty();
    m_pendingBases.Empty();
    m_possibleDuplicates.Empty();
}

void sfFoliageTranslator::PreTick(float deltaTime)
{
    if (SceneFusion::Get().Service->Session()->GetObjectLimit(sfType::Foliage) == 0)
    {
        if (GLevelEditorModeTools().GetActiveMode("EM_Foliage") != nullptr)
        {
            SceneFusion::Get().ShowUpgradeLink(SceneFusion::RestrictedFeature::FOLIAGE);
            if (!m_showedDisabledMessage)
            {
                m_showedDisabledMessage = true;
                FText message = FText::FromString(
                    "Foliage syncing is disabled on your account. Upgrade to enable foliage syncing.");
                FText title = FText::FromString("Scene Fusion");
                FMessageDialog::Open(EAppMsgType::Ok, message, &title);
            }
        }
        return;
    }

    // We need to sync foliage changes in the pretick before we can apply server changes.
    for (AInstancedFoliageActor* actorPtr : m_modifiedFoliageActors)
    {
        SyncFoliageTypes(actorPtr);
    }
    m_modifiedFoliageActors.Empty();

    UpdatePendingBases();
    RemoveDuplicates();

    for (UFoliageInstancedStaticMeshComponent* componentPtr : m_modifiedComponents)
    {
        SyncInstances(componentPtr);
    }
    m_modifiedComponents.Empty();

    if (m_uiStale)
    {
        RefreshFoliageUI();
        m_uiStale = false;
    }
}

void sfFoliageTranslator::SyncFoliageTypes(AInstancedFoliageActor* actorPtr)
{
    TSharedPtr<sfComponentTranslator> componentTranslatorPtr = SceneFusion::Get()
        .GetTranslator<sfComponentTranslator>(sfType::Component);
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
    if (objPtr == nullptr)
    {
        return;
    }
    componentTranslatorPtr->SyncComponents(actorPtr);

    // If we don't set pending kill components to nullptr, we won't be able to add foliage for that type.
    for (auto& iter : GetFoliageInfos(actorPtr))
    {
        UHierarchicalInstancedStaticMeshComponent* componentPtr = GetComponentFromInfo(&iter.Value.Get());
        if (componentPtr != nullptr && componentPtr->IsPendingKill())
        {
            SetComponentOnInfo(&iter.Value.Get(), nullptr);
        }
    }

    objPtr = sfUnrealUtils::FindChildByType(objPtr, sfType::Foliage);
    if (objPtr == nullptr)
    {
        return;
    }
    sfListProperty::SPtr listPtr = objPtr->Property()->AsList();
    if (listPtr->Size() == GetFoliageInfos(actorPtr).Num())
    {
        return;
    }
    TSet<UFoliageType*> keys;
    for (auto& iter : GetFoliageInfos(actorPtr))
    {
        keys.Add(iter.Key);
    }
    // Check for removed foliage types
    for (int i = listPtr->Size() - 1; i >= 0; i--)
    {
        sfDictionaryProperty::SPtr propPtr = listPtr->Get(i)->AsDict();
        UFoliageType* typePtr = sfPropertyManager::Get().To<UFoliageType>(propPtr->Get(sfProp::Type));
        if (keys.Remove(typePtr) == 0)
        {
            if (objPtr->IsLocked())
            {
                OnListAdd(listPtr, i, 1);
            }
            else
            {
                listPtr->Remove(i);
            }
        }
    }
    // Check for new foliage types
    for (auto iter : keys)
    {
        if (objPtr->IsLocked())
        {
            actorPtr->RemoveFoliageType(&iter, 1);
            m_uiStale = true;
        }
        else
        {
            listPtr->Add(CreateFoliageProperty(iter, *FindFoliageInfo(actorPtr, iter)));
        }
    }
}

void sfFoliageTranslator::UpdatePendingBases()
{
    // Pending bases are components that weren't synced when we tried to attach foliage to them. Check if they are
    // synced or deleted now and attach the foliage.
    for (auto iter = m_pendingBases.CreateIterator(); iter; ++iter)
    {
        bool synced = true;
        for (auto idIter : iter.Value())
        {
            sfObject::SPtr objPtr = SceneFusion::Service->Session()->GetObject(idIter);
            if (objPtr != nullptr && sfObjectMap::Get<UActorComponent>(objPtr) == nullptr)
            {
                synced = false;
                break;
            }
        }
        if (synced)
        {
            // All pending base components are synced for this foliage type.
            AInstancedFoliageActor* actorPtr = iter.Key().Key;
            UFoliageType* typePtr = iter.Key().Value;
            iter.RemoveCurrent();
            sfObject::SPtr actorObjPtr = sfObjectMap::GetSFObject(actorPtr);
            if (actorObjPtr == nullptr)
            {
                return;
            }
            sfObject::SPtr objPtr = sfUnrealUtils::FindChildByType(actorObjPtr, sfType::Foliage);
            if (objPtr == nullptr)
            {
                return;
            }
            sfListProperty::SPtr listPtr = objPtr->Property()->AsList();
            // Find the property for this foliage type
            for (auto listIter = listPtr->begin(); listIter != listPtr->end(); ++listIter)
            {
                sfDictionaryProperty::SPtr propPtr = (*listIter)->AsDict();
                if (sfPropertyManager::Get().To<UFoliageType>(propPtr->Get(sfProp::Type)) == typePtr)
                {
                    // Deserialize all the instances for this type.
                    Deserialize(actorPtr, typePtr, *actorPtr->FindOrAddMesh(typePtr),
                        propPtr->Get(sfProp::Instances)->AsList());
                }
            }
        }
    }
}

void sfFoliageTranslator::RemoveDuplicates()
{
    // If two users remove foliage at the same time it's possible to get duplicate instances, so we check for and
    // remove those. This happens because when you remove an instance, Unreal will remove the last instance in the
    // array and move it to the index you removed. When two users remove two different indexes at the same time, they
    // may both remove the last element and move it to the two different indexes, creating a duplicate. We keep the
    // first occurence of the instance and remove duplicates at higher indexes.
    for (auto& iter : m_possibleDuplicates)
    {
        AInstancedFoliageActor* actorPtr = iter.Key.Key;
        UFoliageType* typePtr = iter.Key.Value;
        FFoliageInfo* infoPtr = actorPtr->FindOrAddMesh(typePtr);
        // Cast to our hack class to access the cell map. Any duplicate will be in the same hash cell so we only have
        // to check instances in the same cell.
        FoliageHashHack* hackPtr = reinterpret_cast<FoliageHashHack*>(infoPtr->InstanceHash.Get());
        TArray<int> toRemove;
        for (int index1 : iter.Value)
        {
            if (index1 >= infoPtr->Instances.Num())
            {
                continue;
            }
            uint64_t key = hackPtr->MakeKey(infoPtr->Instances[index1].Location);
            TSet<int>* setPtr = hackPtr->CellMap.Find(key);
            if (setPtr == nullptr)
            {
                continue;
            }
            bool found = false;
            for (int index2 : *setPtr)
            {
                if (Compare(infoPtr->Instances[index1], infoPtr->Instances[index2]))
                {
                    if (found)
                    {
                        toRemove.Add(index2);
                    }
                    else
                    {
                        found = true;
                    }
                }
            }
        }
        if (toRemove.Num() > 0)
        {
            SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
            KS::Log::Info("Removing " + std::to_string(toRemove.Num()) + " duplicate(s)", LOG_CHANNEL);
            infoPtr->RemoveInstances(actorPtr, toRemove, true);
            m_modifiedComponents.Add(Cast<UFoliageInstancedStaticMeshComponent>(GetComponentFromInfo(infoPtr)));
            SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
        }
    }
    m_possibleDuplicates.Empty();
}

void sfFoliageTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    AInstancedFoliageActor* actorPtr = sfObjectMap::Get<AInstancedFoliageActor>(objPtr->Parent());
    if (actorPtr == nullptr)
    {
        return;
    }
    // Create a set of existing foliage types
    TSet<UFoliageType*> keys;
    for (auto& iter : GetFoliageInfos(actorPtr))
    {
        keys.Add(iter.Key);
    }
    // Iterate the list of foliage type properties
    sfListProperty::SPtr listPtr = objPtr->Property()->AsList();
    for (auto iter = listPtr->begin(); iter != listPtr->end(); ++iter)
    {
        sfDictionaryProperty::SPtr propPtr = (*iter)->AsDict();
        // Create the foliage type and its foliage instances
        UFoliageType* typePtr = OnCreateFoliageProperty(actorPtr, propPtr);
        if (typePtr != nullptr)
        {
            // Remove the type from the set so what is left are types that aren't on the server
            keys.Remove(typePtr);
        }
    }
    // Remove foliage types that aren't on the server
    for (auto iter : keys)
    {
        actorPtr->RemoveFoliageType(&iter, 1);
    }
}

sfObject::SPtr sfFoliageTranslator::CreateObject(AInstancedFoliageActor* actorPtr)
{
    // Create the foliage sfObject
    sfListProperty::SPtr listPtr = sfListProperty::Create();
    sfObject::SPtr objPtr = sfObject::Create(sfType::Foliage, listPtr);
    for (auto& iter : GetFoliageInfos(actorPtr))
    {
        listPtr->Add(CreateFoliageProperty(iter.Key, *iter.Value));
    }
    return objPtr;
}

UFoliageType* sfFoliageTranslator::OnCreateFoliageProperty(
    AInstancedFoliageActor* actorPtr,
    sfDictionaryProperty::SPtr propPtr)
{
    // Convert the property to a foliage type
    UFoliageType* typePtr = sfPropertyManager::Get().To<UFoliageType>(propPtr->Get(sfProp::Type));
    if (typePtr != nullptr)
    {
        FFoliageInfo* infoPtr = actorPtr->FindOrAddMesh(typePtr);
        if (infoPtr != nullptr)
        {
            sfProperty::SPtr componentPropPtr;
            if (propPtr->TryGet(sfProp::Component, componentPropPtr))
            {
                // Set the component for the foliage type
                sfObject::SPtr componentObjPtr = SceneFusion::Service->Session()->GetObject(
                    componentPropPtr->AsReference()->GetObjectId());
                SetComponentOnInfo(
                    infoPtr,
                    sfObjectMap::Get<UHierarchicalInstancedStaticMeshComponent>(componentObjPtr));
            }
            if (GetComponentFromInfo(infoPtr) != nullptr)
            {
                // Deserialize the foliage instances
                Deserialize(actorPtr, typePtr, *infoPtr, propPtr->Get(sfProp::Instances)->AsList());
                m_uiStale = true;
            }
        }
    }
    return typePtr;
}

sfDictionaryProperty::SPtr sfFoliageTranslator::CreateFoliageProperty(
    UFoliageType* typePtr,
    FFoliageInfo& meshInfo)
{
    // Creates a property for a foliage type
    sfDictionaryProperty::SPtr propPtr = sfDictionaryProperty::Create();
    propPtr->Set(sfProp::Type, sfPropertyManager::Get().FromUObject(typePtr));
    sfObject::SPtr componentObjPtr = sfObjectMap::GetOrCreateSFObject(
        GetComponentFromInfo(&meshInfo),
        sfType::Component);
    if (componentObjPtr != nullptr)
    {
        propPtr->Set(sfProp::Component, sfReferenceProperty::Create(componentObjPtr->Id()));
    }
    propPtr->Set(sfProp::Instances, Serialize(meshInfo.Instances));
    return propPtr;
}

void sfFoliageTranslator::OnPropertyChange(sfProperty::SPtr propertyPtr)
{
    if (propertyPtr->GetDepth() == 3 && propertyPtr->Index() >= 0)
    {
        // A foliage instance changed.
        UFoliageType* typePtr = sfPropertyManager::Get().To<UFoliageType>(
            propertyPtr->GetParentProperty()->GetParentProperty()->AsDict()->Get(sfProp::Type));
        if (typePtr == nullptr)
        {
            return;
        }
        AInstancedFoliageActor* actorPtr = sfObjectMap::Get<AInstancedFoliageActor>(
            propertyPtr->GetContainerObject()->Parent());
        if (actorPtr == nullptr)
        {
            return;
        }
        FFoliageInfo* infoPtr = actorPtr->FindOrAddMesh(typePtr);
        if (infoPtr == nullptr)
        {
            return;
        }
        if (infoPtr->Instances.Num() <= propertyPtr->Index())
        {
            // The number of instances doesn't match the expected value, so just deserialize everything.
            Deserialize(actorPtr, typePtr, *infoPtr, propertyPtr->GetParentProperty()->AsList());
            return;
        }
        // Deserialize the instance
        const ksMultiType& multiType = propertyPtr->AsValue()->GetValue();
        sfBufferReader reader{ (void*)multiType.GetData().data(), (int)multiType.GetData().size() };
        FFoliageInstance instance;
        uint32_t baseId = Serialize(reader, instance);
        if (baseId != 0)
        {
            // The base component for the instance is not synced yet. Add the component's sfObject id to the pending
            // bases so we can deserialize the foliage once it is synced.
            AddPendingBase(actorPtr, typePtr, baseId);
            return;
        }
        SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
        // Add new instance to the end, then remove the target index, moving the new instance to that index. We do this
        // instead of modifying the instance directly because the AddInstance and RemoveInstances functions update the
        // instance locality hash.
        RebuildTreeIfInvalid(GetComponentFromInfo(infoPtr));
        AddInstanceToFoliageInfo(infoPtr, actorPtr, typePtr, instance, instance.BaseComponent, false);
        TArray<int> toRemove = { propertyPtr->Index() };
        infoPtr->RemoveInstances(actorPtr, toRemove, true);
        SceneFusion::RedrawActiveViewport();
        SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
        // Add the instance to the set of possible duplicates. We'll check for and remove duplicates in the next tick.
        TPair<AInstancedFoliageActor*, UFoliageType*> pair{ actorPtr, typePtr };
        m_possibleDuplicates.FindOrAdd(pair).Add(propertyPtr->Index());
    }
    else if (propertyPtr->Key() == sfProp::Component)
    {
        // The component for a foliage type changed.
        UFoliageType* typePtr = sfPropertyManager::Get().To<UFoliageType>(
            propertyPtr->GetParentProperty()->AsDict()->Get(sfProp::Type));
        if (typePtr == nullptr)
        {
            return;
        }
        AInstancedFoliageActor* actorPtr = sfObjectMap::Get<AInstancedFoliageActor>(
            propertyPtr->GetContainerObject()->Parent());
        if (actorPtr == nullptr)
        {
            return;
        }
        FFoliageInfo* infoPtr = actorPtr->FindOrAddMesh(typePtr);
        if (infoPtr == nullptr)
        {
            return;
        }
        sfObject::SPtr componentObjPtr
            = SceneFusion::Service->Session()->GetObject(propertyPtr->AsReference()->GetObjectId());
        SetComponentOnInfo(
            infoPtr,
            sfObjectMap::Get<UHierarchicalInstancedStaticMeshComponent>(componentObjPtr));
        if (GetComponentFromInfo(infoPtr) != nullptr)
        {
            // Deserialize the foliage instances
            Deserialize(actorPtr, typePtr, *infoPtr, 
                propertyPtr->GetParentProperty()->AsDict()->Get(sfProp::Instances)->AsList());
            m_uiStale = true;
        }
    }
    else if (propertyPtr->Key() == sfProp::Type)
    {
        AInstancedFoliageActor* actorPtr = sfObjectMap::Get<AInstancedFoliageActor>(
            propertyPtr->GetContainerObject()->Parent());
        if (actorPtr == nullptr)
        {
            return;
        }
        SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
        OnCreateFoliageProperty(actorPtr, propertyPtr->GetParentProperty()->AsDict());
        SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
    }
}

void sfFoliageTranslator::OnListAdd(sfListProperty::SPtr listPtr, int index, int count)
{
    if (listPtr->GetDepth() == 2)
    {
        // foliage instances were added
        UFoliageType* typePtr = sfPropertyManager::Get().To<UFoliageType>(
            listPtr->GetParentProperty()->AsDict()->Get(sfProp::Type));
        if (typePtr == nullptr)
        {
            return;
        }
        AInstancedFoliageActor* actorPtr = sfObjectMap::Get<AInstancedFoliageActor>(
            listPtr->GetContainerObject()->Parent());
        if (actorPtr == nullptr)
        {
            return;
        }
        FFoliageInfo* infoPtr = actorPtr->FindOrAddMesh(typePtr);
        if (infoPtr == nullptr)
        {
            return;
        }
        SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
        TArray<FFoliageInstance> movedInstances;
        TArray<int> toRemove;
        // If there are instances at or after the index we are inserting at, remove them and add them to the moved
        // instances array
        for (int i = index; i < infoPtr->Instances.Num(); i++)
        {
            movedInstances.Add(infoPtr->Instances[i]);
            toRemove.Add(i);
        }
        if (toRemove.Num() > 0)
        {
            infoPtr->RemoveInstances(actorPtr, toRemove, false);
        }
        RebuildTreeIfInvalid(GetComponentFromInfo(infoPtr));
        for (int i = index; i < index + count; i++)
        {
            // Deserialize the instance
            const ksMultiType& multiType = listPtr->Get(i)->AsValue()->GetValue();
            sfBufferReader reader{ (void*)multiType.GetData().data(), (int)multiType.GetData().size() };
            FFoliageInstance instance;
            uint32_t baseId = Serialize(reader, instance);
            if (baseId != 0)
            {
                // The base component for the instance is not synced yet. Add the component's sfObject id to the
                // pending bases so we can deserialize the foliage once it is synced.
                AddPendingBase(actorPtr, typePtr, baseId);
                continue;
            }
            AddInstanceToFoliageInfo(infoPtr, actorPtr, typePtr, instance, instance.BaseComponent, false);
        }
        // Reappend the moved instances
        for (int i = 0; i < movedInstances.Num(); i++)
        {
            AddInstanceToFoliageInfo(infoPtr, actorPtr, typePtr, movedInstances[i], movedInstances[i].BaseComponent, false);
        }
        GetComponentFromInfo(infoPtr)->BuildTreeIfOutdated(true, false);
        m_uiStale = true;
        SceneFusion::RedrawActiveViewport();
        SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
    }
    else if (listPtr->GetDepth() == 0)
    {
        // a foliage type was added
        AInstancedFoliageActor* actorPtr = sfObjectMap::Get<AInstancedFoliageActor>(
            listPtr->GetContainerObject()->Parent());
        if (actorPtr != nullptr)
        {
            SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
            for (int i = index; i < index + count; i++)
            {
                OnCreateFoliageProperty(actorPtr, listPtr->Get(i)->AsDict());
            }
            SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
        }
    }
}

void sfFoliageTranslator::OnListRemove(sfListProperty::SPtr listPtr, int index, int count)
{
    if (listPtr->GetDepth() == 2)
    {
        // foliage instances were removed
        UFoliageType* typePtr = sfPropertyManager::Get().To<UFoliageType>(
            listPtr->GetParentProperty()->AsDict()->Get(sfProp::Type));
        if (typePtr == nullptr)
        {
            return;
        }
        AInstancedFoliageActor* actorPtr = sfObjectMap::Get<AInstancedFoliageActor>(
            listPtr->GetContainerObject()->Parent());
        if (actorPtr == nullptr)
        {
            return;
        }
        FFoliageInfo* infoPtr = actorPtr->FindOrAddMesh(typePtr);
        if (infoPtr == nullptr)
        {
            return;
        }
        if (infoPtr->Instances.Num() < index + count)
        {
            Deserialize(actorPtr, typePtr, *infoPtr, listPtr);
            return;
        }
        SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
        TArray<FFoliageInstance> movedInstances;
        TArray<int> toRemove;
        for (int i = index; i < index + count; i++)
        {
            toRemove.Add(i);
        }
        // If there are instances beyond the last index we are removing, we need to remove them too and add them to the
        // moved instances array to be reinserted. This is because when removing an instance, Unreal moves the last
        // instance to the removed index and this would mess up the order if we didn't remove and reinsert those
        // instances.
        for (int i = index + count; i < infoPtr->Instances.Num(); i++)
        {
            movedInstances.Add(infoPtr->Instances[i]);
            toRemove.Add(i);
        }
        if (toRemove.Num() > 0)
        {
            infoPtr->RemoveInstances(actorPtr, toRemove, false);
        }
        RebuildTreeIfInvalid(GetComponentFromInfo(infoPtr));
        // Reappend the moved instances
        for (int i = 0; i < movedInstances.Num(); i++)
        {
            AddInstanceToFoliageInfo(infoPtr, actorPtr, typePtr, movedInstances[i], movedInstances[i].BaseComponent, false);
        }
        GetComponentFromInfo(infoPtr)->BuildTreeIfOutdated(true, false);
        m_uiStale = true;
        SceneFusion::RedrawActiveViewport();
        SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
    }
    else if (listPtr->GetDepth() == 0)
    {
        // a foliage type was removed
        AInstancedFoliageActor* actorPtr = sfObjectMap::Get<AInstancedFoliageActor>(
            listPtr->GetContainerObject()->Parent());
        if (actorPtr == nullptr)
        {
            return;
        }

        SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
        // Create a set of existing foliage types
        TSet<UFoliageType*> keys;
        for (auto& iter : GetFoliageInfos(actorPtr))
        {
            keys.Add(iter.Key);
        }
        // Iterate the list of foliage type properties, and remove foliage types that exist on the server so the set
        // only contains types we need to remove
        for (auto iter = listPtr->begin(); iter != listPtr->end(); ++iter)
        {
            sfDictionaryProperty::SPtr propPtr = (*iter)->AsDict();
            UFoliageType* typePtr = sfPropertyManager::Get().To<UFoliageType>(propPtr->Get(sfProp::Type));
            if (typePtr != nullptr)
            {
                keys.Remove(typePtr);
            }
        }
        // Remove the foliage types that don't exist on the server
        for (auto iter : keys)
        {
            actorPtr->RemoveFoliageType(&iter, 1);
            m_uiStale = true;
        }
        SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
    }
}

void sfFoliageTranslator::SyncInstances(UFoliageInstancedStaticMeshComponent* componentPtr)
{
    AInstancedFoliageActor* actorPtr = Cast<AInstancedFoliageActor>(componentPtr->GetOwner());
    if (actorPtr == nullptr)
    {
        return;
    }
    sfObject::SPtr actorObjPtr = sfObjectMap::GetSFObject(actorPtr);
    if (actorObjPtr == nullptr)
    {
        return;
    }
    sfObject::SPtr objPtr = sfUnrealUtils::FindChildByType(actorObjPtr, sfType::Foliage);
    if (objPtr == nullptr)
    {
        return;
    }
    sfListProperty::SPtr listPtr = objPtr->Property()->AsList();
    // Iterate the foliage meshes to find the one with the matching component
    for (auto& iter : GetFoliageInfos(actorPtr))
    {
        UHierarchicalInstancedStaticMeshComponent* infoComponentPtr = GetComponentFromInfo(&iter.Value.Get());
        // If the componentPtr is pending kill, the corresponding mesh info's component will be nullptr
        if (infoComponentPtr == componentPtr ||
            (infoComponentPtr == nullptr && componentPtr->IsPendingKill()))
        {
            // Iterate the foliage type properties to find the one that matches the component's foliage type
            for (auto listIter = listPtr->begin(); listIter != listPtr->end(); ++listIter)
            {
                sfDictionaryProperty::SPtr propPtr = (*listIter)->AsDict();
                if (sfPropertyManager::Get().To<UFoliageType>(propPtr->Get(sfProp::Type)) == iter.Key)
                {
                    // If the foliage actor is locked or we are waiting for a base component to sync, revert the
                    // instances to the server state
                    if (actorObjPtr->IsLocked() ||
                        m_pendingBases.Contains(TPair<AInstancedFoliageActor*, UFoliageType*>{ actorPtr, iter.Key }))
                    {
                        Deserialize(actorPtr, iter.Key, *iter.Value, propPtr->Get(sfProp::Instances)->AsList());
                    }
                    else
                    {
                        // The component may have changed (from null to a component if there used to be no instances)
                        sfObject::SPtr componentObjPtr = sfObjectMap::GetSFObject(componentPtr);
                        if (componentObjPtr != nullptr)
                        {
                            sfProperty::SPtr oldProp;
                            if (!propPtr->TryGet(sfProp::Component, oldProp) || 
                                oldProp->AsReference()->GetObjectId() != componentObjPtr->Id())
                            {
                                propPtr->Set(sfProp::Component, sfReferenceProperty::Create(componentObjPtr->Id()));
                            }
                        }
                        // Serialize the instances
                        sfListProperty::SPtr dataPtr = Serialize(iter.Value->Instances);
                        sfPropertyManager::Get().CopyList(propPtr->Get(sfProp::Instances)->AsList(), dataPtr);
                    }
                    break;
                }
            }
        }
    }
}

sfListProperty::SPtr sfFoliageTranslator::Serialize(TArray<FFoliageInstance>& instances)
{
    sfListProperty::SPtr listPtr = sfListProperty::Create();
    for (FFoliageInstance& instance : instances)
    {
        sfBufferArchive writer;
        Serialize(writer, instance);
        listPtr->Add(sfValueProperty::Create(
            ksMultiType{ ksMultiType::BYTE_ARRAY, writer.GetData(), (size_t)writer.Num(), writer.Num() }));
    }
    return listPtr;
}

void sfFoliageTranslator::Deserialize(
    AInstancedFoliageActor* actorPtr,
    UFoliageType* typePtr,
    FFoliageInfo& meshInfo,
    sfListProperty::SPtr listPtr)
{
    UHierarchicalInstancedStaticMeshComponent* infoComponentPtr = GetComponentFromInfo(&meshInfo);
    SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
    bool changed = false;
    // Deseriailize each foliage instance in the list
    for (int i = 0; i < listPtr->Size(); i++)
    {
        const ksMultiType& multiType = listPtr->Get(i)->AsValue()->GetValue();
        sfBufferReader reader{ (void*)multiType.GetData().data(), (int)multiType.GetData().size() };
        FFoliageInstance instance;
        int baseId = Serialize(reader, instance);
        if (baseId != 0)
        {
            // The base component for the instance is not synced yet. Add the component's sfObject id to the pending
            // bases so we can deserialize the foliage once it is synced.
            AddPendingBase(actorPtr, typePtr, baseId);
            continue;
        }

        if (meshInfo.Instances.Num() > i)
        {
            if (Compare(instance, meshInfo.Instances[i]))
            {
                // The instance is unchanged.
                continue;
            }
        }
        RebuildTreeIfInvalid(infoComponentPtr);
        AddInstanceToFoliageInfo(&meshInfo, actorPtr, typePtr, instance, instance.BaseComponent, false);
        changed = true;
        if (meshInfo.Instances.Num() > i + 1)
        {
            // Remove the instance at index i, moving the instance we just added to the removed index.
            TArray<int> toRemove;
            toRemove.Add(i);
            meshInfo.RemoveInstances(actorPtr, toRemove, false);
        }
    }
    // Remove extra instances from the end of the array
    int numToRemove = meshInfo.Instances.Num() - listPtr->Size();
    if (numToRemove > 0)
    {
        TArray<int> toRemove{};
        toRemove.Reserve(numToRemove);
        for (int i = 0; i < numToRemove; i++)
        {
            toRemove.Add(listPtr->Size() + i);
        }
        meshInfo.RemoveInstances(actorPtr, toRemove, false);
        changed = true;
    }
    if (changed && infoComponentPtr != nullptr)
    {
        infoComponentPtr->BuildTreeIfOutdated(true, false);
        m_uiStale = true;
    }
    SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
}

uint32_t sfFoliageTranslator::Serialize(FArchive& archive, FFoliageInstance& instance)
{
    uint32_t pendingBaseId = 0;
    if (archive.IsLoading())
    {
        // We are deserializing
        uint32_t baseId;
        archive << baseId;
        if (baseId == 0)
        {
            instance.BaseComponent = nullptr;
        }
        else
        {
            // Find the base component from its sfobject id
            sfObject::SPtr objPtr = SceneFusion::Service->Session()->GetObject(baseId);
            if (objPtr != nullptr && objPtr->IsCreated())
            {
                UActorComponent* baseComponentPtr = sfObjectMap::Get<UActorComponent>(objPtr);
                if (baseComponentPtr == nullptr)
                {
                    pendingBaseId = baseId;
                }
                else
                {
                    // If it's attached to landscape, it should be attached to the landscape collision component, but
                    // because we don't sync landscape collision components, we send the id for the landscape component
                    // instead.
                    ULandscapeComponent* landscapeComponentPtr = Cast<ULandscapeComponent>(baseComponentPtr);
                    instance.BaseComponent = landscapeComponentPtr == nullptr ?
                        baseComponentPtr : landscapeComponentPtr->CollisionComponent.Get();
                }
            }
        }
    }
    else
    {
        // We are serializing.
        uint32_t baseId = 0;
        UActorComponent* baseComponentPtr = instance.BaseComponent;
        if (baseComponentPtr != nullptr && baseComponentPtr->IsValidLowLevel() && !baseComponentPtr->IsPendingKill())
        {
            ULandscapeHeightfieldCollisionComponent* landscapeCollisionPtr =
                Cast<ULandscapeHeightfieldCollisionComponent>(baseComponentPtr);
            if (landscapeCollisionPtr != nullptr)
            {
                // If it's attached to landscape, it should be attached to the landscape collision component, but
                // because we don't sync landscape collision components, we send the id for the landscape component
                // instead.
                baseComponentPtr = landscapeCollisionPtr->RenderComponent.Get();
            }
            if (baseComponentPtr != nullptr)
            {
                // Serialize the base component's sfobject id
                sfObject::SPtr objPtr = sfObjectMap::GetOrCreateSFObject(baseComponentPtr, sfType::Component);
                baseId = objPtr->Id();
            }
        }
        archive << baseId;
    }

    archive << instance.Location;
    archive << instance.Rotation;
    archive << instance.DrawScale3D;
    archive << instance.PreAlignRotation;
    archive << instance.ProceduralGuid;
    archive << instance.Flags;
    archive << instance.ZOffset;

    return pendingBaseId;
}

void sfFoliageTranslator::AddPendingBase(AInstancedFoliageActor* actorPtr, UFoliageType* typePtr, uint32_t baseId)
{
    TPair<AInstancedFoliageActor*, UFoliageType*> pair{ actorPtr, typePtr };
    m_pendingBases.FindOrAdd(pair).Add(baseId);
}

bool sfFoliageTranslator::Compare(const FFoliageInstance& lhs, const FFoliageInstance& rhs)
{
    return lhs.BaseComponent == rhs.BaseComponent &&
        lhs.Location == rhs.Location &&
        lhs.Rotation == rhs.Rotation &&
        lhs.DrawScale3D == rhs.DrawScale3D &&
        lhs.PreAlignRotation == rhs.PreAlignRotation &&
        lhs.ProceduralGuid == rhs.ProceduralGuid &&
        lhs.Flags == rhs.Flags &&
        lhs.ZOffset == rhs.ZOffset;
}

void sfFoliageTranslator::RebuildTreeIfInvalid(UHierarchicalInstancedStaticMeshComponent* componentPtr)
{
    if (componentPtr != nullptr && componentPtr->InstanceReorderTable.Num() != componentPtr->PerInstanceSMData.Num())
    {
        componentPtr->BuildTreeIfOutdated(false, true);
    }
}

void sfFoliageTranslator::RefreshFoliageUI()
{
    FEdMode* modePtr = GLevelEditorModeTools().GetActiveMode("EM_Foliage");
    if (modePtr != nullptr)
    {
        // PostUndo calls a private function to refresh the UI and does nothing else.
        modePtr->PostUndo();
    }
}

bool sfFoliageTranslator::SetComponentOnInfo(
    FFoliageInfo* infoPtr,
    UHierarchicalInstancedStaticMeshComponent* componentPtr)
{
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
    if (infoPtr->Type == EFoliageImplType::StaticMesh && infoPtr->Implementation.IsValid())
    {
        FFoliageStaticMeshHack* foliageStaticMesh
            = static_cast<FFoliageStaticMeshHack*>(infoPtr->Implementation.Get());
        foliageStaticMesh->Component = componentPtr;
        return true;
    }

    return nullptr;
#else
    infoPtr->Component = componentPtr;
    return true;
#endif
}

void sfFoliageTranslator::AddInstanceToFoliageInfo(
    FFoliageInfo* infoPtr,
    AInstancedFoliageActor* actorPtr,
    const UFoliageType* typePtr,
    const FFoliageInstance& instance,
    UActorComponent* baseComponent,
    bool rebuildFoliageTree)
{
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
    infoPtr->AddInstance(actorPtr, typePtr, instance, baseComponent);
    if (rebuildFoliageTree)
    {
        RebuildTreeIfInvalid(infoPtr->GetComponent());
    }
#else
    infoPtr->AddInstance(actorPtr, typePtr, instance, baseComponent, rebuildFoliageTree);
#endif
}

FFoliageInfo* sfFoliageTranslator::FindFoliageInfo(AInstancedFoliageActor* actorPtr, const UFoliageType* type)
{
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
    return actorPtr->FindInfo(type);
#else
    return actorPtr->FindMesh(type);
#endif
}

UHierarchicalInstancedStaticMeshComponent* sfFoliageTranslator::GetComponentFromInfo(FFoliageInfo* infoPtr)
{
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
    return infoPtr->GetComponent();
#else
    return infoPtr->Component;
#endif
}

#undef LOG_CHANNEL