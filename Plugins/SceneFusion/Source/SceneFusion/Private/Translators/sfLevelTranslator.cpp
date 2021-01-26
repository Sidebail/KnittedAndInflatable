#include "../../Public/Translators/sfLevelTranslator.h"
#include "sfUObjectTranslator.h"
#include "sfAvatarTranslator.h"
#include "../../Public/Translators/sfModelTranslator.h"
#include "../../Public/Translators/sfBlueprintTranslator.h"
#include "../../Public/Consts.h"
#include "../../Public/sfPropertyUtil.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/sfUndoManager.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfUnrealUtils.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/sfSelectionManager.h"
#include "../../Public/Actors/sfMissingActor.h"
#include "../../Public/Objects/sfReferenceTracker.h"
#include "../../Public/sfConfig.h"

#include <Editor.h>
#include <Editor/UnrealEdEngine.h>
#include <Engine/Selection.h>
#include <UnrealEdGlobals.h>
#include <LevelUtils.h>
#include <LevelEditor.h>
#include <FileHelpers.h>
#include <EditorLevelUtils.h>
#include <Settings/LevelEditorMiscSettings.h>
#include <EditorSupportDelegates.h>
#include <Engine/WorldComposition.h>
#include <EdMode.h>
#include <EditorModes.h>
#include <EditorModeManager.h>
#include <Framework/Docking/TabManager.h>
#include <Widgets/Docking/SDockTab.h>
#include <InputCoreTypes.h>
#include <Engine/LevelBounds.h>
#include <Widgets/Docking/SDockTab.h>
#include <Kismet2/KismetEditorUtilities.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/WorldSettings.h>
#include <Engine/Blueprint.h>
#include <Engine/LevelScriptBlueprint.h>
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 21
#include <Engine/LevelStreamingDynamic.h>
#endif
#include <IDetailsView.h>

#define LOG_CHANNEL "sfLevelTranslator"

sfLevelTranslator::sfLevelTranslator() :
    m_uploadUnsyncedLevels{ false },
    m_downloadedInitialLevels{ false },
    m_worldPtr{ nullptr },
    m_worldLayersPtr{ nullptr },
    m_layersPropertyPtr{ nullptr },
    m_worldSettingsObjPtr{ nullptr },
    m_lockObject{ nullptr },
    m_worldSettingsDirty{ false },
    m_hierarchicalLODSetupDirty{ false },
    m_worldTileDetailsClassPtr{ nullptr },
    m_packageNamePropertyPtr{ nullptr }
{
    sfPropertyManager::Get().IgnoreDisableEditOnInstanceFlagForClass("LevelStreamingKisMet");
    sfPropertyManager::Get().IgnoreDisableEditOnInstanceFlagForClass("WorldSettings");
    sfPropertyManager::Get().AddPropertyToForceSyncList("LevelStreaming", "bIsStatic");

    RegisterPropertyChangeHandlers();
}

sfLevelTranslator::~sfLevelTranslator()
{

}

void sfLevelTranslator::Initialize()
{
    m_showedDisabledMessage = false;
    m_sessionPtr = SceneFusion::Service->Session();
    m_actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(sfType::Actor);
    m_worldPtr = GEditor->GetEditorWorldContext().World();

#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 23
    m_worldLayersPtr = GEditor->Layers;
#else
    m_worldLayersPtr = GEditor->GetEditorSubsystem<ULayersSubsystem>();
#endif
    RegisterOnLayersChangeHandler();

    m_preTickHandle = SceneFusion::Get().PreTick.AddRaw(this, &sfLevelTranslator::PreTick);
    m_tickHandle = SceneFusion::Get().OnTick.AddRaw(this, &sfLevelTranslator::Tick);

    m_worldTileDetailsClassPtr = FindObject<UClass>(ANY_PACKAGE, UTF8_TO_TCHAR("WorldTileDetails"));
    if (m_worldTileDetailsClassPtr != nullptr)
    {
        m_packageNamePropertyPtr = m_worldTileDetailsClassPtr->FindPropertyByName(sfProp::PackageName->c_str());
    }

    m_onAcknowledgeSubscriptionHandle = m_sessionPtr->RegisterOnAcknowledgeSubscriptionHandler(
        [this](bool isSubscription, sfObject::SPtr objPtr) { OnAcknowledgeSubscription(isSubscription, objPtr); });

    RegisterLevelEvents();

    m_uploadUnsyncedLevels = !SceneFusion::IsSessionCreator;
    m_downloadedInitialLevels = !m_uploadUnsyncedLevels;

    if (SceneFusion::IsSessionCreator)
    {
        // Upload levels
        RequestLock();
        m_levelsToUpload.emplace(m_worldPtr->PersistentLevel); // Upload persistent level first
        for (FConstLevelIterator iter = m_worldPtr->GetLevelIterator(); iter; ++iter)
        {
            if (!(*iter)->IsPersistentLevel())
            {
                m_levelsToUpload.emplace(*iter);
            }
        }
    }

    m_worldSettingsDirty = false;
    m_hierarchicalLODSetupDirty = false;
}

void sfLevelTranslator::CleanUp()
{
    m_onAcknowledgeSubscriptionHandle.reset();
    UnregisterOnLayersChangeHandler();
    SceneFusion::Get().PreTick.Remove(m_preTickHandle);
    SceneFusion::Get().OnTick.Remove(m_tickHandle);
    UnregisterLevelEvents();

    m_lockObject = nullptr;
    m_levelsToUpload.clear();
    m_movedLevels.clear();
    m_levelsNeedToBeLoaded.clear();
    m_unloadedLevelObjects.Empty();
    m_levelsWaitingForChildren.clear();
    m_dirtyStreamingLevels.Empty();
    m_dirtyParentLevels.Empty();
    m_uninitializedLevels.Empty();
    m_onLevelTransformChangeHandles.Empty();
    m_localActorOrderChangedLevels.Empty();
    m_serverActorOrderChangedLevels.Empty();
    m_layerNames.Empty();

    if (m_worldPtr != nullptr &&
        m_worldPtr->GetWorldSettings()->bEnableWorldComposition
        && m_worldPtr->WorldComposition != nullptr)
    {
        m_worldPtr->WorldComposition->bLockTilesLocation = false;
    }
}

void sfLevelTranslator::PreTick(float deltaTime)
{
    // Send actor order changes to the server
    for (ULevel* levelPtr : m_localActorOrderChangedLevels)
    {
        sfObject::SPtr levelObjPtr = sfObjectMap::GetSFObject(levelPtr);
        if (levelObjPtr == nullptr)
        {
            continue;
        }
        sfObject::SPtr propertiesObjPtr = sfUnrealUtils::FindChildByType(levelObjPtr, sfType::LevelProperties);
        if (propertiesObjPtr == nullptr)
        {
            continue;
        }
        sfProperty::SPtr propPtr;
        if (!propertiesObjPtr->Property()->AsDict()->TryGet(sfProp::Actors, propPtr))
        {
            continue;
        }
        sfListProperty::SPtr actorListPtr = sfListProperty::Create();
        for (AActor* actorPtr : levelPtr->Actors)
        {
            sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
            if (objPtr != nullptr && objPtr->IsSyncing())
            {
                actorListPtr->Add(sfValueProperty::Create(objPtr->Id()));
            }
        }
        sfPropertyManager::Get().CopyList(propPtr->AsList(), actorListPtr);
    }
    m_localActorOrderChangedLevels.Empty();
}

void sfLevelTranslator::Tick(float deltaTime)
{
    // After joining a session, uploads levels that don't exist on the server
    if (m_uploadUnsyncedLevels && m_downloadedInitialLevels)
    {
        m_uploadUnsyncedLevels = false;
        UploadUnsyncedLevels();
    }

    // Upload levels when the level lock is acquired
    if (m_lockObject != nullptr && m_lockObject->LockOwner() == m_sessionPtr->LocalUser())
    {
        for (auto iter = m_levelsToUpload.begin(); iter != m_levelsToUpload.end(); iter++)
        {
            sfObject::SPtr objPtr = sfObjectMap::GetSFObject(*iter);
            if (objPtr == nullptr || !objPtr->IsSyncing())
            {
                UploadLevel(*iter);
            }
        }
        m_levelsToUpload.clear();
        m_lockObject->ReleaseLock();
    }

    // Apply the actor order from the server
    UpdateActorOrder();

    // Send level transform change
    for (auto& levelPtr : m_movedLevels)
    {
        SendTransformUpdate(levelPtr);
    }
    m_movedLevels.clear();

    // Send level folder change
    for (ULevelStreaming* streamingLevelPtr : m_dirtyStreamingLevels)
    {
        SendFolderChange(streamingLevelPtr);
    }
    m_dirtyStreamingLevels.Empty();

    // Send level parent change
    for (ULevel* levelPtr : m_dirtyParentLevels)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(levelPtr);
        if (objPtr == nullptr)
        {
            continue;
        }
        sfObject::SPtr propertiesObjPtr = sfUnrealUtils::FindChildByType(objPtr, sfType::LevelProperties);
        if (propertiesObjPtr == nullptr)
        {
            continue;
        }
        UObject* uobjPtr = FindWorldTileDetailsObject(levelPtr->GetOutermost()->GetName());
        if (uobjPtr != nullptr)
        {
            sfDictionaryProperty::SPtr levelPropertiesPtr = propertiesObjPtr->Property()->AsDict();
            sfPropertyManager::Get().SendPropertyChanges(uobjPtr, levelPropertiesPtr);
        }
    }
    m_dirtyParentLevels.Empty();

    // Load levels that were removed but locked by other users
    for (sfObject::SPtr levelObjPtr : m_levelsNeedToBeLoaded)
    {
        OnCreateLevelObject(levelObjPtr);
    }
    m_levelsNeedToBeLoaded.clear();

    // Lock all tiles location if the selected levels contains locked level
    if (m_worldPtr->GetWorldSettings()->bEnableWorldComposition && m_worldPtr->WorldComposition != nullptr)
    {
        m_worldPtr->WorldComposition->bLockTilesLocation = false;
        for (ULevel* levelPtr : m_worldPtr->GetSelectedLevels())
        {
            sfObject::SPtr levelObjPtr = sfObjectMap::GetSFObject(levelPtr);
            if (levelObjPtr != nullptr && levelObjPtr->IsLocked())
            {
                m_worldPtr->WorldComposition->bLockTilesLocation = true;
            }
        }
    }

    // Refresh world settings tab if world settings was changed
    if (m_worldSettingsDirty)
    {
        m_worldSettingsDirty = false;
        RefreshWorldSettingsTab();
    }

    // Apply HierarchicalLODSetupDirty because Unreal set it to a differnet value
    if (m_hierarchicalLODSetupDirty)
    {
        m_hierarchicalLODSetupDirty = false;
        sfPropertyManager::Get().SyncProperty(
            m_worldSettingsObjPtr,
            m_worldPtr->GetWorldSettings(),
            "HierarchicalLODSetup",
            true);
    }
}

void sfLevelTranslator::UpdateActorOrder()
{
    for (ULevel* levelPtr : m_serverActorOrderChangedLevels)
    {
        sfObject::SPtr levelObjPtr = sfObjectMap::GetSFObject(levelPtr);
        if (levelObjPtr == nullptr)
        {
            continue;
        }
        sfObject::SPtr propertiesObjPtr = sfUnrealUtils::FindChildByType(levelObjPtr, sfType::LevelProperties);
        if (propertiesObjPtr == nullptr)
        {
            continue;
        }
        bool rebuildBSP = false;
        sfListProperty::SPtr actorListPtr = propertiesObjPtr->Property()->AsDict()->Get(sfProp::Actors)->AsList();
        int index = 0;// index to actorListPtr
        int shrinkTo = -1; // if non-negative, will shrink the array to this size
        TArray<AActor*> removedActors;// actors that were removed from the array
        TSet<AActor*> actors;// used to detect duplicate actors
        for (int i = 0; i < levelPtr->Actors.Num(); i++)
        {
            sfObject::SPtr objPtr = sfObjectMap::GetSFObject(levelPtr->Actors[i]);
            // skip unsynced actors
            if (objPtr == nullptr || !objPtr->IsSyncing())
            {
                if (shrinkTo >= 0 && levelPtr->Actors[i] != nullptr)
                {
                    shrinkTo = i + 1;
                }
                continue;
            }
            if (index >= actorListPtr->Size())
            {
                // We're passed the end of the server's actor list. Remove the remaining actors.
                if (levelPtr->Actors[i] != nullptr)
                {
                    removedActors.Add(levelPtr->Actors[i]);
                    levelPtr->Actors[i] = nullptr;
                }
                if (shrinkTo < 0)
                {
                    // Shrink the array to remove all elements from this index onwards.
                    shrinkTo = i;
                }
            }
            else
            {
                uint32_t id = actorListPtr->Get(index)->AsValue()->GetValue();
                if (objPtr->Id() != id)
                {
                    // Set the actor at index i to the server value, and add the actor that used to be at this index
                    // to the removed list.
                    if (levelPtr->Actors[i] != nullptr)
                    {
                        removedActors.Add(levelPtr->Actors[i]);
                    }
                    levelPtr->Actors[i] = sfObjectMap::Get<AActor>(m_sessionPtr->GetObject(id));
                    if (levelPtr->Actors[i] != nullptr && levelPtr->Actors[i]->IsA<ABrush>())
                    {
                        rebuildBSP = true;
                    }
                }
                bool duplicate = false;
                actors.Add(levelPtr->Actors[i], &duplicate);
                if (duplicate)
                {
                    KS::Log::Warning("Removing duplicate actor from the level actors list.", LOG_CHANNEL);
                    levelPtr->Actors[i] = nullptr;
                }
            }
            index++;
        }
        if (shrinkTo >= 0)
        {
            levelPtr->Actors.SetNum(shrinkTo);
        }
        // Add the remaining actors to the end of the array.
        while (index < actorListPtr->Size())
        {
            uint32_t id = actorListPtr->Get(index)->AsValue()->GetValue();
            levelPtr->Actors.Add(sfObjectMap::Get<AActor>(m_sessionPtr->GetObject(id)));
            index++;
        }
        if (removedActors.Num() > 0)
        {
            // Iterate the removed actors and check if they are in the actors set (meaning they were added back to the
            // array at a different index). If any are not in the set, add them to the end of the array.
            int count = 0;
            for (AActor* actorPtr : removedActors)
            {
                if (!actors.Contains(actorPtr))
                {
                    levelPtr->Actors.Add(actorPtr);
                    actors.Add(actorPtr);
                    if (actorPtr->IsA<ABrush>())
                    {
                        rebuildBSP = true;
                    }
                    count++;
                }
            }
            if (count > 0)
            {
                KS::Log::Warning("Readded " + std::to_string(count) +
                    " actors that were missing from the level actors list.", LOG_CHANNEL);
            }
        }
        // Rebuild bsp if any brushes were reordered.
        if (rebuildBSP)
        {
            TSharedPtr<sfModelTranslator> modelTranslatorPtr = SceneFusion::Get().GetTranslator<sfModelTranslator>(
                sfType::Model);
            modelTranslatorPtr->MarkBSPStale(levelPtr);
        }
    }
    m_serverActorOrderChangedLevels.Empty();
}

void sfLevelTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    if (objPtr->Type() == sfType::Level)
    {
        OnCreateLevelObject(objPtr);
    }
    else if (objPtr->Type() == sfType::LevelProperties)
    {
        OnCreateLevelPropertiesObject(objPtr);
    }
    else if (objPtr->Type() == sfType::LevelLock)
    {
        OnCreateLevelLockObject(objPtr);
    }
    else if (objPtr->Type() == sfType::GameMode)
    {
        OnCreateGameModeObject(objPtr);
    }
}

void sfLevelTranslator::OnCreateLevelObject(sfObject::SPtr objPtr)
{
    m_downloadedInitialLevels = true;
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    FString levelPath = *sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
    bool isPersistentLevel = propertiesPtr->Get(sfProp::IsPersistentLevel)->AsValue()->GetValue();
    sfObject::SPtr propertiesObjPtr = sfUnrealUtils::FindChildByType(objPtr, sfType::LevelProperties);

    UnregisterLevelEvents();
    SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();

    ULevel* levelPtr = FindLevelInLoadedLevels(levelPath, isPersistentLevel);
    if (levelPtr == nullptr && (isPersistentLevel || !GetWorldCompositionOnServer()))
    {
        levelPtr = LoadOrCreateMap(levelPath, isPersistentLevel);
        if (levelPtr == nullptr)
        {
            return;
        }
    }
    if (levelPtr != nullptr)
    {
        sfObjectMap::Add(objPtr, levelPtr);
    }

    if (!isPersistentLevel)
    {
        if (levelPtr != nullptr)
        {
            if (propertiesObjPtr != nullptr)
            {
                OnCreateLevelPropertiesObject(propertiesObjPtr);
            }
            else if (m_levelsNeedToBeLoaded.find(objPtr) == m_levelsNeedToBeLoaded.end())
            {
                // If it is a new level object and the level is loaded, requests the server for all children.
                // If the level object needs to be loaded, that means the user tried to remove the level
                // while another user had some actors in that level locked. So we want to load the level back.
                // In this case, we have all the children already.
                m_sessionPtr->SubscribeToChildren(objPtr);
                m_levelsWaitingForChildren.emplace(objPtr);
            }

            ULevelStreaming* streamingLevelPtr = FLevelUtils::FindStreamingLevel(levelPtr);
            if (streamingLevelPtr != nullptr)
            {
                sfProperty::SPtr propPtr = nullptr;

                // Set level transform
                if (propertiesPtr->TryGet(sfProp::Location, propPtr))
                {
                    FTransform transform = streamingLevelPtr->LevelTransform;
                    transform.SetLocation(sfPropertyUtil::ToVector(propPtr));
                    FRotator rotation = transform.Rotator();
                    rotation.Yaw = propertiesPtr->Get(sfProp::Rotation)->AsValue()->GetValue();
                    transform.SetRotation(rotation.Quaternion());
                    sfUnrealUtils::PreserveUndoStack([streamingLevelPtr, transform]()
                    {
                        FLevelUtils::SetEditorTransform(streamingLevelPtr, transform);
                    });
                    FDelegateHandle handle = levelPtr->OnApplyLevelTransform.AddLambda(
                        [this, levelPtr](const FTransform& transform) {
                        m_movedLevels.emplace(levelPtr);
                    });
                    m_onLevelTransformChangeHandles.Add(levelPtr, handle);
                }
            }
        }
        else
        {
            m_unloadedLevelObjects.Add(levelPath, objPtr);
            RegisterLevelEvents();
            SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
            return;
        }
    }

    if (m_levelsWaitingForChildren.find(objPtr) == m_levelsWaitingForChildren.end())
    {
        m_actorTranslatorPtr->OnSFLevelObjectCreate(objPtr, levelPtr);
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 20
        // This fixes an error on 4.20 when SceneFusion creates a sky sphere, however this line causes a crash on 4.22
        // when joining a session with the level already loaded and a sky sphere in it, so we only call this on 4.20
        UMaterialInstance::AllMaterialsCacheResourceShadersForRendering();
#endif
    }
    m_levelsToUpload.erase(levelPtr);

    RegisterLevelEvents();
    SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();

    if (isPersistentLevel)
    {
        OnCreateWorldSettingsObject(propertiesObjPtr);
        if (propertiesObjPtr->Property()->AsDict()->HasKey(sfProp::Actors))
        {
            m_serverActorOrderChangedLevels.Add(levelPtr);
        }
    }

    // Refresh levels window and viewport
    FEditorDelegates::RefreshLevelBrowser.Broadcast();
    SceneFusion::RedrawActiveViewport();
}

void sfLevelTranslator::OnCreateLevelPropertiesObject(sfObject::SPtr objPtr)
{
    ULevel* levelPtr = sfObjectMap::Get<ULevel>(objPtr->Parent());
    if (levelPtr == nullptr)
    {
        return;
    }

    FString levelPath = levelPtr->GetOutermost()->GetName();
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();

    ULevelStreaming* streamingLevelPtr = FLevelUtils::FindStreamingLevel(levelPtr);
    if (streamingLevelPtr == nullptr)
    {
        return;
    }

    sfObjectMap::Add(objPtr, streamingLevelPtr);
    sfProperty::SPtr propPtr = nullptr;
    if (m_worldPtr->GetWorldSettings()->bEnableWorldComposition)
    {
        // Set tile details
        UObject* worldTileDetailsObjPtr = FindWorldTileDetailsObject(levelPath);
        if (worldTileDetailsObjPtr != nullptr)
        {
            sfPropertyManager::Get().ApplyProperties(worldTileDetailsObjPtr, propertiesPtr);
        }
    }

    // Set folder path
    if (propertiesPtr->TryGet(sfProp::Folder, propPtr))
    {
        sfUnrealUtils::PreserveUndoStack([streamingLevelPtr, propPtr]()
        {
            streamingLevelPtr->SetFolderPath(*sfPropertyUtil::ToString(propPtr));
        });
        FEditorDelegates::RefreshLevelBrowser.Broadcast();
    }

    // Load parent level if it is not already loaded
    if (propertiesPtr->TryGet("ParentPackageName", propPtr))
    {
        FString parentPath = sfPropertyUtil::ToString(propPtr);
        if (FindLevelInLoadedLevels(parentPath, false) == nullptr)
        {
            LoadOrCreateMap(parentPath, false);
        }
    }

    sfPropertyManager::Get().ApplyProperties(streamingLevelPtr, propertiesPtr, &PROPERTY_BLACKLIST);

    // Set references to this streaming level
    std::vector<sfReferenceProperty::SPtr> references = m_sessionPtr->GetReferences(objPtr);
    sfPropertyManager::Get().SetReferences(streamingLevelPtr, references);

    // Initialize children
    TSharedPtr<sfUObjectTranslator> uobjectTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfUObjectTranslator>(sfType::UObject);
    if (uobjectTranslatorPtr.IsValid())
    {
        for (sfObject::SPtr childPtr : objPtr->Children())
        {
            if (childPtr->Type() == sfType::UObject)
            {
                uobjectTranslatorPtr->InitializeUObject(streamingLevelPtr, childPtr);
            }
        }
    }

    if (propertiesPtr->HasKey(sfProp::Actors))
    {
        m_serverActorOrderChangedLevels.Add(levelPtr);
    }
}

void sfLevelTranslator::OnCreateLevelLockObject(sfObject::SPtr objPtr)
{
    m_lockObject = objPtr;
    if (m_levelsToUpload.size() > 0)
    {
        m_lockObject->RequestLock();
    }
}

void sfLevelTranslator::OnCreateWorldSettingsObject(sfObject::SPtr worldSettingsObjPtr)
{
    if (worldSettingsObjPtr == nullptr || worldSettingsObjPtr->Type() != sfType::LevelProperties)
    {
        KS::Log::Error("Could not find sfObject for world settings. Leaving session.", LOG_CHANNEL);
        SceneFusion::Service->LeaveSession();
        return;
    }

    m_worldSettingsObjPtr = worldSettingsObjPtr;
    sfDictionaryProperty::SPtr propertiesPtr = worldSettingsObjPtr->Property()->AsDict();
    AWorldSettings* worldSettingsPtr = m_worldPtr->GetWorldSettings();
    sfObjectMap::Add(worldSettingsObjPtr, worldSettingsPtr);
    sfPropertyManager::Get().ApplyProperties(
        worldSettingsPtr,
        propertiesPtr,
        &WORLD_SETTINGS_BLACKLIST);
    m_worldSettingsDirty = true;
    m_hierarchicalLODSetupDirty = true;

    // Set layers
    UnregisterOnLayersChangeHandler();
    m_layersPropertyPtr = propertiesPtr->Get(sfProp::Layers)->AsList();
    TArray<FName> layersToDelete;
    m_worldLayersPtr->AddAllLayerNamesTo(layersToDelete);
    for (int i = 0; i < m_layersPropertyPtr->Size(); i++)
    {
        FName layerName(*sfPropertyUtil::ToString(m_layersPropertyPtr->Get(i)));
        m_layerNames.Add(layerName);
        if (m_worldLayersPtr->GetLayer(layerName) != nullptr)
        {
            layersToDelete.Remove(FName(*sfPropertyUtil::ToString(m_layersPropertyPtr->Get(i))));
        }
        else
        {
            m_worldLayersPtr->CreateLayer(layerName);
        }
    }
    m_worldLayersPtr->DeleteLayers(layersToDelete);
    RegisterOnLayersChangeHandler();

    TryToggleWorldComposition(GetWorldCompositionOnServer());

    // Set references to the world settings actor.
    std::vector<sfReferenceProperty::SPtr> references = m_sessionPtr->GetReferences(m_worldSettingsObjPtr);
    sfPropertyManager::Get().SetReferences(worldSettingsPtr, references);

    // Initialize children
    TSharedPtr<sfUObjectTranslator> uobjectTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfUObjectTranslator>(sfType::UObject);
    for (sfObject::SPtr childPtr : m_worldSettingsObjPtr->Children())
    {
        if (childPtr->Type() == sfType::GameMode)
        {
            OnCreateGameModeObject(childPtr);
        }
        else if (childPtr->Type() == sfType::UObject && uobjectTranslatorPtr.IsValid())
        {
            uobjectTranslatorPtr->InitializeUObject(worldSettingsPtr, childPtr);
        }
    }
}

void sfLevelTranslator::OnCreateGameModeObject(sfObject::SPtr objPtr)
{
    UClass* gameModePtr = m_worldPtr->GetWorldSettings()->DefaultGameMode;
    if (objPtr != nullptr && gameModePtr != nullptr && gameModePtr->IsInBlueprint())
    {
        AGameModeBase* defaultObjectPtr = gameModePtr->GetDefaultObject<AGameModeBase>();
        sfObjectMap::Add(objPtr, defaultObjectPtr);
        sfPropertyManager::Get().ApplyProperties(defaultObjectPtr, objPtr->Property()->AsDict());
    }
}

void sfLevelTranslator::OnDelete(sfObject::SPtr objPtr)
{
    ULevel* levelPtr = Cast<ULevel>(sfObjectMap::Remove(objPtr));
    if (objPtr->Type() != sfType::Level)
    {
        return;
    }

    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    FString levelPath = *sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
    m_unloadedLevelObjects.Remove(levelPath);
    if (levelPtr == nullptr)
    {
        return;
    }

    sfObject::SPtr propertiesObjPtr = sfUnrealUtils::FindChildByType(objPtr, sfType::LevelProperties);
    sfObjectMap::Remove(propertiesObjPtr);

    FDelegateHandle handle;
    if (m_onLevelTransformChangeHandles.RemoveAndCopyValue(levelPtr, handle))
    {
        levelPtr->OnApplyLevelTransform.Remove(handle);
    }

    // Temporarily remove PrepareToCleanseEditorObject event handler
    FEditorSupportDelegates::PrepareToCleanseEditorObject.Remove(m_onPrepareToCleanseEditorObjectHandle);

    m_actorTranslatorPtr->OnRemoveLevel(objPtr, levelPtr); // Remove actors in this level from actor translator

    // When a level is unloaded, any actors you had selected will be unselected.
    // We need to record those actors that are not in the level to be unloaded and reselect them after.
    TArray<AActor*> selectedActors;
    for (auto iter = GEditor->GetSelectedActorIterator(); iter; ++iter)
    {
        AActor* actorPtr = Cast<AActor>(*iter);
        if (actorPtr && actorPtr->GetLevel() != levelPtr)
        {
            selectedActors.Add(actorPtr);
        }
    }

    FEdMode* activeMode = GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_StreamingLevel);
    ULevelStreaming* streamingLevelPtr = FLevelUtils::FindStreamingLevel(levelPtr);
    if (activeMode != nullptr && streamingLevelPtr != nullptr)
    {
        // Toggle streaming level viewport transform editing off
        GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_StreamingLevel);
    }

    // Prompt to save level
    FEditorFileUtils::PromptForCheckoutAndSave(
        TArray<UPackage*>{levelPtr->GetOutermost()}, true, true, nullptr, false, false);
    UEditorLevelUtils::RemoveLevelFromWorld(levelPtr); // Remove level from world

    // Reselect actors
    for (AActor* actorPtr : selectedActors)
    {
        GEditor->SelectActor(actorPtr, true, true, true);
    }

    // Add PrepareToCleanseEditorObject event handler back
    m_onPrepareToCleanseEditorObjectHandle = FEditorSupportDelegates::PrepareToCleanseEditorObject.AddRaw(
        this,
        &sfLevelTranslator::OnPrepareToCleanseEditorObject);

    // Refresh levels window
    FEditorDelegates::RefreshLevelBrowser.Broadcast();

    // Notify the Scene Outliner
    GEngine->BroadcastLevelActorListChanged();
}

void sfLevelTranslator::OnPropertyChange(sfProperty::SPtr propertyPtr)
{
    sfProperty::SPtr parentPropertyPtr = propertyPtr->GetParentProperty();
    if (parentPropertyPtr != nullptr &&
        parentPropertyPtr->Type() == sfProperty::Types::LIST &&
        parentPropertyPtr->AsList() == m_layersPropertyPtr)
    {
        UnregisterOnLayersChangeHandler();
        FName newLayerName(*sfPropertyUtil::ToString(propertyPtr));
        FName oldLayerName = m_layerNames[propertyPtr->Index()];
        m_worldLayersPtr->RenameLayer(oldLayerName, newLayerName);
        m_layerNames.Empty();
        m_worldLayersPtr->AddAllLayerNamesTo(m_layerNames);
        RegisterOnLayersChangeHandler();
        return;
    }
    sfBaseUObjectTranslator::OnPropertyChange(propertyPtr);
}

void sfLevelTranslator::OnListAdd(sfListProperty::SPtr listPtr, int index, int count)
{
    if (listPtr == m_layersPropertyPtr)
    {
        UnregisterOnLayersChangeHandler();
        for (int i = 0; i < count; i++)
        {
            FName layerName(*sfPropertyUtil::ToString(m_layersPropertyPtr->Get(index + i)));
            m_layerNames.Insert(layerName, index + i);
            if (m_worldLayersPtr->GetLayer(layerName) == nullptr)
            {
                m_worldLayersPtr->CreateLayer(layerName);
            }
        }
        RegisterOnLayersChangeHandler();
        return;
    }
    if (listPtr->GetPath() == *sfProp::HierarchicalLODSetup)
    {
        m_hierarchicalLODSetupDirty = true;
    }
    sfBaseUObjectTranslator::OnListAdd(listPtr, index, count);
}

void sfLevelTranslator::OnListRemove(sfListProperty::SPtr listPtr, int index, int count)
{
    if (listPtr == m_layersPropertyPtr)
    {
        UnregisterOnLayersChangeHandler();
        TArray<FName> layersToDelete;
        for (int i = 0; i < count; i++)
        {
            layersToDelete.Add(m_layerNames[index]);
            m_layerNames.RemoveAt(index);
        }
        m_worldLayersPtr->DeleteLayers(layersToDelete);
        RegisterOnLayersChangeHandler();
        return;
    }
    sfBaseUObjectTranslator::OnListRemove(listPtr, index, count);
}

ULevel* sfLevelTranslator::FindLevelInLoadedLevels(FString levelPath, bool isPersistentLevel)
{
    // Try to find level in loaded levels
    if (isPersistentLevel)
    {
        if (m_worldPtr->PersistentLevel->GetOutermost()->GetName() == levelPath)
        {
            return m_worldPtr->PersistentLevel;
        }
    }
    else
    {
        ULevelStreaming* streamingLevelPtr = FLevelUtils::FindStreamingLevel(m_worldPtr, *levelPath);
        if (streamingLevelPtr != nullptr)
        {
            return streamingLevelPtr->GetLoadedLevel();
        }
    }
    return nullptr;
}

ULevel* sfLevelTranslator::LoadOrCreateMap(const FString& levelPath, bool isPersistentLevel)
{
    ULevel* levelPtr = nullptr;
    if (!levelPath.StartsWith("/Temp/") && FPackageName::DoesPackageExist(levelPath))
    {
        bool cancel = false;
        levelPtr = TryLoadLevelFromFile(levelPath, isPersistentLevel, cancel);
        if (cancel)
        {
            KS::Log::Info("Disconnecting because the user cancelled saving the open level(s).", LOG_CHANNEL);
            SceneFusion::Service->LeaveSession();
            return nullptr;
        }
    }

    if (levelPtr == nullptr)
    {
        if (!levelPath.StartsWith("/Temp/"))
        {
            KS::Log::Warning("Could not find level " + std::string(TCHAR_TO_UTF8(*levelPath)) +
                ". Please make sure that your project is up to date.", LOG_CHANNEL);
        }
        levelPtr = CreateMap(levelPath, isPersistentLevel);
    }

    if (levelPtr == nullptr)
    {
        KS::Log::Info("Disconnecting because the user cancelled saving the open level(s).", LOG_CHANNEL);
        SceneFusion::Service->LeaveSession();
    }

    return levelPtr;
}

ULevel* sfLevelTranslator::TryLoadLevelFromFile(FString levelPath, bool isPersistentLevel, bool& cancel)
{
    cancel = false;
    // Load map if the level is the persistent level
    if (isPersistentLevel)
    {
        // Loading a level triggers attach events we want to ignore.
        m_actorTranslatorPtr->DisableParentChangeHandler();
        // Prompts the user to save the dirty levels before load map
        if (FEditorFileUtils::SaveDirtyPackages(true, true, false) &&
            FEditorFileUtils::LoadMap(levelPath, false, true))
        {
            m_actorTranslatorPtr->EnableParentChangeHandler();
            // When a new map was loaded as the persistent level, all avatar actors were destroyed.
            // We need to recreate them.
            TSharedPtr<sfAvatarTranslator> avatarTranslatorPtr
                = SceneFusion::Get().GetTranslator<sfAvatarTranslator>(sfType::Avatar);
            if (avatarTranslatorPtr.IsValid())
            {
                avatarTranslatorPtr->RecreateAllAvatars();
            }
            m_actorTranslatorPtr->ClearActorCollections();
            sfSelectionManager::Get().Clear();
            m_worldPtr = GEditor->GetEditorWorldContext().World();
            return m_worldPtr->PersistentLevel;
        }
        else
        {
            cancel = true;
        }
        m_actorTranslatorPtr->EnableParentChangeHandler();
    }
    else // Add level to world if it is a streaming level
    {
        // Record current level so we can set it back
        ULevel* currentLevelPtr = m_worldPtr->GetCurrentLevel();

        // Add level
        ULevelStreaming* streamingLevelPtr = UEditorLevelUtils::AddLevelToWorld(m_worldPtr,
            *levelPath,
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 21
            ULevelStreamingDynamic::StaticClass());
#else
            GetDefault<ULevelEditorMiscSettings>()->DefaultLevelStreamingClass);
#endif
        if (streamingLevelPtr)
        {
            // Restore current level
            if (m_worldPtr->SetCurrentLevel(currentLevelPtr))
            {
                FEditorDelegates::NewCurrentLevel.Broadcast();
            }

            return streamingLevelPtr->GetLoadedLevel();
        }
    }
    return nullptr;
}

ULevel* sfLevelTranslator::CreateMap(FString levelPath, bool isPersistentLevel)
{
    if (isPersistentLevel)
    {
        GEditor->CreateNewMapForEditing();
        if (GWorld != m_worldPtr)
        {
            m_worldPtr = GWorld;
            if (!levelPath.StartsWith("/Temp/"))
            {
                FEditorFileUtils::SaveLevel(m_worldPtr->PersistentLevel, levelPath);
            }
            // When the new map was created as the persistent level, all avatar actors were destroyed.
            // We need to recreate them.
            TSharedPtr<sfAvatarTranslator> avatarManagerPtr
                = SceneFusion::Get().GetTranslator<sfAvatarTranslator>(sfType::Avatar);
            if (avatarManagerPtr.IsValid())
            {
                avatarManagerPtr->RecreateAllAvatars();
            }
            m_actorTranslatorPtr->ClearActorCollections();
            sfSelectionManager::Get().Clear();
            return m_worldPtr->PersistentLevel;
        }
    }
    else
    {
        ULevel* currentLevelPtr = m_worldPtr->GetCurrentLevel();
        ULevelStreaming* streamingLevelPtr = UEditorLevelUtils::CreateNewStreamingLevel(
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 22
            ULevelStreamingDynamic::StaticClass(), levelPath, false);
#else
            GetDefault<ULevelEditorMiscSettings>()->DefaultLevelStreamingClass, levelPath, false);
#endif
        if (m_worldPtr->SetCurrentLevel(currentLevelPtr))
        {
            FEditorDelegates::NewCurrentLevel.Broadcast();
        }
        if (streamingLevelPtr)
        {
            return streamingLevelPtr->GetLoadedLevel();
        }
    }
    return nullptr;
}

void sfLevelTranslator::UploadLevel(ULevel* levelPtr)
{
    // Ignore buffer level. The buffer level is a temporary level used when moving actors to a different level.
    if (levelPtr == nullptr ||
        levelPtr->GetOutermost() == GetTransientPackage())
    {
        return;
    }
    if (!levelPtr->IsPersistentLevel() && m_sessionPtr->GetObjectLimit(sfType::Level) == 1)
    {
        // Temporarily remove PrepareToCleanseEditorObject event handler
        FEditorSupportDelegates::PrepareToCleanseEditorObject.Remove(m_onPrepareToCleanseEditorObjectHandle);

        // Multi-level syncing is disabled. Unload the level.
        UEditorLevelUtils::RemoveLevelFromWorld(levelPtr);

        // Add PrepareToCleanseEditorObject event handler back
        m_onPrepareToCleanseEditorObjectHandle = FEditorSupportDelegates::PrepareToCleanseEditorObject.AddRaw(
            this,
            &sfLevelTranslator::OnPrepareToCleanseEditorObject);

        // Refresh levels window
        FEditorDelegates::RefreshLevelBrowser.Broadcast();

        // Notify the Scene Outliner
        GEngine->BroadcastLevelActorListChanged();

        SceneFusion::Get().ShowUpgradeLink(SceneFusion::RestrictedFeature::MULTI_LEVEL);
        if (!m_showedDisabledMessage)
        {
            m_showedDisabledMessage = true;
            FText message = FText::FromString(
                "Multi-level syncing is disabled on your account. Upgrade to enable multi-level syncing.");
            FText title = FText::FromString("Scene Fusion");
            FMessageDialog::Open(EAppMsgType::Ok, message, &title);
        }
        return;
    }
    // Create level object
    sfObject::ObjectFlags flags = levelPtr->IsPersistentLevel() ? sfObject::NoFlags : sfObject::OptionalChildren;
    sfObject::SPtr levelObjPtr = sfObjectMap::GetOrCreateSFObject(levelPtr, sfType::Level, flags);
    if (levelObjPtr->IsSyncing())
    {
        return;
    }

    // Get level path
    FString levelPath = levelPtr->GetOutermost()->GetName();

    bool worldCompositionEnabled = m_worldPtr->GetWorldSettings()->bEnableWorldComposition;
    if (!levelPtr->IsPersistentLevel())
    {
        // Upload persistent level first
        if (SceneFusion::IsSessionCreator && m_worldSettingsObjPtr == nullptr)
        {
            UploadLevel(m_worldPtr->PersistentLevel);
        }

        // Upload parent level first
        if (worldCompositionEnabled)
        {
            FWorldTileInfo tileInfo = m_worldPtr->WorldComposition->GetTileInfo(FName(*levelPath));
            if (tileInfo.ParentTilePackageName != "None")
            {
                ULevelStreaming* streamingLevelPtr = FLevelUtils::FindStreamingLevel(
                    m_worldPtr, *tileInfo.ParentTilePackageName);
                if (streamingLevelPtr != nullptr && streamingLevelPtr->GetLoadedLevel() != nullptr)
                {
                    UploadLevel(streamingLevelPtr->GetLoadedLevel());
                }
            }
        }
    }

    sfDictionaryProperty::SPtr propertiesPtr = levelObjPtr->Property()->AsDict();
    propertiesPtr->Set(sfProp::Name, sfPropertyUtil::FromString(levelPath));
    propertiesPtr->Set(sfProp::IsPersistentLevel, sfValueProperty::Create(levelPtr->IsPersistentLevel()));

    sfDictionaryProperty::SPtr levelPropertiesPtr = nullptr;
    sfObject::SPtr propertiesObjPtr = nullptr;

    if (levelPtr->IsPersistentLevel())
    {
        AWorldSettings* worldSettingsPtr = m_worldPtr->GetWorldSettings();
        propertiesObjPtr = sfObjectMap::GetOrCreateSFObject(worldSettingsPtr, sfType::LevelProperties);
        levelPropertiesPtr = propertiesObjPtr->Property()->AsDict();

        levelPropertiesPtr->Set(sfProp::WorldComposition, sfValueProperty::Create(worldCompositionEnabled));
        m_worldSettingsObjPtr = propertiesObjPtr;

        InitializeChildren(propertiesObjPtr);
        sfPropertyManager::Get().CreateProperties(worldSettingsPtr, levelPropertiesPtr, &WORLD_SETTINGS_BLACKLIST);

        // Create sfObject for the game mode
        UClass* gameModePtr = worldSettingsPtr->DefaultGameMode;
        if (gameModePtr != nullptr && gameModePtr->IsInBlueprint())
        {
            AGameModeBase* defaultObjectPtr = gameModePtr->GetDefaultObject<AGameModeBase>();
            sfDictionaryProperty::SPtr gameModePropertiesPtr = sfDictionaryProperty::Create();
            sfObject::SPtr gameModeObjPtr = sfObject::Create(sfType::GameMode, gameModePropertiesPtr);
            sfPropertyManager::Get().CreateProperties(defaultObjectPtr, gameModePropertiesPtr);
            sfObjectMap::Add(gameModeObjPtr, defaultObjectPtr);
            m_worldSettingsObjPtr->AddChild(gameModeObjPtr);
        }

        // Create layers properties
        m_layersPropertyPtr = sfListProperty::Create();
        m_worldLayersPtr->AddAllLayerNamesTo(m_layerNames);
        for (int i = 0; i < m_layerNames.Num(); i++)
        {
            m_layersPropertyPtr->Add(sfPropertyUtil::FromString(m_layerNames[i].ToString()));
        }
        levelPropertiesPtr->Set(sfProp::Layers, m_layersPropertyPtr);
    }
    else
    {
        // Sublevel transform properties
        ULevelStreaming* streamingLevelPtr = FLevelUtils::FindStreamingLevel(levelPtr);
        if (streamingLevelPtr != nullptr)
        {
            propertiesObjPtr = sfObjectMap::GetOrCreateSFObject(streamingLevelPtr, sfType::LevelProperties);
            levelPropertiesPtr = propertiesObjPtr->Property()->AsDict();

            // Set transform properties
            FTransform transform = streamingLevelPtr->LevelTransform;
            propertiesPtr->Set(sfProp::Location, sfPropertyUtil::FromVector(transform.GetLocation()));
            propertiesPtr->Set(sfProp::Rotation, sfValueProperty::Create(transform.Rotator().Yaw));
            FDelegateHandle handle = levelPtr->OnApplyLevelTransform.AddLambda(
                [this, levelPtr](const FTransform& transform) {
                m_movedLevels.emplace(levelPtr);
            });

            m_onLevelTransformChangeHandles.Add(levelPtr, handle);// Add transform change handler on level

            // Set folder property
            levelPropertiesPtr->Set(sfProp::Folder,
                sfPropertyUtil::FromString(streamingLevelPtr->GetFolderPath().ToString()));

            InitializeChildren(propertiesObjPtr);
            sfPropertyManager::Get().CreateProperties(streamingLevelPtr, levelPropertiesPtr, &PROPERTY_BLACKLIST);
        }
        else
        {
            propertiesObjPtr = sfObject::Create(sfType::LevelProperties, sfDictionaryProperty::Create());
            levelPropertiesPtr = propertiesObjPtr->Property()->AsDict();
        }

        // Sublevel properties
        if (worldCompositionEnabled)
        {
            // Other tile detail properties
            UObject* worldTileDetailsObjPtr = FindWorldTileDetailsObject(levelPath);
            if (worldTileDetailsObjPtr != nullptr)
            {
                sfPropertyManager::Get().CreateProperties(worldTileDetailsObjPtr, levelPropertiesPtr);
            }
        }
    }

    levelObjPtr->AddChild(propertiesObjPtr);

    for (AActor* actorPtr : levelPtr->Actors)
    {
        if (m_actorTranslatorPtr->IsSyncable(actorPtr) && actorPtr->GetAttachParentActor() == nullptr)
        {
            sfObject::SPtr objPtr = m_actorTranslatorPtr->CreateObject(actorPtr);
            if (objPtr != nullptr)
            {
                levelObjPtr->AddChild(objPtr);
            }
        }
    }

    OnUploadLevel.Broadcast(levelObjPtr, levelPtr);

    // Create
    m_sessionPtr->Create(levelObjPtr);

    // Sync the level blueprint
    if (levelPtr->IsPersistentLevel())
    {
        TSharedPtr<sfBlueprintTranslator> blueprintTranslatorPtr
            = SceneFusion::Get().GetTranslator<sfBlueprintTranslator>(sfType::Blueprint);
        if (blueprintTranslatorPtr.IsValid())
        {
            blueprintTranslatorPtr->UploadBlueprint(m_worldPtr->PersistentLevel->GetLevelScriptBlueprint());
        }
    }

    // Sync actor order after creating actors
    sfListProperty::SPtr actorListPtr = sfListProperty::Create();
    for (AActor* actorPtr : levelPtr->Actors)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
        if (objPtr != nullptr && objPtr->IsSyncing())
        {
            actorListPtr->Add(sfValueProperty::Create(objPtr->Id()));
        }
    }
    levelPropertiesPtr->Set(sfProp::Actors, actorListPtr);
}

bool sfLevelTranslator::Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr)
{
    if (uobjPtr->IsA<AWorldSettings>() || uobjPtr->IsA<ULevelStreaming>())
    {
        outObjPtr = sfObject::Create(sfType::LevelProperties, sfDictionaryProperty::Create());
        sfObjectMap::Add(outObjPtr, uobjPtr);
        return true;
    }
    ULevel* levelPtr = Cast<ULevel>(uobjPtr);
    if (levelPtr != nullptr)
    {
        sfObject::ObjectFlags flags = levelPtr->IsPersistentLevel() ?
            sfObject::ObjectFlags::NoFlags : sfObject::ObjectFlags::OptionalChildren;
        outObjPtr = sfObject::Create(sfType::Level, sfDictionaryProperty::Create(), flags);
        sfObjectMap::Add(outObjPtr, uobjPtr);
        return true;
    }
    return false;
}

void sfLevelTranslator::OnAddLevelToWorld(ULevel* newLevelPtr, UWorld* worldPtr)
{
    if (worldPtr != m_worldPtr || sfObjectMap::Contains(newLevelPtr))
    {
        return;
    }

    FString levelPath = newLevelPtr->GetOutermost()->GetName();
    sfObject::SPtr levelObjPtr;
    if (m_unloadedLevelObjects.RemoveAndCopyValue(levelPath, levelObjPtr))
    {
        m_sessionPtr->SubscribeToChildren(levelObjPtr);
        sfObjectMap::Add(levelObjPtr, newLevelPtr);
        m_levelsWaitingForChildren.emplace(levelObjPtr);
    }
    else
    {
        RequestLock();
        m_levelsToUpload.emplace(newLevelPtr);
    }
}

void sfLevelTranslator::OnPrepareToCleanseEditorObject(UObject* uobjPtr)
{
    // Disconnect if the world is going to be destroyed
    UWorld* worldPtr = Cast<UWorld>(uobjPtr);

    if (worldPtr == m_worldPtr)
    {
        KS::Log::Info("World destroyed. Disconnect from server.", LOG_CHANNEL);
        m_worldPtr = nullptr;
        SceneFusion::Service->LeaveSession();
        SceneFusion::Get().CleanUp();
        return;
    }

    // If the object is a level, unregister the local player from its whitelist on the server side
    ULevel* levelPtr = Cast<ULevel>(uobjPtr);
    if (levelPtr == nullptr)
    {
        return;
    }

    // Remove objects in the level from the uobjectTranslator's pending delete list
    TSharedPtr<sfUObjectTranslator> uobjectTranslatorPtr =
        SceneFusion::Get().GetTranslator<sfUObjectTranslator>(sfType::UObject);
    if (uobjectTranslatorPtr.IsValid())
    {
        uobjectTranslatorPtr->RemovePendingDeletionsInLevel(levelPtr);
    }

    // Allow objects in the level to be garbage collected. Not doing this can cause crashes.
    UsfReferenceTracker::Get().RemoveObjectsInLevel(levelPtr);

    m_levelsToUpload.erase(levelPtr);
    m_dirtyParentLevels.Remove(levelPtr);
    FDelegateHandle handle;
    if (m_onLevelTransformChangeHandles.RemoveAndCopyValue(levelPtr, handle))
    {
        levelPtr->OnApplyLevelTransform.Remove(handle);
    }

    sfObject::SPtr levelObjPtr = sfObjectMap::Remove(levelPtr);
    m_actorTranslatorPtr->OnRemoveLevel(levelObjPtr, levelPtr); // Clear raw pointers to actors in this level
    if (levelObjPtr != nullptr)
    {
        ULevelStreaming* streamingLevelPtr = FLevelUtils::FindStreamingLevel(levelPtr);
        sfObjectMap::Remove(streamingLevelPtr);

        m_levelsWaitingForChildren.erase(levelObjPtr);

        if (m_worldPtr->GetWorldSettings()->bEnableWorldComposition)
        {
            FString levelPath = levelPtr->GetOutermost()->GetName();
            m_unloadedLevelObjects.Add(levelPath, levelObjPtr);
            m_sessionPtr->UnsubscribeFromChildren(levelObjPtr);
        }
        else if (levelObjPtr->IsLocked())
        {
            m_levelsNeedToBeLoaded.emplace(levelObjPtr);
        }
        else
        {
            m_sessionPtr->Delete(levelObjPtr);
        }
    }
}

void sfLevelTranslator::UploadUnsyncedLevels()
{
    for (FConstLevelIterator iter = m_worldPtr->GetLevelIterator(); iter; ++iter)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(*iter);
        if (objPtr == nullptr || !objPtr->IsSyncing())
        {
            RequestLock();
            m_levelsToUpload.emplace(*iter);
        }
    }

    // Refresh levels window
    FEditorDelegates::RefreshLevelBrowser.Broadcast();
}

void sfLevelTranslator::SendTransformUpdate(ULevel* levelPtr)
{
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(levelPtr);
    if (objPtr == nullptr)
    {
        return;
    }

    ULevelStreaming* streamingLevelPtr = FLevelUtils::FindStreamingLevel(levelPtr);
    if (streamingLevelPtr == nullptr)
    {
        return;
    }

    FTransform transform = streamingLevelPtr->LevelTransform;
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    sfProperty::SPtr oldPropPtr;

    if (objPtr->IsLocked())
    {
        // Revert level offset transform
        if (!propertiesPtr->TryGet(sfProp::Location, oldPropPtr) ||
            transform.GetLocation() != sfPropertyUtil::ToVector(oldPropPtr))
        {
            transform.SetLocation(sfPropertyUtil::ToVector(oldPropPtr));
            ModifyLevelWithoutTriggerEvent(levelPtr, [streamingLevelPtr, transform]()
            {
                FLevelUtils::SetEditorTransform(streamingLevelPtr, transform);
            });
        }
        if (!propertiesPtr->TryGet(sfProp::Rotation, oldPropPtr) ||
            transform.Rotator().Yaw != oldPropPtr->AsValue()->GetValue().GetFloat())
        {
            FRotator rotation = transform.Rotator();
            rotation.Yaw = oldPropPtr->AsValue()->GetValue();
            transform.SetRotation(rotation.Quaternion());
            ModifyLevelWithoutTriggerEvent(levelPtr, [streamingLevelPtr, transform]()
            {
                FLevelUtils::SetEditorTransform(streamingLevelPtr, transform);
            });
        }

        // Unreal may set the transform again after we reverted it. In that case, we need to revert it again.
        if (!streamingLevelPtr->LevelTransform.Equals(transform))
        {
            ModifyLevelWithoutTriggerEvent(levelPtr, [streamingLevelPtr, transform]()
            {
                FLevelUtils::SetEditorTransform(streamingLevelPtr, transform);
            });
        }
    }
    else
    {
        if (!propertiesPtr->TryGet(sfProp::Location, oldPropPtr) ||
            transform.GetLocation() != sfPropertyUtil::ToVector(oldPropPtr))
        {
            propertiesPtr->Set(sfProp::Location, sfPropertyUtil::FromVector(transform.GetLocation()));
        }

        if (!propertiesPtr->TryGet(sfProp::Rotation, oldPropPtr) ||
            transform.Rotator().Yaw != oldPropPtr->AsValue()->GetValue().GetFloat())
        {
            propertiesPtr->Set(sfProp::Rotation, sfValueProperty::Create(transform.Rotator().Yaw));
        }
    }
}

void sfLevelTranslator::RegisterPropertyChangeHandlers()
{
    m_propertyChangeHandlers[sfProp::Location] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        ULevelStreaming* streamingLevelPtr = Cast<ULevelStreaming>(uobjPtr);
        if (streamingLevelPtr == nullptr)
        {
            return true;
        }
        FTransform transform = streamingLevelPtr->LevelTransform;
        transform.SetLocation(sfPropertyUtil::ToVector(propertyPtr));
        ModifyLevelWithoutTriggerEvent(streamingLevelPtr->GetLoadedLevel(), [streamingLevelPtr, transform]()
        {
            FLevelUtils::SetEditorTransform(streamingLevelPtr, transform);
        });
        SceneFusion::RedrawActiveViewport();
        return true;
    };

    m_propertyChangeHandlers[sfProp::Rotation] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        ULevelStreaming* streamingLevelPtr = Cast<ULevelStreaming>(uobjPtr);
        if (streamingLevelPtr == nullptr)
        {
            return true;
        }
        FTransform transform = streamingLevelPtr->LevelTransform;
        FRotator rotation = transform.Rotator();
        rotation.Yaw = propertyPtr->AsValue()->GetValue();
        transform.SetRotation(rotation.Quaternion());
        ModifyLevelWithoutTriggerEvent(streamingLevelPtr->GetLoadedLevel(), [streamingLevelPtr, transform]()
        {
            FLevelUtils::SetEditorTransform(streamingLevelPtr, transform);
        });
        SceneFusion::RedrawActiveViewport();
        return true;
    };

    m_propertyChangeHandlers[sfProp::Folder] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        ULevelStreaming* streamingLevelPtr = Cast<ULevelStreaming>(uobjPtr);
        if (streamingLevelPtr == nullptr)
        {
            return true;
        }
        ModifyLevelWithoutTriggerEvent(streamingLevelPtr->GetLoadedLevel(), [streamingLevelPtr, propertyPtr]()
        {
            streamingLevelPtr->SetFolderPath(*sfPropertyUtil::ToString(propertyPtr));
        });
        FEditorDelegates::RefreshLevelBrowser.Broadcast();
        return true;
    };

    m_propertyChangeHandlers[sfProp::WorldComposition] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        TryToggleWorldComposition(propertyPtr->AsValue()->GetValue());
        return true;
    };

    m_propertyChangeHandlers[sfProp::DefaultGameMode] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        if (m_worldSettingsObjPtr != nullptr)
        {
            AWorldSettings* worldSettingsPtr = m_worldPtr->GetWorldSettings();
            sfUPropertyInstance upropInstance = sfPropertyManager::Get().FindUProperty(worldSettingsPtr, propertyPtr);
            if (upropInstance.IsValid())
            {
                sfPropertyManager::Get().SetValue(worldSettingsPtr, upropInstance, propertyPtr);
            }
            else
            {
                UnrealProperty* upropPtr = uobjPtr->GetClass()->FindPropertyByName(sfProp::DefaultGameMode->c_str());
                if (upropPtr != nullptr)
                {
                    sfPropertyManager::Get().SetToDefaultValue(uobjPtr, upropPtr);
                }
            }

            sfObject::SPtr gameModeObjPtr = m_worldSettingsObjPtr->Child(0);
            if (gameModeObjPtr != nullptr)
            {
                UClass* gameModePtr = m_worldPtr->GetWorldSettings()->DefaultGameMode;
                sfObjectMap::Remove(gameModeObjPtr);
                if (gameModePtr != nullptr)
                {
                    sfObjectMap::Add(gameModeObjPtr, gameModePtr->GetDefaultObject());
                }
            }
        }
        return true;
    };

    m_propertyChangeHandlers[sfProp::Actors] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        ULevel* levelPtr = sfObjectMap::Get<ULevel>(propertyPtr->GetContainerObject()->Parent());
        if (levelPtr != nullptr)
        {
            m_serverActorOrderChangedLevels.Add(levelPtr);
        }
        return true;
    };

    m_propertyChangeHandlers[sfProp::HierarchicalLODSetup] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        m_hierarchicalLODSetupDirty = true;
        return false;
    };
}

void sfLevelTranslator::OnUObjectModified(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    ULevelStreaming* streamingLevelPtr = Cast<ULevelStreaming>(uobjPtr);
    if (streamingLevelPtr != nullptr)
    {
        m_dirtyStreamingLevels.Add(streamingLevelPtr);
    }
}

void sfLevelTranslator::SendFolderChange(ULevelStreaming* streamingLevelPtr)
{
    if (streamingLevelPtr == nullptr || streamingLevelPtr->GetLoadedLevel() == nullptr)
    {
        return;
    }
    ULevel* levelPtr = streamingLevelPtr->GetLoadedLevel();

    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(levelPtr);
    if (objPtr == nullptr)
    {
        return;
    }

    sfObject::SPtr propertiesObjPtr = sfUnrealUtils::FindChildByType(objPtr, sfType::LevelProperties);
    if (propertiesObjPtr == nullptr)
    {
        return;
    }
    sfDictionaryProperty::SPtr propertiesPtr = propertiesObjPtr->Property()->AsDict();
    sfProperty::SPtr oldPropPtr;
    FString folder = streamingLevelPtr->GetFolderPath().ToString();

    if (!propertiesPtr->TryGet(sfProp::Folder, oldPropPtr) ||
        folder != sfPropertyUtil::ToString(oldPropPtr))
    {
        propertiesPtr->Set(sfProp::Folder, sfPropertyUtil::FromString(folder));
    }
}

bool sfLevelTranslator::OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    if (uobjPtr->IsA(UWorld::StaticClass()) || uobjPtr->IsA(ULayer::StaticClass()))
    {
        return SyncLayers();
    }
    if (uobjPtr->GetClass()->GetName() == "WorldTileDetails")
    {
        for (TFieldIterator<UnrealProperty> iter(uobjPtr->GetClass()); iter; ++iter)
        {
            OnTileDetailsChange(uobjPtr, *iter);
        }
        return true;
    }
    if (objPtr != nullptr)
    {
        ULevelStreaming* streamingLevelPtr = Cast<ULevelStreaming>(uobjPtr);
        if (streamingLevelPtr != nullptr)
        {
            SendFolderChange(streamingLevelPtr);
            sfPropertyManager::Get().SendPropertyChanges(streamingLevelPtr, objPtr->Property()->AsDict(),
                &PROPERTY_BLACKLIST);
            return true;
        }
        AWorldSettings* worldSettingsPtr = Cast<AWorldSettings>(uobjPtr);
        if (worldSettingsPtr != nullptr)
        {
            sfPropertyManager::Get().SendPropertyChanges(worldSettingsPtr, objPtr->Property()->AsDict(),
                &WORLD_SETTINGS_BLACKLIST);
            return true;
        }
    }
    return false;
}

void sfLevelTranslator::ModifyLevelWithoutTriggerEvent(ULevel* levelPtr, Callback callback)
{
    // Temporarily remove event handlers
    FDelegateHandle handle;
    if (m_onLevelTransformChangeHandles.RemoveAndCopyValue(levelPtr, handle))
    {
        levelPtr->OnApplyLevelTransform.Remove(handle);
    }
    SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();

    // Invoke callback function and prevents any changes to the undo stack during the call.
    sfUnrealUtils::PreserveUndoStack(callback);

    // Add event handlers back
    handle = levelPtr->OnApplyLevelTransform.AddLambda(
        [this, levelPtr](const FTransform& transform) {
        m_movedLevels.emplace(levelPtr);
    });
    m_onLevelTransformChangeHandles.Add(levelPtr, handle);
    SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
}

void sfLevelTranslator::RequestLock()
{
    if (m_lockObject == nullptr && SceneFusion::IsSessionCreator)
    {
        m_lockObject = sfObject::Create(sfType::LevelLock);
        m_sessionPtr->Create(m_lockObject);
    }

    if (m_lockObject != nullptr &&
        (m_sessionPtr->LocalUser() == nullptr ||
            m_lockObject->LockOwner() != m_sessionPtr->LocalUser()))
    {
        m_lockObject->RequestLock();
    }
}

void sfLevelTranslator::RegisterLevelEvents()
{
    m_onAddLevelToWorldHandle = FWorldDelegates::LevelAddedToWorld.AddRaw(this, &sfLevelTranslator::OnAddLevelToWorld);
    m_onPrepareToCleanseEditorObjectHandle = FEditorSupportDelegates::PrepareToCleanseEditorObject.AddRaw(
        this,
        &sfLevelTranslator::OnPrepareToCleanseEditorObject);
    m_onWorldCompositionChangeHandle = UWorldComposition::WorldCompositionChangedEvent.AddRaw(this,
        &sfLevelTranslator::SetWorldCompositionOnServer);
    m_onPackageMarkedDirtyHandle = UPackage::PackageMarkedDirtyEvent.AddRaw(
        this, &sfLevelTranslator::OnPackageMarkedDirty);
    m_onLevelDirtiedHandle = ULevel::LevelDirtiedEvent.AddRaw(this, &sfLevelTranslator::OnLevelDirtied);
    sfPropertyManager::Get().RegisterPropertyChangeHandlerForClass("WorldTileDetails",
        [this](UObject* uobjPtr, UnrealProperty* upropPtr) { OnTileDetailsChange(uobjPtr, upropPtr); });
    m_onPropertyChangeHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
        this, &sfLevelTranslator::OnUPropertyChange);
}

void sfLevelTranslator::UnregisterLevelEvents()
{
    FWorldDelegates::LevelAddedToWorld.Remove(m_onAddLevelToWorldHandle);
    FEditorSupportDelegates::PrepareToCleanseEditorObject.Remove(m_onPrepareToCleanseEditorObjectHandle);
    UWorldComposition::WorldCompositionChangedEvent.Remove(m_onWorldCompositionChangeHandle);
    UPackage::PackageMarkedDirtyEvent.Remove(m_onPackageMarkedDirtyHandle);
    ULevel::LevelDirtiedEvent.Remove(m_onLevelDirtiedHandle);
    sfPropertyManager::Get().UnregisterPropertyChangeHandlerForClass("WorldTileDetails");
    FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(m_onPropertyChangeHandle);
}

void sfLevelTranslator::OnAcknowledgeSubscription(bool isSubscription, sfObject::SPtr objPtr)
{
    if (!isSubscription || objPtr->Type() != sfType::Level)
    {
        return;
    }

    m_levelsWaitingForChildren.erase(objPtr);

    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    FString levelPath = *sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
    bool isPersistentLevel = propertiesPtr->Get(sfProp::IsPersistentLevel)->AsValue()->GetValue();
    ULevel* levelPtr = FindLevelInLoadedLevels(levelPath, isPersistentLevel);
    if (levelPtr != nullptr)
    {
        m_actorTranslatorPtr->DestroyUnsyncedActorsInLevel(levelPtr);
    }
}

void sfLevelTranslator::TryToggleWorldComposition(bool enableWorldComposition)
{
    if (enableWorldComposition == m_worldPtr->GetWorldSettings()->bEnableWorldComposition)
    {
        return;
    }

    UWorldComposition::WorldCompositionChangedEvent.Remove(m_onWorldCompositionChangeHandle);
    ToggleWorldComposition(enableWorldComposition);
    m_onWorldCompositionChangeHandle = UWorldComposition::WorldCompositionChangedEvent.AddRaw(this,
        &sfLevelTranslator::SetWorldCompositionOnServer);

    if (enableWorldComposition == m_worldPtr->GetWorldSettings()->bEnableWorldComposition)
    {
        if (!enableWorldComposition)
        {
            // Load all sublevels
            for (auto pair : m_unloadedLevelObjects)
            {
                OnCreateLevelObject(pair.Value);
            }
            m_unloadedLevelObjects.Empty();
        }

        // Refresh levels window and viewport
        FEditorDelegates::RefreshLevelBrowser.Broadcast();
        SceneFusion::RedrawActiveViewport();
    }
    else
    {
        KS::Log::Error("Failed to " +
            std::string(enableWorldComposition ? "enable" : "disable") +
            " world composition. Leaving session.", LOG_CHANNEL);
        SceneFusion::Service->LeaveSession();
    }
}

void sfLevelTranslator::ToggleWorldComposition(bool enableWorldComposition)
{
    if (!UWorldComposition::EnableWorldCompositionEvent.IsBound())
    {
        return;
    }

    TArray<FString> temporarilyUnloadedLevel;
    if (enableWorldComposition)
    {
        // Save dirty packages before unloading streaming levels
        TArray<UPackage*> packagesToSave;
        FEditorFileUtils::GetDirtyWorldPackages(packagesToSave);
        packagesToSave.Remove(m_worldPtr->PersistentLevel->GetOutermost());
        FEditorFileUtils::EPromptReturnCode result = FEditorFileUtils::PromptForCheckoutAndSave(
            packagesToSave, false, true, nullptr, false, true);
        if (result == FEditorFileUtils::EPromptReturnCode::PR_Cancelled ||
            result == FEditorFileUtils::EPromptReturnCode::PR_Failure)
        {
            return;
        }

        // Unload streaming levels
        TSet<ULevel*> levels(m_worldPtr->GetLevels());
        for (ULevel* levelPtr : levels)
        {
            if (!levelPtr->IsPersistentLevel())
            {
                sfObjectMap::Remove(levelPtr);
                temporarilyUnloadedLevel.Add(levelPtr->GetOutermost()->GetName());
                UEditorLevelUtils::RemoveLevelFromWorld(levelPtr);
            }
        }
    }

    // Set bEnableWorldComposition so when the world composition event is broadcast,
    // the event handlers can get the new value.
    m_worldPtr->GetWorldSettings()->bEnableWorldComposition = enableWorldComposition;
    bool result = UWorldComposition::EnableWorldCompositionEvent.Execute(m_worldPtr, enableWorldComposition);
    m_worldPtr->GetWorldSettings()->bEnableWorldComposition = result; // In case we failed to enable it

    // Load levels back
    bool cancel = false;
    for (FString levelPath : temporarilyUnloadedLevel)
    {
        TryLoadLevelFromFile(levelPath, false, cancel);
    }
}

void sfLevelTranslator::SetWorldCompositionOnServer(UWorld* worldPtr)
{
    if (m_worldSettingsObjPtr == nullptr)
    {
        return;
    }

    bool worldCompositionEnabled = worldPtr->GetWorldSettings()->bEnableWorldComposition;
    sfDictionaryProperty::SPtr worldSettingsPropertiesPtr = m_worldSettingsObjPtr->Property()->AsDict();
    sfProperty::SPtr oldPropPtr;
    if (!worldSettingsPropertiesPtr->TryGet(sfProp::WorldComposition, oldPropPtr) ||
        worldCompositionEnabled != oldPropPtr->AsValue()->GetValue().GetBool())
    {
        worldSettingsPropertiesPtr->Set(sfProp::WorldComposition, sfValueProperty::Create(worldCompositionEnabled));

        if (!worldCompositionEnabled)
        {
            // Load all sublevels
            for (auto iter = m_unloadedLevelObjects.CreateConstIterator(); iter; ++iter)
            {
                OnCreateLevelObject(iter.Value());
            }
            m_unloadedLevelObjects.Empty();
        }
    }
}

bool sfLevelTranslator::GetWorldCompositionOnServer()
{
    sfDictionaryProperty::SPtr worldSettingsPropertiesPtr = m_worldSettingsObjPtr->Property()->AsDict();
    return worldSettingsPropertiesPtr->Get(sfProp::WorldComposition)->AsValue()->GetValue();
}

bool sfLevelTranslator::IsLevelObjectInitialized(ULevel* levelPtr)
{
    sfObject::SPtr levelObjPtr = sfObjectMap::GetSFObject(levelPtr);
    return levelObjPtr != nullptr && m_levelsWaitingForChildren.find(levelObjPtr) == m_levelsWaitingForChildren.end();
}

void sfLevelTranslator::MarkActorOrderStale(ULevel* levelPtr)
{
    m_localActorOrderChangedLevels.Add(levelPtr);
}

void sfLevelTranslator::OnPackageMarkedDirty(UPackage* packagePtr, bool wasDirty)
{
    if (packagePtr == nullptr || !packagePtr->ContainsMap() || m_worldPtr == nullptr)
    {
        return;
    }

    // Sublevel can have parent only when world composition is enabled
    if (m_worldPtr->GetWorldSettings()->bEnableWorldComposition)
    {
        // Find level in loaded streaming levels
        ULevel* levelPtr = FindLevelInLoadedLevels(packagePtr->GetName(), false);
        if (levelPtr != nullptr && !m_uninitializedLevels.Contains(levelPtr))
        {
            m_dirtyParentLevels.Add(levelPtr);
        }
    }
}

void sfLevelTranslator::OnLevelDirtied()
{
    const FString& undoText = sfUndoManager::Get().GetUndoText();
    if (undoText == "MapSendToFront" || undoText == "MapSendToBack")
    {
        m_localActorOrderChangedLevels.Add(m_worldPtr->GetCurrentLevel());
    }

    m_actorTranslatorPtr->CheckLockLocationChanges();
}

bool sfLevelTranslator::OnUPropertyChange(sfObject::SPtr objPtr, UObject* uobjPtr, UnrealProperty* upropPtr)
{
    if (PROPERTY_BLACKLIST.Contains(upropPtr->GetName()))
    {
        return true;
    }

    if (upropPtr->GetFName() == sfProp::DefaultGameMode->c_str())
    {
        FTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([this](float deltaTime)
        {
            UClass* gameModePtr = m_worldPtr->GetWorldSettings()->DefaultGameMode;
            sfObject::SPtr gameModeObjPtr = m_worldSettingsObjPtr->Child(0);
            if (gameModePtr != nullptr && gameModePtr->IsInBlueprint())
            {
                AGameModeBase* defaultObjectPtr = gameModePtr->GetDefaultObject<AGameModeBase>();
                if (gameModeObjPtr == nullptr)
                {
                    sfDictionaryProperty::SPtr gameModePropertiesPtr = sfDictionaryProperty::Create();
                    gameModeObjPtr = sfObject::Create(sfType::GameMode, gameModePropertiesPtr);
                    sfPropertyManager::Get().CreateProperties(defaultObjectPtr, gameModePropertiesPtr);
                    sfObjectMap::Add(gameModeObjPtr, defaultObjectPtr);
                    m_worldSettingsObjPtr->AddChild(gameModeObjPtr);
                    m_sessionPtr->Create(gameModeObjPtr, m_worldSettingsObjPtr, 0);
                }
                else
                {
                    sfPropertyManager::Get().SendPropertyChanges(defaultObjectPtr,
                        gameModeObjPtr->Property()->AsDict());
                }
            }
            else if (gameModeObjPtr != nullptr)
            {
                sfObjectMap::Remove(gameModeObjPtr);
                m_sessionPtr->Delete(gameModeObjPtr);
            }
            return false;
        }));
    }

    return false;
}

void sfLevelTranslator::OnTileDetailsChange(UObject* uobjPtr, UnrealProperty* upropPtr)
{
    sfObject::SPtr levelObjPtr;
    sfDictionaryProperty::SPtr levelPropertiesPtr;
    if (!TryGetLevelObjectAndPropertyForTileDetailObject(uobjPtr, levelObjPtr, levelPropertiesPtr))
    {
        return;
    }

    bool applyServerValue = levelObjPtr->IsLocked() && upropPtr->GetFName() == sfProp::TilePosition->c_str();
    sfPropertyManager::Get().SyncProperty(levelPropertiesPtr->GetContainerObject(), uobjPtr, upropPtr,
        applyServerValue);
}

UObject* sfLevelTranslator::FindWorldTileDetailsObject(FString levelPath)
{
    if (m_worldTileDetailsClassPtr == nullptr || m_packageNamePropertyPtr == nullptr)
    {
        return nullptr;
    }

    TArray<UObject*> worldDetails;
    GetObjectsOfClass(
        m_worldTileDetailsClassPtr,
        worldDetails,
        false,
        RF_ClassDefaultObject,
        EInternalObjectFlags::PendingKill);

    for (auto iter = worldDetails.CreateConstIterator(); iter; ++iter)
    {
        if (levelPath == m_packageNamePropertyPtr->ContainerPtrToValuePtr<FName>(*iter)->ToString())
        {
            return *iter;
        }
    }

    return nullptr;
}

void sfLevelTranslator::OnUPropertyChange(UObject* uobjPtr, FPropertyChangedEvent& ev)
{
    if (!sfPropertyManager::Get().ListeningForPropertyChanges())
    {
        return;
    }

    if (ev.MemberProperty == nullptr)
    {
        if (!uobjPtr->IsA<UBlueprint>() && !uobjPtr->IsInBlueprint())
        {
            return;
        }

        // Send game mode changes
        UClass* classPtr = nullptr;
        UBlueprint* blueprintPtr = Cast<UBlueprint>(uobjPtr);
        if (blueprintPtr != nullptr)
        {
            classPtr = blueprintPtr->GeneratedClass;
        }
        else
        {
            classPtr = uobjPtr->GetClass();
            if (uobjPtr != classPtr->GetDefaultObject())
            {
                return;
            }
        }
        if (classPtr == nullptr || classPtr != m_worldPtr->GetWorldSettings()->DefaultGameMode)
        {
            return;
        }
        sfObject::SPtr gameModeObjPtr = m_worldSettingsObjPtr->Child(0);
        if (classPtr != nullptr && gameModeObjPtr != nullptr)
        {
            sfPropertyManager::Get().SendPropertyChanges(classPtr->GetDefaultObject(),
                gameModeObjPtr->Property()->AsDict());
        }
        return;
    }

    if (uobjPtr->GetClass()->GetFName() == "WorldTileDetails" &&
        ev.MemberProperty->GetFName() == sfProp::TilePosition->c_str())
    {
        sfObject::SPtr levelObjPtr;
        sfDictionaryProperty::SPtr levelPropertiesPtr;
        if (!TryGetLevelObjectAndPropertyForTileDetailObject(uobjPtr, levelObjPtr, levelPropertiesPtr))
        {
            return;
        }

        if (levelObjPtr->IsLocked())
        {
            sfProperty::SPtr oldPropPtr;
            if (levelPropertiesPtr->TryGet(sfProp::TilePosition, oldPropPtr))
            {
                sfUPropertyInstance upropInstance = sfPropertyManager::Get().FindUProperty(uobjPtr, oldPropPtr);
                if (upropInstance.IsValid())
                {
                    sfPropertyManager::Get().SetValue(uobjPtr, upropInstance, oldPropPtr);
                }
            }
        }
    }
}

bool sfLevelTranslator::TryGetLevelObjectAndPropertyForTileDetailObject(
    UObject* worldTileDetailPtr,
    sfObject::SPtr& levelObjPtr,
    sfDictionaryProperty::SPtr& levelPropertiesPtr)
{
    // Get level path
    UnrealProperty* packageNamePropPtr = worldTileDetailPtr->GetClass()->FindPropertyByName(sfProp::PackageName->c_str());
    FString levelPath = packageNamePropPtr->ContainerPtrToValuePtr<FName>(worldTileDetailPtr)->ToString();

    // Get level
    ULevel* levelPtr = FindLevelInLoadedLevels(levelPath, false);
    levelObjPtr = sfObjectMap::GetSFObject(levelPtr);
    if (levelObjPtr == nullptr)
    {
        return false;
    }
    sfObject::SPtr propertiesObjPtr = sfUnrealUtils::FindChildByType(levelObjPtr, sfType::LevelProperties);
    if (propertiesObjPtr == nullptr)
    {
        return false;
    }
    levelPropertiesPtr = propertiesObjPtr->Property()->AsDict();
    return true;
}

void sfLevelTranslator::RefreshWorldSettingsTab()
{
    FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
    TSharedPtr<FTabManager> tabManager = LevelEditorModule.GetLevelEditorTabManager();
    if (tabManager.IsValid())
    {
        TSharedPtr<SDockTab> worldSettingsTab = tabManager->FindExistingLiveTab(FName("WorldSettingsTab"));
        if (worldSettingsTab.IsValid())
        {
            TSharedPtr<IDetailsView> detailView = StaticCastSharedPtr<IDetailsView>(
                sfUnrealUtils::FindWidget(worldSettingsTab->GetContent(), "SDetailsView"));
            if (detailView.IsValid())
            {
                detailView->ForceRefresh();
            }
        }
    }
}

UObject* sfLevelTranslator::GetUObject(sfObject::SPtr objPtr)
{
    if (objPtr == m_worldSettingsObjPtr)
    {
        m_worldSettingsDirty = true;
    }
    else if (objPtr->Type() == sfType::LevelProperties && GetWorldCompositionOnServer())
    {
        sfObject::SPtr levelObjPtr = objPtr->Parent();
        ULevel* levelPtr = sfObjectMap::Get<ULevel>(levelObjPtr);
        if (levelPtr != nullptr)
        {
            return FindWorldTileDetailsObject(levelPtr->GetOutermost()->GetName());
        }
        return nullptr;
    }
    else if (objPtr->Type() == sfType::Level)
    {
        ULevel* levelPtr = sfObjectMap::Get<ULevel>(objPtr);
        if (levelPtr == nullptr)
        {
            return nullptr;
        }
        return FLevelUtils::FindStreamingLevel(levelPtr);
    }
    return sfObjectMap::GetUObject(objPtr);
}

void sfLevelTranslator::RegisterOnLayersChangeHandler()
{
    m_onLayersChangedHandle = m_worldLayersPtr->OnLayersChanged().AddRaw(this, &sfLevelTranslator::OnLayersChanged);
}

void sfLevelTranslator::UnregisterOnLayersChangeHandler()
{
    m_worldLayersPtr->OnLayersChanged().Remove(m_onLayersChangedHandle);
}

void sfLevelTranslator::OnLayersChanged(
    const ELayersAction::Type action,
    const TWeakObjectPtr<ULayer>& changedLayer,
    const FName& changedProperty)
{
    if (m_layersPropertyPtr == nullptr)
    {
        return;
    }
    switch (action)
    {
        case ELayersAction::Add:
        {
            m_layersPropertyPtr->Add(sfPropertyUtil::FromString(changedLayer->LayerName.ToString()));
            m_layerNames.Add(changedLayer->LayerName);
            break;
        }
        case ELayersAction::Delete:
        {
            int i = 0;
            for (; i < m_layerNames.Num(); i++)
            {
                if (m_worldLayersPtr->GetLayer(m_layerNames[i]) == nullptr)
                {
                    break;
                }
            }
            if (m_layersPropertyPtr->Size() > i)
            {
                m_layersPropertyPtr->Remove(i);
                m_layerNames.RemoveAt(i);
            }
            break;
        }
        case ELayersAction::Rename:
        {
            for (int i = 0; i < m_layerNames.Num(); i++)
            {
                if (m_worldLayersPtr->GetLayer(m_layerNames[i]) == nullptr)
                {
                    m_layersPropertyPtr->Set(i, sfPropertyUtil::FromString(changedLayer->LayerName.ToString()));
                    m_layerNames[i] = changedLayer->LayerName;
                    break;
                }
            }
            break;
        }
        default:
        {
            break;
        }
    }
    m_actorTranslatorPtr->OnLayersChanged(action, changedLayer, changedProperty);
}

bool sfLevelTranslator::SyncLayers()
{
    bool hasChange = false;
    TArray<FName> layers;
    m_worldLayersPtr->AddAllLayerNamesTo(layers);
    for (int i = 0; i < layers.Num(); i++)
    {
        if (i == m_layersPropertyPtr->Size())
        {
            m_layersPropertyPtr->Add(sfPropertyUtil::FromString(layers[i].ToString()));
            hasChange = true;
        }
        else if (layers[i] != m_layerNames[i])
        {
            m_layersPropertyPtr->Set(i, sfPropertyUtil::FromString(layers[i].ToString()));
            hasChange = true;
        }
    }
    if (m_layersPropertyPtr->Size() > layers.Num())
    {
        m_layersPropertyPtr->Resize(layers.Num());
        hasChange = true;
    }
    if (hasChange)
    {
        m_layerNames.Empty();
        m_layerNames.Append(layers);
    }
    return hasChange;
}

#undef LOG_CHANNEL
