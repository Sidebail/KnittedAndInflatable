#include "../../Public/Components/sfLockComponent.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfUnrealUtils.h"
#include <Editor.h>
#include <Engine/Brush.h>
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 25
#include <Runtime/RawMesh/Public/RawMesh.h>
#else
#include <Developer/RawMesh/Public/RawMesh.h>
#endif
#include <LandscapeProxy.h>
#include <LandscapeComponent.h>
#include <LandscapeStreamingProxy.h>
#include <LandscapeMaterialInstanceConstant.h>
#include <Materials/MaterialInstanceConstant.h>
#include <Components/InstancedStaticMeshComponent.h>

TMap<ABrush*, UsfLockComponent::MeshData> UsfLockComponent::m_modelMeshes;

UsfLockComponent::UsfLockComponent()
{
    m_copied = false;
    m_initialized = false;
    m_materialPtr = nullptr;
    ClearFlags(RF_Transactional);// Prevent component from being recorded in transactions
    SetFlags(RF_Transient);// Prevent component from being saved
}

UsfLockComponent::~UsfLockComponent()
{
    FTicker::GetCoreTicker().RemoveTicker(m_tickerHandle);
}

void UsfLockComponent::InitializeComponent()
{
    Super::InitializeComponent();
    // Prevents the component from saving and showing in the details panel
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 21
    SetIsVisualizationComponent(true);
#else
    bIsEditorOnly = true;
#endif
    AActor* actorPtr = GetOwner();
    if (actorPtr != nullptr)
    {
        if (actorPtr->GetClass()->IsInBlueprint())
        {
            CreationMethod = EComponentCreationMethod::UserConstructionScript;
        }
    }
    FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UsfLockComponent::OnUPropertyChange);
}

void UsfLockComponent::DuplicateParentMesh(UMaterialInterface* materialPtr)
{
    m_initialized = true;
    if (materialPtr != nullptr)
    {
        m_materialPtr = materialPtr;
    }
    UMeshComponent* parentPtr = Cast<UMeshComponent>(GetAttachParent());
    if (parentPtr == nullptr)
    {
        return;
    }
    FString name = GetName();
    name.Append("Mesh");
    UMeshComponent* copyPtr = DuplicateObject(parentPtr, this, *name);
    if (copyPtr->IsPendingKill())
    {
        return;
    }
    InitializeMeshComponent(copyPtr);
    copyPtr->SetRelativeRotation(FQuat::Identity);
    copyPtr->SetRelativeScale3D(FVector::OneVector);
}

void UsfLockComponent::CreateOrFindModelMesh(UMaterialInterface* materialPtr)
{
    m_initialized = true;
    if (materialPtr != nullptr)
    {
        m_materialPtr = materialPtr;
    }
    ABrush* brushPtr = Cast<ABrush>(GetOwner());
    if (brushPtr == nullptr || brushPtr->Brush == nullptr)
    {
        return;
    }
    FString name = GetName();
    name.Append("Mesh");
    UStaticMeshComponent* componentPtr = NewObject<UStaticMeshComponent>(brushPtr, *name);
    UStaticMesh* meshPtr;
    MeshData* dataPtr = m_modelMeshes.Find(brushPtr);
    if (dataPtr == nullptr)
    {
        // We haven't generated a mesh for this brush, so generate one.
        name.Append("_");
        meshPtr = CreateMeshFromBrush(brushPtr, *name, brushPtr);
    }
    else
    {
        // Use the mesh we already generated for this brush
        meshPtr = dataPtr->MeshPtr;
    }
    componentPtr->SetStaticMesh(meshPtr);
    InitializeMeshComponent(componentPtr);
    if (dataPtr == nullptr)
    {
        // Because the brush's rotation and scale are applied to the mesh when it is generated, we need to set world
        // rotation to 0 and world scale to 1 to avoid applying rotation and scale twice.
        componentPtr->SetWorldRotation(FQuat::Identity);
        componentPtr->SetWorldScale3D(FVector::OneVector);
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 24
        m_modelMeshes.Emplace(brushPtr, 
            MeshData{ meshPtr, componentPtr->GetRelativeRotation().Quaternion(), componentPtr->GetRelativeScale3D() });
#else
        m_modelMeshes.Emplace(brushPtr,
            MeshData{ meshPtr, componentPtr->RelativeRotation.Quaternion(), componentPtr->RelativeScale3D });
#endif
    }
    else
    {
        // Set relative rotation and scale to the values we set when the mesh was generated.
        componentPtr->SetRelativeRotation(dataPtr->Rotation);
        componentPtr->SetRelativeScale3D(dataPtr->Scale);
    }
}

void UsfLockComponent::SetLandscapeMaterial(UMaterialInterface* materialPtr)
{
    m_materialPtr = materialPtr;
    UpdateLandscapeMaterials();
    SceneFusion::RedrawActiveViewport();
    if (materialPtr == nullptr)
    {
        FTicker::GetCoreTicker().RemoveTicker(m_tickerHandle);
        m_tickerHandle.Reset();
        return;
    }
    if (m_tickerHandle.IsValid())
    {
        return;
    }
    // We set the editor-only material that is intended to show the landscape brush indicator to the lock shader.
    // Because Unreal uses this material too, we need to check it every frame and set it back to the lock shader if it
    // has changed.
    m_tickerHandle = FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this](float deltaTime)
    {
        return UpdateLandscapeMaterials();
    }));
}

bool UsfLockComponent::UpdateLandscapeMaterials()
{
    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(GetOwner());
    if (landscapePtr == nullptr)
    {
        return false;
    }
    for (ULandscapeComponent* componentPtr : landscapePtr->LandscapeComponents)
    {
        if (componentPtr->EditToolRenderData.ToolMaterial != m_materialPtr ||
            componentPtr->EditToolRenderData.GizmoMaterial != m_materialPtr)
        {
            componentPtr->EditToolRenderData.ToolMaterial = m_materialPtr;
            componentPtr->EditToolRenderData.GizmoMaterial = m_materialPtr;
            componentPtr->UpdateEditToolRenderData();
        }
    }
    return true;
}

void UsfLockComponent::InitializeMeshComponent(UMeshComponent* componentPtr)
{
    componentPtr->CreationMethod = CreationMethod;
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 21
    componentPtr->SetIsVisualizationComponent(true);
#else
    componentPtr->bIsEditorOnly = true;
#endif
    componentPtr->SetRelativeLocation(FVector::ZeroVector);
    for (int i = 0; i < componentPtr->GetNumMaterials(); i++)
    {
        componentPtr->SetMaterial(i, m_materialPtr);
    }
    componentPtr->SetMobility(Mobility);
    componentPtr->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
    componentPtr->RegisterComponent();
    componentPtr->InitializeComponent();
    componentPtr->ClearFlags(RF_Transactional);// Prevent component from being recorded in transactions
    componentPtr->SetFlags(RF_Transient);// Prevent component from being saved
    SceneFusion::RedrawActiveViewport();
}

void UsfLockComponent::DestroyChildren()
{
    for (int i = GetNumChildrenComponents() - 1; i >= 0; i--)
    {
        USceneComponent* childPtr = GetChildComponent(i);
        if (childPtr != nullptr)
        {
            childPtr->DestroyComponent();
        }
    }
}

void UsfLockComponent::SetMaterial(UMaterialInterface* materialPtr)
{
    if (materialPtr != nullptr)
    {
        m_materialPtr = materialPtr;
    }
    for (USceneComponent* childPtr : GetAttachChildren())
    {
        UMeshComponent* meshPtr = Cast<UMeshComponent>(childPtr);
        if (meshPtr != nullptr)
        {
            for (int i = 0; i < meshPtr->GetNumMaterials(); i++)
            {
                meshPtr->SetMaterial(i, m_materialPtr);
            }
        }
    }
}

void UsfLockComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    AActor* actorPtr = GetOwner();
    if (GetNumChildrenComponents() > 0 && !bDestroyingHierarchy)
    {
        for (int i = GetNumChildrenComponents() - 1; i >= 0; i--)
        {
            USceneComponent* childPtr = GetChildComponent(i);
            if (childPtr != nullptr)
            {
                childPtr->DestroyComponent();
            }
        }
    }

    ALandscapeProxy* landscapePtr = Cast<ALandscapeProxy>(actorPtr);
    if (landscapePtr != nullptr)
    {
        // Remove the lock shader from landscape components
        for (ULandscapeComponent* componentPtr : landscapePtr->LandscapeComponents)
        {
            componentPtr->EditToolRenderData.ToolMaterial = nullptr;
            componentPtr->EditToolRenderData.GizmoMaterial = nullptr;
            componentPtr->UpdateEditToolRenderData();
        }
        // Remove the tick function that keeps the lock shader on landscape components
        FTicker::GetCoreTicker().RemoveTicker(m_tickerHandle);
    }

    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

// When a component is destroyed, the children are attached to the root. When a lock component's parent changes, we
// assume the parent was destroyed and we destroy the lock component as well, unless this is the only lock component
// on the actor, in which case we destroy the children of this lock component.
void UsfLockComponent::OnAttachmentChanged()
{
    if (m_initialized && bRegistered)
    {
        AActor* actorPtr = GetOwner();
        if (actorPtr != nullptr && actorPtr->GetRootComponent() != nullptr)
        {
            TArray<UsfLockComponent*> locks;
            actorPtr->GetComponents(locks);
            if (locks.Num() == 1)
            {
                DestroyChildren();
                return;
            }
        }
        // We want to destroy this component and its child, but if we do it now we'll get a null reference in Unreal's
        // code that runs after this function, so we wait a tick.
        FTicker::GetCoreTicker().RemoveTicker(m_tickerHandle);
        m_tickerHandle = FTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([this, actorPtr](float deltaTime)
        {
            DestroyComponent();
            SceneFusion::RedrawActiveViewport();
            return false;
        }), 1.0f / 60.0f); 
    }
}

void UsfLockComponent::PostEditImport()
{
    // This is called twice when the object is duplicated, so we check if it was already called
    if (m_copied)
    {
        return;
    }
    m_copied = true;
    // We want to destroy this component and its child, but we have to wait a tick for the child to be created
    FTicker::GetCoreTicker().RemoveTicker(m_tickerHandle);
    m_tickerHandle = FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this](float deltaTime)
    {
        DestroyComponent();
        return false;
    }), 1.0f / 60.0f);
}

void UsfLockComponent::OnUPropertyChange(UObject* uobjPtr, FPropertyChangedEvent& ev)
{
    if (uobjPtr != GetAttachParent() || ev.MemberProperty == nullptr)
    {
        return;
    }
    if (ev.MemberProperty->GetName().Contains("mesh"))
    {
        // Destroy child mesh and create a new copy of the parent mesh
        for (int i = GetNumChildrenComponents() - 1; i >= 0; i--)
        {
            USceneComponent* childPtr = GetChildComponent(i);
            if (childPtr != nullptr && childPtr->GetClass() == uobjPtr->GetClass())
            {
                if (ev.MemberProperty->Identical_InContainer(childPtr, uobjPtr))
                {
                    // The mesh is the same as the lock mesh. Do nothing.
                    return;
                }
                childPtr->DestroyComponent();
            }
        }
        DuplicateParentMesh(m_materialPtr);
    }
    else if (ev.MemberProperty->GetName() == "PerInstanceSMData" && GetNumChildrenComponents() == 1)
    {
        UInstancedStaticMeshComponent* instancedMeshPtr = Cast<UInstancedStaticMeshComponent>(GetChildComponent(0));
        ev.MemberProperty->CopyCompleteValue_InContainer(instancedMeshPtr, uobjPtr);
        if (instancedMeshPtr != nullptr)
        {
            instancedMeshPtr->InstanceUpdateCmdBuffer.NumEdits++;// Refreshes the render data
        }
    }
    SceneFusion::RedrawActiveViewport();
}

UStaticMesh* UsfLockComponent::CreateMeshFromBrush(UObject* outerPtr, FName name, ABrush* brushPtr)
{
    GIsSlowTask = true;
    UStaticMesh* meshPtr = NewObject<UStaticMesh>(outerPtr, name, RF_Standalone | RF_Transient);   
    int index = sfUnrealUtils::GetSourceModels(meshPtr).Emplace();

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 22
    FMeshDescription* meshDescPtr = meshPtr->CreateMeshDescription(index);
    GetBrushMesh(brushPtr, brushPtr->Brush, *meshDescPtr, meshPtr->StaticMaterials);
    meshPtr->CommitMeshDescription(index);
    meshPtr->ImportVersion = EImportStaticMeshVersion::LastVersion;
    meshPtr->Build();
    meshPtr->MarkPackageDirty();
#else
    FRawMesh mesh;
    GetBrushMesh(brushPtr, brushPtr->Brush, mesh, meshPtr->StaticMaterials);
    FStaticMeshSourceModel& model = meshPtr->SourceModels[index];
    model.RawMeshBulkData->SaveRawMesh(mesh);

    model.BuildSettings.bRecomputeNormals = true;
    model.BuildSettings.bRecomputeTangents = true;
    model.BuildSettings.bUseMikkTSpace = false;
    model.BuildSettings.bGenerateLightmapUVs = true;
    model.BuildSettings.bBuildAdjacencyBuffer = false;
    model.BuildSettings.bBuildReversedIndexBuffer = false;
    model.BuildSettings.bUseFullPrecisionUVs = false;
    model.BuildSettings.bUseHighPrecisionTangentBasis = false;

    meshPtr->SectionInfoMap.Set(0, 0, FMeshSectionInfo(0));

    meshPtr->CreateBodySetup();
    meshPtr->SetLightingGuid();
    meshPtr->PostEditChange();
#endif

    GIsSlowTask = false;
    return meshPtr;
}

void UsfLockComponent::DestroyModelMesh(ABrush* brushPtr)
{
    MeshData data;
    if (m_modelMeshes.RemoveAndCopyValue(brushPtr, data))
    {
        if (!data.MeshPtr->HasAnyFlags(RF_FinishDestroyed))
        {
            // Rename the mesh so its name can be resused
            sfUnrealUtils::Rename(data.MeshPtr, data.MeshPtr->GetName() + " (deleted)");
        }
        data.MeshPtr->ClearFlags(RF_Standalone); // Allow Unreal to garbage collect the mesh
    }
}

void UsfLockComponent::DestroyModelMeshes()
{
    for (TPair<ABrush*, MeshData> pair : m_modelMeshes)
    {
        UStaticMesh* meshPtr = pair.Value.MeshPtr;
        if (!meshPtr->HasAnyFlags(RF_FinishDestroyed))
        {
            // Rename the mesh so its name can be resused
            sfUnrealUtils::Rename(meshPtr, meshPtr->GetName() + " (deleted)");
        }
        meshPtr->ClearFlags(RF_Standalone); // Allow Unreal to garbage collect the mesh
    }
    m_modelMeshes.Empty();
}