#include "../../Public/Translators/sfModelTranslator.h"
#include "../../Public/Translators/sfComponentTranslator.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/Consts.h"
#include "../../Public/sfBufferArchive.h"
#include "../../Public/sfLoader.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/Components/sfLockComponent.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/sfUndoManager.h"
#include "../../Public/sfSelectionManager.h"
#include "../../Public/sfActorUtil.h"
#include "../UI/sfDetailsPanelManager.h"
#include <sfValueProperty.h>
#include <Engine/Brush.h>
#include <Engine/BrushBuilder.h>
#include <GameFramework/Volume.h>
#include <Editor.h>
#include <EdMode.h>
#include <Components/BrushComponent.h>

// In seconds
#define BSP_REBUILD_DELAY .2f;
#define SYNC_DELAY 0.1f;

using namespace KS;

sfModelTranslator::sfModelTranslator()
{
    sfPropertyManager::Get().AddPropertyToForceSyncList("Brush", "BrushBuilder");
    sfPropertyManager::Get().AddPropertyToForceSyncList("Brush", "PolyFlags");
}

void sfModelTranslator::Initialize()
{
    m_modelSyncEnabled = SceneFusion::Get().Service->Session()->GetObjectLimit(sfType::Model) != 0;
    m_showedDisabledMessage = false;
    m_actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(sfType::Actor);
    m_checkChangeTimer = 0;
    m_bspRebuildDelay = -1.0f;

    m_tickHandle = SceneFusion::Get().OnTick.AddRaw(this, &sfModelTranslator::Tick);
    m_onLevelDirtiedHandle = ULevel::LevelDirtiedEvent.AddRaw(this, &sfModelTranslator::OnLevelDirtied);
    m_onLockHandle = m_actorTranslatorPtr->OnLockStateChange.AddRaw(this, &sfModelTranslator::OnLockChange);
    if (m_modelSyncEnabled)
    {
        m_onDeselectHandle = sfSelectionManager::Get().OnDeselect.AddRaw(this, &sfModelTranslator::OnDeselect);
    }
    
    // Register ABrush initializers to initialize/sync the model
    m_actorTranslatorPtr->RegisterActorInitializer<ABrush>([this](sfObject::SPtr objPtr, AActor* actorPtr)
    {
        ABrush* brushPtr = Cast<ABrush>(actorPtr);
        brushPtr->PolyFlags = 0;
        UModel* modelPtr = NewObject<UModel>(brushPtr, NAME_None, RF_Transactional);
        modelPtr->Initialize(nullptr, true);
        modelPtr->Polys = NewObject<UPolys>(modelPtr, NAME_None, RF_Transactional);
        brushPtr->GetBrushComponent()->Brush = modelPtr;
        brushPtr->Brush = modelPtr;
    });
    if (m_modelSyncEnabled)
    {
        m_actorTranslatorPtr->RegisterObjectInitializer<ABrush>([this](sfObject::SPtr objPtr, AActor* actorPtr)
        {
            ABrush* brushPtr = Cast<ABrush>(actorPtr);
            if (brushPtr->Brush == nullptr)
            {
                return;
            }
            sfObject::SPtr childPtr = CreateObject(brushPtr->Brush);
            objPtr->AddChild(childPtr);
        });
    }
    m_actorTranslatorPtr->RegisterPostPropertyChangeHandler<ABrush>("BrushType", [this](UObject* uobjPtr)
    {
        ABrush* brushPtr = Cast<ABrush>(uobjPtr);
        if (brushPtr != nullptr)
        {
            MarkBSPStale(brushPtr->GetLevel());
        }
    });
    m_actorTranslatorPtr->RegisterPostPropertyChangeHandler<ABrush>("PolyFlags", [this](UObject* uobjPtr)
    {
        ABrush* brushPtr = Cast<ABrush>(uobjPtr);
        if (brushPtr != nullptr)
        {
            MarkBSPStale(brushPtr->GetLevel());
        }
    });

    TSharedPtr<sfComponentTranslator> componentTranslatorPtr = SceneFusion::Get().GetTranslator<sfComponentTranslator>(
        sfType::Component);
    componentTranslatorPtr->RegisterTransformChangeHandler<UBrushComponent>(
        [this](sfObject::SPtr objPtr, USceneComponent* componentPtr)
    {
        ABrush* brushPtr = Cast<ABrush>(componentPtr->GetOwner());
        if (brushPtr != nullptr)
        {
            MarkBSPStale(brushPtr->GetLevel());
        }
    });

    sfObjectMap::RegisterRemoveHandler<ABrush>([this](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        m_bspRebuildDelay = BSP_REBUILD_DELAY;
        ABrush* brushPtr = Cast<ABrush>(uobjPtr);
        UsfLockComponent::DestroyModelMesh(brushPtr);
    });
}

void sfModelTranslator::CleanUp()
{
    m_staleModels.clear();
    m_rebuiltModels.clear();
    SceneFusion::Get().OnTick.Remove(m_tickHandle);
    ULevel::LevelDirtiedEvent.Remove(m_onLevelDirtiedHandle);
    m_actorTranslatorPtr->OnLockStateChange.Remove(m_onLockHandle);
    if (m_modelSyncEnabled)
    {
        sfSelectionManager::Get().OnDeselect.Remove(m_onDeselectHandle);
    }
    m_actorTranslatorPtr->UnregisterActorInitializer<ABrush>();
    m_actorTranslatorPtr->UnregisterObjectInitializer<ABrush>();
}

void sfModelTranslator::Tick(float deltaTime)
{
    RebuildBSPIfNeeded(deltaTime);
    if (!m_modelSyncEnabled)
    {
        if (GLevelEditorModeTools().GetActiveMode("EM_Geometry") != nullptr)
        {
            SceneFusion::Get().ShowUpgradeLink(SceneFusion::RestrictedFeature::MODEL);
            if (!m_showedDisabledMessage)
            {
                m_showedDisabledMessage = true;
                FText message = FText::FromString(
                    "Geometry brush (bsp) syncing is disabled on your account. Upgrade to enable geometry brush syncing.");
                FText title = FText::FromString("Scene Fusion");
                FMessageDialog::Open(EAppMsgType::Ok, message, &title);
            }
        }
        return;
    }

    if (m_rebuiltModels.size() > 0 && !ABrush::NeedsRebuild())
    {
        for (sfObject::SPtr objPtr : m_rebuiltModels)
        {
            UModel* modelPtr = sfObjectMap::Get<UModel>(objPtr);
            if (modelPtr == nullptr)
            {
                continue;
            }
            ABrush* brushPtr = Cast<ABrush>(modelPtr->GetOuter());
            if (brushPtr != nullptr)
            {
                UsfLockComponent::DestroyModelMesh(brushPtr);
                UsfLockComponent* lockPtr = Cast<UsfLockComponent>(brushPtr->GetComponentByClass(
                    UsfLockComponent::StaticClass()));
                if (lockPtr != nullptr)
                {
                    lockPtr->DestroyChildren();
                    lockPtr->CreateOrFindModelMesh();
                }
            }
        }
        m_rebuiltModels.clear();
    }

    if (m_checkChangeTimer <= 0)
    {
        return;
    }
    m_checkChangeTimer -= deltaTime;
    if (m_checkChangeTimer > 0)
    {
        return;
    }
    for (sfObject::SPtr objPtr : m_staleModels)
    {
        UModel* modelPtr = sfObjectMap::Get<UModel>(objPtr);
        if (modelPtr != nullptr)
        {
            Sync(objPtr, modelPtr);
        }
    }
    m_staleModels.clear();
}

void sfModelTranslator::Sync(sfObject::SPtr objPtr, UModel* modelPtr)
{
    if (objPtr->IsLocked())
    {
        ApplyServerData(modelPtr, objPtr->Property());
    }
    else
    {
        sfBufferArchive writer;
        Serialize(writer, modelPtr);
        sfValueProperty::SPtr propPtr = sfValueProperty::Create(
            ksMultiType{ ksMultiType::BYTE_ARRAY, writer.GetData(), (size_t)writer.Num(), writer.Num() });
        if (!propPtr->Equals(objPtr->Property()))
        {
            objPtr->SetProperty(propPtr);
            for (uint32_t pathId : writer.MissingPathIds())
            {
                sfLoader::Get().AddStandInReference(pathId, propPtr);
            }
        }
    }
}

void sfModelTranslator::ApplyServerData(UModel* modelPtr, sfProperty::SPtr propPtr)
{
    const ksMultiType& multiType = propPtr->AsValue()->GetValue();
    sfBufferReader reader{ (void*)multiType.GetData().data(), (int)multiType.GetData().size() };
    Serialize(reader, modelPtr);
    for (uint32_t pathId : reader.MissingPathIds())
    {
        sfLoader::Get().AddStandInReference(pathId, propPtr);
    }

    AVolume* volumePtr = Cast<AVolume>(modelPtr->GetOuter());
    if (volumePtr != nullptr && volumePtr->GetBrushComponent() != nullptr)
    {
        // Refresh volume geometry
        volumePtr->GetBrushComponent()->ReregisterComponent();
    }

    ABrush* brushPtr = Cast<ABrush>(modelPtr->GetOuter());
    if (brushPtr != nullptr)
    {
        MarkBSPStale(brushPtr->GetLevel());
    }
}

bool sfModelTranslator::Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr)
{
    if (!uobjPtr->IsA<UModel>())
    {
        return false;
    }
    if (m_modelSyncEnabled)
    {
        outObjPtr = sfObject::Create(sfType::Model);
        sfObjectMap::Add(outObjPtr, uobjPtr);
    }
    return true;
}

sfObject::SPtr sfModelTranslator::CreateObject(UModel* modelPtr)
{
    sfObject::SPtr objPtr = sfObjectMap::GetOrCreateSFObject(modelPtr, sfType::Model);
    sfBufferArchive writer;
    Serialize(writer, modelPtr);
    sfValueProperty::SPtr propPtr = sfValueProperty::Create(
        ksMultiType{ ksMultiType::BYTE_ARRAY, writer.GetData(), (size_t)writer.Num(), writer.Num() });
    objPtr->SetProperty(propPtr);
    for (uint32_t pathId : writer.MissingPathIds())
    {
        sfLoader::Get().AddStandInReference(pathId, propPtr);
    }
    return objPtr;
}

void sfModelTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    ABrush* brushPtr = sfObjectMap::Get<ABrush>(objPtr->Parent());
    if (brushPtr == nullptr || brushPtr->Brush == nullptr)
    {
        return;
    }
    sfObjectMap::Add(objPtr, brushPtr->Brush);
    ApplyServerData(brushPtr->Brush, objPtr->Property());
    m_rebuiltModels.emplace(objPtr);

    // Set references to this model
    std::vector<sfReferenceProperty::SPtr> references = SceneFusion::Service->Session()->GetReferences(objPtr);
    sfPropertyManager::Get().SetReferences(brushPtr->Brush, references);
}

void sfModelTranslator::OnPropertyChange(sfProperty::SPtr propPtr)
{
    UModel* modelPtr = sfObjectMap::Get<UModel>(propPtr->GetContainerObject());
    if (modelPtr != nullptr)
    {
        ApplyServerData(modelPtr, propPtr);
        m_rebuiltModels.emplace(propPtr->GetContainerObject());
    }
}

void sfModelTranslator::OnUObjectModified(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    // When we modify bsp, this gets triggered for all models even though they didn't actually change, so we only check
    // for changes if the model's outer (brush) is selected. However it is possible to change surface materials without
    // selecting the brush. We can detect this by checking if the undo text is "CreateActors", which seems like the
    // the wrong undo text but that's Unreal's fault.
    if (uobjPtr->GetOuter()->IsSelected() || sfUndoManager::Get().GetUndoText() == "CreateActors")
    {
        m_staleModels.emplace(objPtr);
        m_checkChangeTimer = SYNC_DELAY;
    }
}

void sfModelTranslator::OnDeselect(AActor* actorPtr)
{
    ABrush* brushPtr = Cast<ABrush>(actorPtr);
    if (brushPtr != nullptr && brushPtr->Brush != nullptr)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(brushPtr->Brush);
        if (objPtr != nullptr && m_staleModels.erase(objPtr))
        {
            Sync(objPtr, brushPtr->Brush);
        }
    }
}

void sfModelTranslator::OnLevelDirtied()
{
    if (sfUndoManager::Get().GetUndoText() == "SetBrushProperties")
    {
        TSet<AActor*> selectedActors;
        sfDetailsPanelManager::Get().GetSelectedActors(selectedActors);
        for (AActor* actorPtr : selectedActors)
        {
            ABrush* brushPtr = Cast<ABrush>(actorPtr);
            if (brushPtr != nullptr)
            {
                sfPropertyManager::Get().SyncProperty(sfObjectMap::GetSFObject(brushPtr), brushPtr, "PolyFlags");
            }
        }
        return;
    }

    // Changing a model surface's alignment doesn't trigger the OnUObjectModified, but it does trigger OnLevelDirtied
    // with no transaction and sets InvalidSurfaces and bOnRebuildMaterialIndexBuffers to true on the level model,
    // which we detect to determine if we need to send model changes for selected brushes.
    if (!m_modelSyncEnabled || sfUndoManager::Get().GetUndoText() != "")
    {
        return;
    }
    TSet<AActor*> selectedActors;
    sfDetailsPanelManager::Get().GetSelectedActors(selectedActors);
    for (AActor* actorPtr : selectedActors)
    {
        ABrush* brushPtr = Cast<ABrush>(actorPtr);
        if (brushPtr == nullptr || brushPtr->Brush == nullptr)
        {
            continue;
        }
        UModel* levelModelPtr = brushPtr->GetLevel()->Model;
        if (levelModelPtr != nullptr && levelModelPtr->InvalidSurfaces && levelModelPtr->bOnlyRebuildMaterialIndexBuffers)
        {
            sfObject::SPtr objPtr = sfObjectMap::GetSFObject(brushPtr->Brush);
            if (objPtr != nullptr)
            {
                m_staleModels.emplace(objPtr);
                m_checkChangeTimer = SYNC_DELAY;
            }
        }
    }
}

void sfModelTranslator::OnLockChange(AActor* actorPtr, sfActorTranslator::LockType lockType, sfUser::SPtr userPtr)
{
    if (!actorPtr->IsA<ABrush>() || lockType == sfActorTranslator::LockType::Unlocked ||
        lockType == sfActorTranslator::LockType::NotSynced)
    {
        return;
    }
    UsfLockComponent* lockPtr = sfActorUtil::GetComponent<UsfLockComponent>(actorPtr);
    if (lockPtr != nullptr)
    {
        lockPtr->CreateOrFindModelMesh(SceneFusion::GetLockMaterial(userPtr));
    }
}

void sfModelTranslator::RebuildBSPIfNeeded(float deltaTime)
{
    if (m_bspRebuildDelay >= 0.0f)
    {
        if (sfSelectionManager::Get().MovingBrush())
        {
            // If we rebuild bsp while moving a brush, it will interfere with the brush movement. The bsp will get
            // rebuilt automatically when we stop moving the brush so we don't have to do anything.
            m_bspRebuildDelay = 0;
        }
        else
        {
            m_bspRebuildDelay -= deltaTime;
            if (m_bspRebuildDelay < 0.0f)
            {
                SceneFusion::ObjectEventDispatcher->DisableOnUObjectModified();
                SceneFusion::RedrawActiveViewport();
                GEditor->RebuildAlteredBSP();
                SceneFusion::ObjectEventDispatcher->EnableOnUObjectModified();
            }
        }
    }
}

void sfModelTranslator::MarkBSPStale(ULevel* levelPtr)
{
    ABrush::SetNeedRebuild(levelPtr);
    m_bspRebuildDelay = BSP_REBUILD_DELAY;
}

void sfModelTranslator::Serialize(FArchive& archive, UModel* modelPtr)
{
    archive << modelPtr->Bounds;
    archive << modelPtr->Surfs;
    archive << modelPtr->NumSharedSides;
    archive << modelPtr->Polys->Element;
    archive << modelPtr->RootOutside;
    archive << modelPtr->Linked;
    archive << modelPtr->NumUniqueVertices;

    int num = modelPtr->VertexBuffer.Vertices.Num();
    archive << num;
    modelPtr->VertexBuffer.Vertices.SetNum(num);
    for (FModelVertex& vertex : modelPtr->VertexBuffer.Vertices)
    {
        Serialize(archive, vertex);
    }

    num = modelPtr->LightmassSettings.Num();
    archive << num;
    modelPtr->LightmassSettings.SetNum(num);
    for (FLightmassPrimitiveSettings& lightmassSettings : modelPtr->LightmassSettings)
    {
        Serialize(archive, lightmassSettings);
    }

    modelPtr->Vectors.BulkSerialize(archive);
    modelPtr->Points.BulkSerialize(archive);
    modelPtr->Nodes.BulkSerialize(archive);
    modelPtr->Verts.BulkSerialize(archive);
    modelPtr->LeafHulls.BulkSerialize(archive);
    modelPtr->Leaves.BulkSerialize(archive);
}

void sfModelTranslator::Serialize(FArchive& archive, FModelVertex& vertex)
{
    archive << vertex.Position;
    archive << vertex.ShadowTexCoord;
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 20
    archive << vertex.TangentX;
    archive << vertex.TangentZ;
#else
    archive << vertex.TangentX.Vector.Packed;
    archive << vertex.TangentZ.Vector.Packed;
#endif
    archive << vertex.TexCoord;
}

void sfModelTranslator::Serialize(FArchive& archive, FLightmassPrimitiveSettings& lightmassSettings)
{
    uint8_t value = 0;
    if (lightmassSettings.bShadowIndirectOnly)
    {
        value += 1;
    }
    if (lightmassSettings.bUseEmissiveForStaticLighting)
    {
        value += 1 << 1;
    }
    if (lightmassSettings.bUseTwoSidedLighting)
    {
        value += 1 << 2;
    }
    if (lightmassSettings.bUseVertexNormalForHemisphereGather)
    {
        value += 1 << 3;
    }
    archive << value;
    lightmassSettings.bShadowIndirectOnly = (value & 1) != 0;
    lightmassSettings.bUseEmissiveForStaticLighting = (value & (1 << 1)) != 0;
    lightmassSettings.bUseTwoSidedLighting = (value & (1 << 2)) != 0;
    lightmassSettings.bUseVertexNormalForHemisphereGather = (value & (1 << 3)) != 0;

    archive << lightmassSettings.EmissiveLightFalloffExponent;
    archive << lightmassSettings.EmissiveLightExplicitInfluenceRadius;
    archive << lightmassSettings.EmissiveBoost;
    archive << lightmassSettings.DiffuseBoost;
}

#undef SYNC_DELAY
#undef BSP_REBUILD_DELAY