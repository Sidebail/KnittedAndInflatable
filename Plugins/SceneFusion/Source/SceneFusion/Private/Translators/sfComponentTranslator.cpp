#include "../../Public/Translators/sfComponentTranslator.h"
#include "sfUObjectTranslator.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/sfPropertyUtil.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/Consts.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/Components/sfLockComponent.h"
#include "../../Public/Components/sfMissingComponent.h"
#include "../../Public/Components/sfMissingSceneComponent.h"
#include "../../Public/Actors/sfMissingActor.h"
#include "../../Public/sfActorUtil.h"
#include "../../Public/sfUnrealUtils.h"
#include "../../Public/sfConfig.h"
#include "../../Public/sfSelectionManager.h"
#include <sfDictionaryProperty.h>
#include <Editor.h>
#include <LandscapeHeightfieldCollisionComponent.h>
#include <Components/SplineMeshComponent.h>
#include <Components/SkeletalMeshComponent.h>
#include <LandscapeSplinesComponent.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <LandscapeProxy.h>

#define LOG_CHANNEL "sfComponentTranslator"
#define DEFAULT_FLAGS (RF_Transactional | RF_DefaultSubObject | RF_WasLoaded | RF_LoadCompleted)

using namespace KS::SceneFusion2;

sfComponentTranslator::sfComponentTranslator()
{
    RegisterPropertyChangeHandlers();
}

sfComponentTranslator::~sfComponentTranslator()
{

}

void sfComponentTranslator::Initialize()
{
    m_sessionPtr = SceneFusion::Service->Session();
    m_actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(sfType::Actor);
    m_tickHandle = SceneFusion::Get().OnTick.AddRaw(this, &sfComponentTranslator::Tick);
    m_onDeselectHandle = sfSelectionManager::Get().OnDeselect.AddRaw(this, &sfComponentTranslator::SyncComponents);
    m_onMoveHandle = sfSelectionManager::Get().OnMove.AddRaw(this, &sfComponentTranslator::SyncComponentTransforms);
    m_onApplyObjectToActorHandle
        = FEditorDelegates::OnApplyObjectToActor.AddRaw(this, &sfComponentTranslator::OnApplyObjectToActor);
}

void sfComponentTranslator::CleanUp()
{
    SceneFusion::Get().OnTick.Remove(m_tickHandle);
    sfSelectionManager::Get().OnDeselect.Remove(m_onDeselectHandle);
    sfSelectionManager::Get().OnMove.Remove(m_onMoveHandle);
    FEditorDelegates::OnApplyObjectToActor.Remove(m_onApplyObjectToActorHandle);
    m_syncComponentsList.Empty();
    m_transformChangedSet.Empty();
}

void sfComponentTranslator::Tick(float deltaTime)
{
    for (AActor* actorPtr : sfSelectionManager::Get().SelectedActors())
    {
        SyncComponents(actorPtr);
    }
    if (m_syncComponentsList.Num() > 0)
    {
        TArray<AActor*> actors = std::move(m_syncComponentsList.Array());
        m_syncComponentsList.Empty();
        for (AActor* actorPtr : actors)
        {
            SyncComponents(actorPtr);
        }
    }
    if (m_transformChangedSet.Num() > 0)
    {
        for (USceneComponent* componentPtr : m_transformChangedSet)
        {
            sfObject::SPtr objPtr = sfObjectMap::GetSFObject(componentPtr);
            if (objPtr == nullptr)
            {
                continue;
            }
            UClass* classPtr = componentPtr->GetClass();
            while (classPtr != nullptr)
            {
                auto iter = m_transformChangeHandlers.find(classPtr);
                if (iter != m_transformChangeHandlers.end())
                {
                    iter->second(objPtr, componentPtr);
                }
                classPtr = classPtr->GetSuperClass();
            }
        }
        m_transformChangedSet.Empty();
    }
}

bool sfComponentTranslator::IsSyncable(UActorComponent* componentPtr, bool allowPendingKill)
{
    if (componentPtr == nullptr || (componentPtr->IsPendingKill() && !allowPendingKill) ||
        componentPtr->HasAnyFlags(RF_Transient) || 
        // Components whose outer don't match the owner are not editable so we don't sync them
        componentPtr->GetOuter() != componentPtr->GetOwner() ||
        (componentPtr->CreationMethod != EComponentCreationMethod::Instance && componentPtr->IsEditorOnly()) ||
        // Landscape collision components are generated automatically and trying to sync them can cause crashes when
        // creating the landscape
        componentPtr->IsA<ULandscapeHeightfieldCollisionComponent>() ||
        // Spline mesh components are generated automatically and trying to sync them can cause weird behaviour
        componentPtr->IsA<USplineMeshComponent>() ||
        // Don't sync landscape components or splines if landscape syncing is disabled
        (!sfConfig::Get().SyncLandscape &&
        (componentPtr->IsA<ULandscapeSplinesComponent>() || componentPtr->IsA<ULandscapeComponent>())))
    {
        return false;
    }
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 21
    if (componentPtr->IsVisualizationComponent())
    {
        return false;
    }
#endif
    return true;
}

void sfComponentTranslator::SyncComponents(AActor* actorPtr)
{
    sfObject::SPtr actorObjPtr = sfObjectMap::GetSFObject(actorPtr);
    if (actorObjPtr == nullptr)
    {
        return;
    }
    if (actorObjPtr->IsLocked())
    {
        RestoreDeletedComponents(actorObjPtr);
    }
    TSet<UActorComponent*> components = actorPtr->GetComponents();
    for (UActorComponent* componentPtr : components)
    {
        if (!IsSyncable(componentPtr))
        {
            continue;
        }
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(componentPtr);
        // Check if the component is new
        if (objPtr == nullptr || !objPtr->IsSyncing())
        {
            if (actorObjPtr->IsLocked())
            {
                SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
                if (actorPtr->GetRootComponent() == componentPtr)
                {
                    actorPtr->SetRootComponent(nullptr);
                }
                CallDeleteHandler(nullptr, componentPtr);
                componentPtr->DestroyComponent();
                sfActorUtil::Reselect(actorPtr);
                SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
            }
            else
            {
                Upload(componentPtr);
            }
            continue;
        }

        // Check for a parent change
        SyncParent(actorPtr, componentPtr, objPtr);

        // Check for a name change
        sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
        FString name = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
        if (componentPtr->GetName() != name)
        {
            if (objPtr->IsLocked())
            {
                sfUnrealUtils::TryRename(componentPtr, name);
                sfActorUtil::Reselect(componentPtr->GetOwner());
            }
            else
            {
                propertiesPtr->Set(sfProp::Name, sfPropertyUtil::FromString(componentPtr->GetName()));
            }
        }
    }
    if (!actorObjPtr->IsLocked())
    {
        FindDeletedComponents(actorObjPtr);
    }
}

void sfComponentTranslator::RestoreDeletedComponents(sfObject::SPtr objPtr)
{
    for (sfObject::SPtr childPtr : objPtr->Children())
    {
        if (childPtr->Type() != sfType::Component)
        {
            continue;
        }
        UActorComponent* componentPtr = sfObjectMap::Get<UActorComponent>(childPtr);
        if (componentPtr != nullptr && componentPtr->IsPendingKill())
        {
            sfObjectMap::Remove(componentPtr);
            SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
            OnCreate(childPtr, 0);
            SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
        }
        RestoreDeletedComponents(childPtr);
    }
}

void sfComponentTranslator::FindDeletedComponents(sfObject::SPtr objPtr)
{
    TSharedPtr<sfUObjectTranslator> uobjectTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfUObjectTranslator>(sfType::UObject);
    for (int i = objPtr->Children().size() - 1; i >= 0; i--)
    {
        sfObject::SPtr childPtr = objPtr->Child(i);
        if (childPtr->Type() != sfType::Component || SceneFusion::ObjectEventDispatcher->IsCreateQueued(childPtr))
        {
            continue;
        }
        UActorComponent* componentPtr = sfObjectMap::Get<UActorComponent>(childPtr);
        if (componentPtr == nullptr || componentPtr->IsPendingKill() || componentPtr->GetOwner() == nullptr)
        {
            if (componentPtr != nullptr && !componentPtr->IsPendingKill())
            {
                // Unreal has a bug where if you duplicate a component of a blueprint and undo, the component isn't
                // destroyed properly but has it's owner set to nullptr, so we destroy it.
                componentPtr->DestroyComponent();
            }
            sfObjectMap::Remove(componentPtr);
            // Component children are already reparented, but actor children still need to be reparented.
            for (int j = childPtr->Children().size() - 1; j >= 0; j--)
            {
                sfObject::SPtr grandChildPtr = childPtr->Child(j);
                if (grandChildPtr->Type() == sfType::Actor)
                {
                    AActor* actorPtr = sfObjectMap::Get<AActor>(grandChildPtr);
                    if (actorPtr != nullptr && actorPtr->GetRootComponent() != nullptr)
                    {
                        sfObject::SPtr parentPtr = sfObjectMap::GetSFObject(
                            actorPtr->GetRootComponent()->GetAttachParent());
                        if (parentPtr == nullptr)
                        {
                            parentPtr = sfObjectMap::GetSFObject(actorPtr->GetLevel());
                        }
                        if (parentPtr != nullptr)
                        {
                            parentPtr->AddChild(grandChildPtr);
                            SyncTransform(actorPtr->GetRootComponent());
                        }
                    }
                }
                else if (grandChildPtr->Type() == sfType::UObject && uobjectTranslatorPtr.IsValid())
                {
                    uobjectTranslatorPtr->AddPendingDeletion(grandChildPtr);
                }
            }
            if (componentPtr != nullptr)
            {
                CallDestroyHandler(childPtr, componentPtr);
            }
            m_sessionPtr->Delete(childPtr);
        }
        FindDeletedComponents(childPtr);
    }
}

void sfComponentTranslator::CallDeleteHandler(sfObject::SPtr objPtr, UActorComponent* componentPtr)
{
    UClass* classPtr = componentPtr->GetClass();
    while (classPtr != nullptr)
    {
        auto iter = m_deleteHandlers.find(classPtr);
        if (iter != m_deleteHandlers.end())
        {
            iter->second(objPtr, componentPtr);
        }
        classPtr = classPtr->GetSuperClass();
    }
}

void sfComponentTranslator::CallDestroyHandler(sfObject::SPtr objPtr, UActorComponent* componentPtr)
{
    UClass* classPtr = componentPtr->GetClass();
    while (classPtr != nullptr)
    {
        auto iter = m_destroyHandlers.find(classPtr);
        if (iter != m_destroyHandlers.end())
        {
            iter->second(objPtr, componentPtr);
        }
        classPtr = classPtr->GetSuperClass();
    }
}

void sfComponentTranslator::SyncParent(AActor* actorPtr, UActorComponent* componentPtr, sfObject::SPtr objPtr)
{
    if (actorPtr == nullptr || objPtr == nullptr)
    {
        return;
    }
    USceneComponent* sceneComponentPtr = Cast<USceneComponent>(componentPtr);
    if (sceneComponentPtr == nullptr)
    {
        return;
    }
    UObject* uparentPtr = sceneComponentPtr == actorPtr->GetRootComponent() ?
        actorPtr : (UObject*)sceneComponentPtr->GetAttachParent();
    if (uparentPtr == nullptr)
    {
        uparentPtr = actorPtr;
    }
    sfObject::SPtr parentPtr = sfObjectMap::GetSFObject(uparentPtr);
    if (parentPtr == nullptr || !parentPtr->IsSyncing())
    {
        return;
    }
    if (objPtr->Parent() != parentPtr)
    {
        if (objPtr->IsLocked() || parentPtr->IsFullyLocked())
        {
            OnParentChange(objPtr, 0);
        }
        else
        {
            if (actorPtr->GetRootComponent() == componentPtr)
            {
                objPtr->Property()->AsDict()->Set(sfProp::IsRoot, sfValueProperty::Create(true));
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 22
                // Unreal has a bug where if you change the root component of a child actor, the actor is no longer
                // attached to the parent but it appears to be in the World Outliner, so we attach it back.
                USceneComponent* parentCompPtr = sfObjectMap::Get<USceneComponent>(parentPtr->Parent());
                if (parentCompPtr != nullptr)
                {
                    m_actorTranslatorPtr->DisableParentChangeHandler();
                    sceneComponentPtr->AttachToComponent(parentCompPtr, FAttachmentTransformRules::KeepWorldTransform);
                    m_actorTranslatorPtr->EnableParentChangeHandler();
                }
#endif
            }
            else if (objPtr->Parent()->Type() == sfType::Actor)
            {
                // This component is no longer the root
                objPtr->Property()->AsDict()->Remove(sfProp::IsRoot);
            }
            sfObject::SPtr currentPtr = parentPtr;
            while (parentPtr->IsDescendantOf(objPtr) && currentPtr != objPtr)
            {
                // Adding the child now creates a cyclic loop, so we sync the parent for parentPtr and its ancestors
                // until the loop is broken.
                UActorComponent* childPtr = sfObjectMap::Get<UActorComponent>(currentPtr);
                if (childPtr != nullptr)
                {
                    SyncParent(actorPtr, childPtr, currentPtr);
                }
                currentPtr = currentPtr->Parent();
            }
            parentPtr->AddChild(objPtr);
        }
        SyncTransform(sceneComponentPtr, parentPtr->IsFullyLocked());
    }
    else
    {
        sfProperty::SPtr propPtr;
        bool wasRoot = parentPtr->Type() == sfType::Actor &&
            objPtr->Property()->AsDict()->TryGet(sfProp::IsRoot, propPtr) && (bool)propPtr->AsValue()->GetValue();
        if (wasRoot != (componentPtr == actorPtr->GetRootComponent()))
        {
            if (wasRoot)
            {
                // This component is no longer the root
                objPtr->Property()->AsDict()->Remove(sfProp::IsRoot);
            }
            else
            {
                // This component became the root
                objPtr->Property()->AsDict()->Set(sfProp::IsRoot, sfValueProperty::Create(true));
            }
        }
    }
}

void sfComponentTranslator::Upload(UActorComponent* componentPtr)
{
    AActor* actorPtr = componentPtr->GetOwner();
    if (actorPtr == nullptr)
    {
        return;
    }

    UObject* uparentPtr = actorPtr;
    if (componentPtr != actorPtr->GetRootComponent())
    {
        USceneComponent* sceneComponentPtr = Cast<USceneComponent>(componentPtr);
        if (sceneComponentPtr != nullptr && sceneComponentPtr->GetAttachParent() != nullptr)
        {
            uparentPtr = sceneComponentPtr->GetAttachParent();
        }
    }
    sfObject::SPtr parentPtr = sfObjectMap::GetSFObject(uparentPtr);
    if (parentPtr == nullptr || !parentPtr->IsSyncing())
    {
        return;
    }

    sfObject::SPtr objPtr = CreateObject(componentPtr);
    if (objPtr == nullptr)
    {
        return;
    }
    if (componentPtr == actorPtr->GetRootComponent())
    {
        objPtr->Property()->AsDict()->Set(sfProp::IsRoot, sfValueProperty::Create(true));
        // Unreal has a bug where if you change the root component of a child actor, the actor is no longer
        // attached to the parent but it appears to be in the World Outliner, so we attach it back.
        USceneComponent* parentCompPtr = sfObjectMap::Get<USceneComponent>(parentPtr->Parent());
        if (parentCompPtr != nullptr)
        {
            USceneComponent* sceneComponentPtr = Cast<USceneComponent>(componentPtr);
            m_actorTranslatorPtr->DisableParentChangeHandler();
            sceneComponentPtr->AttachToComponent(parentCompPtr, FAttachmentTransformRules::KeepWorldTransform);
            m_actorTranslatorPtr->EnableParentChangeHandler();
            SyncTransform(sceneComponentPtr);
        }
    }
    m_sessionPtr->Create(objPtr, parentPtr, 0);
    // Pre-existing child objects can only be attached after calling Create.
    FindAndAttachChildren(objPtr);
}

void sfComponentTranslator::FindAndAttachChildren(sfObject::SPtr objPtr)
{
    auto iter = objPtr->SelfAndDescendants();
    while (iter.Value() != nullptr)
    {
        sfObject::SPtr currentPtr = iter.Value();
        iter.Next();
        USceneComponent* componentPtr = sfObjectMap::Get<USceneComponent>(currentPtr);
        if (componentPtr != nullptr)
        {
            for (USceneComponent* childPtr : componentPtr->GetAttachChildren())
            {
                sfObject::SPtr childObjPtr = sfObjectMap::GetSFObject(childPtr);
                if (childObjPtr == nullptr)
                {
                    continue;
                }
                if (childObjPtr->Parent() != nullptr && childObjPtr->Parent()->Type() == sfType::Actor)
                {
                    childObjPtr = childObjPtr->Parent();
                }
                if (childObjPtr->Parent() != currentPtr)
                {
                    currentPtr->AddChild(childObjPtr);
                    SyncTransform(childPtr);

                    sfDictionaryProperty::SPtr propertiesPtr = childObjPtr->Property()->AsDict();
                    sfProperty::SPtr propPtr;
                    if (propertiesPtr->TryGet(sfProp::IsRoot, propPtr) && (bool)propPtr->AsValue()->GetValue())
                    {
                        propPtr->AsValue()->SetValue(false);
                    }
                }
            }
        }
    }
}

bool sfComponentTranslator::Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr)
{
    UActorComponent* componentPtr = Cast<UActorComponent>(uobjPtr);
    if (componentPtr == nullptr)
    {
        return false;
    }
    if (IsSyncable(componentPtr, true))
    {
        outObjPtr = sfObject::Create(sfType::Component, sfDictionaryProperty::Create());
        sfObjectMap::Add(outObjPtr, uobjPtr);
    }
    return true;
}

sfObject::SPtr sfComponentTranslator::CreateObject(UActorComponent* componentPtr)
{
    sfObject::SPtr objPtr = sfObjectMap::GetOrCreateSFObject(componentPtr, sfType::Component);
    if (objPtr->IsSyncing())
    {
        // This happens when a new component or actor has a child component that already exists. We can't add this
        // child as part of the create request, so we sync the actor's components in the next Tick.
        m_syncComponentsList.Add(componentPtr->GetOwner());
        return nullptr;
    }
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();

    FString className;
    IsfMissingObject* missingComponentPtr = Cast<IsfMissingObject>(componentPtr);
    if (missingComponentPtr != nullptr)
    {
        // This is a stand-in for a missing component class.
        SceneFusion::MissingObjectManager->AddStandIn(missingComponentPtr);
        className = missingComponentPtr->MissingClass();
    }
    else
    {
        className = sfUnrealUtils::ClassToFString(componentPtr->GetClass());
    }

    propertiesPtr->Set(sfProp::Name, sfPropertyUtil::FromString(componentPtr->GetName()));
    propertiesPtr->Set(sfProp::Class, sfPropertyUtil::FromString(className));

    EComponentCreationMethod creationMethod = componentPtr->CreationMethod;
    // Check if the component should use creation method SimpleConstructionScript but we couldn't set it to that
    // doing so on a non-blueprint stand-in would delete the component.
    AsfMissingActor* missingActorPtr = Cast<AsfMissingActor>(componentPtr->GetOwner());
    if (missingActorPtr != nullptr && missingActorPtr->SimpleConstructionComponents.Contains(componentPtr))
    {
        creationMethod = EComponentCreationMethod::SimpleConstructionScript;
    }
    propertiesPtr->Set(sfProp::CreationMethod, sfValueProperty::Create((uint8_t)creationMethod));

    EObjectFlags flags = componentPtr->GetFlags();
    if (flags != DEFAULT_FLAGS)
    {
        propertiesPtr->Set(sfProp::Flags, sfValueProperty::Create((uint32_t)flags));
    }

    InitializeChildren(objPtr);
    sfPropertyManager::Get().CreateProperties(componentPtr, propertiesPtr);

    UClass* classPtr = componentPtr->GetClass();
    while (classPtr != nullptr)
    {
        auto iter = m_objectInitializers.find(classPtr);
        if (iter != m_objectInitializers.end())
        {
            iter->second(objPtr, componentPtr);
        }
        classPtr = classPtr->GetSuperClass();
    }

    USceneComponent* sceneComponentPtr = Cast<USceneComponent>(componentPtr);
    if (sceneComponentPtr == nullptr)
    {
        return objPtr;
    }
    if (sceneComponentPtr->bVisualizeComponent)
    {
        propertiesPtr->Set(sfProp::Visualize, sfValueProperty::Create(true));
    }
    for (USceneComponent* childComponentPtr : sceneComponentPtr->GetAttachChildren())
    {
        if (!IsSyncable(childComponentPtr))
        {
            continue;
        }
        sfObject::SPtr childPtr;
        if (childComponentPtr->GetOwner() == componentPtr->GetOwner())
        {
            // Child is part of the same actor
            childPtr = CreateObject(childComponentPtr);
        }
        else
        {
            // Child is the root component of a different actor
            AActor* actorPtr = childComponentPtr->GetOwner();
            // Actor may be pending kill even when its component isn't because of undo
            if (m_actorTranslatorPtr->IsSyncable(actorPtr))
            {
                childPtr = m_actorTranslatorPtr->CreateObject(actorPtr);
            }
        }
        if (childPtr != nullptr)
        {
            objPtr->AddChild(childPtr);
        }
    }
    return objPtr;
}

void sfComponentTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    sfObject::SPtr actorObjPtr = objPtr->Parent();
    while (actorObjPtr != nullptr && actorObjPtr->Type() != sfType::Actor)
    {
        actorObjPtr = actorObjPtr->Parent();
    }
    if (actorObjPtr == nullptr)
    {
        KS::Log::Warning("Component object cannot be created without an actor ancestor.", LOG_CHANNEL);
        return;
    }
    AActor* actorPtr = sfObjectMap::Get<AActor>(actorObjPtr);
    if (actorPtr == nullptr)
    {
        return;
    }
    if (actorPtr->IsPendingKill())
    {
        m_actorTranslatorPtr->RecreateActor(actorObjPtr);
        return;
    }
    USceneComponent* componentPtr = Cast<USceneComponent>(InitializeComponent(actorPtr, objPtr));
    if (componentPtr == nullptr)
    {
        sfActorUtil::Reselect(actorPtr);
        return;
    }
    SceneFusion::RedrawActiveViewport();
}

UActorComponent* sfComponentTranslator::InitializeComponent(AActor* actorPtr, sfObject::SPtr objPtr)
{
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    FString className = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Class));
    UClass* classPtr = sfUnrealUtils::LoadClass(className);
    FString name = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
    UActorComponent* componentPtr =
        Cast<UActorComponent>(StaticFindObjectFast(UActorComponent::StaticClass(), actorPtr, FName(*name)));
    bool isRegistered = false;
    if (componentPtr != nullptr)
    {
        if (componentPtr->IsPendingKill())
        {
            // Rename the other component so we can reuse this name
            sfUnrealUtils::Rename(componentPtr, componentPtr->GetName() + " (deleted)");
            componentPtr = nullptr;
        }
        else if (sfObjectMap::Contains(componentPtr) || (classPtr != nullptr && classPtr != componentPtr->GetClass()))
        {
            // The component may already be in the map if we created a component with the same name at the same time as
            // another user. Rename the other component so we can reuse this name.
            sfUnrealUtils::Rename(componentPtr, componentPtr->GetName() + "_");
            componentPtr = nullptr;
        }
    }

    EObjectFlags flags = DEFAULT_FLAGS;
    sfProperty::SPtr propPtr;
    if (propertiesPtr->TryGet(sfProp::Flags, propPtr))
    {
        flags = (EObjectFlags)(uint32_t)propPtr->AsValue()->GetValue();
    }

    if (componentPtr == nullptr)
    {
        // If this is not the root and the create time is exceeded, queue it to be created later
        if (SceneFusion::CreateTimeExceeded() && 
            (!propertiesPtr->TryGet(sfProp::IsRoot, propPtr) || !(bool)propPtr->AsValue()->GetValue()))
        {
            SceneFusion::ObjectEventDispatcher->QueueCreate(objPtr);
            return nullptr;
        }

        bool isMissingClass = classPtr == nullptr;
        if (isMissingClass)
        {
            if ((objPtr->Parent() != nullptr && objPtr->Parent()->Type() == sfType::Component) ||
                (propertiesPtr->TryGet(sfProp::IsRoot, propPtr) && (bool)propPtr->AsValue()->GetValue()))
            {
                classPtr = UsfMissingSceneComponent::StaticClass();
            }
            else
            {
                classPtr = UsfMissingComponent::StaticClass();
            }
        }
        componentPtr = NewObject<UActorComponent>(actorPtr, classPtr, FName(*name), flags);
        if (isMissingClass)
        {
            IsfMissingObject* missingComponentPtr = Cast<IsfMissingObject>(componentPtr);
            missingComponentPtr->MissingClass() = className;
            SceneFusion::MissingObjectManager->AddStandIn(missingComponentPtr);
        }
    }
    else
    {
        isRegistered = true;
        componentPtr->ClearFlags(RF_AllFlags);
        componentPtr->SetFlags(flags);
    }

    sfObjectMap::Add(objPtr, componentPtr);
    EComponentCreationMethod creationMethod = (EComponentCreationMethod)propertiesPtr->Get(sfProp::CreationMethod)
        ->AsValue()->GetValue().GetByte();
    if (creationMethod != EComponentCreationMethod::SimpleConstructionScript || !actorPtr->IsA<AsfMissingActor>())
    {
        componentPtr->CreationMethod = creationMethod;
    }
    else
    {
        // Setting the creation method to SimpleConstructionScript on a non-blueprint stand-in will delete the
        // component, so we store the components that should have that creation method and instead assign them the
        // default creation method.
        Cast<AsfMissingActor>(actorPtr)->SimpleConstructionComponents.Add(componentPtr);
    }

    sfPropertyManager::Get().ApplyProperties(componentPtr, propertiesPtr);

    // Set references to this component
    std::vector<sfReferenceProperty::SPtr> references = m_sessionPtr->GetReferences(objPtr);
    sfPropertyManager::Get().SetReferences(componentPtr, references);

    if (!isRegistered)
    {
        classPtr = componentPtr->GetClass();
        while (classPtr != nullptr)
        {
            auto iter = m_componentInitializers.find(classPtr);
            if (iter != m_componentInitializers.end())
            {
                iter->second(objPtr, componentPtr);
            }
            classPtr = classPtr->GetSuperClass();
        }
    }

    // Initialize children
    USceneComponent* sceneComponentPtr = Cast<USceneComponent>(componentPtr);
    for (sfObject::SPtr childPtr : objPtr->Children())
    {
        if (childPtr->Type() == sfType::Component)
        {
            if (sceneComponentPtr == nullptr)
            {
                FString str = "Ignoring child component on " + componentPtr->GetClass()->GetName() + " " +
                    componentPtr->GetName() + " because it is not a scene component and cannot have children.";
                KS::Log::Warning(TCHAR_TO_UTF8(*str), LOG_CHANNEL);
                continue;
            }
            USceneComponent* childComponentPtr = sfObjectMap::Get<USceneComponent>(childPtr);
            if (childComponentPtr != nullptr)
            {
                if (childComponentPtr->GetAttachParent() != sceneComponentPtr)
                {
                    SyncTransform(childComponentPtr, true);
                    if (sceneComponentPtr->IsAttachedTo(childComponentPtr))
                    {
                        // Detach from parent to avoid loops when we try to attach its child
                        m_actorTranslatorPtr->DisableParentChangeHandler();
                        sceneComponentPtr->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
                        m_actorTranslatorPtr->EnableParentChangeHandler();
                    }
                    OnParentChange(childPtr, 0);
                }
            }
            else
            {
                childComponentPtr = Cast<USceneComponent>(InitializeComponent(actorPtr, childPtr));
                if (childComponentPtr != nullptr)
                {
                    childComponentPtr->AttachToComponent(
                        sceneComponentPtr,
                        FAttachmentTransformRules::KeepRelativeTransform);
                }
            }
        }
        else if (childPtr->Type() == sfType::Actor)
        {
            if (sceneComponentPtr == nullptr)
            {
                FString str = "Ignoring child actor on " + componentPtr->GetClass()->GetName() + " " +
                    componentPtr->GetName() + " because it is not a scene component and cannot have children.";
                KS::Log::Warning(TCHAR_TO_UTF8(*str), LOG_CHANNEL);
                continue;
            }
            AActor* childActorPtr = sfObjectMap::Get<AActor>(childPtr);
            if (childActorPtr != nullptr)
            {
                SyncTransform(childActorPtr->GetRootComponent(), true);
            }
            else
            {
                childActorPtr = m_actorTranslatorPtr->InitializeActor(childPtr, actorPtr->GetLevel());
            }
            if (childActorPtr != nullptr && childActorPtr->GetRootComponent() != nullptr)
            {
                m_actorTranslatorPtr->DisableParentChangeHandler();
                if (sceneComponentPtr->IsAttachedTo(childActorPtr->GetRootComponent()))
                {
                    // Detach from parent to avoid loops when we try to attach its child
                    sceneComponentPtr->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
                }
                if (childActorPtr->GetRootComponent()->GetAttachParent() != sceneComponentPtr)
                {
                    childActorPtr->AttachToComponent(
                        sceneComponentPtr,
                        FAttachmentTransformRules::KeepRelativeTransform);
                }
                m_actorTranslatorPtr->EnableParentChangeHandler();
            }
        }
        else
        {
            SceneFusion::ObjectEventDispatcher->OnCreate(childPtr, 0);
        }
    }

    if (sceneComponentPtr != nullptr)
    {
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 24
        USkinnedMeshComponent* skinnedMeshComponentPtr = Cast<USkinnedMeshComponent>(sceneComponentPtr);
        if (skinnedMeshComponentPtr != nullptr && skinnedMeshComponentPtr->SkeletalMesh != nullptr)
        {
            skinnedMeshComponentPtr->SkeletalMesh->Build();
        }
#endif
        // If the component is a mesh or the root component and is locked, add a lock component.
        if (objPtr->IsLocked())
        {
            bool isMesh = sceneComponentPtr->IsA<UMeshComponent>() && !sceneComponentPtr->IsA<USplineMeshComponent>();
            if (isMesh || (propertiesPtr->TryGet(sfProp::IsRoot, propPtr) && (bool)propPtr->AsValue()->GetValue() &&
                objPtr->Parent() != nullptr && objPtr->Parent()->Type() == sfType::Actor))
            {
                UMaterialInterface* lockMaterialPtr = SceneFusion::GetLockMaterial(objPtr->LockOwner());
                if (lockMaterialPtr != nullptr)
                {
                    UsfLockComponent* lockPtr = NewObject<UsfLockComponent>(actorPtr,
                        *FString("SFLock" + sceneComponentPtr->GetName()));
                    lockPtr->SetMobility(sceneComponentPtr->Mobility);
                    lockPtr->AttachToComponent(sceneComponentPtr, FAttachmentTransformRules::KeepRelativeTransform);
                    lockPtr->RegisterComponent();
                    lockPtr->InitializeComponent();
                    if (isMesh)
                    {
                        lockPtr->DuplicateParentMesh(lockMaterialPtr);
                    }
                    else if (actorPtr->IsA<ABrush>())
                    {
                        lockPtr->CreateOrFindModelMesh(lockMaterialPtr);
                    }
                    else if (actorPtr->IsA<ALandscapeProxy>() && objPtr->LockOwner() != nullptr)
                    {
                        lockPtr->SetLandscapeMaterial(SceneFusion::GetLandscapeLockMaterial(objPtr->LockOwner()));
                    }
                }
            }
        }

        sceneComponentPtr->bVisualizeComponent = propertiesPtr->TryGet(sfProp::Visualize, propPtr) &&
            (bool)propPtr->AsValue()->GetValue();

        sfObject::SPtr parentObjPtr = objPtr->Parent();
        USceneComponent* oldRootPtr = nullptr;
        if (parentObjPtr->Type() == sfType::Actor)
        {
            if (propertiesPtr->TryGet(sfProp::IsRoot, propPtr) && (bool)propPtr->AsValue()->GetValue())
            {
                if (actorPtr->GetRootComponent() != sceneComponentPtr)
                {
                    oldRootPtr = actorPtr->GetRootComponent();
                    actorPtr->SetRootComponent(sceneComponentPtr);
                    parentObjPtr = parentObjPtr->Parent();
                }
            }
            else
            {
                parentObjPtr = nullptr;
                if (actorPtr->GetRootComponent() == sceneComponentPtr)
                {
                    actorPtr->SetRootComponent(nullptr);
                }
            }
        }

        USceneComponent* parentPtr = sfObjectMap::Get<USceneComponent>(parentObjPtr);
        if (parentPtr != nullptr)
        {
            m_actorTranslatorPtr->DisableParentChangeHandler();
            sceneComponentPtr->AttachToComponent(parentPtr, FAttachmentTransformRules::KeepRelativeTransform);
            m_actorTranslatorPtr->EnableParentChangeHandler();
        }
        sfActorUtil::Reselect(actorPtr);
        if (oldRootPtr != nullptr && actorPtr->IsSelected())
        {
            // For some reason if the root component was replaced, the old root's transform is changed when we reselect
            // the actor, so we set it back.
            SyncTransform(oldRootPtr, true);
        }
    }

    SceneFusion::RedrawActiveViewport();

    if (!isRegistered)
    {
        componentPtr->RegisterComponent();
        componentPtr->InitializeComponent();
    }
    return componentPtr;
}

void sfComponentTranslator::OnDelete(sfObject::SPtr objPtr)
{
    UActorComponent* componentPtr = Cast<UActorComponent>(sfObjectMap::Remove(objPtr));
    if (componentPtr != nullptr)
    {
        CallDeleteHandler(objPtr, componentPtr);
        AActor* actorPtr = componentPtr->GetOwner();
        componentPtr->DestroyComponent();
        m_actorTranslatorPtr->CleanUpChildrenOfDeletedObject(objPtr, nullptr, true);
        SceneFusion::RedrawActiveViewport();
        sfActorUtil::Reselect(actorPtr);
    }
}

void sfComponentTranslator::RegisterPropertyChangeHandlers()
{
    m_propertyChangeHandlers[sfProp::Name] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        UActorComponent* componentPtr = Cast<UActorComponent>(uobjPtr);
        sfUnrealUtils::TryRename(componentPtr, sfPropertyUtil::ToString(propertyPtr));
        sfActorUtil::Reselect(componentPtr->GetOwner());
        return true;
    };
    m_propertyChangeHandlers[sfProp::IsRoot] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(uobjPtr);
        if (objPtr == nullptr)
        {
            return true;
        }
        sfObject::SPtr parentPtr = objPtr->Parent();
        if (parentPtr != nullptr && parentPtr->Type() == sfType::Actor)
        {
            OnParentChange(objPtr, 0);
        }
        return true;
    };
    RegisterPostPropertyChangeHandler<USceneComponent>(FName(sfProp::Location->c_str()), [this](UObject* uobjPtr)
    {
        USceneComponent* componentPtr = Cast<USceneComponent>(uobjPtr);
        componentPtr->UpdateComponentToWorld();
        m_transformChangedSet.Add(componentPtr);
        AActor* actorPtr = componentPtr->GetOwner();
        actorPtr->InvalidateLightingCache();
    });
    RegisterPostPropertyChangeHandler<USceneComponent>(FName(sfProp::Rotation->c_str()), [this](UObject* uobjPtr)
    {
        USceneComponent* componentPtr = Cast<USceneComponent>(uobjPtr);
        componentPtr->UpdateComponentToWorld();
        m_transformChangedSet.Add(componentPtr);
        AActor* actorPtr = componentPtr->GetOwner();
        actorPtr->InvalidateLightingCache();
    });
    RegisterPostPropertyChangeHandler<USceneComponent>(FName(sfProp::Scale->c_str()), [this](UObject* uobjPtr)
    {
        USceneComponent* componentPtr = Cast<USceneComponent>(uobjPtr);
        componentPtr->UpdateComponentToWorld();
        m_transformChangedSet.Add(componentPtr);
        AActor* actorPtr = componentPtr->GetOwner();
        actorPtr->InvalidateLightingCache();
    });
    RegisterPostPropertyChangeHandler<UInstancedStaticMeshComponent>("PerInstanceSMData", [this](UObject* uobjPtr)
    {
        UInstancedStaticMeshComponent* componentPtr = Cast<UInstancedStaticMeshComponent>(uobjPtr);
        componentPtr->InstanceUpdateCmdBuffer.NumEdits++;// Refreshes the render data
        SceneFusion::RedrawActiveViewport();
    });
}

bool sfComponentTranslator::OnUPropertyChange(sfObject::SPtr objPtr, UObject* uobjPtr, UnrealProperty* upropPtr)
{
    UMeshComponent* meshPtr = Cast<UMeshComponent>(uobjPtr);
    if (meshPtr != nullptr && upropPtr->GetName().Contains("mesh"))
    {
        sfPropertyManager::Get().SyncProperty(objPtr, uobjPtr, "OverrideMaterials");
    }
    else if (upropPtr->GetName() == "bAbsoluteLocation")
    {
        sfPropertyManager::Get().SyncProperty(objPtr, uobjPtr, FName(sfProp::Location->c_str()));
    }
    return false;
}

void sfComponentTranslator::OnApplyObjectToActor(UObject* uobjPtr, AActor* actorPtr)
{
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
    if (objPtr == nullptr || !objPtr->IsSyncing())
    {
        return;
    }
    
    TArray<UMeshComponent*> meshComponents;
    actorPtr->GetComponents<UMeshComponent>(meshComponents);
    for (UMeshComponent* meshComponentPtr : meshComponents)
    {
        if (!IsSyncable(meshComponentPtr))
        {
            UsfLockComponent* lockPtr = Cast<UsfLockComponent>(meshComponentPtr->GetAttachParent());
            if (lockPtr != nullptr)
            {
                // Set material back to lock material
                lockPtr->SetMaterial();
            }
            continue;
        }
        sfObject::SPtr componentObjPtr = sfObjectMap::GetSFObject(meshComponentPtr);
        // Check if the component is new
        if (componentObjPtr == nullptr || !componentObjPtr->IsSyncing())
        {
            continue;
        }
        // Check for material change
        sfPropertyManager::Get().SyncProperty(componentObjPtr, meshComponentPtr, "OverrideMaterials");
    }
}

void sfComponentTranslator::SyncTransform(USceneComponent* componentPtr, bool applyServerValues)
{
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(componentPtr);
    if (objPtr == nullptr)
    {
        return;
    }
    sfPropertyManager::Get().SyncProperty(objPtr, componentPtr, FName(sfProp::Location->c_str()), applyServerValues);
    sfPropertyManager::Get().SyncProperty(objPtr, componentPtr, FName(sfProp::Rotation->c_str()), applyServerValues);
    sfPropertyManager::Get().SyncProperty(objPtr, componentPtr, FName(sfProp::Scale->c_str()), applyServerValues);
}

void sfComponentTranslator::SyncComponentTransforms(AActor* actorPtr)
{
    TArray<USceneComponent*> sceneComponents;
    actorPtr->GetComponents(sceneComponents);
    for (USceneComponent* componentPtr : sceneComponents)
    {
        SyncTransform(componentPtr);
    }
}

void sfComponentTranslator::OnPropertyChange(sfProperty::SPtr propertyPtr)
{
    USceneComponent* sceneComponent = sfObjectMap::Get<USceneComponent>(propertyPtr->GetContainerObject());
    if (sceneComponent != nullptr)
    {
        SceneFusion::RedrawActiveViewport();
    }
    sfBaseUObjectTranslator::OnPropertyChange(propertyPtr);
}

void sfComponentTranslator::OnRemoveField(sfDictionaryProperty::SPtr dictPtr, const sfName& name)
{
    USceneComponent* sceneComponent = sfObjectMap::Get<USceneComponent>(dictPtr->GetContainerObject());
    if (sceneComponent != nullptr)
    {
        SceneFusion::RedrawActiveViewport();
    }
    sfBaseUObjectTranslator::OnRemoveField(dictPtr, name);
}

void sfComponentTranslator::OnParentChange(sfObject::SPtr objPtr, int childIndex)
{
    if (objPtr->Parent() == nullptr)
    {
        KS::Log::Warning("Component became a root object. Components should always have a component or actor parent.",
            LOG_CHANNEL);
        return;
    }
    USceneComponent* componentPtr = sfObjectMap::Get<USceneComponent>(objPtr);
    if (componentPtr == nullptr)
    {
        return;
    }
    AActor* oldActorPtr = componentPtr->GetOwner();
    AActor* newActorPtr = sfObjectMap::Get<AActor>(sfUnrealUtils::FindAncestorByType(objPtr, sfType::Actor));
    if (newActorPtr == nullptr)
    {
        return;
    }
    bool isRoot = false;
    USceneComponent* parentPtr;
    if (objPtr->Parent()->Type() == sfType::Actor)
    {
        sfProperty::SPtr propPtr;
        if (objPtr->Property()->AsDict()->TryGet(sfProp::IsRoot, propPtr) && (bool)propPtr->AsValue()->GetValue())
        {
            parentPtr = sfObjectMap::Get<USceneComponent>(objPtr->Parent()->Parent());
            isRoot = true;
        }
        else
        {
            parentPtr = nullptr;
        }
    }
    else
    {
        parentPtr = sfObjectMap::Get<USceneComponent>(objPtr->Parent());
        AActor* actorPtr = componentPtr->GetOwner();
        if (actorPtr != nullptr && actorPtr->GetRootComponent() == componentPtr)
        {
            actorPtr->SetRootComponent(nullptr);
        }
    }
    if (newActorPtr != oldActorPtr && !newActorPtr->IsPendingKill())
    {
        // Component was moved to another actor. Call PreOwnerChange handler.
        UClass* classPtr = componentPtr->GetClass();
        while (classPtr != nullptr)
        {
            auto iter = m_preOwnerChangeHandlers.find(classPtr);
            if (iter != m_preOwnerChangeHandlers.end())
            {
                iter->second(objPtr, componentPtr, newActorPtr);
            }
            classPtr = classPtr->GetSuperClass();
        }
        componentPtr->UnregisterComponent();
        componentPtr->Rename(nullptr, newActorPtr);// Set the outer to the new actor
    }
    if (parentPtr != nullptr)
    {
        if (parentPtr->IsPendingKill())
        {
            OnCreate(sfObjectMap::GetSFObject(parentPtr), 0);
        }
        else
        {
            m_actorTranslatorPtr->DisableParentChangeHandler();
            componentPtr->AttachToComponent(parentPtr, FAttachmentTransformRules::KeepRelativeTransform);
            m_actorTranslatorPtr->EnableParentChangeHandler();
            sfActorUtil::Reselect(componentPtr->GetOwner());
        }
    }
    else
    {
        m_actorTranslatorPtr->DisableParentChangeHandler();
        componentPtr->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
        m_actorTranslatorPtr->EnableParentChangeHandler();
    }
    if (isRoot)
    {
        componentPtr->GetOwner()->SetRootComponent(componentPtr);
    }
    if (newActorPtr != oldActorPtr && !newActorPtr->IsPendingKill())
    {
        // Component was moved to another actor. Call PostOwnerChange handler.
        UClass* classPtr = componentPtr->GetClass();
        while (classPtr != nullptr)
        {
            auto iter = m_postOwnerChangeHandlers.find(classPtr);
            if (iter != m_postOwnerChangeHandlers.end())
            {
                iter->second(objPtr, componentPtr, newActorPtr);
            }
            classPtr = classPtr->GetSuperClass();
        }
        newActorPtr->RegisterAllComponents();
        oldActorPtr->RegisterAllComponents();
    }
}

bool sfComponentTranslator::OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    UActorComponent* componentPtr = Cast<UActorComponent>(uobjPtr);
    if (componentPtr == nullptr)
    {
        return false;
    }
    if (componentPtr->IsPendingKill())
    {
        return true;
    }
    if (objPtr != nullptr)
    {
        sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
        if (objPtr->IsLocked())
        {
            sfPropertyManager::Get().ApplyProperties(componentPtr, propertiesPtr);
        }
        else
        {
            sfPropertyManager::Get().SendPropertyChanges(componentPtr, propertiesPtr);
        }
        
        AActor* actorPtr = componentPtr->GetOwner();
        if (actorPtr != nullptr && componentPtr == actorPtr->GetRootComponent())
        {
            m_actorTranslatorPtr->SyncParent(actorPtr, sfObjectMap::GetSFObject(actorPtr));
        }

        SyncParent(actorPtr, componentPtr, objPtr);

        componentPtr->MarkRenderStateDirty();
    }
    else if (!componentPtr->IsRenderStateCreated() && !componentPtr->IsTemplate())
    {
        // This component was deleted by another user and is in a bad state. Delete it.
        KS::Log::Debug("Destroying component with no render state", LOG_CHANNEL);
        componentPtr->DestroyComponent();
    }
    return true;
}

#undef DEFAULT_FLAGS
#undef LOG_CHANNEL