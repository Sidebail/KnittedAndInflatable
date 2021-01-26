#include "sfLandscapeTranslator.h"
#include "sfUObjectTranslator.h"
#include "../../Public/Translators/sfComponentTranslator.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/sfPropertyManager.h"
#include "../SceneFusionEdMode.h"
#include "../../Public/sfPropertyUtil.h"
#include "../../Public/sfUnrealUtils.h"
#include "../../Public/sfActorUtil.h"
#include "../../Public/sfConfig.h"
#include "../sfLandscapeEdModeHack.h"
#include "../../Public/Components/sfLockComponent.h"
#include "../../Public/sfLoader.h"
#include <sfLandscapeCompression.h>
#include <sfNullProperty.h>
#include <LandscapeEdit.h>
#include <LandscapeSplineControlPoint.h>
#include <LandscapeSplineSegment.h>
#include <LandscapeSplinesComponent.h>
#include <LandscapeStreamingProxy.h>
#include <LandscapeInfoMap.h>
#include <EngineUtils.h>
#include <Log.h>
#include <Editor.h>
#include <EditorModeManager.h>
#include <ComponentReregisterContext.h>

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
#include <LandscapeWeightmapUsage.h>
typedef ULandscapeWeightmapUsage LandscapeWeightmapUsage;
#else
typedef FLandscapeWeightmapUsage LandscapeWeightmapUsage;
#endif

// Time in seconds between sending heightmap changes
#define SEND_CHANGE_INTERVAL 1.0f
#define APPLY_CHANGE_TIME_MS 30.0f
#define LOG_CHANNEL "sfLandscapeTranslator"

sfLandscapeTranslator::sfLandscapeTranslator() :
    m_lockedObjPtr{ nullptr },
    m_activeLandscapePtr{ nullptr },
    m_tool{ Tools::None },
    m_canEditId{ 0 },
    m_checkChangeTimer{ 0.0f },
    m_iteratingModifiedComponents{ false },
    m_heightmapModified{ false },
    m_offsetmapModified{ false },
    m_weightmapModified{ false },
    m_splineDeleted{ false },
    m_landscapeInfoName{ "LandscapeInfo" },
    m_landscapeInfoMapName{ "LandscapeInfoMap" }
{
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeProxy", "LandscapeGuid");
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeProxy", "LandscapeSectionOffset");
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeProxy", "ComponentSizeQuads");
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeProxy", "SubsectionSizeQuads");
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeProxy", "NumSubsections");
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeProxy", sfProp::EditorLayerSettings->c_str());
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeSplinesComponent", sfProp::ControlPoints->c_str());
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeSplinesComponent", sfProp::Segments->c_str());
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeSplineControlPoint", "ConnectedSegments");
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeComponent", "SectionBaseX");
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeComponent", "SectionBaseY");
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeLayerInfoObject", "LayerName");
}

void sfLandscapeTranslator::Initialize()
{
    m_landscapeSyncEnabled = sfConfig::Get().SyncLandscape &&
        SceneFusion::Get().Service->Session()->GetObjectLimit(sfType::Landscape) != 0;
    m_showedDisabledMessage = !sfConfig::Get().SyncLandscape;

    m_tickHandle = SceneFusion::Get().OnTick.AddRaw(this, &sfLandscapeTranslator::Tick);

    TSharedPtr<sfActorTranslator> actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(
        sfType::Actor);
    // Register ALandscape initializers to initialize/sync the landscape
    actorTranslatorPtr->RegisterActorInitializer<ALandscape>([this](sfObject::SPtr objPtr, AActor* actorPtr)
    {
        ALandscape* landscapePtr = Cast<ALandscape>(actorPtr);
        if (m_landscapeSyncEnabled)
        {
            landscapePtr->CreateLandscapeInfo();
        }
        else
        {
            // Initialize an empty landscape with 8x8 components
            int size = 8 * landscapePtr->ComponentSizeQuads;
            TArray<uint16_t> data;
            data.AddUninitialized((size + 1) * (size + 1));
            for (int i = 0; i < data.Num(); i++)
            {
                // Initialize heights to the midpoint (this is the default)
                data[i] = 32768;
            }
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
            TMap<FGuid, TArray<FLandscapeImportLayerInfo>> layerMap;
            layerMap.Add(landscapePtr->GetLandscapeGuid(), TArray<FLandscapeImportLayerInfo>());
            TMap<FGuid, TArray<uint16>> heightData;
            heightData.Add(landscapePtr->GetLandscapeGuid(), data);
            landscapePtr->Import(landscapePtr->GetLandscapeGuid(), 0, 0, size, size, landscapePtr->NumSubsections,
                landscapePtr->SubsectionSizeQuads, heightData, nullptr, layerMap,
                ELandscapeImportAlphamapType::Additive);
#else
            TArray<FLandscapeImportLayerInfo> layers;
            landscapePtr->Import(landscapePtr->GetLandscapeGuid(), 0, 0, size, size, landscapePtr->NumSubsections,
                landscapePtr->SubsectionSizeQuads, data.GetData(), nullptr, layers,
                ELandscapeImportAlphamapType::Additive);
#endif
        }
    });
    actorTranslatorPtr->RegisterActorInitializer<ALandscapeStreamingProxy>(
        [this](sfObject::SPtr objPtr, AActor* actorPtr)
    {
        ALandscapeStreamingProxy* landscapePtr = Cast<ALandscapeStreamingProxy>(actorPtr);
        landscapePtr->CreateLandscapeInfo();
    });
    actorTranslatorPtr->RegisterPostPropertyChangeHandler<ALandscapeProxy>("LandscapeGuid", [this](UObject* uobjPtr)
    {
        // If the landscape already has a landscape info with a different guid, we need to update the guid on the
        // landscape info the match the new guid.
        ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(uobjPtr);
        if (landscapePtr->GetLandscapeInfo() != nullptr)
        {
            return;
        }
        // Cast to our hack class to access the private map
        ULandscapeInfoMap* mapPtr = reinterpret_cast<ULandscapeInfoMap*>(
            &ULandscapeInfoMap::GetLandscapeInfoMap(landscapePtr->GetWorld()));
        ULandscapeInfo* infoPtr = nullptr;
        if (mapPtr->World != nullptr)
        {
            // Search for a landscape info for this landscape
            for (auto iter : mapPtr->Map)
            {
                if (iter.Value->GetLandscapeProxy() == landscapePtr)
                {
                    infoPtr = iter.Value;
                    break;
                }
            }
        }
        if (infoPtr != nullptr)
        {
            // Update the guid for the landscape info
            mapPtr->Map.Remove(infoPtr->LandscapeGuid);
            mapPtr->Map.Add(landscapePtr->GetLandscapeGuid(), infoPtr);
            infoPtr->LandscapeGuid = landscapePtr->GetLandscapeGuid();
        }
    });
    if (!m_landscapeSyncEnabled)
    {
        return;
    }
    sfLoader::Get().RegisterCreatableAssetType<ULandscapeLayerInfoObject>();

    // Register spline component property
    sfPropertyManager::Get().AddPropertyToForceSyncList("LandscapeProxy", sfProp::SplineComponent->c_str());
    actorTranslatorPtr->RegisterPostPropertyChangeHandler<ALandscapeProxy>(sfProp::SplineComponent->c_str(),
        [this](UObject* uobjPtr)
    {
        if (m_tool != Tools::Splines)
        {
            return;
        }
        ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(uobjPtr);
        if (landscapePtr != nullptr && landscapePtr->SplineComponent != nullptr)
        {
            landscapePtr->SplineComponent->ShowSplineEditorMesh(true);
            if (landscapePtr == m_activeLandscapePtr)
            {
                // Trigger a tool change event to unlock the landscape and lock the spline component
                OnToolChange(m_tool, landscapePtr);
            }
        }
    });
    actorTranslatorPtr->RegisterPostPropertyChangeHandler<ALandscapeProxy>(sfProp::EditorLayerSettings->c_str(),
        [this](UObject* uobjPtr)
    {
        // Add the landscape to the stale layers set. On the next tick if the active landscape is stale, we use a hack
        // to make the UI notice the layer changes.
        ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(uobjPtr);
        m_staleLandscapeLayers.Add(landscapePtr);
    });
    actorTranslatorPtr->RegisterOnModifyHandler<ALandscapeProxy>([this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        // Changes to EditorLayerSettings don't trigger a property event and only trigger a modify event before the
        // changes are applied, so we need to check for changes on the next tick
        m_modifiedLandscapes.Add(Cast<ALandscapeProxy>(uobjPtr));
    });
    m_onLockHandle = actorTranslatorPtr->OnLockStateChange.AddRaw(this, &sfLandscapeTranslator::OnLockChange);

    TSharedPtr<sfComponentTranslator> componentTranslatorPtr = SceneFusion::Get()
        .GetTranslator<sfComponentTranslator>(sfType::Component);
    componentTranslatorPtr->RegisterOnModifyHandler<ULandscapeComponent>(
        [this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        // I have no idea how this happened and I've only seen it once, but a component got added to the modified
        // components set while we were iterating, so now we check if we are iterating before adding to the set.
        // Some undo operations (such as painting weights) will modify every component, so we ignore modified
        // components during undo and only track which components are in the transaction.
        if (!m_iteratingModifiedComponents && !sfUndoManager::Get().InUndoRedo())
        {
            m_modifiedComponents.Add(Cast<ULandscapeComponent>(uobjPtr));
        }
    });
    componentTranslatorPtr->RegisterRemoveElementsHandler<ULandscapeSplinesComponent>(sfProp::Segments->c_str(),
        [this](UObject* uobjPtr, int index, int count)
    {
        UnrealArrayProperty* upropPtr = CastUnrealProperty<UnrealArrayProperty>(
            ULandscapeSplinesComponent::StaticClass()->FindPropertyByName(sfProp::Segments->c_str()));
        FScriptArrayHelper array{ upropPtr, upropPtr->ContainerPtrToValuePtr<void>(uobjPtr) };
        for (int i = index; i < index + count; i++)
        {
            ULandscapeSplineSegment* segmentPtr = *(ULandscapeSplineSegment**)(array.GetRawPtr(i));
            segmentPtr->DeleteSplinePoints();
        }
    });
    componentTranslatorPtr->RegisterObjectInitializer<ULandscapeComponent>(
        [this](sfObject::SPtr objPtr, UActorComponent* componentPtr)
    {
        // Create the landscape object. Each landscape component has a child landscape object that stores the texture
        // data.
        ULandscapeComponent* landscapeCompPtr = Cast<ULandscapeComponent>(componentPtr);
        sfObject::SPtr landscapeObjPtr = CreateObject(landscapeCompPtr);
        if (landscapeObjPtr != nullptr)
        {
            objPtr->AddChild(landscapeObjPtr);
        }
    });
    componentTranslatorPtr->RegisterTransformChangeHandler<ULandscapeComponent>(
        [this](sfObject::SPtr objPtr, USceneComponent* componentPtr)
    {
        ULandscapeComponent* landscapeCompPtr = Cast<ULandscapeComponent>(componentPtr);
        ULandscapeHeightfieldCollisionComponent* collisionPtr = landscapeCompPtr->CollisionComponent.Get();
        if (collisionPtr != nullptr)
        {
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 24
            collisionPtr->SetRelativeLocation(landscapeCompPtr->GetRelativeLocation());
            collisionPtr->SetRelativeRotation(landscapeCompPtr->GetRelativeRotation());
            collisionPtr->SetRelativeScale3D(landscapeCompPtr->GetRelativeScale3D());
#else
            collisionPtr->SetRelativeLocation(landscapeCompPtr->RelativeLocation);
            collisionPtr->SetRelativeRotation(landscapeCompPtr->RelativeRotation);
            collisionPtr->SetRelativeScale3D(landscapeCompPtr->RelativeScale3D);
#endif
        }
    });
    componentTranslatorPtr->RegisterComponentInitializer<ULandscapeComponent>(
        [this](sfObject::SPtr objPtr, UActorComponent* componentPtr)
    {
        OnAddComponent(Cast<ULandscapeComponent>(componentPtr));
    });
    componentTranslatorPtr->RegisterDeleteHandler<ULandscapeComponent>(
        [this](sfObject::SPtr objPtr, UActorComponent* componentPtr)
    {
        OnDeleteComponent(Cast<ULandscapeComponent>(componentPtr));
    });
    componentTranslatorPtr->RegisterPreOwnerChangeHandler<ULandscapeComponent>(
        [this](sfObject::SPtr objPtr, USceneComponent* componentPtr, AActor* ownerPtr)
    {
        PreMoveComponent(Cast<ULandscapeComponent>(componentPtr), Cast<ALandscapeProxy>(ownerPtr));
    });
    componentTranslatorPtr->RegisterPostOwnerChangeHandler<ULandscapeComponent>(
        [this](sfObject::SPtr objPtr, USceneComponent* componentPtr, AActor* ownerPtr)
    {
        Cast<ULandscapeComponent>(componentPtr)->UpdateMaterialInstances();
    });

    TSharedPtr<sfUObjectTranslator> uobjTranslatorPtr = SceneFusion::Get().GetTranslator<sfUObjectTranslator>(
        sfType::UObject);
    uobjTranslatorPtr->RegisterOnModifyHandler<ULandscapeSplineControlPoint>(
        [this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        OnSplineModified(objPtr, uobjPtr);
    });
    uobjTranslatorPtr->RegisterOnModifyHandler<ULandscapeSplineSegment>(
        [this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        OnSplineModified(objPtr, uobjPtr);
    });

    m_onModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddLambda(
        [this](UObject* uobjPtr)
    {
        OnUObjectModified(uobjPtr);
    });

    // Register CanEdit callback to disable editing when landscape is locked
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 24
    SceneFusionEdMode* sfEdModePtr = (SceneFusionEdMode*)GLevelEditorModeTools().GetActiveMode("SceneFusion");
#else
    SceneFusionEdMode* sfEdModePtr = (SceneFusionEdMode*)GLevelEditorModeTools().FindMode("SceneFusion");
#endif
    if (sfEdModePtr != nullptr)
    {
        m_canEditId = sfEdModePtr->RegisterPermitter([this]()
        {
            return CanEdit();
        });
    }

    sfUndoManager::Get().RegisterUndoHandler<ALandscapeProxy>([this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        OnUndoLandscape(Cast<ALandscapeProxy>(uobjPtr));
    });
    sfUndoManager::Get().RegisterPreUndoHandler<ULandscapeComponent>([this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        PreUndoLandscapeComponent(Cast<ULandscapeComponent>(uobjPtr));
    });
    sfUndoManager::Get().RegisterUndoHandler<ULandscapeComponent>([this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        OnUndoLandscapeComponent(Cast<ULandscapeComponent>(uobjPtr));
    });

    sfObjectMap::RegisterRemoveHandler<ALandscapeProxy>([this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(uobjPtr);
        m_modifiedLandscapes.Remove(landscapePtr);
        m_pendingWeightmaps.Remove(landscapePtr);
    });
    sfObjectMap::RegisterRemoveHandler<ULandscapeComponent>([this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        ULandscapeComponent* componentPtr = Cast<ULandscapeComponent>(uobjPtr);
        m_modifiedComponents.Remove(componentPtr);
        m_componentsToUpdate.Remove(componentPtr);
        m_textureInfos.Remove(GetHeightmap(componentPtr));
        if (componentPtr->XYOffsetmapTexture != nullptr)
        {
            m_textureInfos.Remove(componentPtr->XYOffsetmapTexture);
        }
        for (UTexture2D* texturePtr : GetWeightmapTextures(componentPtr))
        {
            m_textureInfos.Remove(texturePtr);
        }
    });
}

void sfLandscapeTranslator::CleanUp()
{
    m_checkChangeTimer = 0.0f;
    m_lockedObjPtr = nullptr;
    m_activeLandscapePtr = nullptr;
    m_tool = Tools::None;
    m_heightmapModified = false;
    m_offsetmapModified = false;
    m_splineDeleted = false;
    m_textureInfos.Empty();
    m_modifiedLandscapes.Empty();
    m_modifiedComponents.Empty();
    m_componentsToUpdate.Empty();
    m_modifiedSplines.Empty();
    m_pendingWeightmaps.Empty();
    m_staleLandscapeLayers.Empty();
    m_duplicateComponents.Empty();
    m_addCollisionSet.Empty();
    sfLoader::Get().UnregisterCreatableAssetType<ULandscapeLayerInfoObject>();
    SceneFusion::Get().OnTick.Remove(m_tickHandle);
    FCoreUObjectDelegates::OnObjectModified.Remove(m_onModifiedHandle);
    sfPropertyManager::Get().RemovePropertyFromForceSyncList("LandscapeProxy", "SplineComponent");
    TSharedPtr<sfActorTranslator> actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(
        sfType::Actor);
    actorTranslatorPtr->OnLockStateChange.Remove(m_onLockHandle);
    actorTranslatorPtr->UnregisterPostPropertyChangeHandler<ALandscapeProxy>(sfProp::SplineComponent->c_str());
    actorTranslatorPtr->UnregisterPostPropertyChangeHandler<ALandscapeProxy>(sfProp::EditorLayerSettings->c_str());
    actorTranslatorPtr->UnregisterOnModifyHandler<ALandscapeProxy>();

    TSharedPtr<sfComponentTranslator> componentTranslatorPtr = SceneFusion::Get().GetTranslator<sfComponentTranslator>(
        sfType::Component);
    componentTranslatorPtr->UnregisterOnModifyHandler<ULandscapeComponent>();
    componentTranslatorPtr->UnregisterRemoveElementsHandler<ULandscapeSplinesComponent>(sfProp::Segments->c_str());
    componentTranslatorPtr->UnregisterObjectInitializer<ULandscapeComponent>();
    componentTranslatorPtr->UnregisterTransformChangeHandler<ULandscapeComponent>();
    componentTranslatorPtr->UnregisterComponentInitializer<ULandscapeComponent>();
    componentTranslatorPtr->UnregisterDeleteHandler<ULandscapeComponent>();
    componentTranslatorPtr->UnregisterPreOwnerChangeHandler<ULandscapeComponent>();
    componentTranslatorPtr->UnregisterPostOwnerChangeHandler<ULandscapeComponent>();

    TSharedPtr<sfUObjectTranslator> uobjTranslatorPtr = SceneFusion::Get().GetTranslator<sfUObjectTranslator>(
        sfType::UObject);
    uobjTranslatorPtr->UnregisterOnModifyHandler<ULandscapeSplineControlPoint>();
    uobjTranslatorPtr->UnregisterOnModifyHandler<ULandscapeSplineSegment>();

    sfUndoManager::Get().UnregisterUndoHandler<ALandscapeProxy>();
    sfUndoManager::Get().UnregisterPreUndoHandler<ULandscapeComponent>();
    sfUndoManager::Get().UnregisterUndoHandler<ULandscapeComponent>();
    sfObjectMap::UnregisterRemoveHandler<ALandscapeProxy>();
    sfObjectMap::UnregisterRemoveHandler<ULandscapeComponent>();

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 24
    SceneFusionEdMode* sfEdModePtr = (SceneFusionEdMode*)GLevelEditorModeTools().GetActiveMode("SceneFusion");
#else
    SceneFusionEdMode* sfEdModePtr = (SceneFusionEdMode*)GLevelEditorModeTools().FindMode("SceneFusion");
#endif
    if (sfEdModePtr != nullptr)
    {
        sfEdModePtr->UnregisterPermitter(m_canEditId);
    }
}

void sfLandscapeTranslator::Tick(float deltaTime)
{
    if (!m_landscapeSyncEnabled)
    {
        if (GLevelEditorModeTools().GetActiveMode("EM_Landscape") != nullptr)
        {
            SceneFusion::Get().ShowUpgradeLink(SceneFusion::RestrictedFeature::LANDSCAPE);
            if (!m_showedDisabledMessage)
            {
                m_showedDisabledMessage = true;
                FText message = FText::FromString(
                    "Landscape syncing is disabled on your account. Upgrade to enable landscape syncing.");
                FText title = FText::FromString("Scene Fusion");
                FMessageDialog::Open(EAppMsgType::Ok, message, &title);
            }
        }
        return;
    }
    UpdateTool();
    SyncSplines();
    ApplyServerData();

    // Delete duplicate components
    for (ULandscapeComponent * componentPtr : m_duplicateComponents)
    {
        componentPtr->DestroyComponent();
    }
    m_duplicateComponents.Empty();

    for (ULandscapeInfo* infoPtr : m_addCollisionSet)
    {
        infoPtr->UpdateAllAddCollisions();
    }
    m_addCollisionSet.Empty();

    if (m_staleLandscapeLayers.Num() > 0)
    {
        // Trigger UI to notice weight layer changes
        RefreshLayers();
        m_staleLandscapeLayers.Empty();
    }

    ApplyPendingWeightmaps();

    if (m_checkChangeTimer > 0.0f)
    {
        m_checkChangeTimer -= deltaTime;
        if (m_checkChangeTimer <= 0.0f)
        {
            SyncTextures();
        }
    }
    else
    {
        m_modifiedComponents.Empty();
    }
    
    // Sync weight layer changes
    SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
    for (ALandscapeProxy* landscapePtr : m_modifiedLandscapes)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(landscapePtr);
        if (objPtr != nullptr)
        {
            sfPropertyManager::Get().SyncProperty(objPtr, landscapePtr, sfProp::EditorLayerSettings->c_str());
            if (m_tool == Tools::AddComponent || m_tool == Tools::DeleteComponent || m_tool == Tools::MoveToLevel)
            {
                // Check for added/deleted components
                SceneFusion::Get().GetTranslator<sfComponentTranslator>(sfType::Component)
                    ->SyncComponents(landscapePtr);
            }
        }
    }
    m_modifiedLandscapes.Empty();
    SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
}

void sfLandscapeTranslator::SyncSplines()
{
    if (m_modifiedSplines.Num() <= 0)
    {
        return;
    }
    for (UObject* splinePtr : m_modifiedSplines)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(splinePtr);
        if (objPtr != nullptr)
        {
            if (objPtr->IsLocked())
            {
                sfPropertyManager::Get().ApplyProperties(splinePtr, objPtr->Property()->AsDict());
            }
            else
            {
                sfPropertyManager::Get().SendPropertyChanges(splinePtr, objPtr->Property()->AsDict());
            }
        }

        if (m_splineDeleted)
        {
            m_splineDeleted = false;
            objPtr = sfObjectMap::GetSFObject(splinePtr->GetOuter());
            if (objPtr != nullptr)
            {
                sfPropertyManager::Get().SyncProperty(objPtr, splinePtr->GetOuter(), sfProp::Segments->c_str());
                sfPropertyManager::Get().SyncProperty(objPtr, splinePtr->GetOuter(), sfProp::ControlPoints->c_str());
            }
        }
    }
    m_modifiedSplines.Empty();
}

void sfLandscapeTranslator::SyncTextures()
{
    // Check for texture map changes on modified components
    m_iteratingModifiedComponents = true;
    for (ULandscapeComponent* componentPtr : m_modifiedComponents)
    {
        if (componentPtr->IsPendingKill())
        {
            continue;
        }
        sfObject::SPtr objPtr = sfUnrealUtils::FindChildByType(sfObjectMap::GetSFObject(componentPtr), sfType::Landscape);
        if (objPtr == nullptr)
        {
            continue;
        }
        ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(componentPtr->GetOwner());
        if (landscapePtr == nullptr)
        {
            continue;
        }

        sfDictionaryProperty::SPtr propsPtr = objPtr->Property()->AsDict();
        if (m_heightmapModified)
        {
            if (objPtr->IsLocked())
            {
                ApplyServerHeightmapData(componentPtr, propsPtr->Get(sfProp::Heightmap));
            }
            else if ((m_componentsToUpdate.FindRef(componentPtr) & TextureTypes::HEIGHTMAP) == 0)
            {
                sfValueProperty::SPtr propPtr = SerializeHeightmap(componentPtr);
                if (!propPtr->Equals(propsPtr->Get(sfProp::Heightmap)))
                {
                    propsPtr->Set(sfProp::Heightmap, propPtr);
                    if (!m_textureInfos.Contains(GetHeightmap(componentPtr)))
                    {
                        m_textureInfos.Add(GetHeightmap(componentPtr), TextureInfo{ landscapePtr,
                            TextureTypes::HEIGHTMAP });
                    }
                }
            }
        }
        if (m_offsetmapModified)
        {
            if (objPtr->IsLocked())
            {
                ApplyServerOffsetmapData(componentPtr, propsPtr->Get(sfProp::Offsetmap));
            }
            else if ((m_componentsToUpdate.FindRef(componentPtr) & TextureTypes::OFFSETMAP) == 0)
            {
                sfProperty::SPtr propPtr = SerializeOffsetmap(componentPtr);
                if (!propPtr->Equals(propsPtr->Get(sfProp::Offsetmap)))
                {
                    propsPtr->Set(sfProp::Offsetmap, propPtr);
                    if (componentPtr->XYOffsetmapTexture != nullptr &&
                        !m_textureInfos.Contains(componentPtr->XYOffsetmapTexture))
                    {
                        m_textureInfos.Add(componentPtr->XYOffsetmapTexture, TextureInfo{ landscapePtr,
                            TextureTypes::OFFSETMAP });
                    }
                }
            }
        }
        if (m_weightmapModified)
        {
            if (objPtr->IsLocked())
            {
                ApplyServerWeightmapData(componentPtr, propsPtr->Get(sfProp::Weightmap));
            }
            else if ((m_componentsToUpdate.FindRef(componentPtr) & TextureTypes::WEIGHTMAP) == 0)
            {
                sfProperty::SPtr propPtr = SerializeWeightmap(componentPtr);
                if (!propPtr->Equals(propsPtr->Get(sfProp::Weightmap)))
                {
                    propsPtr->Set(sfProp::Weightmap, propPtr);
                }
            }
            // If 2 components have the same weightmap data they will use the same texture, and then when one component
            // is painted on it will get a new texture, so we check for new textures
            for (UTexture2D* texturePtr : GetWeightmapTextures(componentPtr))
            {
                if (!m_textureInfos.Contains(texturePtr))
                {
                    m_textureInfos.Add(texturePtr, TextureInfo{ landscapePtr, TextureTypes::WEIGHTMAP });
                }
            }
        }
    }
    m_iteratingModifiedComponents = false;
    m_heightmapModified = false;
    m_offsetmapModified = false;
    m_weightmapModified = false;
    m_modifiedComponents.Empty();
}

void sfLandscapeTranslator::ApplyPendingWeightmaps()
{
    for (auto iter = m_pendingWeightmaps.CreateIterator(); iter; ++iter)
    {
        ALandscapeProxy* landscapePtr = *iter;
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(landscapePtr);
        if (objPtr == nullptr)
        {
            iter.RemoveCurrent();
            continue;
        }
        sfProperty::SPtr propPtr;
        if (!objPtr->Property()->AsDict()->TryGet(sfProp::EditorLayerSettings, propPtr))
        {
            iter.RemoveCurrent();
            continue;
        }
        if (HasNumLayersOrMore(landscapePtr, propPtr->AsList()->Size()))
        {
            for (ULandscapeComponent* componentPtr : landscapePtr->LandscapeComponents)
            {
                objPtr = sfUnrealUtils::FindChildByType(sfObjectMap::GetSFObject(componentPtr), sfType::Landscape);
                if (objPtr != nullptr)
                {
                    ApplyServerWeightmapData(componentPtr, objPtr->Property()->AsDict()->Get(sfProp::Weightmap));
                }
            }
            iter.RemoveCurrent();
        }
    }
}

void sfLandscapeTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    ULandscapeComponent* componentPtr = sfObjectMap::Get<ULandscapeComponent>(objPtr->Parent());
    if (componentPtr == nullptr)
    {
        return;
    }
    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(componentPtr->GetOwner());
    if (landscapePtr == nullptr)
    {
        return;
    }
    m_componentsToUpdate.Add(componentPtr, TextureTypes::ALL);
    m_textureInfos.Add(GetHeightmap(componentPtr), TextureInfo{ landscapePtr, TextureTypes::HEIGHTMAP });
    if (componentPtr->XYOffsetmapTexture != nullptr)
    {
        m_textureInfos.Add(componentPtr->XYOffsetmapTexture, TextureInfo{ landscapePtr, TextureTypes::OFFSETMAP });
    }
    for (UTexture2D* texturePtr : GetWeightmapTextures(componentPtr))
    {
        m_textureInfos.Add(texturePtr, TextureInfo{ landscapePtr, TextureTypes::WEIGHTMAP });
    }
}

void sfLandscapeTranslator::OnPropertyChange(sfProperty::SPtr propPtr)
{
    ULandscapeComponent* componentPtr = sfObjectMap::Get<ULandscapeComponent>(propPtr->GetContainerObject()->Parent());
    if (componentPtr == nullptr)
    {
        return;
    }
    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(componentPtr->GetOwner());
    if (landscapePtr == nullptr)
    {
        return;
    }
    uint8_t flags = m_componentsToUpdate.FindRef(componentPtr);
    if (propPtr->Key() == sfProp::Heightmap)
    {
        m_componentsToUpdate.Add(componentPtr, flags | TextureTypes::HEIGHTMAP);
        
    }
    else if (propPtr->Key() == sfProp::Offsetmap)
    {
        m_componentsToUpdate.Add(componentPtr, flags | TextureTypes::OFFSETMAP);
        if (componentPtr->XYOffsetmapTexture != nullptr &&
            !m_textureInfos.Contains(componentPtr->XYOffsetmapTexture))
        {
            m_textureInfos.Add(componentPtr->XYOffsetmapTexture, TextureInfo{ landscapePtr, TextureTypes::OFFSETMAP });
        }
    }
    else if (propPtr->Key() == sfProp::Weightmap)
    {
        m_componentsToUpdate.Add(componentPtr, flags | TextureTypes::WEIGHTMAP);
        // If 2 components have the same weightmap data they will use the same texture, and then when one component
        // is painted on it will get a new texture, so we check for new textures
        for (UTexture2D* texturePtr : GetWeightmapTextures(componentPtr))
        {
            if (!m_textureInfos.Contains(texturePtr))
            {
                m_textureInfos.Add(texturePtr, TextureInfo{ landscapePtr, TextureTypes::WEIGHTMAP });
            }
        }
    }
}

void sfLandscapeTranslator::OnLockChange(AActor* actorPtr, sfActorTranslator::LockType lockType, sfUser::SPtr userPtr)
{
    if (!actorPtr->IsA<ALandscapeProxy>() ||
        lockType == sfActorTranslator::LockType::Unlocked || lockType == sfActorTranslator::LockType::NotSynced ||
        // If it's partially locked, only show the lock shader if we're editing splines on the same landscape
        (lockType == sfActorTranslator::LockType::PartiallyLocked && (m_tool != Tools::Splines ||
            m_activeLandscapePtr != actorPtr || m_lockedObjPtr == nullptr || !m_lockedObjPtr->IsFullyLocked())))
    {
        return;
    }
    UsfLockComponent* lockPtr = sfActorUtil::GetComponent<UsfLockComponent>(actorPtr);
    if (lockPtr != nullptr)
    {
        lockPtr->SetLandscapeMaterial(SceneFusion::GetLandscapeLockMaterial(userPtr));
    }
}

void sfLandscapeTranslator::OnSplineModified(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    // When adding new splines, Unreal will modify the connected spline AFTER triggering the on modify event, so we
    // need to store it and check for changes on the next tick.
    m_modifiedSplines.Add(uobjPtr);
    if (sfUndoManager::Get().GetUndoText() == "LandscapeSpline_Delete")
    {
        m_splineDeleted = true;
    }
}

void sfLandscapeTranslator::OnUObjectModified(UObject* uobjPtr)
{
    UTexture2D* texturePtr = Cast<UTexture2D>(uobjPtr);
    if (texturePtr == nullptr)
    {
        if ((uobjPtr->IsA<ULandscapeSplineControlPoint>() || uobjPtr->IsA<ULandscapeSplineSegment>()) &&
            !sfObjectMap::Contains(uobjPtr))
        {
            sfObject::SPtr objPtr = sfObjectMap::GetSFObject(uobjPtr->GetOuter());
            if (objPtr != nullptr)
            {
                if (uobjPtr->IsA<ULandscapeSplineControlPoint>())
                {
                    sfPropertyManager::Get().SyncProperty(objPtr, uobjPtr->GetOuter(), sfProp::ControlPoints->c_str());
                }
                else
                {
                    sfPropertyManager::Get().SyncProperty(objPtr, uobjPtr->GetOuter(), sfProp::Segments->c_str());
                }
            }
            else if (uobjPtr->GetOuter() != nullptr)
            {
                // Outer is a newly created spline component. Outer->Outer is the landscape actor. Sync components of
                // the landscape actor to sync the new spline component.
                ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(uobjPtr->GetOuter()->GetOuter());
                if (landscapePtr != nullptr)
                {
                    objPtr = sfObjectMap::GetSFObject(landscapePtr);
                    if (objPtr != nullptr)
                    {
                        TSharedPtr<sfComponentTranslator> componentTranslatorPtr = SceneFusion::Get()
                            .GetTranslator<sfComponentTranslator>(sfType::Component);
                        componentTranslatorPtr->SyncComponents(landscapePtr);
                        sfPropertyManager::Get().SyncProperty(objPtr, landscapePtr, sfProp::SplineComponent->c_str());
                        // Trigger a tool change event to unlock the landscape and lock the spline component
                        if (landscapePtr == m_activeLandscapePtr && m_tool == Tools::Splines)
                        {
                            OnToolChange(m_tool, landscapePtr);
                        }
                    }
                }
            }
        }
        return;
    }

    TextureInfo* infoPtr = m_textureInfos.Find(texturePtr);
    if (infoPtr != nullptr)
    {
        switch (infoPtr->Type)
        {
            case TextureTypes::HEIGHTMAP:
            {
                m_heightmapModified = true;
                break;
            }
            case TextureTypes::OFFSETMAP:
            {
                m_offsetmapModified = true;
                break;
            }
            case TextureTypes::WEIGHTMAP:
            {
                m_weightmapModified = true;
                break;
            }
        }
        if (m_checkChangeTimer <= 0)
        {
            sfObject::SPtr objPtr = sfObjectMap::GetSFObject(infoPtr->LandscapePtr);
            m_checkChangeTimer = objPtr != nullptr && objPtr->IsFullyLocked() ? 0.1f : SEND_CHANGE_INTERVAL;
        }
    }
    else
    {
        // This may be any of types or none of them.
        m_heightmapModified = true;
        m_offsetmapModified = true;
        m_weightmapModified = true;
        if (m_checkChangeTimer <= 0)
        {
            m_checkChangeTimer = SEND_CHANGE_INTERVAL;
        }
    }
}

void sfLandscapeTranslator::OnUndoLandscape(ALandscapeProxy* landscapePtr)
{
    m_landscapesToFixComponents.Add(landscapePtr);
}

void sfLandscapeTranslator::PreUndoLandscapeComponent(ULandscapeComponent* componentPtr)
{
    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(componentPtr->GetOwner());
    if (landscapePtr == nullptr)
    {
        return;
    }
    m_landscapesToFixComponents.Add(landscapePtr);
    sfUndoManager::Get().AddPostUndoHandler(this);

    // Check if this undo will create a second component at the same location as another
    if (!componentPtr->IsPendingKill() || componentPtr->ComponentSizeQuads == 0)
    {
        return;
    }
    ULandscapeInfo* infoPtr = landscapePtr->GetLandscapeInfo();
    if (infoPtr != nullptr)
    {
        FIntPoint coords = componentPtr->GetSectionBase() / componentPtr->ComponentSizeQuads;
        ULandscapeComponent* otherPtr = infoPtr->XYtoComponentMap.FindRef(coords);
        if (otherPtr != componentPtr && otherPtr != nullptr)
        {
            m_duplicateComponents.Add(componentPtr);
        }
    }
}

void sfLandscapeTranslator::OnUndoLandscapeComponent(ULandscapeComponent* componentPtr)
{
    if (m_duplicateComponents.Remove(componentPtr) > 0)
    {
        componentPtr->DestroyComponent();
        return;
    }
    m_modifiedComponents.Add(componentPtr);
    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(componentPtr->GetOwner());
    if (landscapePtr != nullptr)
    {
        m_undoLandscapes.Add(landscapePtr);
        sfUndoManager::Get().AddPostUndoHandler(this);
    }
}

void sfLandscapeTranslator::PostUndo()
{
    // Landscape's component arrays can get in a bad state if another user added or removed components after the
    // transaction was recorded. Iterate landscapes that may have been effected and fix their component lists.
    for (ALandscapeProxy* landscapePtr : m_landscapesToFixComponents)
    {
        // Remove components that are deleted or attached to a different landscape. Add the rest to the component set.
        TSet<ULandscapeComponent*> componentSet;
        for (int i = landscapePtr->LandscapeComponents.Num() - 1; i >= 0; i--)
        {
            ULandscapeComponent* componentPtr = landscapePtr->LandscapeComponents[i];
            if (componentPtr == nullptr || componentPtr->IsPendingKill() || componentPtr->GetOwner() != landscapePtr)
            {
                landscapePtr->LandscapeComponents.RemoveAt(i);
            }
            else
            {
                componentSet.Add(componentPtr);
            }
        }
        // Get all landscape components and add any that are missing from the component set.
        TArray<ULandscapeComponent*> components;
        landscapePtr->GetComponents(components);
        for (ULandscapeComponent* componentPtr : components)
        {
            if (!componentSet.Contains(componentPtr))
            {
                landscapePtr->LandscapeComponents.Add(componentPtr);
            }
        }

        // The map of which components use which weight textures can be corrupted by undo when multiple users are
        // modifying weight maps, so we need to fix it.
        landscapePtr->WeightmapUsageMap.Empty();
        for (ULandscapeComponent* componentPtr : landscapePtr->LandscapeComponents)
        {
            componentPtr->FixupWeightmaps();
        }
    }
    m_landscapesToFixComponents.Empty();

    // The landscape info XYtoComponentMap can be in a bad state if another user added a component after the
    // transaction was recorded. We add any components that are missing from the map.
    for (ULandscapeInfo* infoPtr : m_undoLandscapeInfos)
    {
        infoPtr->ForAllLandscapeProxies([this, infoPtr](ALandscapeProxy* landscapePtr)
        {
            for (ULandscapeComponent* componentPtr : landscapePtr->LandscapeComponents)
            {
                if (componentPtr->ComponentSizeQuads > 0)
                {
                    infoPtr->XYtoComponentMap.Add(componentPtr->GetSectionBase() / componentPtr->ComponentSizeQuads,
                        componentPtr);
                }
            }
        });
    }
    m_undoLandscapeInfos.Empty();

    if (m_undoLandscapes.Num() > 0)
    {
        for (ALandscapeProxy* landscapePtr : m_undoLandscapes)
        {
            // Undo will restore the entire texture to an old state that may undo changes to other components by other
            // users, so we iterate all components and revert changes if the texture was in the transaction but the
            // component wasn't. It is way faster to serialize the texture data then to apply data to a texture, so
            // before reverting changes we serialize the data and check if it's different from the server data.
            for (ULandscapeComponent* componentPtr : landscapePtr->LandscapeComponents)
            {
                sfObject::SPtr objPtr = sfUnrealUtils::FindChildByType(sfObjectMap::GetSFObject(componentPtr),
                    sfType::Landscape);
                if (objPtr == nullptr)
                {
                    // Sync added components
                    SceneFusion::Get().GetTranslator<sfComponentTranslator>(sfType::Component)
                        ->SyncComponents(landscapePtr);
                    continue;
                }
                sfDictionaryProperty::SPtr propsPtr = objPtr->Property()->AsDict();
                if (m_heightmapModified && m_undoTextures.Contains(GetHeightmap(componentPtr)) &&
                    (m_componentsToUpdate.FindRef(componentPtr) & TextureTypes::HEIGHTMAP) == 0)
                {
                    sfValueProperty::SPtr propPtr = SerializeHeightmap(componentPtr);
                    if (!propPtr->Equals(propsPtr->Get(sfProp::Heightmap)))
                    {
                        if (m_modifiedComponents.Contains(componentPtr) && !objPtr->IsLocked())
                        {
                            propsPtr->Set(sfProp::Heightmap, propPtr);
                        }
                        else
                        {
                            ApplyServerHeightmapData(componentPtr, propsPtr->Get(sfProp::Heightmap));
                        }
                    }
                }
                if (m_offsetmapModified && (componentPtr->XYOffsetmapTexture == nullptr ||
                    m_undoTextures.Contains(componentPtr->XYOffsetmapTexture)) &&
                    (m_componentsToUpdate.FindRef(componentPtr) & TextureTypes::OFFSETMAP) == 0)
                {
                    sfProperty::SPtr propPtr = SerializeOffsetmap(componentPtr);
                    if (!propPtr->Equals(propsPtr->Get(sfProp::Offsetmap)))
                    {
                        if (m_modifiedComponents.Contains(componentPtr) && !objPtr->IsLocked())
                        {
                            propsPtr->Set(sfProp::Offsetmap, propPtr);
                            if (componentPtr->XYOffsetmapTexture != nullptr &&
                                !m_textureInfos.Contains(componentPtr->XYOffsetmapTexture))
                            {
                                m_textureInfos.Add(componentPtr->XYOffsetmapTexture, TextureInfo{ landscapePtr,
                                    TextureTypes::OFFSETMAP });
                            }
                        }
                        else
                        {
                            ApplyServerOffsetmapData(componentPtr, propsPtr->Get(sfProp::Offsetmap));
                        }
                    }
                }
                if (m_weightmapModified && (m_componentsToUpdate.FindRef(componentPtr) & TextureTypes::WEIGHTMAP) == 0)
                {
                    // Because components can change which weightmap textures they are using, components may have
                    // weightmap changes even when none of their weightmap textures are in the transaction, so we don't
                    // check m_undoTextures
                    sfProperty::SPtr propPtr = SerializeWeightmap(componentPtr);
                    if (!propPtr->Equals(propsPtr->Get(sfProp::Weightmap)))
                    {
                        if (m_modifiedComponents.Contains(componentPtr) && !objPtr->IsLocked())
                        {
                            propsPtr->Set(sfProp::Weightmap, propPtr);
                        }
                        else
                        {
                            ApplyServerWeightmapData(componentPtr, propsPtr->Get(sfProp::Weightmap));
                        }
                    }
                }
            }
        }
        m_checkChangeTimer = 0.0f;
        m_heightmapModified = false;
        m_offsetmapModified = false;
        m_weightmapModified = false;
        m_modifiedComponents.Empty();
        m_undoLandscapes.Empty();
    }
    m_undoTextures.Empty();
}

bool sfLandscapeTranslator::OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    UTexture2D* texturePtr = Cast<UTexture2D>(uobjPtr);
    if (texturePtr != nullptr)
    {
        m_undoTextures.Add(texturePtr);
        sfUndoManager::Get().AddPostUndoHandler(this);
        return false;
    }

    // ULandscapeInfo::StaticClass() causes unresolved externals, so to avoid errors we check the class FName and use a
    // static cast.
    if (uobjPtr->GetClass()->GetFName() == m_landscapeInfoName)
    {
        m_undoLandscapeInfos.Add(static_cast<ULandscapeInfo*>(uobjPtr));
        sfUndoManager::Get().AddPostUndoHandler(this);
        return true;
    }

    if (uobjPtr->GetClass()->GetFName() != m_landscapeInfoMapName)
    {
        return false;
    }
    // ULandscapeInfo map is a map of landscape guids to landscape infos. When we undo it to a previous state, we
    // may lose the mapping to landscapes created by other users or get mappings to invalid landscapes deleted by
    // other users, so we need to rebuild it. However it is a private class, so we reinterpret cast to our own
    // class that has the same members.
    ULandscapeInfoMap* mapPtr = static_cast<ULandscapeInfoMap*>(uobjPtr);
    if (mapPtr->World != nullptr)
    {
        TArray<FGuid> keys;
        mapPtr->Map.GetKeys(keys);
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 24
        for (TActorIterator<ALandscapeProxy> iter(mapPtr->World); iter; ++iter)
#else
        for (TActorIterator<ALandscapeProxy> iter(mapPtr->World.Get()); iter; ++iter)
#endif
        {
            if (keys.Remove(iter->GetLandscapeGuid()) == 0)
            {
                // This creates a new landscape info and adds it to the map
                iter->CreateLandscapeInfo();
            }
        }
        // Remove the keys we didn't find landscapes for
        for (FGuid guid : keys)
        {
            mapPtr->Map.Remove(guid);
        }
    }
    return true;
}

sfObject::SPtr sfLandscapeTranslator::CreateObject(ULandscapeComponent* componentPtr)
{
    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(componentPtr->GetOwner());
    if (landscapePtr == nullptr)
    {
        return nullptr;
    }
    sfDictionaryProperty::SPtr propsPtr = sfDictionaryProperty::Create();
    sfObject::SPtr objPtr = sfObject::Create(sfType::Landscape, propsPtr);
    m_textureInfos.Add(GetHeightmap(componentPtr), TextureInfo{ landscapePtr, TextureTypes::HEIGHTMAP });
    propsPtr->Set(sfProp::Heightmap, SerializeHeightmap(componentPtr));

    if (componentPtr->XYOffsetmapTexture != nullptr)
    {
        m_textureInfos.Add(componentPtr->XYOffsetmapTexture, TextureInfo{ landscapePtr, TextureTypes::OFFSETMAP });
    }
    propsPtr->Set(sfProp::Offsetmap, SerializeOffsetmap(componentPtr));

    for (UTexture2D* texturePtr : GetWeightmapTextures(componentPtr))
    {
        m_textureInfos.Add(texturePtr, TextureInfo{ landscapePtr, TextureTypes::WEIGHTMAP });
    }
    propsPtr->Set(sfProp::Weightmap, SerializeWeightmap(componentPtr));
    return objPtr;
}

sfValueProperty::SPtr sfLandscapeTranslator::SerializeHeightmap(ULandscapeComponent* componentPtr)
{
    std::vector<uint8_t> data;
    int numQuads = componentPtr->ComponentSizeQuads + 1;
    data.resize(numQuads * numQuads * 4);
    uint16_t* dataPtr = reinterpret_cast<uint16_t*>(data.data());
    // We have to send normals because normals on component edges aren't calculated correctly
    uint16_t* normalDataPtr = dataPtr + numQuads * numQuads;
    FLandscapeEditDataInterface dataInterface{ componentPtr->GetLandscapeInfo() };
    dataInterface.GetHeightDataFast(componentPtr->SectionBaseX, componentPtr->SectionBaseY,
        componentPtr->SectionBaseX + componentPtr->ComponentSizeQuads,
        componentPtr->SectionBaseY + componentPtr->ComponentSizeQuads, dataPtr, 0, normalDataPtr);
 
    std::vector<uint8_t> encodedData = sfLandscapeCompression::CompressHeightmap(dataPtr, numQuads);
    return sfValueProperty::Create(ksMultiType{ std::move(encodedData) });
}

sfProperty::SPtr sfLandscapeTranslator::SerializeOffsetmap(ULandscapeComponent* componentPtr)
{
    if (componentPtr->XYOffsetmapTexture == nullptr)
    {
        return sfNullProperty::Create();
    }
    std::vector<uint8_t> data;
    int numQuads = componentPtr->ComponentSizeQuads + 1;
    data.resize(numQuads * numQuads * 8);
    FVector2D* dataPtr = reinterpret_cast<FVector2D*>(data.data());
    FLandscapeEditDataInterface dataInterface{ componentPtr->GetLandscapeInfo() };
    dataInterface.GetXYOffsetDataFast(componentPtr->SectionBaseX, componentPtr->SectionBaseY,
        componentPtr->SectionBaseX + componentPtr->ComponentSizeQuads,
        componentPtr->SectionBaseY + componentPtr->ComponentSizeQuads, dataPtr, 0);

    std::vector<uint8_t> encodedData = sfLandscapeCompression::CompressOffsetmap(
        reinterpret_cast<float*>(dataPtr),
        numQuads);
    return sfValueProperty::Create(ksMultiType{ std::move(encodedData) });
}

sfProperty::SPtr sfLandscapeTranslator::SerializeWeightmap(ULandscapeComponent* componentPtr)
{
    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(componentPtr->GetOwner());
    if (landscapePtr == nullptr)
    {
        return sfNullProperty::Create();
    }

    int numLayers = landscapePtr->EditorLayerSettings.Num();
    if (componentPtr->ComponentHasVisibilityPainted())
    {
        numLayers++;
    }
    std::vector<uint8_t> data;
    int numQuads = componentPtr->ComponentSizeQuads + 1;
    data.resize(numQuads * numQuads * numLayers);
    FLandscapeEditDataInterface dataInterface{ componentPtr->GetLandscapeInfo() };
    uint8_t* dataPtr = data.data();
    for (int i = 0; i < numLayers; i++, dataPtr += numQuads * numQuads)
    {
        ULandscapeLayerInfoObject* layerPtr = i == landscapePtr->EditorLayerSettings.Num() ?
            ALandscapeProxy::VisibilityLayer : landscapePtr->EditorLayerSettings[i].LayerInfoObj;
        dataInterface.GetWeightDataFast(layerPtr,
            componentPtr->SectionBaseX, componentPtr->SectionBaseY,
            componentPtr->SectionBaseX + componentPtr->ComponentSizeQuads,
            componentPtr->SectionBaseY + componentPtr->ComponentSizeQuads, dataPtr, 0);
    }
    
    // Validate that weights sum to 255 or 256. Fix invalid weights.
    if (landscapePtr->EditorLayerSettings.Num() > 0)
    {
        std::vector<bool> layerChanged;
        layerChanged.resize(landscapePtr->EditorLayerSettings.Num(), false);
        int size = numQuads * numQuads;
        for (int i = 0; i < size; i++)
        {
            uint16_t sum = 0;
            for (int j = 0; j < landscapePtr->EditorLayerSettings.Num(); j++)
            {
                ULandscapeLayerInfoObject* layerPtr = landscapePtr->EditorLayerSettings[j].LayerInfoObj;
                if (layerPtr == nullptr || layerPtr->bNoWeightBlend)
                {
                    // Layers with no weight blending don't count towards the sum
                    continue;
                }
                int index = i + size * j;
                sum += data[index];
                // Sum is usually 255 but sometimes 256 occurs under normal circumstances. Anything higher we need to
                // fix.
                if (sum > 256)
                {
                    data[index] -= (uint8_t)(sum - 256);
                    sum = 256;
                    if (data[index] > 0)
                    {
                        data[index]--;
                        sum = 255;
                    }
                    layerChanged[j] = true;
                }
            }
            if (sum < 255)
            {
                data[i] += 255 - (uint8_t)sum;
                layerChanged[0] = true;
            }
        }
        dataPtr = data.data();
        for (int i = 0; i < landscapePtr->EditorLayerSettings.Num(); i++, dataPtr += size)
        {
            if (layerChanged[i])
            {
                KS::Log::Info("Fixed invalid weights on layer " + std::to_string(i) + " on " +
                    std::string(TCHAR_TO_UTF8(*componentPtr->GetName())), LOG_CHANNEL);
                dataInterface.SetAlphaData(landscapePtr->EditorLayerSettings[i].LayerInfoObj,
                    componentPtr->SectionBaseX, componentPtr->SectionBaseY,
                    componentPtr->SectionBaseX + componentPtr->ComponentSizeQuads,
                    componentPtr->SectionBaseY + componentPtr->ComponentSizeQuads, dataPtr, 0,
                    ELandscapeLayerPaintingRestriction::None, false);
            }
        }
    }

    std::vector<uint8_t> encodedData;
    if (data.size() > 0)
    {
        encodedData = sfLandscapeCompression::CompressWeightmap(data.data(), numQuads, numLayers);
    }
    return sfValueProperty::Create(ksMultiType{ std::move(encodedData) });
}

void sfLandscapeTranslator::ApplyServerHeightmapData(ULandscapeComponent* componentPtr, sfProperty::SPtr propPtr)
{
    FCoreUObjectDelegates::OnObjectModified.Remove(m_onModifiedHandle);
    SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();

    const std::vector<uint8_t>& data = propPtr->AsValue()->GetValue().GetData();
    std::vector<uint16_t> decodedData = sfLandscapeCompression::DecompressHeightmap(
        data,
        componentPtr->ComponentSizeQuads + 1);

    uint16_t* normalDataPtr = decodedData.data() + decodedData.size() / 2;
    FLandscapeEditDataInterface dataInterface{ componentPtr->GetLandscapeInfo() };
    dataInterface.SetHeightData(componentPtr->SectionBaseX, componentPtr->SectionBaseY,
        componentPtr->SectionBaseX + componentPtr->ComponentSizeQuads,
        componentPtr->SectionBaseY + componentPtr->ComponentSizeQuads,
        decodedData.data(), 0, false, normalDataPtr);
    SceneFusion::RedrawActiveViewport();
    dataInterface.Flush();

    componentPtr->InvalidateLightingCache();
    componentPtr->UpdateCachedBounds();
    componentPtr->UpdateComponentToWorld();
    // Recreate collision for modified components to update the physical materials
    ULandscapeHeightfieldCollisionComponent* collisionPtr = componentPtr->CollisionComponent.Get();
    if (collisionPtr != nullptr)
    {
        collisionPtr->RecreateCollision();
        FNavigationSystem::UpdateComponentData(*collisionPtr);
    }

    SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
    m_onModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddLambda(
        [this](UObject* uobjPtr)
    {
        OnUObjectModified(uobjPtr);
    });
}

void sfLandscapeTranslator::ApplyServerOffsetmapData(ULandscapeComponent* componentPtr, sfProperty::SPtr propPtr)
{
    int numQuads = componentPtr->ComponentSizeQuads + 1;
    std::vector<float> decodedData;
    if (propPtr->Type() == sfProperty::VALUE)
    {
        const std::vector<uint8_t>& data = propPtr->AsValue()->GetValue().GetData();
        decodedData = sfLandscapeCompression::DecompressOffsetmap(data, numQuads);
    }
    else if (componentPtr->XYOffsetmapTexture != nullptr)
    {
        decodedData.resize(numQuads * numQuads * 2, 0);
    }
    else
    {
        return;
    }

    FCoreUObjectDelegates::OnObjectModified.Remove(m_onModifiedHandle);
    SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();

    FLandscapeEditDataInterface dataInterface{ componentPtr->GetLandscapeInfo() };
    dataInterface.SetXYOffsetData(componentPtr->SectionBaseX, componentPtr->SectionBaseY,
        componentPtr->SectionBaseX + componentPtr->ComponentSizeQuads,
        componentPtr->SectionBaseY + componentPtr->ComponentSizeQuads,
        reinterpret_cast<FVector2D*>(decodedData.data()), 0);
    SceneFusion::RedrawActiveViewport();

    SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
    m_onModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddLambda(
        [this](UObject* uobjPtr)
    {
        OnUObjectModified(uobjPtr);
    });
}

void sfLandscapeTranslator::ApplyServerWeightmapData(ULandscapeComponent* componentPtr, sfProperty::SPtr propPtr)
{
    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(componentPtr->GetOwner());
    if (landscapePtr == nullptr || propPtr->Type() != sfProperty::VALUE)
    {
        return;
    }

    FCoreUObjectDelegates::OnObjectModified.Remove(m_onModifiedHandle);
    SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();

    const std::vector<uint8_t>& data = propPtr->AsValue()->GetValue().GetData();
    int numQuads = componentPtr->ComponentSizeQuads + 1;
    int numLayers = 0;
    std::vector<uint8_t> decodedData = sfLandscapeCompression::DecompressWeightmap(data, numQuads, numLayers);

    if (!HasNumLayersOrMore(landscapePtr, numLayers))
    {
        // We can't set the weightmaps until the landscape has the correct number of layers. Add this to the pending
        // set to apply data once the layers are correct.
        m_pendingWeightmaps.Add(landscapePtr);
        return;
    }

    FLandscapeEditDataInterface dataInterface{ componentPtr->GetLandscapeInfo() };
    for (int i = 0; i < numLayers; i++)
    {
        int offset = numQuads * numQuads * i;
        if (offset >= decodedData.size())
        {
            break;
        }
        ULandscapeLayerInfoObject* layerPtr = i == landscapePtr->EditorLayerSettings.Num() ?
            ALandscapeProxy::VisibilityLayer : landscapePtr->EditorLayerSettings[i].LayerInfoObj;
        const uint8_t* dataPtr = decodedData.data() + offset;
        // Adding layer data to a component that has no data for that layer is slow. We can avoid the slow operation
        // by setting the restriction to ExistingOnly if the layer data is all zeroes.
        ELandscapeLayerPaintingRestriction restriction = ELandscapeLayerPaintingRestriction::ExistingOnly;
        for (int j = 0; j < numQuads * numQuads; j++)
        {
            if (dataPtr[j] != 0)
            {
                restriction = ELandscapeLayerPaintingRestriction::None;
                break;
            }
        }
        dataInterface.SetAlphaData(layerPtr,
            componentPtr->SectionBaseX, componentPtr->SectionBaseY,
            componentPtr->SectionBaseX + componentPtr->ComponentSizeQuads,
            componentPtr->SectionBaseY + componentPtr->ComponentSizeQuads, dataPtr, 0,
            restriction, false);
    }
    if (numLayers == landscapePtr->EditorLayerSettings.Num() && componentPtr->ComponentHasVisibilityPainted())
    {
        // Clear visibility data
        std::vector<uint8_t> zeroData;
        zeroData.resize(numQuads * numQuads, 0);
        dataInterface.SetAlphaData(ALandscapeProxy::VisibilityLayer,
            componentPtr->SectionBaseX, componentPtr->SectionBaseY,
            componentPtr->SectionBaseX + componentPtr->ComponentSizeQuads,
            componentPtr->SectionBaseY + componentPtr->ComponentSizeQuads, zeroData.data(), 0,
            ELandscapeLayerPaintingRestriction::None, false);
    }
    SceneFusion::RedrawActiveViewport();

    SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
    m_onModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddLambda(
        [this](UObject* uobjPtr)
    {
        OnUObjectModified(uobjPtr);
    });
}

void sfLandscapeTranslator::ApplyServerData()
{
    int64_t startTime = FDateTime().Now().GetTicks();
    for (auto iter = m_componentsToUpdate.CreateIterator(); iter; ++iter)
    {
        ULandscapeComponent* componentPtr = iter.Key();
        uint8_t flags = iter.Value();
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(componentPtr);
        if (objPtr == nullptr)
        {
            iter.RemoveCurrent();
            continue;
        }
        objPtr = sfUnrealUtils::FindChildByType(objPtr, sfType::Landscape);
        if (objPtr == nullptr)
        {
            iter.RemoveCurrent();
            continue;
        }
        sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
        if ((flags & TextureTypes::HEIGHTMAP) != 0)
        {
            ApplyServerHeightmapData(componentPtr, propertiesPtr->Get(sfProp::Heightmap));
            iter.Value() &= ~TextureTypes::HEIGHTMAP;

            if (FTimespan(FDateTime().Now().GetTicks() - startTime).GetTotalMilliseconds() > APPLY_CHANGE_TIME_MS)
            {
                break;
            }
        }
        if ((flags & TextureTypes::OFFSETMAP) != 0)
        {
            ApplyServerOffsetmapData(componentPtr, propertiesPtr->Get(sfProp::Offsetmap));
            iter.Value() &= ~TextureTypes::OFFSETMAP;

            if (FTimespan(FDateTime().Now().GetTicks() - startTime).GetTotalMilliseconds() > APPLY_CHANGE_TIME_MS)
            {
                break;
            }
        }
        if ((flags & TextureTypes::WEIGHTMAP) != 0)
        {
            ApplyServerWeightmapData(componentPtr, propertiesPtr->Get(sfProp::Weightmap));
            iter.Value() &= ~TextureTypes::WEIGHTMAP;
        }
        iter.RemoveCurrent();
        if (FTimespan(FDateTime().Now().GetTicks() - startTime).GetTotalMilliseconds() > APPLY_CHANGE_TIME_MS)
        {
            break;
        }
    }
}

ALandscapeProxy* sfLandscapeTranslator::GetActiveLandscape()
{
    return m_activeLandscapePtr;
}

void sfLandscapeTranslator::UpdateTool()
{
    int tool = Tools::None;
    ALandscapeProxy* landscapePtr = nullptr;
    FEdMode* modePtr = GLevelEditorModeTools().GetActiveMode("EM_Landscape");
    if (modePtr != nullptr)
    {
        sfLandscapeEdModeHack* hackPtr = static_cast<sfLandscapeEdModeHack*>(modePtr);
        tool = hackPtr->CurrentToolIndex;
        if (hackPtr->CurrentToolTarget.LandscapeInfo != nullptr)
        {
            landscapePtr = hackPtr->CurrentToolTarget.LandscapeInfo->LandscapeActor.Get();
        }
    }
    if (landscapePtr != nullptr && !sfObjectMap::Contains(landscapePtr))
    {
        landscapePtr = nullptr;
    }
    if (tool != m_tool || landscapePtr != m_activeLandscapePtr)
    {
        OnToolChange(tool, landscapePtr);
        m_tool = tool;
        m_activeLandscapePtr = landscapePtr;
    }
}

void sfLandscapeTranslator::OnToolChange(int tool, ALandscapeProxy* landscapePtr)
{
    if (m_lockedObjPtr != nullptr && m_tool == Tools::Splines)
    {
        // We used to be editing splines. 
        if (m_lockedObjPtr->IsLocked())
        {
            sfObject::SPtr objPtr = sfObjectMap::GetSFObject(m_activeLandscapePtr);
            if (objPtr != nullptr && !objPtr->IsFullyLocked())
            {
                // Splines are locked. Remove the partial lock shader the only shows in spline mode.
                UsfLockComponent* lockPtr = sfActorUtil::GetComponent<UsfLockComponent>(m_activeLandscapePtr);
                if (lockPtr != nullptr)
                {
                    lockPtr->SetLandscapeMaterial(nullptr);
                }
            }
        }
        // Release the lock on the spline component.
        m_lockedObjPtr->ReleaseLock();
        m_lockedObjPtr = nullptr;
    }
    if (tool == Tools::Splines && landscapePtr != nullptr)
    {
        // We are editing splines. Bad things happen if 2 people try to edit splines at once, so lock the spline
        // component to prevent others from editing splines.
        m_lockedObjPtr = sfObjectMap::GetSFObject(landscapePtr->SplineComponent);
        if (m_lockedObjPtr == nullptr)
        {
            // If there is no spline component, lock the landscape instead.
            m_lockedObjPtr = sfObjectMap::GetSFObject(landscapePtr);
        }
        else if (m_lockedObjPtr->IsLockedDirectly())
        {
            sfObject::SPtr objPtr = sfObjectMap::GetSFObject(landscapePtr);
            if (objPtr != nullptr && !objPtr->IsFullyLocked())
            {
                // Splines are locked. Show the partial lock shader that only shows in spline mode.
                UsfLockComponent* lockPtr = sfActorUtil::GetComponent<UsfLockComponent>(landscapePtr);
                if (lockPtr != nullptr)
                {
                    lockPtr->SetLandscapeMaterial(SceneFusion::GetLandscapeLockMaterial(nullptr));
                }
            }
        }
        if (m_lockedObjPtr != nullptr)
        {
            m_lockedObjPtr->RequestLock();
        }
    }
    if (tool == Tools::None)
    {
        m_lockedObjPtr = nullptr;
    }
    else if (tool == Tools::MoveToLevel)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(landscapePtr);
    }
    else if (m_lockedObjPtr == nullptr)
    {
        // Set locked object to the landscape object to prevent edits while the landscape is locked.
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(landscapePtr);
        if (objPtr != nullptr)
        {
            m_lockedObjPtr = sfUnrealUtils::FindChildByType(objPtr, sfType::Landscape, true);
        }
    }
}

bool sfLandscapeTranslator::CanEdit()
{
    if (m_tool == Tools::MoveToLevel && m_activeLandscapePtr != nullptr)
    {
        // If we are using the move to level tool and the current level's landscape proxy is locked, prevent edits.
        ULandscapeInfo* infoPtr = m_activeLandscapePtr->GetLandscapeInfo();
        if (infoPtr != nullptr)
        {
            sfObject::SPtr objPtr = sfObjectMap::GetSFObject(infoPtr->GetCurrentLevelLandscapeProxy(false));
            if (objPtr != nullptr && !objPtr->CanEdit())
            {
                return false;
            }
        }
    }
    return m_lockedObjPtr == nullptr || m_lockedObjPtr->CanEdit();
}

void sfLandscapeTranslator::RefreshLayers()
{
    // This is a hack to make the UI notice changes to the list of weight layers.
    if (m_tool == Tools::None || m_activeLandscapePtr == nullptr)
    {
        return;
    }
    FEdMode* modePtr = GLevelEditorModeTools().GetActiveMode("EM_Landscape");
    if (modePtr == nullptr)
    {
        return;
    }
    // Cast to our hack class to expose the current tool target member.
    sfLandscapeEdModeHack* hackPtr = static_cast<sfLandscapeEdModeHack*>(modePtr);
    ULandscapeInfo* infoPtr = hackPtr->CurrentToolTarget.LandscapeInfo.Get();
    if (infoPtr != nullptr && !m_staleLandscapeLayers.Contains(infoPtr->LandscapeActor.Get()))
    {
        // The active landscape did not have weight layer changes.
        return;
    }
    // ULandscapeInfo::StaticClass() causes unresolved externals, so to avoid errors we load the class this way.
    UClass* classPtr = sfUnrealUtils::LoadClass("LandscapeInfo");
    if (classPtr == nullptr)
    {
        return;
    }
    // Calling select will rebuild the layers list in the UI, but only if the current tool target's layer info is
    // different from the passed in landscape's info and is not null, so we set it to a dummy landscape info before
    // calling select.
    ULandscapeInfo* dummyInfoPtr = static_cast<ULandscapeInfo*>(NewObject<UObject>(GetTransientPackage(), classPtr));
    hackPtr->CurrentToolTarget.LandscapeInfo = dummyInfoPtr;
    hackPtr->Select(m_activeLandscapePtr, true);
    // If the landscape was not in an editable state, the landscape info will not have been set back and will still be
    // pointing to the dummy, so we may need to set it back.
    if (hackPtr->CurrentToolTarget.LandscapeInfo == dummyInfoPtr)
    {
        hackPtr->CurrentToolTarget.LandscapeInfo = m_activeLandscapePtr->GetLandscapeInfo();
    }
}

bool sfLandscapeTranslator::HasNumLayersOrMore(ALandscapeProxy* landscapePtr, int numLayers)
{
    // + 1 for visibility layer
    if (landscapePtr->EditorLayerSettings.Num() + 1 < numLayers)
    {
        return false;
    }
    int num = landscapePtr->EditorLayerSettings.Num() < numLayers ?
        landscapePtr->EditorLayerSettings.Num() : numLayers;
    for (int i = 0; i < num; i++)
    {
        if (landscapePtr->EditorLayerSettings[i].LayerInfoObj == nullptr)
        {
            return false;
        }
    }
    return true;
}

UTexture2D* sfLandscapeTranslator::GetHeightmap(ULandscapeComponent* componentPtr)
{
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 22
    return componentPtr->GetHeightmap();
#else
    return componentPtr->HeightmapTexture;
#endif
}

void sfLandscapeTranslator::SetHeightmap(ULandscapeComponent* componentPtr, UTexture2D* heightmapPtr)
{
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 22
    componentPtr->SetHeightmap(heightmapPtr);
#else
    componentPtr->HeightmapTexture = heightmapPtr;
#endif
}

// This function is adapted from FLandscapeToolStrokeAddComponent::Apply in LandscapeEdModeComponentTools.cpp
void sfLandscapeTranslator::OnAddComponent(ULandscapeComponent* componentPtr)
{
    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(componentPtr->GetOwner());
    if (landscapePtr == nullptr)
    {
        return;
    }

    landscapePtr->LandscapeComponents.Add(componentPtr);
    componentPtr->Init(componentPtr->SectionBaseX, componentPtr->SectionBaseY,
        landscapePtr->ComponentSizeQuads, landscapePtr->NumSubsections, landscapePtr->SubsectionSizeQuads);
    componentPtr->UpdatedSharedPropertiesFromActor();

    int verts = (landscapePtr->SubsectionSizeQuads + 1) * landscapePtr->NumSubsections;
    componentPtr->WeightmapScaleBias = FVector4(1.0f / (float)verts, 1.0f / (float)verts, 0.5f / (float)verts,
        0.5f / (float)verts);
    componentPtr->WeightmapSubsectionOffset = (float)(landscapePtr->SubsectionSizeQuads + 1) / (float)verts;

    TArray<FColor> heightData;
    heightData.Empty(FMath::Square(verts));
    heightData.AddZeroed(FMath::Square(verts));
    componentPtr->InitHeightmapData(heightData, true);
    componentPtr->UpdateMaterialInstances();

    ULandscapeInfo* infoPtr = landscapePtr->GetLandscapeInfo();
    FIntPoint coords{ componentPtr->SectionBaseX, componentPtr->SectionBaseY };
    coords /= componentPtr->SubsectionSizeQuads;
    ULandscapeComponent* otherPtr = infoPtr->XYtoComponentMap.FindRef(coords);
    if (otherPtr != nullptr)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(otherPtr);
        if (objPtr != nullptr && objPtr->IsCreatePending())
        {
            // We tried to create a component at the same location. Add ours to the duplicate set to be deleted.
            m_duplicateComponents.Add(otherPtr);
        }
    }
    infoPtr->XYtoComponentMap.Add(coords, componentPtr);
    infoPtr->XYtoAddCollisionMap.Remove(coords);

    // Update collision
    componentPtr->UpdateCachedBounds();
    componentPtr->UpdateBounds();
    componentPtr->MarkRenderStateDirty();

    // Update/add collision around the component
    for (int x = coords.X - 1; x <= coords.X + 1; x++)
    {
        for (int y = coords.Y - 1; y <= coords.Y + 1; y++)
        {
            if (x != coords.X || y != coords.Y)
            {
                FIntPoint p{ x, y };
                if (!infoPtr->XYtoComponentMap.FindRef(p))
                {
                    infoPtr->UpdateAddCollision(p);
                }
            }
        }
    }
}

// This function is adapted from FEdModeLandscape::DeleteLandscapeComponents in LandscapeEdMode.cpp. Unlike the
// original function, this one deletes one component instead of a set of components, updates the add collision around
// the component, and it does not trigger as many modify events or change the user's selection.
void sfLandscapeTranslator::OnDeleteComponent(ULandscapeComponent* componentPtr)
{
    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(componentPtr->GetOwner());
    if (landscapePtr == nullptr)
    {
        return;
    }

    ULandscapeInfo* infoPtr = landscapePtr->GetLandscapeInfo();
    int componentSizeVerts = infoPtr->ComponentNumSubsections * (infoPtr->SubsectionSizeQuads + 1);
    int needHeightmapSize = 1 << FMath::CeilLogTwo(componentSizeVerts);

    // Need to split all the components which share a heightmap with the deleted component
    TSet<ULandscapeComponent*> splitList;
    int searchX = GetHeightmap(componentPtr)->Source.GetSizeX() / needHeightmapSize;
    int searchY = GetHeightmap(componentPtr)->Source.GetSizeY() / needHeightmapSize;
    FIntPoint coords = componentPtr->GetSectionBase() / componentPtr->ComponentSizeQuads;
    for (int y = 0; y < searchY; y++)
    {
        for (int x = 0; x < searchX; x++)
        {
            // Search for four directions...
            for (int dir = 0; dir < 4; dir++)
            {
                int xDir = (dir >> 1) ? 1 : -1;
                int yDir = (dir % 2) ? 1 : -1;
                ULandscapeComponent* neighbourPtr = infoPtr->XYtoComponentMap.FindRef(
                    coords + FIntPoint(xDir * x, yDir * y));
                if (neighbourPtr != nullptr && GetHeightmap(neighbourPtr) == GetHeightmap(componentPtr))
                {
                    splitList.Add(neighbourPtr);
                }
            }
        }
    }
    for (ULandscapeComponent* currentPtr : splitList)
    {
        SplitHeightmap(currentPtr);
    }

    // Invalidate lighting and regregister components around the deleted component
    for (int x = coords.X - 1; x <= coords.X + 1; x++)
    {
        for (int y = coords.Y - 1; y <= coords.Y + 1; y++)
        {
            if (x != coords.X || y != coords.Y)
            {
                FIntPoint p{ x, y };
                ULandscapeComponent* neighbourPtr = infoPtr->XYtoComponentMap.FindRef(p);
                if (neighbourPtr != nullptr)
                {
                    neighbourPtr->InvalidateLightingCache();
                    FComponentReregisterContext reregisterContext(neighbourPtr);
                }
            }
        }
    }

    UTexture2D* heightmapPtr = GetHeightmap(componentPtr);
    if (heightmapPtr != nullptr)
    {
        heightmapPtr->SetFlags(RF_Transactional);
        heightmapPtr->Modify();
        heightmapPtr->MarkPackageDirty();
        heightmapPtr->ClearFlags(RF_Standalone);
    }
    for (int i = 0; i < GetWeightmapTextures(componentPtr).Num(); i++)
    {
        GetWeightmapTextures(componentPtr)[i]->SetFlags(RF_Transactional);
        GetWeightmapTextures(componentPtr)[i]->Modify();
        GetWeightmapTextures(componentPtr)[i]->MarkPackageDirty();
        GetWeightmapTextures(componentPtr)[i]->ClearFlags(RF_Standalone);
    }
    if (componentPtr->XYOffsetmapTexture)
    {
        componentPtr->XYOffsetmapTexture->SetFlags(RF_Transactional);
        componentPtr->XYOffsetmapTexture->Modify();
        componentPtr->XYOffsetmapTexture->MarkPackageDirty();
        componentPtr->XYOffsetmapTexture->ClearFlags(RF_Standalone);
    }

    // Update add collision on the next tick after the component is destroyed.
    m_addCollisionSet.Add(infoPtr);

    ULandscapeHeightfieldCollisionComponent* collisionCompPtr = componentPtr->CollisionComponent.Get();
    if (collisionCompPtr != nullptr)
    {
        collisionCompPtr->DestroyComponent();
    }
}

// This function is adapted from FLandscapeToolStrokeMoveToLevel::Apply in LandscapeEdModeComponentTools.cpp. Unlike
// the original function, this one moves a specific component rather than moving all components within the brush area,
// does not create a new landscape streaming proxy to be the parent if there isn't one already, does not check if
// referenced assets need to be moved to different package, triggers less modify events and does not modify the user's
// selection.
void sfLandscapeTranslator::PreMoveComponent(ULandscapeComponent* componentPtr, ALandscapeProxy* newParentPtr)
{
    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(componentPtr->GetOwner());
    if (landscapePtr == nullptr)
    {
        return;
    }

    // If the old parent is locked and the new one is not, remove the lock shader from the component
    sfObject::SPtr oldPtr = sfObjectMap::GetSFObject(landscapePtr);
    sfObject::SPtr newPtr = sfObjectMap::GetSFObject(newParentPtr);
    if (oldPtr != nullptr && oldPtr->IsFullyLocked() && (newPtr == nullptr || !newPtr->IsFullyLocked()))
    {
        componentPtr->EditToolRenderData.ToolMaterial = nullptr;
        componentPtr->EditToolRenderData.GizmoMaterial = nullptr;
        componentPtr->UpdateEditToolRenderData();
    }

    ULandscapeInfo* infoPtr = landscapePtr->GetLandscapeInfo();
    int componentSizeVerts = infoPtr->ComponentNumSubsections * (infoPtr->SubsectionSizeQuads + 1);
    int needHeightmapSize = 1 << FMath::CeilLogTwo(componentSizeVerts);

    // Need to split all the components which share a heightmap with the moved component
    TSet<ULandscapeComponent*> splitList;
    // Search neighbours only
    int searchX = GetHeightmap(componentPtr)->Source.GetSizeX() / needHeightmapSize - 1;
    int searchY = GetHeightmap(componentPtr)->Source.GetSizeY() / needHeightmapSize - 1;
    FIntPoint coords = componentPtr->GetSectionBase() / componentPtr->ComponentSizeQuads;

    for (int y = -searchY; y <= searchY; y++)
    {
        for (int x = -searchX; x <= searchX; x++)
        {
            ULandscapeComponent* neighbourPtr = infoPtr->XYtoComponentMap.FindRef(coords + FIntPoint(x, y));
            if (neighbourPtr != nullptr && GetHeightmap(neighbourPtr) == GetHeightmap(componentPtr))
            {
                splitList.Add(neighbourPtr);
            }
        }
    }
    UObject* outermost = newParentPtr->GetOutermost();
    for (ULandscapeComponent* currentPtr : splitList)
    {
        SplitHeightmap(currentPtr, currentPtr == componentPtr ? outermost : nullptr);
    }

    UTexture2D* heightmapPtr = GetHeightmap(componentPtr);
    if (heightmapPtr != nullptr)
    {
        heightmapPtr->SetFlags(RF_Transactional);
        heightmapPtr->Modify();
        heightmapPtr->MarkPackageDirty();
        heightmapPtr->ClearFlags(RF_Standalone);
    }
    if (componentPtr->XYOffsetmapTexture != nullptr)
    {
        componentPtr->XYOffsetmapTexture->Rename(nullptr, outermost);
    }

    // Change Weight maps...
    {
        FLandscapeEditDataInterface landscapeEdit(infoPtr);
        int totalNeededChannels = GetWeightmapLayerAllocations(componentPtr).Num();
        int currentLayer = 0;
        TArray<UTexture2D*> newWeightmapTextures;

        // Code from ULandscapeComponent::ReallocateWeightmaps
        // Move to other channels left
        while (totalNeededChannels > 0)
        {
            UTexture2D* currentWeightmapPtr = nullptr;
            LandscapeWeightmapUsage* currentWeightmapUsagePtr = nullptr;

            if (totalNeededChannels < 4)
            {
                // see if we can find a suitable existing weightmap texture with sufficient channels
                int bestDistanceSquared = MAX_int32;
                for (auto& weightmapUsagePair : newParentPtr->WeightmapUsageMap)
                {
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
                    LandscapeWeightmapUsage* tryWeightmapUsagePtr = weightmapUsagePair.Value;
#else
                    LandscapeWeightmapUsage* tryWeightmapUsagePtr = &weightmapUsagePair.Value;
#endif
                    if (tryWeightmapUsagePtr->FreeChannelCount() >= totalNeededChannels)
                    {
                        // See if this candidate is closer than any others we've found
                        for (int chanIdx = 0; chanIdx < 4; chanIdx++)
                        {
                            if (tryWeightmapUsagePtr->ChannelUsage[chanIdx] != nullptr)
                            {
                                int tryDistanceSquared =
                                    (tryWeightmapUsagePtr->ChannelUsage[chanIdx]->GetSectionBase() -
                                    componentPtr->GetSectionBase()).SizeSquared();
                                if (tryDistanceSquared < bestDistanceSquared)
                                {
                                    currentWeightmapPtr = weightmapUsagePair.Key;
                                    currentWeightmapUsagePtr = tryWeightmapUsagePtr;
                                    bestDistanceSquared = tryDistanceSquared;
                                }
                            }
                        }
                    }
                }
            }

            bool needsUpdateResource = false;
            // No suitable weightmap texture
            if (currentWeightmapPtr == nullptr)
            {
                componentPtr->MarkPackageDirty();

                // Weightmap is sized the same as the component
                int weightmapSize = (componentPtr->SubsectionSizeQuads + 1) * componentPtr->NumSubsections;

                // We need a new weightmap texture
                currentWeightmapPtr = newParentPtr->CreateLandscapeTexture(weightmapSize, weightmapSize,
                    TEXTUREGROUP_Terrain_Weightmap, TSF_BGRA8);
                // Alloc dummy mips
                componentPtr->CreateEmptyTextureMips(currentWeightmapPtr);
                currentWeightmapPtr->PostEditChange();

                // Store it in the usage map
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
                currentWeightmapUsagePtr = newParentPtr->WeightmapUsageMap.Add(currentWeightmapPtr,
                    newParentPtr->CreateWeightmapUsage());
#else
                currentWeightmapUsagePtr = &newParentPtr->WeightmapUsageMap.Add(currentWeightmapPtr,
                    LandscapeWeightmapUsage());
#endif
            }

            newWeightmapTextures.Add(currentWeightmapPtr);

            for (int chanIdx = 0; chanIdx < 4 && totalNeededChannels > 0; chanIdx++)
            {
                if (currentWeightmapUsagePtr->ChannelUsage[chanIdx] == nullptr)
                {
                    // Use this allocation
                    FWeightmapLayerAllocationInfo& allocInfo
                        = GetWeightmapLayerAllocations(componentPtr)[currentLayer];

                    if (allocInfo.WeightmapTextureIndex == 255)
                    {
                        // New layer - zero out the data for this texture channel
                        landscapeEdit.ZeroTextureChannel(currentWeightmapPtr, chanIdx);
                    }
                    else
                    {
                        UTexture2D* oldWeightmapPtr
                            = GetWeightmapTextures(componentPtr)[allocInfo.WeightmapTextureIndex];

                        // Copy the data
                        landscapeEdit.CopyTextureChannel(currentWeightmapPtr, chanIdx, oldWeightmapPtr,
                            allocInfo.WeightmapTextureChannel);
                        landscapeEdit.ZeroTextureChannel(oldWeightmapPtr, allocInfo.WeightmapTextureChannel);

                        // Remove the old allocation
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
                        LandscapeWeightmapUsage* oldWeightmapUsagePtr = componentPtr->GetLandscapeProxy()
                            ->WeightmapUsageMap.FindRef(oldWeightmapPtr);
#else
                        LandscapeWeightmapUsage* oldWeightmapUsagePtr = componentPtr->GetLandscapeProxy()
                            ->WeightmapUsageMap.Find(oldWeightmapPtr);
#endif
                        oldWeightmapUsagePtr->ChannelUsage[allocInfo.WeightmapTextureChannel] = nullptr;
                    }

                    // Assign the new allocation
                    currentWeightmapUsagePtr->ChannelUsage[chanIdx] = componentPtr;
                    allocInfo.WeightmapTextureIndex = newWeightmapTextures.Num() - 1;
                    allocInfo.WeightmapTextureChannel = chanIdx;
                    currentLayer++;
                    totalNeededChannels--;
                }
            }
        }

        // Replace the weightmap textures
        GetWeightmapTextures(componentPtr) = newWeightmapTextures;

        // Update the mipmaps for the textures we edited
        for (UTexture2D* weightmapPtr : GetWeightmapTextures(componentPtr))
        {
            FLandscapeTextureDataInfo* WeightmapDataInfo = landscapeEdit.GetTextureDataInfo(weightmapPtr);

            int numMips = weightmapPtr->Source.GetNumMips();
            TArray<FColor*> weightmapMipData;
            weightmapMipData.AddUninitialized(numMips);
            for (int32 mipIdx = 0; mipIdx < numMips; mipIdx++)
            {
                weightmapMipData[mipIdx] = (FColor*)WeightmapDataInfo->GetMipData(mipIdx);
            }

            ULandscapeComponent::UpdateWeightmapMips(componentPtr->NumSubsections, componentPtr->SubsectionSizeQuads,
                weightmapPtr, weightmapMipData, 0, 0, MAX_int32, MAX_int32, WeightmapDataInfo);
        }
        // Need to repack all the weight maps (to make it packed well...)
        landscapePtr->RemoveInvalidWeightmaps();
    }

    landscapePtr->LandscapeComponents.Remove(componentPtr);
    newParentPtr->LandscapeComponents.Add(componentPtr);
    componentPtr->InvalidateLightingCache();

    // Clear transient mobile data
    componentPtr->MobileDataSourceHash.Invalidate();
    componentPtr->MobileMaterialInterfaces.Reset();
    componentPtr->MobileWeightmapTextures.Reset();

    // Move collision component
    ULandscapeHeightfieldCollisionComponent* collisionPtr = componentPtr->CollisionComponent.Get();
    if (collisionPtr != nullptr)
    {
        landscapePtr->CollisionComponents.Remove(collisionPtr);
        collisionPtr->UnregisterComponent();
        collisionPtr->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        collisionPtr->Rename(nullptr, newParentPtr);
        newParentPtr->CollisionComponents.Add(collisionPtr);
        collisionPtr->AttachToComponent(newParentPtr->GetRootComponent(),
            FAttachmentTransformRules::KeepWorldTransform);
    }
}

// This function is copied from LandscapeEdMode.cpp and modified to fit our coding style
void sfLandscapeTranslator::SplitHeightmap(ULandscapeComponent* componentPtr, UObject* outerPtr)
{
    ULandscapeInfo* infoPtr = componentPtr->GetLandscapeInfo();
    ALandscape* landscapePtr = infoPtr->LandscapeActor.Get();
    int componentSizeVerts = componentPtr->NumSubsections * (componentPtr->SubsectionSizeQuads + 1);
    // make sure the heightmap UVs are powers of two.
    int heightmapSizeU = (1 << FMath::CeilLogTwo(componentSizeVerts));
    int heightmapSizeV = (1 << FMath::CeilLogTwo(componentSizeVerts));

    UTexture2D* heightmapPtr = NULL;
    TArray<FColor*> heightmapMipData;
    // Scope for FLandscapeEditDataInterface
    {
        // Read old data and split
        FLandscapeEditDataInterface landscapeEdit(infoPtr);
        TArray<uint8> heightData;
        heightData.AddZeroed(
            (1 + componentPtr->ComponentSizeQuads)*(1 + componentPtr->ComponentSizeQuads) * sizeof(uint16));
        // Because of edge problem, normal would be just copy from old component data
        TArray<uint8> normalData;
        normalData.AddZeroed(
            (1 + componentPtr->ComponentSizeQuads)*(1 + componentPtr->ComponentSizeQuads) * sizeof(uint16));
        landscapeEdit.GetHeightDataFast(componentPtr->GetSectionBase().X, componentPtr->GetSectionBase().Y,
            componentPtr->GetSectionBase().X + componentPtr->ComponentSizeQuads,
            componentPtr->GetSectionBase().Y + componentPtr->ComponentSizeQuads,
            (uint16*)heightData.GetData(), 0, (uint16*)normalData.GetData());

        // Construct the heightmap textures
        heightmapPtr = componentPtr->GetLandscapeProxy()->CreateLandscapeTexture(heightmapSizeU, heightmapSizeV,
            TEXTUREGROUP_Terrain_Heightmap, TSF_BGRA8, outerPtr);

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
        ULandscapeComponent::CreateEmptyTextureMips(heightmapPtr, true);
#else
        int mipSubsectionSizeQuads = componentPtr->SubsectionSizeQuads;
        int mipSizeU = heightmapSizeU;
        int mipSizeV = heightmapSizeV;
        while (mipSizeU > 1 && mipSizeV > 1 && mipSubsectionSizeQuads >= 1)
        {
            FColor* heightmapData = (FColor*)heightmapPtr->Source.LockMip(heightmapMipData.Num());
            FMemory::Memzero(heightmapData, mipSizeU*mipSizeV * sizeof(FColor));
            heightmapMipData.Add(heightmapData);

            mipSizeU >>= 1;
            mipSizeV >>= 1;

            mipSubsectionSizeQuads = ((mipSubsectionSizeQuads + 1) >> 1) - 1;
        }
#endif

        componentPtr->HeightmapScaleBias = FVector4(1.0f / (float)heightmapSizeU,
            1.0f / (float)heightmapSizeV, 0.0f, 0.0f);

        SetHeightmap(componentPtr, heightmapPtr);

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
        landscapeEdit.SetHeightData(componentPtr->GetSectionBase().X, componentPtr->GetSectionBase().Y,
            componentPtr->GetSectionBase().X + componentPtr->ComponentSizeQuads,
            componentPtr->GetSectionBase().Y + componentPtr->ComponentSizeQuads,
            (uint16*)heightData.GetData(), 0, false, (uint16*)normalData.GetData());
        componentPtr->UpdateMaterialInstances();
#else
        componentPtr->UpdateMaterialInstances();
        for (int i = 0; i < heightmapMipData.Num(); i++)
        {
            heightmapPtr->Source.UnlockMip(i);
        }
        landscapeEdit.SetHeightData(componentPtr->GetSectionBase().X, componentPtr->GetSectionBase().Y,
            componentPtr->GetSectionBase().X + componentPtr->ComponentSizeQuads,
            componentPtr->GetSectionBase().Y + componentPtr->ComponentSizeQuads,
            (uint16*)heightData.GetData(), 0, false, (uint16*)normalData.GetData());
#endif
    }

    // End of LandscapeEdit interface
    heightmapPtr->PostEditChange();
    // Reregister
    FComponentReregisterContext reregisterContext(componentPtr);
}

TArray<UTexture2D*>& sfLandscapeTranslator::GetWeightmapTextures(ULandscapeComponent* componentPtr)
{
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
    return componentPtr->GetWeightmapTextures();
#else
    return componentPtr->WeightmapTextures;
#endif
}

TArray<FWeightmapLayerAllocationInfo>& sfLandscapeTranslator::GetWeightmapLayerAllocations(
    ULandscapeComponent* componentPtr)
{
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
    return componentPtr->GetWeightmapLayerAllocations();
#else
    return componentPtr->WeightmapLayerAllocations;
#endif
}

#undef SEND_CHANGE_INTERVAL
#undef APPLY_CHANGE_TIME_MS
#undef LOG_CHANNEL
