#include "sfAvatarActor.h"
#include <Editor.h>
#include <Materials/MaterialInstanceDynamic.h>

#include "Log.h"

#define LOG_CHANNEL "sfAvatarActor"

AsfAvatarActor* AsfAvatarActor::Create(
    const FVector& location,
    const FRotator& rotation,
    UStaticMesh* meshPtr,
    UMaterialInstanceDynamic* materialPtr)
{
    UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
    FActorSpawnParameters spawnInfo;
    spawnInfo.ObjectFlags = EObjectFlags::RF_Transient;
    spawnInfo.OverrideLevel = worldPtr->PersistentLevel;
    AsfAvatarActor* avatarPtr = worldPtr->SpawnActor<AsfAvatarActor>(
        location,
        rotation,
        spawnInfo);

    if (avatarPtr)
    {
        avatarPtr->SetMobility(EComponentMobility::Movable);
        UStaticMeshComponent* staticMeshComponentPtr = avatarPtr->GetStaticMeshComponent();
        staticMeshComponentPtr->SetStaticMesh(meshPtr);
        staticMeshComponentPtr->SetMaterial(0, materialPtr);
        staticMeshComponentPtr->CastShadow = false;
        // prevent blocking foliage painting
        staticMeshComponentPtr->BodyInstance.SetResponseToChannel(ECC_WorldStatic, ECR_Ignore);
    }
    else
    {
        KS::Log::Warning("Failed to create actor for XR controller.", LOG_CHANNEL);
    }
    return avatarPtr;
}

bool AsfAvatarActor::IsSelectable() const
{ 
    return false; 
}

bool AsfAvatarActor::IsListedInSceneOutliner() const
{
    return false;
}

void AsfAvatarActor::SetLocation(const FVector& newLocation)
{
    AActor::SetActorLocation(newLocation);
}

void AsfAvatarActor::SetRotation(const FQuat& newRotation)
{
    AActor::SetActorRotation(newRotation);
}

void AsfAvatarActor::SetScale(const FVector& newScale)
{
    AActor::SetActorScale3D(newScale);
}

#undef LOG_CHANNEL