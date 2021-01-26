#include "sfBodyActor.h"

#include "Log.h"

#include <Editor.h>

#define HEAD_PATH "/SceneFusion/CameraModels/head"
#define HMD_PATH "/SceneFusion/CameraModels/hmd"
#define BODY_PATH "/SceneFusion/CameraModels/body"

#define HEAD_BODY_DISTANCE 45.0f
#define MAX_ANGLE PI * 0.67f

#define LOG_CHANNEL "sfBodyActor"

AsfAvatarActor* AsfBodyActor::Create(
    const FVector& location,
    const FRotator& rotation,
    UStaticMesh* headMeshPtr,
    UStaticMesh* hmdMeshPtr,
    UStaticMesh* bodyMeshPtr,
    UMaterialInstanceDynamic* materialPtr)
{
    UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
    FActorSpawnParameters spawnInfo;
    spawnInfo.ObjectFlags = EObjectFlags::RF_Transient;
    spawnInfo.OverrideLevel = worldPtr->PersistentLevel;
    AsfBodyActor* avatarPtr = worldPtr->SpawnActor<AsfBodyActor>(
        location,
        rotation,
        spawnInfo);
    if (avatarPtr)
    {
        avatarPtr->Initialize(headMeshPtr, hmdMeshPtr, bodyMeshPtr, materialPtr);
    }
    else
    {
        KS::Log::Warning("Failed to create actor for XR controller.", LOG_CHANNEL);
    }
    return avatarPtr;
}

void AsfBodyActor::Initialize(
    UStaticMesh* headMeshPtr,
    UStaticMesh* hmdMeshPtr,
    UStaticMesh* bodyMeshPtr,
    UMaterialInstanceDynamic* materialPtr)
{
    UStaticMeshComponent* headComponentPtr = GetStaticMeshComponent();
    headComponentPtr->SetStaticMesh(headMeshPtr);
    headComponentPtr->SetMaterial(0, materialPtr);
    headComponentPtr->CastShadow = false;
    // prevent blocking foliage painting
    headComponentPtr->BodyInstance.SetResponseToChannel(ECC_WorldStatic, ECR_Ignore);

    UStaticMeshComponent* hmdComponentPtr = AddStaticMeshComponent(TEXT("sfHMD"));
    hmdComponentPtr->SetStaticMesh(hmdMeshPtr);
    UMaterialInstanceDynamic* hmdMaterialPtr = UMaterialInstanceDynamic::Create(materialPtr->Parent, nullptr);
    hmdMaterialPtr->SetVectorParameterValue("Color", FLinearColor::Black);
    hmdComponentPtr->SetMaterial(0, hmdMaterialPtr);

    m_bodyComponentPtr = AddStaticMeshComponent(TEXT("sfBody"));
    m_bodyComponentPtr->SetStaticMesh(bodyMeshPtr);
    m_bodyComponentPtr->SetMaterial(0, materialPtr);
    m_bodyComponentPtr->SetRelativeLocation(FVector(0.0f, 0.0f, HEAD_BODY_DISTANCE));

    SetMobility(EComponentMobility::Movable);
}

UStaticMeshComponent* AsfBodyActor::AddStaticMeshComponent(const FName& name)
{
    UStaticMeshComponent* staticMeshComponentPtr = NewObject<UStaticMeshComponent>(this, name);
    staticMeshComponentPtr->CreationMethod = EComponentCreationMethod::Instance;
    staticMeshComponentPtr->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
    staticMeshComponentPtr->RegisterComponent();
    staticMeshComponentPtr->InitializeComponent();
    staticMeshComponentPtr->CastShadow = false;
    // prevent blocking foliage painting
    staticMeshComponentPtr->BodyInstance.SetResponseToChannel(ECC_WorldStatic, ECR_Ignore);
    return staticMeshComponentPtr;
}

void AsfBodyActor::SetRotation(const FQuat& newRotation)
{
    AsfAvatarActor::SetRotation(newRotation);
    UpdateBodyRotation();
    UpdateBodyLocation();
}

void AsfBodyActor::SetScale(const FVector& newScale)
{
    AsfAvatarActor::SetScale(newScale);
    UpdateBodyLocation();
}

//Rotate body so that the angle to the up vector is half of the head does but does not pass 90 degrees.
void AsfBodyActor::UpdateBodyRotation()
{
    FVector bodyForward = GetActorForwardVector();
    bodyForward.Z = 0.0f;
    FVector up = GetActorUpVector();
    float angle = FMath::Acos(FVector::DotProduct(up, FVector::UpVector));
    if (angle > PI * 0.5f)
    {
        bodyForward = -bodyForward;
    }
    FRotator rotator = GetActorRotation();
    rotator.Yaw = 0.0f;
    rotator.Roll = 0.0f;
    if (FMath::Acos(FVector::DotProduct(bodyForward, FVector::UpVector)) < PI * 0.5f)
    {
        angle = 0.0f;
    }
    if (angle < 0.0f)
    {
        rotator.Pitch = -rotator.Pitch;
    }
    else if (angle > MAX_ANGLE)
    {
        rotator.Pitch = 90.0f - rotator.Pitch;
    }
    else
    {
        rotator.Pitch = -rotator.Pitch * 0.5f;
    }
    m_bodyComponentPtr->SetRelativeRotation(rotator);
}

void AsfBodyActor::UpdateBodyLocation()
{
    FVector scale = GetActorScale3D();
    FVector neckPosition = GetActorLocation() - GetActorUpVector() * HEAD_BODY_DISTANCE * scale.X;
    m_bodyComponentPtr->SetWorldLocation(neckPosition - m_bodyComponentPtr->GetUpVector() * 0.5f * scale.X);
}

#undef LOG_CHANNEL
#undef HEAD_PATH
#undef HMD_PATH
#undef BODY_PATH
#undef HEAD_BODY_DISTANCE
#undef MAX_ANGLE
