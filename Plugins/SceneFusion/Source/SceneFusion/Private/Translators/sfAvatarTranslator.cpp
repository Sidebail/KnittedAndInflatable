#include "sfAvatarTranslator.h"
#include "../../Public/sfPropertyUtil.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/Consts.h"
#include "../Actors/sfBodyActor.h"
#include "../Components/sfFlashlightComponent.h"

#include <Editor.h>
#include <LevelEditorViewport.h>
#include <Materials/MaterialInstanceDynamic.h>
#include <Math/Color.h>
#include <Runtime/HeadMountedDisplay/Public/IXRTrackingSystem.h>
#include <Editor/VREditor/Public/IVREditorModule.h>
#include <Editor/VREditor/Public/VREditorMode.h>

#include <sfPropertyUtils.h>

#define LOG_CHANNEL "sfAvatarTranslator"

#define SF_AVATAR_DISAPPEAR_DISTANCE 200.0f

#define MATERIAL_PATH "/SceneFusion/CameraModels/Material"
#define CAMERA_PATH "/SceneFusion/CameraModels/camera"
#define HEAD_PATH "/SceneFusion/CameraModels/head"
#define HMD_PATH "/SceneFusion/CameraModels/hmd"
#define BODY_PATH "/SceneFusion/CameraModels/body"
#define OCULUS_LEFT_PATH "/SceneFusion/CameraModels/oculus_l"
#define OCULUS_RIGHT_PATH "/SceneFusion/CameraModels/oculus_r"
#define VIVE_PATH "/SceneFusion/CameraModels/vive_low_poly"

#define OCULUS_DEVICE_TYPE "OculusHMD"
#define STEAMVR_DEVICE_TYPE "SteamVR"

#define RIGHT_HAND_INDEX 1

#define INTERPOLATING_FRAME_NUM 35

sfAvatarTranslator::sfAvatarTranslator() :
    m_leftId{ -1 },
    m_rightId{ -1 },
    m_cameraObjPtr{ nullptr },
    m_leftObjPtr{ nullptr },
    m_rightObjPtr{ nullptr },
    m_materialPtr{ nullptr },
    m_isInXRMode{ false },
    m_worldScaleFactor{ 1.0f },
    m_flashlightOn{ false },
    m_followingCameraPtr{ nullptr },
    m_interpolatingFrame{ -1 },
    m_showAvatar { true }
{
    LoadMaterialAndMeshes();
    RegisterPropertyChangeHandlers();
}

sfAvatarTranslator::~sfAvatarTranslator()
{

}

void sfAvatarTranslator::Initialize()
{
    m_sessionPtr = SceneFusion::Service->Session();
    m_userJoinEventPtr = m_sessionPtr->RegisterOnUserJoinHandler([this](sfUser::SPtr userPtr)
    {
        //Call OnUserColorChange when a user joined.
        OnUserColorChange(userPtr);
    });
    m_userLeaveEventPtr = m_sessionPtr->RegisterOnUserLeaveHandler([this](sfUser::SPtr userPtr)
    {
        OnUserLeave(userPtr);
    });
    m_colorChangeEventPtr = m_sessionPtr->RegisterOnUserColorChangeHandler([this](sfUser::SPtr userPtr)
    {
        OnUserColorChange(userPtr);
    });

    m_tickHandle = SceneFusion::Get().OnTick.AddRaw(this, &sfAvatarTranslator::Tick);
    m_onCameraMovedHandle = FEditorDelegates::OnEditorCameraMoved.AddRaw(this, &sfAvatarTranslator::OnCameraMoved);

    //create camera object
    m_isInXRMode = InXRMode();
    FVector location;
    FQuat rotation;
    GetCameraLocationAndRotation(location, rotation);
    sfDictionaryProperty::SPtr cameraPropertiesPtr = CreateAvatarProperty(
        (int)(m_isInXRMode ? HEAD : CAMERA),
        location,
        rotation);
    cameraPropertiesPtr->Set(sfProp::Id, sfValueProperty::Create(m_sessionPtr->LocalUserId()));
    m_cameraObjPtr = sfObject::Create(sfType::Avatar, cameraPropertiesPtr, sfObject::ObjectFlags::Transient);
    if (m_isInXRMode)
    {
        UVREditorMode* vrEditorModePtr = IVREditorModule::Get().GetVRMode();
        //World scale
        m_worldScaleFactor = vrEditorModePtr->GetWorldScaleFactor();
        cameraPropertiesPtr->Set(sfProp::Scale, sfValueProperty::Create(m_worldScaleFactor));

        //Flashlight
        m_flashlightOn = vrEditorModePtr->IsFlashlightOn();
        cameraPropertiesPtr->Set(sfProp::Flashlight, sfValueProperty::Create(m_flashlightOn));

        //Create controllerActorPtr sfObjects
        TArray<int> ids;
        GEngine->XRSystem->EnumerateTrackedDevices(ids, EXRTrackedDeviceType::Controller);
        FString deviceType = GEngine->XRSystem->GetSystemName().ToString();
        //When there is only one controllerActorPtr, we don't know if it is left or right.
        //If we found two controllers, then the one with smaller id is the left hand.
        //So we only sync them if we found both.
        if (ids.Num() == 2)
        {
            m_leftId = ids[0];
            m_rightId = ids[1];
            //Left
            GEngine->XRSystem->GetCurrentPose(m_leftId, rotation, location);
            sfDictionaryProperty::SPtr leftPropertiesPtr = CreateAvatarProperty(
                (int)(deviceType == OCULUS_DEVICE_TYPE ? OCULUS_LEFT : VIVE),
                location,
                rotation);
            m_leftObjPtr = sfObject::Create(sfType::Avatar, leftPropertiesPtr);
            m_cameraObjPtr->AddChild(m_leftObjPtr);

            //Right
            GEngine->XRSystem->GetCurrentPose(m_rightId, rotation, location);
            sfDictionaryProperty::SPtr rightPropertiesPtr = CreateAvatarProperty(
                (int)(deviceType == OCULUS_DEVICE_TYPE ? OCULUS_LEFT : VIVE),
                location,
                rotation);
            m_rightObjPtr = sfObject::Create(sfType::Avatar, rightPropertiesPtr);
            m_cameraObjPtr->AddChild(m_rightObjPtr);
        }
    }
    m_sessionPtr->Create(m_cameraObjPtr);
}

sfDictionaryProperty::SPtr sfAvatarTranslator::CreateAvatarProperty(
    int meshId,
    const FVector& location,
    const FQuat& rotation)
{
    sfDictionaryProperty::SPtr propertiesPtr = sfDictionaryProperty::Create();
    propertiesPtr->Set(sfProp::Mesh, sfValueProperty::Create(meshId));
    propertiesPtr->Set(sfProp::Location, sfPropertyUtil::FromVector(location));
    propertiesPtr->Set(sfProp::Rotation, sfPropertyUtil::FromQuat(rotation));
    return propertiesPtr;
}

void sfAvatarTranslator::CleanUp()
{
    m_sessionPtr->UnregisterOnUserJoinHandler(m_userJoinEventPtr);
    m_sessionPtr->UnregisterOnUserLeaveHandler(m_userLeaveEventPtr);
    m_sessionPtr->UnregisterOnUserColorChangeHandler(m_colorChangeEventPtr);
    SceneFusion::Get().OnTick.Remove(m_tickHandle);
    FEditorDelegates::OnEditorCameraMoved.Remove(m_onCameraMovedHandle);
    DestroyAvatars();
}

void sfAvatarTranslator::DestroyAvatars()
{
    for (auto pair : m_sfObjToActor)
    {
        AsfAvatarActor* actorPtr = pair.Value;
        if (IsActorValid(actorPtr))
        {
            GEditor->GetEditorWorldContext().World()->EditorDestroyActor(pair.Value, false);
        }
    }
    m_sfObjToActor.Empty();
    m_userIdToCamera.Empty();
    for (auto pair : m_userIdToMaterial)
    {
        pair.Value->ClearFlags(EObjectFlags::RF_Standalone);// Allow unreal to destroy the material instance
    }
    m_userIdToMaterial.Empty();
}

void sfAvatarTranslator::RegisterPropertyChangeHandlers()
{
    m_propertyChangeHandlers[sfProp::Mesh] = [this](sfProperty::SPtr propertyPtr)
    {
        sfObject::SPtr objPtr = propertyPtr->GetContainerObject();
        AsfAvatarActor* actorPtr = m_sfObjToActor.FindRef(objPtr->Id());
        if (!actorPtr)
        {
            return;
        }
        int newMeshId = KS::SceneFusion2::ToInt(propertyPtr);
        bool setFollowingCameraActor = (actorPtr == m_followingCameraPtr) &&
            (newMeshId == CAMERA || newMeshId == HEAD);
        switch (newMeshId)
        {
            case CAMERA:
            {
                GEditor->GetEditorWorldContext().World()->EditorDestroyActor(actorPtr, false);
                uint32_t userId = GetOwnerId(objPtr);
                actorPtr = AsfAvatarActor::Create(
                    actorPtr->GetActorLocation(),
                    actorPtr->GetActorRotation(),
                    m_meshPtrs[CAMERA],
                    m_userIdToMaterial[userId]);
                m_sfObjToActor[objPtr->Id()] = actorPtr;
                m_userIdToCamera[userId] = actorPtr;
                break;
            }
            case HEAD:
            {
                GEditor->GetEditorWorldContext().World()->EditorDestroyActor(actorPtr, false);
                uint32_t userId = GetOwnerId(objPtr);
                actorPtr = AsfBodyActor::Create(
                    actorPtr->GetActorLocation(),
                    actorPtr->GetActorRotation(),
                    m_meshPtrs[HEAD],
                    m_meshPtrs[HMD],
                    m_meshPtrs[BODY],
                    m_userIdToMaterial[userId]);
                m_sfObjToActor[objPtr->Id()] = actorPtr;
                m_userIdToCamera[userId] = actorPtr;
                break;
            }
            default:
            {
                UStaticMeshComponent* staticMeshComponentPtr = actorPtr->GetStaticMeshComponent();
                if (staticMeshComponentPtr)
                {
                    staticMeshComponentPtr->SetStaticMesh(m_meshPtrs[newMeshId]);
                }
                break;
            }
        }
        if (setFollowingCameraActor)
        {
            m_followingCameraPtr = actorPtr;
        }
    };

    m_propertyChangeHandlers[sfProp::Location] = [this](sfProperty::SPtr propertyPtr)
    {
        AsfAvatarActor* actorPtr = m_sfObjToActor.FindRef(propertyPtr->GetContainerObject()->Id());
        if (IsActorValid(actorPtr))
        {
            actorPtr->SetActorLocation(sfPropertyUtil::ToVector(propertyPtr));
            if (m_followingCameraPtr == actorPtr)
            {
                StartFollowing();
            }
        }
    };

    m_propertyChangeHandlers[sfProp::Rotation] = [this](sfProperty::SPtr propertyPtr)
    {
        AsfAvatarActor* actorPtr = m_sfObjToActor.FindRef(propertyPtr->GetContainerObject()->Id());
        if (IsActorValid(actorPtr))
        {
            actorPtr->SetRotation(sfPropertyUtil::ToQuat(propertyPtr));
            if (m_followingCameraPtr == actorPtr)
            {
                StartFollowing();
            }
        }
    };

    m_propertyChangeHandlers[sfProp::Scale] = [this](sfProperty::SPtr propertyPtr)
    {
        sfObject::SPtr objPtr = propertyPtr->GetContainerObject();
        float worldScaleFactor = KS::SceneFusion2::ToFloat(propertyPtr);
        AsfAvatarActor* actorPtr = m_sfObjToActor.FindRef(objPtr->Id());
        if (IsActorValid(actorPtr))
        {
            actorPtr->SetScale(worldScaleFactor * FVector::OneVector);
        }
        for (sfObject::SPtr controllerObjPtr : objPtr->Children())
        {
            AsfAvatarActor* controllerActorPtr = m_sfObjToActor.FindRef(controllerObjPtr->Id());
            if (IsActorValid(controllerActorPtr))
            {
                controllerActorPtr->SetScale(worldScaleFactor * FVector::OneVector);
            }
        }
    };

    m_propertyChangeHandlers[sfProp::Flashlight] = [this](sfProperty::SPtr propertyPtr)
    {
        sfObject::SPtr rightHandPtr = propertyPtr->GetContainerObject()->Child(RIGHT_HAND_INDEX);
        if (rightHandPtr)
        {
            ToggleFlashlightOnController(m_sfObjToActor[rightHandPtr->Id()], KS::SceneFusion2::ToBool(propertyPtr));
        }
    };
}

void sfAvatarTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    objPtr->ForSelfAndDescendants([this](sfObject::SPtr currentObjectPtr)
    {
        //create avatar actor
        AsfAvatarActor* actorPtr = nullptr;
        uint32_t userId = GetOwnerId(currentObjectPtr);
        sfDictionaryProperty::SPtr propertiesPtr = currentObjectPtr->Property()->AsDict();
        int meshId = KS::SceneFusion2::ToInt(propertiesPtr->Get(sfProp::Mesh));
        if (meshId == HEAD)
        {
            actorPtr = AsfBodyActor::Create(
                sfPropertyUtil::ToVector(propertiesPtr->Get(sfProp::Location)),
                sfPropertyUtil::ToQuat(propertiesPtr->Get(sfProp::Rotation)).Rotator(),
                m_meshPtrs[HEAD],
                m_meshPtrs[HMD],
                m_meshPtrs[BODY],
                m_userIdToMaterial[GetOwnerId(currentObjectPtr)]);
        }
        else
        {
            actorPtr = AsfAvatarActor::Create(
                sfPropertyUtil::ToVector(propertiesPtr->Get(sfProp::Location)),
                sfPropertyUtil::ToQuat(propertiesPtr->Get(sfProp::Rotation)).Rotator(),
                m_meshPtrs[meshId],
                m_userIdToMaterial[userId]);
        }
        if (actorPtr)
        {
            //Flashlight
            sfObject::SPtr parentPtr = currentObjectPtr->Parent();
            if (parentPtr && parentPtr->IndexOfChild(currentObjectPtr) == RIGHT_HAND_INDEX)
            {
                sfProperty::SPtr flashLightPropPtr = nullptr;
                sfDictionaryProperty::SPtr cameraPropertiesPtr = currentObjectPtr->Parent()->Property()->AsDict();
                if (cameraPropertiesPtr->TryGet(sfProp::Flashlight, flashLightPropPtr))
                {
                    ToggleFlashlightOnController(actorPtr, KS::SceneFusion2::ToBool(flashLightPropPtr));
                }
            }

            m_sfObjToActor.Add(currentObjectPtr->Id(), actorPtr);
            if (meshId == HEAD || meshId == CAMERA)
            {
                m_userIdToCamera.Add(userId, actorPtr);
            }
            SceneFusion::RedrawActiveViewport();
        }
        return true;
    });
}

void sfAvatarTranslator::OnDelete(sfObject::SPtr objPtr)
{
    objPtr->ForSelfAndDescendants([this](sfObject::SPtr currentObjectPtr)
    {
        AsfAvatarActor* actorPtr = m_sfObjToActor.FindRef(currentObjectPtr->Id());
        if (IsActorValid(actorPtr))
        {
            GEditor->GetEditorWorldContext().World()->EditorDestroyActor(actorPtr, false);
        }
        m_sfObjToActor.Remove(currentObjectPtr->Id());
        return true;
    });
}

void sfAvatarTranslator::OnPropertyChange(sfProperty::SPtr propertyPtr)
{
    //update camera actor
    if (SceneFusion::Service->Session() == nullptr)
    {
        return;
    }

    if (propertyPtr->Type() == sfProperty::VALUE)
    {
        sfValueProperty::SPtr propPtr = propertyPtr->AsValue();
        auto iter = m_propertyChangeHandlers.find(propPtr->Key());
        if (iter == m_propertyChangeHandlers.end())
        {
            KS::Log::Warning("No property change handler for " + *propPtr->Key(), LOG_CHANNEL);
            return;
        }
        iter->second(propertyPtr);
        SceneFusion::RedrawActiveViewport();
    }
}

void sfAvatarTranslator::OnUserLeave(sfUser::SPtr& userPtr)
{
    UMaterialInstanceDynamic* materialPtr;
    if (m_userIdToMaterial.RemoveAndCopyValue(userPtr->Id(), materialPtr))
    {
        materialPtr->ClearFlags(EObjectFlags::RF_Standalone);// Allow unreal to destroy the material instance
    }
    m_userIdToCamera.Remove(userPtr->Id());
}

void sfAvatarTranslator::OnUserColorChange(sfUser::SPtr userPtr)
{
    UMaterialInstanceDynamic* materialPtr = m_userIdToMaterial.FindRef(userPtr->Id());
    if (!materialPtr)
    {
        materialPtr = UMaterialInstanceDynamic::Create(m_materialPtr, nullptr);
        materialPtr->SetFlags(EObjectFlags::RF_Standalone);//prevent material from being destroyed
        m_userIdToMaterial.Add(userPtr->Id(), materialPtr);
    }
    if (materialPtr)
    {
        ksColor color = userPtr->Color();
        FLinearColor ucolor(color.R(), color.G(), color.B());
        materialPtr->SetVectorParameterValue("Color", ucolor);
        SceneFusion::RedrawActiveViewport();
    }
}

void sfAvatarTranslator::OnCameraMoved(const FVector& location,
    const FRotator& rotation,
    ELevelViewportType viewportType,
    int viewIndex)
{
    Follow(0);//Stop following.
    OnUnfollow.ExecuteIfBound();
}

void sfAvatarTranslator::LoadMaterialAndMeshes()
{
    // Disable loading dialog that causes a crash if we are dragging objects
    GIsSlowTask = true;
    m_materialPtr = LoadObject<UMaterialInterface>(nullptr, TEXT(MATERIAL_PATH));
    m_meshPtrs.Add(LoadObject<UStaticMesh>(nullptr, TEXT(CAMERA_PATH)));
    m_meshPtrs.Add(LoadObject<UStaticMesh>(nullptr, TEXT(HEAD_PATH)));
    m_meshPtrs.Add(LoadObject<UStaticMesh>(nullptr, TEXT(HMD_PATH)));
    m_meshPtrs.Add(LoadObject<UStaticMesh>(nullptr, TEXT(BODY_PATH)));
    m_meshPtrs.Add(LoadObject<UStaticMesh>(nullptr, TEXT(OCULUS_LEFT_PATH)));
    m_meshPtrs.Add(LoadObject<UStaticMesh>(nullptr, TEXT(OCULUS_RIGHT_PATH)));
    m_meshPtrs.Add(LoadObject<UStaticMesh>(nullptr, TEXT(VIVE_PATH)));
    GIsSlowTask = false;
}

uint32_t sfAvatarTranslator::GetOwnerId(sfObject::SPtr objPtr)
{
    while (objPtr->Parent())
    {
        objPtr = objPtr->Parent();
    }
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    return KS::SceneFusion2::ToUInt(propertiesPtr->Get(sfProp::Id));
}

void sfAvatarTranslator::Tick(float deltaTime)
{
    HideUserAvatar();
    SendChange();
    MoveViewportTowardsFollowedCamera();
}

bool sfAvatarTranslator::InXRMode()
{
    return GEngine->XRSystem.IsValid() && IVREditorModule::Get().IsVREditorModeActive() &&
        IVREditorModule::Get().GetVRMode() != nullptr;
}

void sfAvatarTranslator::HideUserAvatar()
{
    if (!GCurrentLevelEditingViewportClient)
    {
        return;
    }
    FVector location = GCurrentLevelEditingViewportClient->GetViewLocation();
    for (auto iter = m_sfObjToActor.CreateConstIterator(); iter; ++iter)
    {
        if (!IsActorValid(iter.Value()))
        {
            continue;
        }
        bool hideCamera = !m_showAvatar || (m_followingCameraPtr == iter.Value());
        hideCamera |= FVector::Distance(iter.Value()->GetActorLocation(), location) < SF_AVATAR_DISAPPEAR_DISTANCE;
        iter.Value()->SetIsTemporarilyHiddenInEditor(hideCamera);
    }
}

void sfAvatarTranslator::SendChange()
{
    //Handle XR mode change
    sfDictionaryProperty::SPtr cameraPropertiesPtr = m_cameraObjPtr->Property()->AsDict();
    if (m_isInXRMode != InXRMode())
    {
        m_isInXRMode = !m_isInXRMode;
        cameraPropertiesPtr->Set(sfProp::Mesh, sfValueProperty::Create((int)(m_isInXRMode ? HEAD : CAMERA)));
        if (!m_isInXRMode)
        {
            if (m_leftObjPtr)
            {
                m_sessionPtr->Delete(m_leftObjPtr);
                m_leftObjPtr = nullptr;
            }

            if (m_rightObjPtr)
            {
                m_sessionPtr->Delete(m_rightObjPtr);
                m_rightObjPtr = nullptr;
            }
        }
    }

    //Send camera location and rotation to server
    FVector location;
    FQuat rotation;
    if (GetCameraLocationAndRotation(location, rotation))
    {
        SendTransform(cameraPropertiesPtr, location, rotation);
    }

    //Send controllerActorPtr location and rotation to server
    SendControllerTransformToServer();

    if (m_isInXRMode)
    {
        UVREditorMode* vrEditorModePtr = IVREditorModule::Get().GetVRMode();

        //Send world scale
        if (m_worldScaleFactor != vrEditorModePtr->GetWorldScaleFactor())
        {
            m_worldScaleFactor = vrEditorModePtr->GetWorldScaleFactor();
            cameraPropertiesPtr->Set(sfProp::Scale, sfValueProperty::Create(m_worldScaleFactor));
        }

        //Send flashlight state
        if (m_flashlightOn != vrEditorModePtr->IsFlashlightOn())
        {
            m_flashlightOn = !m_flashlightOn;
            cameraPropertiesPtr->Set(sfProp::Flashlight, sfValueProperty::Create(m_flashlightOn));
        }
    }
}

bool sfAvatarTranslator::GetCameraLocationAndRotation(FVector& location, FQuat& rotation)
{
    if (InXRMode())
    {
        GetXRDeviceLocationAndRotation(GEngine->XRSystem->HMDDeviceId, location, rotation);
        return true;
    }

    if (GCurrentLevelEditingViewportClient)
    {
        location = GCurrentLevelEditingViewportClient->GetViewLocation();
        rotation = GCurrentLevelEditingViewportClient->GetViewRotation().Quaternion();
        if (GCurrentLevelEditingViewportClient->bUsingOrbitCamera)
        {
            rotation = (GCurrentLevelEditingViewportClient->GetLookAtLocation() - location).ToOrientationQuat();
        }
        return true;
    }

    return false;
}

void sfAvatarTranslator::GetXRDeviceLocationAndRotation(int deviceId, FVector& location, FQuat& rotation)
{
    GEngine->XRSystem->GetCurrentPose(deviceId, rotation, location);
    FTransform trackingToWorldTransform = IVREditorModule::Get().GetVRMode()->GetRoomTransform();
    location = trackingToWorldTransform.TransformPosition(location);
    rotation = trackingToWorldTransform.TransformRotation(rotation);
}

void sfAvatarTranslator::SendControllerTransformToServer()
{
    if (m_isInXRMode)
    {
        bool createControllers = false;
        if (m_leftObjPtr == nullptr || m_rightObjPtr == nullptr)
        {
            TArray<int> ids;
            GEngine->XRSystem->EnumerateTrackedDevices(ids, EXRTrackedDeviceType::Controller);
            //When there is only one controllerActorPtr, we don't know if it is left or right.
            //If we found two controllers, then the one with smaller id is the left hand.
            //So we only sync them if we found both.
            if (ids.Num() == 2)
            {
                m_leftId = ids[0];
                m_rightId = ids[1];
                createControllers = true;
            }
            else
            {
                return;
            }
        }

        sfDictionaryProperty::SPtr leftPropertiesPtr = nullptr;
        FVector leftLocation;
        FQuat leftRotation;
        sfDictionaryProperty::SPtr rightPropertiesPtr = nullptr;
        FVector rightLocation;
        FQuat rightRotation;
        GetXRDeviceLocationAndRotation(m_leftId, leftLocation, leftRotation);
        GetXRDeviceLocationAndRotation(m_rightId, rightLocation, rightRotation);
        if (createControllers)
        {
            FString deviceType = GEngine->XRSystem->GetSystemName().ToString();
            leftPropertiesPtr = CreateAvatarProperty(
                (int)(deviceType == OCULUS_DEVICE_TYPE ? OCULUS_LEFT : VIVE),
                leftLocation,
                leftRotation);
            rightPropertiesPtr = CreateAvatarProperty(
                (int)(deviceType == OCULUS_DEVICE_TYPE ? OCULUS_LEFT : VIVE),
                rightLocation,
                rightRotation);
        }
        else
        {
            leftPropertiesPtr = m_leftObjPtr->Property()->AsDict();
            SendTransform(leftPropertiesPtr, leftLocation, leftRotation);

            rightPropertiesPtr = m_rightObjPtr->Property()->AsDict();
            SendTransform(rightPropertiesPtr, rightLocation, rightRotation);
        }

        if (createControllers)
        {
            m_leftObjPtr = sfObject::Create(sfType::Avatar, leftPropertiesPtr);
            m_sessionPtr->Create(m_leftObjPtr, m_cameraObjPtr, 0);
            m_rightObjPtr = sfObject::Create(sfType::Avatar, rightPropertiesPtr);
            m_sessionPtr->Create(m_rightObjPtr, m_cameraObjPtr, 1);
        }
    }
}

void sfAvatarTranslator::SendTransform(
    sfDictionaryProperty::SPtr propertiesPtr,
    const FVector& location,
    const FQuat& rotation)
{

    if (sfPropertyUtil::ToVector(propertiesPtr->Get(sfProp::Location)) != location)
    {
        propertiesPtr->Set(sfProp::Location, sfPropertyUtil::FromVector(location));
    }

    if (sfPropertyUtil::ToQuat(propertiesPtr->Get(sfProp::Rotation)) != rotation)
    {
        propertiesPtr->Set(sfProp::Rotation, sfPropertyUtil::FromQuat(rotation));
    }
}

void sfAvatarTranslator::ToggleFlashlightOnController(AsfAvatarActor* controllerActorPtr, bool flashlightOn)
{
    if (!controllerActorPtr)
    {
        return;
    }
    UActorComponent* componentPtr = controllerActorPtr->GetComponentByClass(USpotLightComponent::StaticClass());
    USpotLightComponent* flashLightPtr = nullptr;
    if (componentPtr)
    {
        flashLightPtr = Cast<USpotLightComponent>(componentPtr);
    }
    if (!flashLightPtr)
    {
        if (flashlightOn)
        {
            //Create an UsfFlashlightComponent as the flashlight and use the same settings as Unreal does.
            flashLightPtr = NewObject<UsfFlashlightComponent>(
                controllerActorPtr,
                "Flashlight",
                EObjectFlags::RF_Transient);
            controllerActorPtr->AddOwnedComponent(flashLightPtr);
            flashLightPtr->RegisterComponent();
            flashLightPtr->SetMobility(EComponentMobility::Movable);
            flashLightPtr->SetCastShadows(false);
            flashLightPtr->bUseInverseSquaredFalloff = false;
            flashLightPtr->SetLightFalloffExponent(8.0f);
            flashLightPtr->SetIntensity(20.0f);
            flashLightPtr->SetOuterConeAngle(25.0f);
            flashLightPtr->SetInnerConeAngle(25.0f);

            const FAttachmentTransformRules AttachmentTransformRules
                = FAttachmentTransformRules(EAttachmentRule::KeepRelative, true);
            flashLightPtr->AttachToComponent(controllerActorPtr->GetRootComponent(), AttachmentTransformRules);
        }
        else
        {
            return;
        }
    }

    flashLightPtr->SetVisibility(flashlightOn);
}

bool sfAvatarTranslator::IsActorValid(AsfAvatarActor* actorPtr)
{
    return actorPtr && actorPtr->IsValidLowLevel() && !actorPtr->IsPendingKill();
}

void sfAvatarTranslator::MoveViewportToUser(uint32_t userId)
{
    AsfAvatarActor* cameraActorPtr = m_userIdToCamera.FindRef(userId);
    if (cameraActorPtr && GCurrentLevelEditingViewportClient)
    {
        FVector location = cameraActorPtr->GetActorLocation();
        FRotator rotation = cameraActorPtr->GetActorRotation();
        GCurrentLevelEditingViewportClient->SetViewLocation(location);
        GCurrentLevelEditingViewportClient->SetViewRotation(rotation);
        SceneFusion::RedrawActiveViewport();
    }
}

uint32_t sfAvatarTranslator::Follow(uint32_t userId)
{
    AsfAvatarActor* cameraActorPtr = m_userIdToCamera.FindRef(userId);
    if (cameraActorPtr != nullptr && cameraActorPtr != m_followingCameraPtr)
    {
        m_followingCameraPtr = cameraActorPtr;
        StartFollowing();
        return userId;
    }
    else
    {
        MoveViewportToUser(userId);
        m_followingCameraPtr = nullptr;
        m_interpolatingFrame = -1;
        return 0;
    }
}

void sfAvatarTranslator::StartFollowing()
{
    m_interpolatingFrame = INTERPOLATING_FRAME_NUM;
    m_oldCameraLocation = m_followingCameraPtr->GetActorLocation();
    m_oldCameraRotation = m_followingCameraPtr->GetActorQuat();
}

void sfAvatarTranslator::MoveViewportTowardsFollowedCamera()
{
    if (m_followingCameraPtr != nullptr && m_interpolatingFrame > 0)
    {
        m_interpolatingFrame--;
        float t = 1.0f - m_interpolatingFrame / INTERPOLATING_FRAME_NUM;
        FVector targetLocation = m_followingCameraPtr->GetActorLocation();
        FQuat targetRotation = m_followingCameraPtr->GetActorQuat();
        FVector cameraLocation = FMath::Lerp(m_oldCameraLocation, targetLocation, t);
        FQuat cameraRotation = FQuat::Slerp(m_oldCameraRotation, targetRotation, t);
        GCurrentLevelEditingViewportClient->SetViewLocation(cameraLocation);
        GCurrentLevelEditingViewportClient->SetViewRotation(cameraRotation.Rotator());
    }
}

void sfAvatarTranslator::SetAvatarVisibility(bool showAvatar)
{
    m_showAvatar = showAvatar;
}

void sfAvatarTranslator::RecreateAllAvatars()
{
    for (auto iter = m_sfObjToActor.CreateConstIterator(); iter; ++iter)
    {
        OnCreate(m_sessionPtr->GetObject(iter.Key()), 0);
    }
}

#undef LOG_CHANNEL
#undef SF_AVATAR_DISAPPEAR_DISTANCE
#undef MATERIAL_PATH
#undef CAMERA_PATH
#undef HEAD_PATH
#undef HMD_PATH
#undef BODY_PATH
#undef OCULUS_LEFT_PATH
#undef OCULUS_RIGHT_PATH
#undef VIVE_PATH
#undef OCULUS_DEVICE_TYPE
#undef STEAMVR_DEVICE_TYPE
#undef RIGHT_HAND_INDEX
#undef INTERPOLATING_FRAME_NUM