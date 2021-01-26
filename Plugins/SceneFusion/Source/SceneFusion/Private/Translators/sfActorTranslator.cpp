#include "../../Public/Translators/sfActorTranslator.h"
#include "../../Public/Translators/sfBlueprintTranslator.h"
#include "../../Public/Translators/sfLevelTranslator.h"
#include "../../Public/Translators/sfComponentTranslator.h"
#include "sfUObjectTranslator.h"
#include "../../Public/Components/sfLockComponent.h"
#include "../../Public/Actors/sfMissingActor.h"
#include "../../Public/sfPropertyUtil.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/sfSelectionManager.h"
#include "../../Public/sfActorUtil.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/sfUnrealUtils.h"
#include "../../Public/sfLoader.h"
#include "../UI/sfDetailsPanelManager.h"
#include "../../Public/sfConfig.h"

#include <Editor.h>
#include <EngineUtils.h>
#include <ActorEditorUtils.h>
#include <Engine/StaticMeshActor.h>
#include <Engine/Selection.h>
#include <Engine/LevelScriptActor.h>
#include <Engine/LevelScriptBlueprint.h>
#include <Materials/MaterialInstanceDynamic.h>
#include <Particles/Emitter.h>
#include <Particles/ParticleSystemComponent.h>
#include <EditorActorFolders.h>
#include <Animation/SkeletalMeshActor.h>
#include <Components/SkeletalMeshComponent.h>
#include <UObject/ObjectMacros.h>
#include <LevelEditor.h>
#include <LevelEditorViewport.h>
#include <UObject/GarbageCollection.h>
#include <Landscape.h>
#include <Engine/Polys.h>
#include <Model.h>
#include <LandscapeGizmoActiveActor.h>
#include <Components/SplineMeshComponent.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <LandscapeStreamingProxy.h>
#include <Misc/MessageDialog.h>

#define LOG_CHANNEL "sfActorTranslator"

sfActorTranslator::sfActorTranslator()
{
    RegisterPropertyChangeHandlers();
}

sfActorTranslator::~sfActorTranslator()
{
    
}

void sfActorTranslator::Initialize()
{
    m_sessionPtr = SceneFusion::Service->Session();
    m_tickHandle = SceneFusion::Get().OnTick.AddRaw(this, &sfActorTranslator::Tick);
    m_onActorAddedHandle = GEngine->OnLevelActorAdded().AddRaw(this, &sfActorTranslator::OnActorAdded);
    m_onActorDeletedHandle = GEngine->OnLevelActorDeleted().AddRaw(this, &sfActorTranslator::OnActorDeleted);
    m_onActorAttachedHandle = GEngine->OnLevelActorAttached().AddRaw(this, &sfActorTranslator::OnAttachDetach);
    m_onActorDetachedHandle = GEngine->OnLevelActorDetached().AddRaw(this, &sfActorTranslator::OnAttachDetach);
    m_onFolderChangeHandle = GEngine->OnLevelActorFolderChanged().AddRaw(this, &sfActorTranslator::OnFolderChange);
    m_onLabelChangeHandle = FCoreDelegates::OnActorLabelChanged.AddRaw(this, &sfActorTranslator::OnLabelChanged);
    m_onSelectHandle = sfSelectionManager::Get().OnSelect.AddRaw(this, &sfActorTranslator::OnSelect);
    m_onDeselectHandle = sfSelectionManager::Get().OnDeselect.AddRaw(this, &sfActorTranslator::OnDeselect);
    m_collectGarbage = false;
    m_reachedActorLimit = false;
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 23
    m_worldLayersPtr = GEditor->Layers;
#else
    m_worldLayersPtr = GEditor->GetEditorSubsystem<ULayersSubsystem>();
#endif

    KS::Log::Info("actor limit: " + std::to_string(m_sessionPtr->GetObjectLimit(sfType::Actor)));
}

void sfActorTranslator::CleanUp()
{
    SceneFusion::Get().OnTick.Remove(m_tickHandle);
    GEngine->OnLevelActorAdded().Remove(m_onActorAddedHandle);
    GEngine->OnLevelActorDeleted().Remove(m_onActorDeletedHandle);
    GEngine->OnLevelActorAttached().Remove(m_onActorAttachedHandle);
    GEngine->OnLevelActorDetached().Remove(m_onActorDetachedHandle);
    GEngine->OnLevelActorFolderChanged().Remove(m_onFolderChangeHandle);
    FCoreDelegates::OnActorLabelChanged.Remove(m_onLabelChangeHandle);
    sfSelectionManager::Get().OnSelect.Remove(m_onSelectHandle);
    sfSelectionManager::Get().OnDeselect.Remove(m_onDeselectHandle);

    UWorld* world = GEditor->GetEditorWorldContext().World();
    for (TActorIterator<AActor> iter(world); iter; ++iter)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(*iter);
        if (objPtr != nullptr && objPtr->IsLocked())
        {
            Unlock(*iter);
        }
    }

    for (AActor* actorPtr : m_uploadList)
    {
        actorPtr->ClearFlags(RF_Standalone);// allow the actor to be garbage collected
    }
    m_uploadList.Empty();
    m_recreateSet.clear();
    m_revertFolderQueue.Empty();
    m_syncParentList.Empty();
    m_foldersToCheck.Empty();
}

void sfActorTranslator::Tick(float deltaTime)
{
    // Create server objects for actors in the upload list
    if (m_uploadList.Num() > 0)
    {
        UploadActors(m_uploadList);
    }

    // Revert folders to server values for actors whose folder changed while locked
    if (!m_revertFolderQueue.IsEmpty())
    {
        sfUnrealUtils::PreserveUndoStack([this]()
        {
            RevertLockedFolders();
        });
    }

    // Recreate actors that were deleted while locked.
    RecreateLockedActors();

    // Send parent changes for attached/detached actors or reset them to server values if they are locked
    for (AActor* actorPtr : m_syncParentList)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
        if (objPtr != nullptr && objPtr->IsSyncing())
        {
            SyncParent(actorPtr, objPtr);
        }
    }
    m_syncParentList.Empty();

    // Empty folders are gone when you reload a level, so we delete folders that become empty
    if (m_foldersToCheck.Num() > 0)
    {
        sfUnrealUtils::PreserveUndoStack([this]()
        {
            DeleteEmptyFolders();
        });
    }

    ApplyLayersChange();
    SendLayersChange();

    // Garbage collection
    if (m_collectGarbage)
    {
        m_collectGarbage = false;
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    }
}

void sfActorTranslator::DestroyActor(AActor* actorPtr)
{
    if (actorPtr->IsSelected())
    {
        // Unselect the actor before deleting it to avoid UI bugs/crashes
        GEditor->SelectActor(actorPtr, false, true);
        // We need to update the SSCEditor tree in the details panel to avoid a crash if the user was renaming a
        // component of the deleted actor. The crash occurs the next time the user selects something.
        sfDetailsPanelManager::Get().UpdateDetailsPanelTree();
    }
    UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
    GEngine->OnLevelActorDeleted().Remove(m_onActorDeletedHandle);
    worldPtr->EditorDestroyActor(actorPtr, true);
    m_collectGarbage = true;// Collect garbage to set references to this actor to nullptr
    m_onActorDeletedHandle = GEngine->OnLevelActorDeleted().AddRaw(this,
        &sfActorTranslator::OnActorDeleted);
    SceneFusion::RedrawActiveViewport();
}

void sfActorTranslator::DestroyUnsyncedActorsInLevel(ULevel* levelPtr)
{
    for (AActor* actorPtr : levelPtr->Actors)
    {
        if (IsSyncable(actorPtr) && !sfObjectMap::Contains(actorPtr))
        {
            DestroyActor(actorPtr);
        }
    }
}

void sfActorTranslator::DestroyUnsyncedComponents(AActor* actorPtr)
{
    TInlineComponentArray<UActorComponent*> components;
    actorPtr->GetComponents(components);
    TSharedPtr<sfComponentTranslator> componentTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfComponentTranslator>(sfType::Component);
    for (UActorComponent* componentPtr : components)
    {
        // Destroy the component if it is not synced
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(componentPtr);
        if (objPtr == nullptr && componentTranslatorPtr->IsSyncable(componentPtr))
        {
            componentTranslatorPtr->CallDeleteHandler(nullptr, componentPtr);
            componentPtr->DestroyComponent();
            SceneFusion::RedrawActiveViewport();
            continue;
        }
        // If the component is from a different actor, move it back
        if (objPtr != nullptr)
        {
            AActor* oldActorPtr = sfObjectMap::Get<AActor>(sfUnrealUtils::FindAncestorByType(objPtr, sfType::Actor));
            if (oldActorPtr != actorPtr)
            {
                componentTranslatorPtr->OnParentChange(objPtr, 0);
                componentTranslatorPtr->SyncTransform(Cast<USceneComponent>(componentPtr), true);
            }
        }
    }
}

void sfActorTranslator::RevertLockedFolders()
{
    while (!m_revertFolderQueue.IsEmpty())
    {
        AActor* actorPtr;
        m_revertFolderQueue.Dequeue(actorPtr);
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
        if (objPtr != nullptr && objPtr->IsSyncing())
        {
            sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
            GEngine->OnLevelActorFolderChanged().Remove(m_onFolderChangeHandle);
            actorPtr->SetFolderPath(FName(*sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Folder))));
            m_onFolderChangeHandle = GEngine->OnLevelActorFolderChanged().AddRaw(
                this, &sfActorTranslator::OnFolderChange);
        }
    }
}

void sfActorTranslator::RecreateLockedActors()
{
    for (auto iter = m_recreateSet.begin(); iter != m_recreateSet.end(); ++iter)
    {
        if (!sfObjectMap::Contains(*iter))
        {
            OnCreate(*iter, 0);
        }
    }
    m_recreateSet.clear();
}

void sfActorTranslator::DeleteEmptyFolders()
{
    // The only way to tell if a folder is empty is to iterate all the actors
    if (m_foldersToCheck.Num() > 0 && FActorFolders::IsAvailable())
    {
        UWorld* world = GEditor->GetEditorWorldContext().World();
        for (TActorIterator<AActor> iter(world); iter && m_foldersToCheck.Num() > 0; ++iter)
        {
            FString folder = iter->GetFolderPath().ToString();
            for (int i = m_foldersToCheck.Num() - 1; i >= 0; i--)
            {
                if (folder == m_foldersToCheck[i] || FActorFolders::Get().PathIsChildOf(folder, m_foldersToCheck[i]))
                {
                    m_foldersToCheck.RemoveAt(i);
                    break;
                }
            }
        }
        for (int i = 0; i < m_foldersToCheck.Num(); i++)
        {
            FActorFolders::Get().DeleteFolder(*world, FName(*m_foldersToCheck[i]));
        }
        m_foldersToCheck.Empty();
    }
}

bool sfActorTranslator::IsSyncable(AActor* actorPtr, bool allowPendingKill)
{
    // Actors are syncable if:
    return actorPtr != nullptr && // the actor is not null and is valid
        actorPtr->GetFName().IsValid() &&
        (!actorPtr->IsPendingKill() || allowPendingKill) && // the actor is not destroyed
        !actorPtr->HasAnyFlags(RF_BeginDestroyed | RF_Transient) &&
        actorPtr->GetOuter() != nullptr &&  // the actor is part of the world
        actorPtr->GetWorld() == GEditor->GetEditorWorldContext().World() &&
        (IsListedinWorldOutliner(actorPtr) ||  // the actor is shown in the world outliner,
            IsHiddenSyncType(actorPtr->GetClass())) && // or its class is in the hidden sync set
        !FActorEditorUtils::IsABuilderBrush(actorPtr) && // the actor is not a builder brush,
        !actorPtr->IsA<AWorldSettings>() && // world settings,
        !actorPtr->IsA<ALandscapeGizmoActiveActor>() && // landscape gizmo actor,
        !IsChildActor(actorPtr); // or a child actor (actor created by the UChildComponent).
}

bool sfActorTranslator::IsListedinWorldOutliner(AActor* actorPtr)
{
    if (!actorPtr->IsEditable())
    {
        return false;
    }

    if (actorPtr->IsListedInSceneOutliner())
    {
        return true;
    }

    // Look for UChildActorComponent
    for (UActorComponent* componentPtr : actorPtr->GetComponents())
    {
        if (componentPtr->IsA<UChildActorComponent>())
        {
            return true;
        }
    }

    return false;
}

bool sfActorTranslator::IsChildActor(AActor* actorPtr)
{
    if (actorPtr != nullptr)
    {
        USceneComponent* rootComponentPtr = actorPtr->GetRootComponent();
        if (rootComponentPtr != nullptr)
        {
            USceneComponent* attachParentPtr = rootComponentPtr->GetAttachParent();
            if (attachParentPtr != nullptr && attachParentPtr->IsA<UChildActorComponent>())
            {
                UChildActorComponent* childActorComponentPtr = Cast<UChildActorComponent>(attachParentPtr);
                return childActorComponentPtr != nullptr && childActorComponentPtr->GetChildActor() == actorPtr;
            }
        }
    }
    return false;
}

bool sfActorTranslator::IsHiddenSyncType(UClass* classPtr)
{
    while (classPtr != nullptr && classPtr != AActor::StaticClass())
    {
        if (m_hiddenSyncTypes.Contains(classPtr))
        {
            return true;
        }
        classPtr = classPtr->GetSuperClass();
    }
    return false;
}

void sfActorTranslator::CallActorInitializer(sfObject::SPtr objPtr, AActor* actorPtr)
{
    for (auto& iter : m_actorInitializers)
    {
        if (actorPtr->IsA(iter.first))
        {
            iter.second(objPtr, actorPtr);
        }
    }
}

void sfActorTranslator::CallObjectInitializer(sfObject::SPtr objPtr, AActor* actorPtr)
{
    for (auto& iter : m_objectInitializers)
    {
        if (actorPtr->IsA(iter.first))
        {
            iter.second(objPtr, actorPtr);
        }
    }
}

void sfActorTranslator::OnActorAdded(AActor* actorPtr)
{
    // Ignore actors in the buffer level.
    // The buffer level is a temporary level used when moving actors to a different level.
    // Ignore the level script actor. It get recreated when the persistent level is saved.
    if (actorPtr->GetOutermost() == GetTransientPackage() || actorPtr->IsA<ALevelScriptActor>())
    {
        return;
    }

    // We add this to a list for processing later because the actor's properties may not be initialized yet.
    m_uploadList.Add(actorPtr);
    actorPtr->SetFlags(RF_Standalone);// prevent the actor from being garbage collected
}

void sfActorTranslator::UploadActors(TArray<AActor*>& actors)
{
    TSharedPtr<sfLevelTranslator> levelTranslatorPtr =
        SceneFusion::Get().GetTranslator<sfLevelTranslator>(sfType::Level);
    std::list<sfObject::SPtr> objects;
    sfObject::SPtr parentPtr = nullptr;
    sfObject::SPtr currentParentPtr = nullptr;
    ULevel* levelPtr = nullptr;
    uint32_t limit = m_sessionPtr->GetObjectLimit(sfType::Actor);
    int numActors = 0;
    for (auto iter = actors.CreateIterator(); iter; ++iter)
    {
        AActor* actorPtr = *iter;
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
        if (objPtr != nullptr && objPtr->IsDeletePending())
        {
            // We need to wait until the server acknowledges the delete before we can recreate the actor.
            continue;
        }
        iter.RemoveCurrent();
        actorPtr->ClearFlags(RF_Standalone);// allow the actor to be garbage collected
        if (!IsSyncable(actorPtr) || (objPtr != nullptr && objPtr->IsSyncing()))
        {
            continue;
        }

        USceneComponent* parentComponentPtr = actorPtr->GetRootComponent() == nullptr ?
            nullptr :actorPtr->GetRootComponent()->GetAttachParent();
        if (parentComponentPtr == nullptr)
        {
            currentParentPtr = sfObjectMap::GetSFObject(actorPtr->GetLevel());
        }
        else
        {
            currentParentPtr = sfObjectMap::GetSFObject(parentComponentPtr);
        }

        if (currentParentPtr == nullptr || !currentParentPtr->IsSyncing())
        {
            continue;
        }
        else if (currentParentPtr->IsFullyLocked())
        {
            KS::Log::Warning("Failed to attach " + std::string(TCHAR_TO_UTF8(*actorPtr->GetName())) +
                " to " + std::string(TCHAR_TO_UTF8(*parentComponentPtr->GetOwner()->GetName())) +
                " because it is fully locked by another user.",
                LOG_CHANNEL);
            DisableParentChangeHandler();
            actorPtr->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            EnableParentChangeHandler();
            currentParentPtr = sfObjectMap::GetSFObject(actorPtr->GetLevel());
        }

        if (parentPtr == nullptr)
        {
            parentPtr = currentParentPtr;
            levelPtr = actorPtr->GetLevel();
        }

        // All objects in one request must have the same parent, so if we encounter a different parent, send a request
        // for all objects we already processed and clear the objects list to start a new request.
        if (currentParentPtr != parentPtr)
        {
            Upload(objects, numActors, limit, parentPtr, levelPtr);
            objects.clear();
            parentPtr = currentParentPtr;
            levelPtr = actorPtr->GetLevel();
        }
        objPtr = CreateObject(actorPtr);
        if (objPtr != nullptr)
        {
            objects.push_back(objPtr);
            if (limit < UINT32_MAX)
            {
                // There is an actor limit. Count the number of actors we are uploading.
                auto objIter = objPtr->SelfAndDescendants();
                while (objIter.Value() != nullptr)
                {
                    if (objIter.Value()->Type() == sfType::Actor)
                    {
                        numActors++;
                    }
                    objIter.Next();
                }
            }
        }
    }
    Upload(objects, numActors, limit, parentPtr, levelPtr);
}

void sfActorTranslator::Upload(
    std::list<sfObject::SPtr>& objects,
    int numActors,
    uint32_t limit,
    sfObject::SPtr parentPtr,
    ULevel* levelPtr)
{
    if (objects.size() == 0)
    {
        return;
    }
    if (limit < UINT32_MAX && numActors + m_sessionPtr->GetObjectCount(sfType::Actor) > limit)
    {
        // Placing the actors puts us over the actor limit. Delete the actors.
        for (auto iter : objects)
        {
            OnDelete(iter);
        }
        // Display a dialog message the first time we reach the actor limit.
        SceneFusion::Get().ShowUpgradeLink(SceneFusion::RestrictedFeature::ACTOR_LIMIT);
        if (!m_reachedActorLimit)
        {
            m_reachedActorLimit = true;
            OnReachActorLimit.Broadcast();
            FText message = FText::FromString("You cannot place actors because you reached the " +
                FString::FromInt((int)limit) + " actor limit.");
            FText title = FText::FromString("Scene Fusion");
            FMessageDialog::Open(EAppMsgType::Ok, message, &title);
        }
    }
    else
    {
        m_sessionPtr->Create(objects, parentPtr, 0);
        // Pre-existing child objects can only be attached after calling Create.
        FindAndAttachChildren(objects);
        TSharedPtr<sfLevelTranslator> levelTranslatorPtr =
            SceneFusion::Get().GetTranslator<sfLevelTranslator>(sfType::Level);
        levelTranslatorPtr->MarkActorOrderStale(levelPtr);
    }
}

void sfActorTranslator::FindAndAttachChildren(const std::list<sfObject::SPtr>& objects)
{
    TSharedPtr<sfComponentTranslator> componentTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfComponentTranslator>(sfType::Component);
    for (sfObject::SPtr objPtr : objects)
    {
        auto iter = objPtr->SelfAndDescendants();
        while (iter.Value() != nullptr)
        {
            AActor* actorPtr = sfObjectMap::Get<AActor>(iter.Value());
            iter.Next();
            if (actorPtr != nullptr)
            {
                TArray<AActor*> children;
                actorPtr->GetAttachedActors(children);
                for (AActor* childPtr : children)
                {
                    sfObject::SPtr childObjPtr = sfObjectMap::GetSFObject(childPtr);
                    if (childObjPtr == nullptr)
                    {
                        continue;
                    }
                    USceneComponent* childRootPtr = childPtr->GetRootComponent();
                    if (childRootPtr == nullptr)
                    {
                        // This can happen after an undo delete if the child was deleted by another user
                        continue;
                    }
                    sfObject::SPtr parentObjPtr = sfObjectMap::GetSFObject(childRootPtr->GetAttachParent());
                    if (parentObjPtr != nullptr && childObjPtr->Parent() != parentObjPtr)
                    {
                        parentObjPtr->AddChild(childObjPtr);
                        if (componentTranslatorPtr.IsValid())
                        {
                            componentTranslatorPtr->SyncTransform(childPtr->GetRootComponent());
                        }
                    }
                }
            }
        }
    }
}

bool sfActorTranslator::Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr)
{
    AActor* actorPtr = Cast<AActor>(uobjPtr);
    if (actorPtr == nullptr || actorPtr->IsA<AWorldSettings>())
    {
        return false;
    }
    if (IsSyncable(actorPtr, true))
    {
        outObjPtr = sfObject::Create(sfType::Actor, sfDictionaryProperty::Create());
        sfObjectMap::Add(outObjPtr, uobjPtr);
    }
    return true;
}

sfObject::SPtr sfActorTranslator::CreateObject(AActor* actorPtr)
{
    ULevel* levelPtr = actorPtr->GetLevel();
    TSharedPtr<sfLevelTranslator> levelTranslator = SceneFusion::Get().GetTranslator<sfLevelTranslator>(sfType::Level);
    if (levelPtr != nullptr && levelTranslator.IsValid() && !levelTranslator->IsLevelObjectInitialized(levelPtr))
    {
        return nullptr;
    }
    sfObject::SPtr objPtr = sfObjectMap::GetOrCreateSFObject(actorPtr, sfType::Actor);
    if (objPtr->IsSyncing())
    {
        return nullptr;
    }
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();

    if (actorPtr->IsSelected())
    {
        objPtr->RequestLock();
    }

    FString className;
    AsfMissingActor* missingActorPtr = Cast<AsfMissingActor>(actorPtr);
    if (missingActorPtr != nullptr)
    {
        // This is a stand-in for a missing actor class.
        SceneFusion::MissingObjectManager->AddStandIn(missingActorPtr);
        className = missingActorPtr->MissingClass();
    }
    else
    {
        UClass* classPtr = actorPtr->GetClass();
        if (classPtr->IsInBlueprint() &&
            !classPtr->IsChildOf(ALevelScriptActor::StaticClass()))
        {
            UObject* outer = classPtr->GetOuter();
            GIsEditorLoadingPackage = true;
            UBlueprint* blueprintPtr = LoadObject<UBlueprint>(outer, *outer->GetName());
            GIsEditorLoadingPackage = false;

            TSharedPtr<sfBlueprintTranslator> blueprintTranslatorPtr
                = SceneFusion::Get().GetTranslator<sfBlueprintTranslator>(sfType::Blueprint);
            if (blueprintTranslatorPtr.IsValid() &&
                blueprintPtr != nullptr &&
                sfObjectMap::GetSFObject(blueprintPtr) == nullptr)
            {
                // Upload blueprint first
                blueprintTranslatorPtr->UploadBlueprint(blueprintPtr);
            }
        }
        className = sfUnrealUtils::ClassToFString(classPtr);
    }

    propertiesPtr->Set(sfProp::Name, sfPropertyUtil::FromString(actorPtr->GetName()));
    propertiesPtr->Set(sfProp::Class, sfPropertyUtil::FromString(className));
    propertiesPtr->Set(sfProp::Label, sfPropertyUtil::FromString(actorPtr->GetActorLabel()));
    propertiesPtr->Set(sfProp::Folder, sfPropertyUtil::FromString(actorPtr->GetFolderPath().ToString()));
    if (actorPtr->bLockLocation)
    {
        propertiesPtr->Set(sfProp::LockLocation, sfValueProperty::Create(true));
    }
    TArray<FName>& layers = actorPtr->Layers;
    if (layers.Num() > 0)
    {
        sfListProperty::SPtr layersPropPtr = sfListProperty::Create();
        for (int i = 0; i < layers.Num(); i++)
        {
            layersPropPtr->Add(sfPropertyUtil::FromString(layers[i].ToString()));
        }
        propertiesPtr->Set(sfProp::Layers, layersPropPtr);
    }
    InitializeChildren(objPtr);
    sfPropertyManager::Get().CreateProperties(actorPtr, propertiesPtr);

    TSharedPtr<sfComponentTranslator> componentTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfComponentTranslator>(sfType::Component);
    USceneComponent* rootComponentPtr = actorPtr->GetRootComponent();
    if (rootComponentPtr != nullptr)
    {
        sfObject::SPtr childPtr = nullptr;
        if (componentTranslatorPtr.IsValid())
        {
            childPtr = componentTranslatorPtr->CreateObject(rootComponentPtr);
        }
        if (childPtr != nullptr)
        {
            childPtr->Property()->AsDict()->Set(sfProp::IsRoot, sfValueProperty::Create(true));
            objPtr->AddChild(childPtr);
        }
    }

    // Create objects for non-scene components
    if (componentTranslatorPtr.IsValid())
    {
        for (UActorComponent* componentPtr : actorPtr->GetComponents())
        {
            if (!componentTranslatorPtr->IsSyncable(componentPtr))
            {
                continue;
            }
            sfObject::SPtr childPtr = sfObjectMap::GetSFObject(componentPtr);
            if (childPtr != nullptr && childPtr->Property()->AsDict()->Size() > 0)
            {
                continue;
            }
            childPtr = componentTranslatorPtr->CreateObject(componentPtr);
            if (childPtr != nullptr)
            {
                objPtr->AddChild(childPtr);
            }
        }
    }

    // Call the object initializer for the actor's class, if one is registered
    CallObjectInitializer(objPtr, actorPtr);

    InvokeOnLockStateChange(objPtr, actorPtr);
    
    return objPtr;
}

void sfActorTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    if (objPtr->Parent() == nullptr)
    {
        LogNoParentErrorAndDisconnect(objPtr);
        return;
    }

    sfObject::SPtr rootObjectPtr = objPtr->Parent();
    while (rootObjectPtr->Parent() != nullptr)
    {
        rootObjectPtr = rootObjectPtr->Parent();
    }

    ULevel* levelPtr = nullptr;
    
    if (rootObjectPtr->Type() != sfType::Blueprint)
    {
        levelPtr = sfObjectMap::Get<ULevel>(rootObjectPtr);
        if (levelPtr == nullptr)
        {
            return;
        }
    }

    USceneComponent* parentPtr = nullptr;
    if (objPtr->Parent()->Type() == sfType::Component)
    {
        parentPtr = sfObjectMap::Get<USceneComponent>(objPtr->Parent());
        if (parentPtr == nullptr)
        {
            // Do not create until parent is created.
            return;
        }
    }

    AActor* actorPtr = InitializeActor(objPtr, levelPtr);
    if (actorPtr == nullptr)
    {
        return;
    }

    if (DetachIfParentIsLevel(objPtr, actorPtr))
    {
        return;
    }
    if (parentPtr != nullptr)
    {
        DisableParentChangeHandler();
        actorPtr->AttachToComponent(parentPtr, FAttachmentTransformRules::KeepRelativeTransform);
        EnableParentChangeHandler();
    }
}

AActor* sfActorTranslator::InitializeActor(sfObject::SPtr objPtr, ULevel* levelPtr)
{
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    FString className = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Class));
    UClass* classPtr = sfUnrealUtils::LoadClass(className);
    FString name = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
    AActor* actorPtr = nullptr;
    if (levelPtr == nullptr)
    {
        // If we could not find the blueprint, return.
        if (classPtr == nullptr)
        {
            return nullptr;
        }

        if (classPtr == ALevelScriptActor::StaticClass())
        {
            // This actor is the default object of the level blueprint
            UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
            ULevelScriptBlueprint* levelBlueprintPtr = worldPtr->PersistentLevel->GetLevelScriptBlueprint();
            if (levelBlueprintPtr == nullptr || levelBlueprintPtr->GeneratedClass == nullptr)
            {
                return nullptr;
            }
            classPtr = levelBlueprintPtr->GeneratedClass;
        }

        // The actor is a blueprint default object
        UObject* defaultObjPtr = classPtr->GetDefaultObject(true);
        if (defaultObjPtr != nullptr)
        {
            actorPtr = Cast<AActor>(defaultObjPtr);
        }
    }
    else
    {
        actorPtr = sfActorUtil::FindActorWithNameInLevel(levelPtr, name);
    }

    if (actorPtr != nullptr)
    {
        if (actorPtr->IsPendingKill())
        {
            // Rename the deleted actor so we can reuse its name.
            sfUnrealUtils::Rename(actorPtr, name + " (deleted)");
            actorPtr = nullptr;
        }
        else if (sfObjectMap::Contains(actorPtr) || (classPtr != nullptr && actorPtr->GetClass() != classPtr))
        {
            // The actor may already be in the map if we created an actor with the same name at the same time as
            // another user.
            actorPtr = nullptr;
        }
    }

    bool createActor = actorPtr == nullptr;
    if (createActor)
    {
        if (SceneFusion::CreateTimeExceeded())
        {
            SceneFusion::ObjectEventDispatcher->QueueCreate(objPtr);
            return nullptr;
        }

        bool isClassMissing = classPtr == nullptr;
        if (isClassMissing)
        {
            classPtr = AsfMissingActor::StaticClass();
        }
        GEngine->OnLevelActorAdded().Remove(m_onActorAddedHandle);
        UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
        FActorSpawnParameters spawnParameters;
        spawnParameters.OverrideLevel = levelPtr;
        actorPtr = worldPtr->SpawnActor<AActor>(classPtr, spawnParameters);

        sfActorUtil::UpdateActorVisibilityWithLevel(actorPtr);
        m_onActorAddedHandle = GEngine->OnLevelActorAdded().AddRaw(this, &sfActorTranslator::OnActorAdded);
        if (isClassMissing)
        {
            AsfMissingActor* missingActorPtr = Cast<AsfMissingActor>(actorPtr);
            missingActorPtr->ClassName = className;
            SceneFusion::MissingObjectManager->AddStandIn(missingActorPtr);
        }
    }
    else
    {
        if (actorPtr->IsSelected())
        {
            objPtr->RequestLock();
        }
    }
    sfObjectMap::Add(objPtr, actorPtr);

    actorPtr->SetFolderPath(FName(*sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Folder))));

    sfProperty::SPtr propPtr;
    actorPtr->bLockLocation = propertiesPtr->TryGet(sfProp::LockLocation, propPtr) &&
        propPtr->AsValue()->GetValue().GetBool();

    if (propertiesPtr->TryGet(sfProp::Layers, propPtr))
    {
        SetLayers(actorPtr, propPtr->AsList());
    }

    sfPropertyManager::Get().ApplyProperties(actorPtr, propertiesPtr);

    if (createActor)
    {
        // Call the actor initializer for the actor's class, if one is registered
        CallActorInitializer(objPtr, actorPtr);
    }

    FString label = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Label));
    // Calling SetActorLabel will change the actor's name (id), even if the label doesn't change. So we check first if
    // the label is different
    if (label != actorPtr->GetActorLabel())
    {
        FCoreDelegates::OnActorLabelChanged.Remove(m_onLabelChangeHandle);
        actorPtr->SetActorLabel(sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Label)));
        m_onLabelChangeHandle = FCoreDelegates::OnActorLabelChanged.AddRaw(this, &sfActorTranslator::OnLabelChanged);
    }
    // Set name after setting label because setting label changes the name
    sfUnrealUtils::TryRename(actorPtr, name);

    // Set references to this actor
    std::vector<sfReferenceProperty::SPtr> references = m_sessionPtr->GetReferences(objPtr);
    sfPropertyManager::Get().SetReferences(actorPtr, references);

    SceneFusion::RedrawActiveViewport();

    // Initialize children
    TSharedPtr<sfComponentTranslator> componentTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfComponentTranslator>(sfType::Component);
    TSharedPtr<sfUObjectTranslator> uobjectTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfUObjectTranslator>(sfType::UObject);
    for (sfObject::SPtr childPtr : objPtr->Children())
    {
        if (childPtr->Type() == sfType::Component && componentTranslatorPtr.IsValid())
        {
            componentTranslatorPtr->InitializeComponent(actorPtr, childPtr);
        }
        else
        {
            SceneFusion::ObjectEventDispatcher->OnCreate(childPtr, 0);
        }
    }
    DestroyUnsyncedComponents(actorPtr);

    if (objPtr->IsLocked())
    {
        OnLock(objPtr);
    }
    InvokeOnLockStateChange(objPtr, actorPtr);
   
    sfActorUtil::Reselect(actorPtr);
    return actorPtr;
}

void sfActorTranslator::OnActorDeleted(AActor* actorPtr)
{
    // Ignore actors in the buffer level.
    // The buffer level is a temporary level used when moving actors to a different level.
    if (actorPtr->GetOutermost() == GetTransientPackage())
    {
        return;
    }

    // Do not remove the sfObject so it will be reused if the actor is recreated and references to it will be
    // preserved.
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
    if (objPtr != nullptr && objPtr->IsSyncing() && m_recreateSet.find(objPtr) == m_recreateSet.end())
    {
        if (objPtr->IsLocked())
        {
            RecreateActor(objPtr);
        }
        else
        {
            // Attach child actor objects to the level object before deleting the object
            sfObject::SPtr levelObjPtr = sfObjectMap::GetSFObject(actorPtr->GetLevel());
            CleanUpChildrenOfDeletedObject(objPtr, levelObjPtr);
            m_sessionPtr->Delete(objPtr);
            TSharedPtr<sfLevelTranslator> levelTranslatorPtr =
                SceneFusion::Get().GetTranslator<sfLevelTranslator>(sfType::Level);
            levelTranslatorPtr->MarkActorOrderStale(actorPtr->GetLevel());
        }
    }
    actorPtr->ClearFlags(RF_Standalone);// allow the actor to be garbage collected
    m_uploadList.Remove(actorPtr);
    sfSelectionManager::Get().RemoveActor(actorPtr);
}

void sfActorTranslator::CleanUpChildrenOfDeletedObject(sfObject::SPtr objPtr, sfObject::SPtr levelObjPtr,
    bool recurseChildActors)
{
    TSharedPtr<sfComponentTranslator> componentTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfComponentTranslator>(sfType::Component);
    TSharedPtr<sfUObjectTranslator> uobjectTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfUObjectTranslator>(sfType::UObject);
    for (int i = objPtr->Children().size() - 1; i >= 0; i--)
    {
        sfObject::SPtr childPtr = objPtr->Child(i);
        if (childPtr->Type() == sfType::Actor)
        {
            AActor* childActorPtr = sfObjectMap::Get<AActor>(childPtr);
            if (recurseChildActors || (childActorPtr != nullptr && childActorPtr->IsPendingKill()))
            {
                // Destroy the actor if it's not already destroyed
                if (childActorPtr != nullptr && !childActorPtr->IsPendingKill())
                {
                    DestroyActor(childActorPtr);
                }
                // Do not remove the sfObject if levelObjPtr is not nullptr so it will be reused if the actor is
                // recreated and references to it will be preserved.
                if (levelObjPtr == nullptr)
                {
                    sfObjectMap::Remove(childPtr);
                }
                CleanUpChildrenOfDeletedObject(childPtr, levelObjPtr, recurseChildActors);
            }
            else if (levelObjPtr != nullptr)
            {
                // Add the actor's object to the level object and sync the transform
                levelObjPtr->AddChild(childPtr);
                if (childActorPtr != nullptr && componentTranslatorPtr.IsValid())
                {
                    componentTranslatorPtr->SyncTransform(childActorPtr->GetRootComponent());
                }
            }
        }
        else if (childPtr->Type() == sfType::Component)
        {
            // Destroy the component if it's not already destroyed and the level object is null. If level object is not
            // null, Unreal will destroy the components later and destroying them now can mess up other handlers of the
            // on actor destroy event.
            if (levelObjPtr == nullptr)
            {
                UActorComponent* componentPtr = sfObjectMap::Get<UActorComponent>(childPtr);
                if (componentPtr != nullptr && !componentPtr->IsPendingKill())
                {
                    componentPtr->DestroyComponent();
                }
            }
            sfObjectMap::Remove(childPtr);
            CleanUpChildrenOfDeletedObject(childPtr, levelObjPtr, recurseChildActors);
        }
        else if (childPtr->Type() == sfType::UObject)
        {
            if (levelObjPtr == nullptr)
            {
                sfObjectMap::Remove(childPtr);
                CleanUpChildrenOfDeletedObject(childPtr, levelObjPtr, recurseChildActors);
            }
            else if (uobjectTranslatorPtr.IsValid())
            {
                // To preserve references, reuse the childPtr sfObject if its outer is recreated.
                uobjectTranslatorPtr->AddPendingDeletion(childPtr);
            }
        }
        else
        {
            sfObjectMap::Remove(childPtr);
            CleanUpChildrenOfDeletedObject(childPtr, levelObjPtr, recurseChildActors);
        }
    }
}

void sfActorTranslator::OnDelete(sfObject::SPtr objPtr)
{
    CleanUpChildrenOfDeletedObject(objPtr, nullptr, true);
    AActor* actorPtr = Cast<AActor>(sfObjectMap::Remove(objPtr));
    if (actorPtr != nullptr)
    {
        DestroyActor(actorPtr);
    }
}

void sfActorTranslator::OnConfirmDelete(sfObject::SPtr objPtr)
{
    // Clear property and children from the actor (and child actors), but keep the objects around so they can be
    // reused to preserve ids if the actor is recreated.
    std::vector<sfObject::SPtr> actorObjects;
    objPtr->ForSelfAndDescendants([&actorObjects](sfObject::SPtr childPtr)
    {
        if (childPtr->Type() == sfType::Actor)
        {
            // Clear deleted actor properties
            childPtr->SetProperty(sfDictionaryProperty::Create());
            actorObjects.emplace_back(childPtr);
        }
        return true;
    });
    for (sfObject::SPtr actorObjPtr : actorObjects)
    {
        // Remove old child sfObjects for non-uobjects
        for (int i = objPtr->Children().size() - 1; i >= 0; i--)
        {
            sfObject::SPtr childPtr = objPtr->Child(i);
            if (childPtr->Type() != sfType::UObject)
            {
                objPtr->RemoveChild(childPtr);
            }
        }
    }
}

void sfActorTranslator::OnLock(sfObject::SPtr objPtr)
{
    AActor* actorPtr = sfObjectMap::Get<AActor>(objPtr);
    if (actorPtr == nullptr)
    {
        OnCreate(objPtr, 0);
        return;
    }
    Lock(actorPtr, objPtr);
    InvokeOnLockStateChange(objPtr, actorPtr);
}

void sfActorTranslator::Lock(AActor* actorPtr, sfObject::SPtr objPtr)
{
    if (sfActorUtil::GetComponent<UsfLockComponent>(actorPtr) != nullptr)
    {
        // Actor is already locked
        return;
    }
    UMaterialInterface* lockMaterialPtr = SceneFusion::GetLockMaterial(objPtr->LockOwner());
    if (lockMaterialPtr != nullptr)
    {
        TArray<UMeshComponent*> meshes;
        actorPtr->GetComponents(meshes);
        bool addedLock = false;
        if (meshes.Num() > 0)
        {
            for (int i = 0; i < meshes.Num(); i++)
            {
                
                if (meshes[i]->IsA<USplineMeshComponent>())
                {
                    // Spline mesh components behave weirdly with the lock component so we ignore them
                    continue;
                }
                UsfLockComponent* lockPtr = NewObject<UsfLockComponent>(actorPtr, *FString("SFLock" + FString::FromInt(i)));
                lockPtr->SetMobility(meshes[i]->Mobility);
                lockPtr->AttachToComponent(meshes[i], FAttachmentTransformRules::KeepRelativeTransform);
                lockPtr->RegisterComponent();
                lockPtr->InitializeComponent();
                lockPtr->DuplicateParentMesh(lockMaterialPtr);
                SceneFusion::RedrawActiveViewport();
                addedLock = true;
            }
            if (addedLock)
            {
                return;
            }
        }
    }
    UsfLockComponent* lockPtr = NewObject<UsfLockComponent>(actorPtr, *FString("SFLock"));
    lockPtr->AttachToComponent(actorPtr->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    lockPtr->RegisterComponent();
    lockPtr->InitializeComponent();
}

void sfActorTranslator::OnUnlock(sfObject::SPtr objPtr)
{
    AActor* actorPtr = sfObjectMap::Get<AActor>(objPtr);
    if (actorPtr != nullptr)
    {
        Unlock(actorPtr);
        InvokeOnLockStateChange(objPtr, actorPtr);
    }
}

void sfActorTranslator::Unlock(AActor* actorPtr)
{
    // If you undo the deletion of an actor with lock components, the lock components will not be part of the
    // OwnedComponents set so we have to use our own function to find them instead of AActor->GetComponents.
    // Not sure why this happens. It seems like an Unreal bug.
    TArray<UsfLockComponent*> locks;
    sfActorUtil::GetSceneComponents<UsfLockComponent>(actorPtr, locks);
    for (UsfLockComponent* lockPtr : locks)
    {
        lockPtr->DestroyComponent();
        SceneFusion::RedrawActiveViewport();
    }
    // When a selected actor becomes unlocked you have to unselect and reselect it to unlock the handles
    sfActorUtil::Reselect(actorPtr);
}

void sfActorTranslator::OnLockOwnerChange(sfObject::SPtr objPtr)
{
    AActor* actorPtr = sfObjectMap::Get<AActor>(objPtr);
    if (actorPtr == nullptr)
    {
        return;
    }

    InvokeOnLockStateChange(objPtr, actorPtr);

    bool isLandscape = actorPtr->IsA<ALandscapeProxy>();
    UMaterialInterface* lockMaterialPtr = isLandscape ?
        SceneFusion::GetLandscapeLockMaterial(objPtr->LockOwner()) : SceneFusion::GetLockMaterial(objPtr->LockOwner());
    if (lockMaterialPtr == nullptr)
    {
        return;
    }
    TArray<UsfLockComponent*> locks;
    sfActorUtil::GetSceneComponents<UsfLockComponent>(actorPtr, locks);
    for (UsfLockComponent* lockPtr : locks)
    {
        if (isLandscape)
        {
            lockPtr->SetLandscapeMaterial(lockMaterialPtr);
        }
        else
        {
            lockPtr->SetMaterial(lockMaterialPtr);
        }
    }
}

void sfActorTranslator::OnAttachDetach(AActor* actorPtr, const AActor* parentPtr)
{
    // Unreal fires the detach event before updating the relative transform, and if we need to change the parent back
    // because of locks Unreal won't let us here, so we queue the actor to be processed later.
    m_syncParentList.AddUnique(actorPtr);
}

void sfActorTranslator::EnableParentChangeHandler()
{
    m_onActorAttachedHandle = GEngine->OnLevelActorAttached().AddRaw(this, &sfActorTranslator::OnAttachDetach);
    m_onActorDetachedHandle = GEngine->OnLevelActorDetached().AddRaw(this, &sfActorTranslator::OnAttachDetach);
}

void sfActorTranslator::DisableParentChangeHandler()
{
    GEngine->OnLevelActorAttached().Remove(m_onActorAttachedHandle);
    GEngine->OnLevelActorDetached().Remove(m_onActorDetachedHandle);
}

void sfActorTranslator::OnParentChange(sfObject::SPtr objPtr, int childIndex)
{
    AActor* actorPtr = sfObjectMap::Get<AActor>(objPtr);
    if (actorPtr == nullptr)
    {
        return;
    }
    if (objPtr->Parent() == nullptr)
    {
        LogNoParentErrorAndDisconnect(objPtr);
    }
    else if (!DetachIfParentIsLevel(objPtr, actorPtr))
    {
        USceneComponent* parentPtr = sfObjectMap::Get<USceneComponent>(objPtr->Parent());
        if (parentPtr != nullptr)
        {
            DisableParentChangeHandler();
            actorPtr->AttachToComponent(parentPtr, FAttachmentTransformRules::KeepRelativeTransform);
            EnableParentChangeHandler();
        }
    }
}

void sfActorTranslator::OnFolderChange(const AActor* actorPtr, FName oldFolder)
{
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
    if (objPtr == nullptr || !objPtr->IsSyncing())
    {
        return;
    }
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    if (objPtr->IsLocked())
    {
        // Reverting the folder now can break the world outliner, so we queue it to be done on the next tick
        m_revertFolderQueue.Enqueue(const_cast<AActor*>(actorPtr));
    }
    else
    {
        propertiesPtr->Set(sfProp::Folder,
            sfPropertyUtil::FromString(actorPtr->GetFolderPath().ToString()));
    }
}

void sfActorTranslator::OnSelect(AActor* actorPtr)
{
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
    if (objPtr != nullptr)
    {
        objPtr->RequestLock();
        if (!sfSelectionManager::Get().MovingActors())
        {
            sfLoader::Get().LoadAssetsFor(objPtr);
        }
    }
}

void sfActorTranslator::OnDeselect(AActor* actorPtr)
{
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
    if (objPtr != nullptr)
    {
        objPtr->ReleaseLock();
    }
}

bool sfActorTranslator::OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    AActor* actorPtr = Cast<AActor>(uobjPtr);
    if (actorPtr == nullptr)
    {
        return false;
    }
    if (actorPtr->IsPendingKill())
    {
        OnActorDeleted(actorPtr);
    }
    else if (objPtr == nullptr)
    {
        OnUndoDelete(actorPtr);
    }
    else
    {
        sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
        SyncLabelAndName(actorPtr, objPtr, propertiesPtr);
        SyncFolder(actorPtr, objPtr, propertiesPtr);
        if (objPtr->IsLocked())
        {
            sfPropertyManager::Get().ApplyProperties(actorPtr, propertiesPtr);
        }
        else
        {
            sfPropertyManager::Get().SendPropertyChanges(actorPtr, propertiesPtr);
        }
        m_actorsToCheckLayerChange.Add(actorPtr);
    }
    return true;
}

void sfActorTranslator::OnUndoDelete(AActor* actorPtr)
{
    if (!IsSyncable(actorPtr))
    {
        return;
    }
    bool inLevel = false;
    UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
    for (AActor* existActorPtr : actorPtr->GetLevel()->Actors)
    {
        if (existActorPtr == nullptr)
        {
            continue;
        }

        if (existActorPtr == actorPtr)
        {
            inLevel = true;
        }
        else if (existActorPtr->GetFName() == actorPtr->GetFName())
        {
            // An actor with the same name already exists. Rename and delete the one that was just created. Although we
            // will delete it, we still need to rename it because names of deleted actors are still in use.
            sfUnrealUtils::Rename(actorPtr, actorPtr->GetName() + " (deleted)");
            DestroyActor(actorPtr);
            return;
        }
    }
    if (!inLevel)
    {
        // The actor is not in the world. This means the actor was deleted by another user and should not be recreated,
        // so we delete it.
        DestroyActor(actorPtr);
        return;
    }
    // If the actor was locked when it was deleted, it will still have a lock component, so we need to unlock it.
    Unlock(actorPtr);
    m_uploadList.AddUnique(actorPtr);
    actorPtr->SetFlags(RF_Standalone);// prevent the actor from being garbage collected
}

void sfActorTranslator::RecreateActor(sfObject::SPtr objPtr)
{
    sfObjectMap::Remove(objPtr);
    objPtr->ReleaseLock();
    CleanUpChildrenOfDeletedObject(objPtr);
    m_recreateSet.emplace(objPtr);
}

void sfActorTranslator::SyncLabelAndName(
    AActor* actorPtr,
    sfObject::SPtr objPtr,
    sfDictionaryProperty::SPtr propertiesPtr)
{
    if (propertiesPtr != nullptr)
    {
        if (objPtr->IsLocked())
        {
            FCoreDelegates::OnActorLabelChanged.Remove(m_onLabelChangeHandle);
            actorPtr->SetActorLabel(sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Label)));
            m_onLabelChangeHandle = FCoreDelegates::OnActorLabelChanged.AddRaw(this, &sfActorTranslator::OnLabelChanged);
            sfUnrealUtils::TryRename(actorPtr, sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name)));
        }
        else
        {
            if (sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Label)) != actorPtr->GetActorLabel())
            {
                propertiesPtr->Set(sfProp::Label, sfPropertyUtil::FromString(actorPtr->GetActorLabel()));
            }
            FString name = actorPtr->GetName();
            if (sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name)) != name)
            {
                propertiesPtr->Set(sfProp::Name, sfPropertyUtil::FromString(name));
            }
        }
    }
}

void sfActorTranslator::SyncFolder(AActor* actorPtr, sfObject::SPtr objPtr, sfDictionaryProperty::SPtr propertiesPtr)
{
    if (propertiesPtr != nullptr)
    {
        FString newFolder = actorPtr->GetFolderPath().ToString();
        if (newFolder != sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Folder)))
        {
            if (objPtr->IsLocked())
            {
                // Setting folder during a transaction causes a crash, so we queue it to be done on the next tick
                m_revertFolderQueue.Enqueue(actorPtr);
            }
            else
            {
                propertiesPtr->Set(sfProp::Folder, sfPropertyUtil::FromString(newFolder));
            }
        }
    }
}

void sfActorTranslator::SyncParent(AActor* actorPtr, sfObject::SPtr objPtr)
{
    if (objPtr == nullptr)
    {
        return;
    }

    sfObject::SPtr parentPtr = nullptr;
    if (actorPtr->GetAttachParentActor() != nullptr)
    {
        parentPtr = sfObjectMap::GetSFObject(actorPtr->GetRootComponent()->GetAttachParent());
    }
    if (parentPtr == nullptr || !parentPtr->IsSyncing())
    {
        parentPtr = sfObjectMap::GetSFObject(actorPtr->GetLevel());
    }
    if (parentPtr == objPtr->Parent())
    {
        return;
    }
    TSharedPtr<sfComponentTranslator> componentTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfComponentTranslator>(sfType::Component);
    if (objPtr->IsLocked() || (parentPtr != nullptr && parentPtr->IsFullyLocked()))
    {
        if (objPtr->Parent() == nullptr)
        {
            if (objPtr->IsSyncing())
            {
                LogNoParentErrorAndDisconnect(objPtr);
            }
            return;
        }

        if (DetachIfParentIsLevel(objPtr, actorPtr))
        {
            if (componentTranslatorPtr.IsValid())
            {
                componentTranslatorPtr->SyncTransform(actorPtr->GetRootComponent(), true);
            }
            return;
        }

        USceneComponent* componentPtr = sfObjectMap::Get<USceneComponent>(objPtr->Parent());
        if (componentPtr == nullptr)
        {
            return;
        }
        DisableParentChangeHandler();
        actorPtr->AttachToComponent(componentPtr, FAttachmentTransformRules::KeepRelativeTransform);
        EnableParentChangeHandler();
        if (componentTranslatorPtr.IsValid())
        {
            componentTranslatorPtr->SyncTransform(actorPtr->GetRootComponent(), true);
        }
    }
    else if (parentPtr != nullptr)
    {
        parentPtr->AddChild(objPtr);
        if (componentTranslatorPtr.IsValid())
        {
            componentTranslatorPtr->SyncTransform(actorPtr->GetRootComponent());
        }
    }
}

void sfActorTranslator::OnLabelChanged(AActor* actorPtr)
{
    if (actorPtr == nullptr || actorPtr->GetOutermost() == GetTransientPackage())
    {
        return;
    }
    
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
    if (objPtr == nullptr)
    {
        return;
    }
    SyncLabelAndName(actorPtr, objPtr, objPtr->Property()->AsDict());
}

void sfActorTranslator::RegisterPropertyChangeHandlers()
{
    m_propertyChangeHandlers[sfProp::Name] = 
        [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        AActor* actorPtr = Cast<AActor>(uobjPtr);
        sfUnrealUtils::TryRename(actorPtr, sfPropertyUtil::ToString(propertyPtr));
        return true;
    };
    m_propertyChangeHandlers[sfProp::Label] =
        [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        AActor* actorPtr = Cast<AActor>(uobjPtr);
        FCoreDelegates::OnActorLabelChanged.Remove(m_onLabelChangeHandle);
        actorPtr->SetActorLabel(sfPropertyUtil::ToString(propertyPtr));
        m_onLabelChangeHandle = FCoreDelegates::OnActorLabelChanged.AddRaw(this, &sfActorTranslator::OnLabelChanged);
        return true;
    };
    m_propertyChangeHandlers[sfProp::Folder] = 
        [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        AActor* actorPtr = Cast<AActor>(uobjPtr);
        m_foldersToCheck.AddUnique(actorPtr->GetFolderPath().ToString());
        GEngine->OnLevelActorFolderChanged().Remove(m_onFolderChangeHandle);
        actorPtr->SetFolderPath(FName(*sfPropertyUtil::ToString(propertyPtr)));
        m_onFolderChangeHandle = GEngine->OnLevelActorFolderChanged().AddRaw(this, &sfActorTranslator::OnFolderChange);
        return true;
    };
    m_propertyChangeHandlers[sfProp::LockLocation] =
        [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        AActor* actorPtr = Cast<AActor>(uobjPtr);
        actorPtr->bLockLocation = propertyPtr != nullptr &&
            propertyPtr->AsValue()->GetValue().GetBool();
        return true;
    };
    m_propertyChangeHandlers[sfProp::Layers] = 
        [this](UObject* ubojPtr, sfProperty::SPtr propertyPtr)
    {
        AActor* actorPtr = Cast<AActor>(ubojPtr);
        if (actorPtr == nullptr)
        {
            return true;
        }
        if (propertyPtr == nullptr)
        {
            TSharedPtr<sfLevelTranslator> levelTranslatorPtr =
                SceneFusion::Get().GetTranslator<sfLevelTranslator>(sfType::Level);
            levelTranslatorPtr->UnregisterOnLayersChangeHandler();
            TArray<FName> layers;
            layers.Append(actorPtr->Layers);
            m_worldLayersPtr->RemoveActorFromLayers(actorPtr, layers);
            levelTranslatorPtr->RegisterOnLayersChangeHandler();
            return true;
        }
        m_actorsToApplyLayerChange.Add(actorPtr, propertyPtr->AsList());
        return true;
    };
}

void sfActorTranslator::InvokeOnLockStateChange(sfObject::SPtr objPtr, AActor* actorPtr)
{
    // Note Unreal to check if there are locked actors in the selection.
    GEditor->bCheckForLockActors = true;

    LockType lockType = Unlocked;
    if (objPtr->IsFullyLocked())
    {
        lockType = FullyLocked;
    }
    else if (objPtr->IsPartiallyLocked())
    {
        lockType = PartiallyLocked;
    }
    OnLockStateChange.Broadcast(actorPtr, lockType, objPtr->LockOwner());
}

void sfActorTranslator::ClearActorCollections()
{
    for (AActor* actorPtr : m_uploadList)
    {
        actorPtr->ClearFlags(RF_Standalone); // allow the actor to be garbage collected
    }
    m_uploadList.Empty();
    m_revertFolderQueue.Empty();
    m_syncParentList.Empty();
}

void sfActorTranslator::OnRemoveLevel(sfObject::SPtr levelObjPtr, ULevel* levelPtr)
{
    if (levelObjPtr != nullptr)
    {
        levelObjPtr->ForEachDescendant([this](sfObject::SPtr objPtr)
        {
            UObject* uobjPtr = sfObjectMap::Remove(objPtr);
            AActor* actorPtr = Cast<AActor>(uobjPtr);
            if (actorPtr != nullptr)
            {
                sfSelectionManager::Get().RemoveActor(actorPtr);
                m_actorsToCheckLayerChange.Remove(actorPtr);
            }
            return true;
        });
    }

    for (int i = m_uploadList.Num() - 1; i >= 0; i--)
    {
        AActor* actorPtr = m_uploadList[i];
        if (actorPtr->GetOuter() == levelPtr)
        {
            actorPtr->ClearFlags(RF_Standalone); // allow the actor to be garbage collected
            m_uploadList.RemoveAt(i);
        }
    }
}

void sfActorTranslator::OnSFLevelObjectCreate(sfObject::SPtr sfLevelObjPtr, ULevel* levelPtr)
{
    for (sfObject::SPtr childPtr : sfLevelObjPtr->Children())
    {
        if (childPtr->Type() == sfType::Actor)
        {
            OnCreate(childPtr, 0); // Child index does not matter
        }
    }
    DestroyUnsyncedActorsInLevel(levelPtr);
}

bool sfActorTranslator::DetachIfParentIsLevel(sfObject::SPtr objPtr, AActor* actorPtr)
{
    if (objPtr->Parent()->Type() == sfType::Level)
    {
        DisableParentChangeHandler();
        actorPtr->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
        EnableParentChangeHandler();
        return true;
    }
    return false;
}

void sfActorTranslator::LogNoParentErrorAndDisconnect(sfObject::SPtr objPtr)
{
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    KS::Log::Error("Disconnecting because no parent object was found for actor " +
        propertiesPtr->Get(sfProp::Name)->ToString() +
        ". Root actor's parent object should be the level object.");
    SceneFusion::Service->LeaveSession();
}

void sfActorTranslator::CheckLockLocationChanges()
{
    for (FSelectionIterator it(GEditor->GetSelectedActorIterator()); it; ++it)
    {
        AActor* actorPtr = Cast<AActor>(*it);
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
        if (objPtr == nullptr)
        {
            continue;
        }
        sfDictionaryProperty::SPtr propertiesPtr= objPtr->Property()->AsDict();
        if (objPtr->IsLocked())
        {
            sfProperty::SPtr lockLocationPropPtr = nullptr;
            actorPtr->bLockLocation = propertiesPtr->TryGet(sfProp::LockLocation, lockLocationPropPtr) &&
                lockLocationPropPtr->AsValue()->GetValue().GetBool();
        }
        else if (actorPtr->bLockLocation)
        {
            propertiesPtr->Set(sfProp::LockLocation, sfValueProperty::Create(true));
        }
        else
        {
            propertiesPtr->Remove(sfProp::LockLocation);
        }
    }
}

void sfActorTranslator::OnLayersChanged(
    const ELayersAction::Type action,
    const TWeakObjectPtr<ULayer>& changedLayer,
    const FName& changedProperty)
{
    // We don't sync the actors' visibility
    if (action == ELayersAction::Modify && changedProperty == "bIsVisible")
    {
        return;
    }

    UTransBuffer* undoBufferPtr = Cast<UTransBuffer>(GEditor->Trans);
    const FTransaction* transactionPtr = undoBufferPtr->GetTransaction(undoBufferPtr->UndoBuffer.Num() - 1);
    if (transactionPtr != nullptr)
    {
        TArray<UObject*> objs;
        transactionPtr->GetTransactionObjects(objs);
        for (UObject* uobjPtr : objs)
        {
            AActor* actorPtr = Cast<AActor>(uobjPtr);
            if (actorPtr != nullptr)
            {
                m_actorsToCheckLayerChange.Add(actorPtr);
            }
        }
    }
}

void sfActorTranslator::SendLayersChange()
{
    for (AActor* actorPtr : m_actorsToCheckLayerChange)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
        if (objPtr == nullptr)
        {
            continue;
        }
        sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
        if (objPtr->IsLocked())
        {
            sfProperty::SPtr propPtr = nullptr;
            if (propertiesPtr->TryGet(sfProp::Layers, propPtr))
            {
                SetLayers(actorPtr, propPtr->AsList());
            }
        }
        else
        {
            TArray<FName> layers = actorPtr->Layers;
            if (layers.Num() > 0)
            {
                sfListProperty::SPtr layersPropPtr = sfListProperty::Create();
                for (int i = 0; i < layers.Num(); i++)
                {
                    layersPropPtr->Add(sfPropertyUtil::FromString(layers[i].ToString()));
                }
                sfProperty::SPtr propPtr = nullptr;
                if (propertiesPtr->TryGet(sfProp::Layers, propPtr))
                {
                    sfPropertyManager::Get().CopyList(propPtr->AsList(), layersPropPtr);
                }
                else
                {
                    propertiesPtr->Set(sfProp::Layers, layersPropPtr);
                }
            }
            else
            {
                propertiesPtr->Remove(sfProp::Layers);
            }
        }
    }
    m_actorsToCheckLayerChange.Empty();
}

void sfActorTranslator::ApplyLayersChange()
{
    for (TPair<AActor*, sfListProperty::SPtr> pair : m_actorsToApplyLayerChange)
    {
        SetLayers(pair.Key, pair.Value);
    }
    m_actorsToApplyLayerChange.Empty();
}

void sfActorTranslator::SetLayers(AActor* actorPtr, sfListProperty::SPtr layersPropPtr)
{
    TSharedPtr<sfLevelTranslator> levelTranslatorPtr =
        SceneFusion::Get().GetTranslator<sfLevelTranslator>(sfType::Level);
    levelTranslatorPtr->UnregisterOnLayersChangeHandler();
    TArray<FName> layers;
    for (int i = 0; i < layersPropPtr->Size(); i++)
    {
        FName layerName(*sfPropertyUtil::ToString(layersPropPtr->Get(i)));
        layers.Add(layerName);
    }
    TArray<FName> layersToAdd(layers);
    TArray<FName> layersToRemove;
    for (FName layer : actorPtr->Layers)
    {
        if (layers.Contains(layer))
        {
            layersToAdd.Remove(layer);
        }
        else
        {
            layersToRemove.Add(layer);
        }
    }
    m_worldLayersPtr->RemoveActorFromLayers(actorPtr, layersToRemove);
    if (layers.Num() > 0)
    {
        m_worldLayersPtr->AddActorToLayers(actorPtr, layersToAdd);
        //Make sure the order is the same
        actorPtr->Layers.Empty();
        actorPtr->Layers.Append(layers);
    }
    levelTranslatorPtr->RegisterOnLayersChangeHandler();
}

#undef LOG_CHANNEL