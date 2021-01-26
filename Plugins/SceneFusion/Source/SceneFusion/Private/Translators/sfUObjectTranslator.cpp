#include "sfUObjectTranslator.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/Consts.h"
#include "../../Public/sfUnrealUtils.h"
#include "../../Public/sfPropertyUtil.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/Objects/sfReferenceTracker.h"
#include <Engine/BrushBuilder.h>

#define DEFAULT_FLAGS (RF_Transactional | RF_DefaultSubObject | RF_WasLoaded | RF_LoadCompleted)
#define LOG_CHANNEL "sfUObjectTranslator"

void sfUObjectTranslator::Initialize()
{
    m_sessionPtr = SceneFusion::Service->Session();
    m_tickHandle = SceneFusion::Get().OnTick.AddRaw(this, &sfUObjectTranslator::Tick);
}

void sfUObjectTranslator::CleanUp()
{
    SceneFusion::Get().OnTick.Remove(m_tickHandle);
    m_pendingDeletions.clear();
}

void sfUObjectTranslator::Tick(float deltaTime)
{
    // Check for locally deleted objects the server has confirmed as deleted
    auto iter = m_pendingDeletions.begin();
    while (iter != m_pendingDeletions.end())
    {
        sfObject::SPtr objPtr = *iter;
        if (objPtr->IsDeletePending())
        {
            ++iter;
            continue;
        }
        m_pendingDeletions.erase(iter++);
        if (objPtr->IsCreated())
        {
            // The delete was rejected
            continue;
        }
        // Add a new parent object to the sfObjectMap and attach this object to it so it will
        // be reused with the same id to preserve references if the parent gets created.
        UObject* uobjPtr = sfObjectMap::GetUObject(objPtr);
        if (uobjPtr == nullptr)
        {
            continue;
        }
        if (!uobjPtr->IsPendingKill() && !uobjPtr->HasAnyFlags(RF_BeginDestroyed))
        {
            FindOrCreateParent(objPtr, uobjPtr);
            if (objPtr->Parent() == nullptr)
            {
                sfObjectMap::Remove(objPtr);
            }
        }
        else
        {
            sfObjectMap::Remove(objPtr);
        }
    }
}

void sfUObjectTranslator::AddPendingDeletion(sfObject::SPtr objPtr)
{
    m_pendingDeletions.emplace_back(objPtr);
}

void sfUObjectTranslator::RemovePendingDeletionsInLevel(ULevel* levelPtr)
{
    for (auto iter = m_pendingDeletions.begin(); iter != m_pendingDeletions.end();)
    {
        sfObject::SPtr objPtr = *iter;
        UObject* uobjPtr = sfObjectMap::GetUObject(*iter);
        if (uobjPtr->GetTypedOuter<ULevel>() == levelPtr)
        {
            iter = m_pendingDeletions.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

void sfUObjectTranslator::FindOrCreateParent(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    sfObject::SPtr parentPtr = sfObjectMap::GetSFObject(uobjPtr->GetOuter());
    if (parentPtr == nullptr)
    {
        parentPtr = SceneFusion::ObjectEventDispatcher->Create(uobjPtr->GetOuter());
        if (parentPtr == nullptr)
        {
            return;
        }
    }
    if (parentPtr->IsSyncing())
    {
        InitializeObjectProperties(objPtr, uobjPtr);
        m_sessionPtr->Create(objPtr, parentPtr, parentPtr->Children().size());
    }
    else
    {
        if (parentPtr->Property()->Type() != sfProperty::DICTIONARY || parentPtr->Property()->AsDict()->Size() > 0)
        {
            InitializeObjectProperties(objPtr, uobjPtr);
        }
        parentPtr->AddChild(objPtr);
    }
}

bool sfUObjectTranslator::Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr)
{
    if (uobjPtr->IsPendingKill())
    {
        return true;
    }
    outObjPtr = sfObject::Create(sfType::UObject, sfDictionaryProperty::Create());
    sfObjectMap::Add(outObjPtr, uobjPtr);
    UsfReferenceTracker::Get().Track(uobjPtr);
    FindOrCreateParent(outObjPtr, uobjPtr);
    if (outObjPtr->Parent() == nullptr)
    {
        sfObjectMap::Remove(outObjPtr);
        outObjPtr = nullptr;
    }
    return true;
}

void sfUObjectTranslator::InitializeObjectProperties(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    if (uobjPtr == nullptr || uobjPtr->IsPendingKill())
    {
        objPtr->Detach();
        sfObjectMap::Remove(objPtr);
        return;
    }
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    propertiesPtr->Set(sfProp::Name, sfPropertyUtil::FromString(uobjPtr->GetName()));
    propertiesPtr->Set(sfProp::Class, sfPropertyUtil::FromString(sfUnrealUtils::ClassToFString(uobjPtr->GetClass())));
    EObjectFlags flags = uobjPtr->GetFlags();
    if (flags != DEFAULT_FLAGS)
    {
        propertiesPtr->Set(sfProp::Flags, sfValueProperty::Create((uint32_t)flags));
    }
    InitializeChildren(objPtr);
    sfPropertyManager::Get().CreateProperties(uobjPtr, propertiesPtr);
}

void sfUObjectTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    UObject* outerPtr = sfObjectMap::GetUObject(objPtr->Parent());
    if (outerPtr == nullptr)
    {
        return;
    }
    InitializeUObject(outerPtr, objPtr);
}

void sfUObjectTranslator::InitializeUObject(UObject* outerPtr, sfObject::SPtr objPtr)
{
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    FString className = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Class));
    UClass* classPtr = sfUnrealUtils::LoadClass(className);
    FString name = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
    UObject* uobjPtr = StaticFindObjectFast(UObject::StaticClass(), outerPtr, FName(*name));
    if (uobjPtr != nullptr)
    {
        if (uobjPtr->IsPendingKill())
        {
            // Rename the other uobject so we can reuse this name
            sfUnrealUtils::Rename(uobjPtr, uobjPtr->GetName() + " (deleted)");
            uobjPtr = nullptr;
        }
        else if (sfObjectMap::Contains(uobjPtr) || (classPtr != nullptr && classPtr != uobjPtr->GetClass()))
        {
            // The uobject may already be in the map if we created a uobject with the same name at the same time as
            // another user. Rename the other uobject so we can reuse this name.
            sfUnrealUtils::Rename(uobjPtr, uobjPtr->GetName() + "_");
            uobjPtr = nullptr;
        }
    }

    EObjectFlags flags = DEFAULT_FLAGS;
    sfProperty::SPtr propPtr;
    if (propertiesPtr->TryGet(sfProp::Flags, propPtr))
    {
        flags = (EObjectFlags)(uint32_t)propPtr->AsValue()->GetValue();
    }

    if (uobjPtr == nullptr)
    {
        if (classPtr == nullptr)
        {
            return;
        }
        uobjPtr = NewObject<UObject>(outerPtr, classPtr, *name, flags);

        UBrushBuilder* builderPtr = Cast<UBrushBuilder>(uobjPtr);
        if (builderPtr != nullptr)
        {
            ABrush* brushPtr = Cast<ABrush>(outerPtr);
            if (brushPtr != nullptr)
            {
                builderPtr->Build(brushPtr->GetWorld(), brushPtr);
            }
            else
            {
                builderPtr->Build(GEditor->GetEditorWorldContext().World());
            }
        }
    }
    else
    {
        uobjPtr->ClearFlags(RF_AllFlags);
        uobjPtr->SetFlags(flags);
    }
    UsfReferenceTracker::Get().Track(uobjPtr);

    sfObjectMap::Add(objPtr, uobjPtr);
    sfPropertyManager::Get().ApplyProperties(uobjPtr, propertiesPtr);

    // Set references to this uobject
    std::vector<sfReferenceProperty::SPtr> references = m_sessionPtr->GetReferences(objPtr);
    sfPropertyManager::Get().SetReferences(uobjPtr, references);

    // Initialize children
    for (sfObject::SPtr childPtr : objPtr->Children())
    {
        if (childPtr->Type() == sfType::UObject)
        {
            InitializeUObject(uobjPtr, childPtr);
        }
        else
        {
            KS::Log::Error("UObject has unexpected child type " + *childPtr->Type(), LOG_CHANNEL);
        }
    }
}

void sfUObjectTranslator::OnConfirmDelete(sfObject::SPtr objPtr)
{
    // Clear the properties on the object, but it keep it around so it will be reused to preserve ids if the uobject is
    // recreated.
    objPtr->SetProperty(sfDictionaryProperty::Create());
}

bool sfUObjectTranslator::OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    if (objPtr == nullptr)
    {
        return false;
    }
    if (!objPtr->IsSyncing() || uobjPtr->IsPendingKill())
    {
        return true;
    }
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    if (objPtr->IsLocked())
    {
        sfPropertyManager::Get().ApplyProperties(uobjPtr, propertiesPtr);
    }
    else
    {
        sfPropertyManager::Get().SendPropertyChanges(uobjPtr, propertiesPtr);
    }
    return true;
}

#undef LOG_CHANNEL
#undef DEFAULT_FLAGS