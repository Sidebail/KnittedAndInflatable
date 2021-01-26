/*************************************************************************
 *
 * KINEMATICOUP CONFIDENTIAL
 * __________________
 *
 *  Copyright (2017-2020) KinematicSoup Technologies Incorporated
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of KinematicSoup Technologies Incorporated and its
 * suppliers, if any.  The intellectual and technical concepts contained
 * herein are proprietary to KinematicSoup Technologies Incorporated
 * and its suppliers and may be covered by Canadian and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from KinematicSoup Technologies Incorporated.
 */
#pragma once

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>
#include <Materials/MaterialInstanceDynamic.h>
#include <Components/SpotLightComponent.h>
#include <Editor/UnrealEdTypes.h>

#include <unordered_map>

#include <sfObject.h>
#include <sfSession.h>

#include "../../Public/Translators/sfBaseTranslator.h"
#include "../Actors/sfAvatarActor.h"

using namespace KS::SceneFusion2;

/**
 * This class manages avatars for other users and sends local camera and controllers info to server.
 */
class sfAvatarTranslator : public sfBaseTranslator
{
public:
    /**
     * Delegate for stop following camera
     */
    DECLARE_DELEGATE(OnUnfollowDelegate);

    OnUnfollowDelegate OnUnfollow;

    /**
     * Constructor
     */
    sfAvatarTranslator();

    /**
     * Destructor
     */
    virtual ~sfAvatarTranslator();

    /**
     * Initialization. Called after connecting to a session.
     */
    virtual void Initialize() override;

    /**
     * Deinitialization. Called after disconnecting from a session.
     */
    virtual void CleanUp() override;

    /**
     * Called when an object is created by another user.
     *
     * @param   sfObject::SPtr objPtr that was created.
     * @param   int childIndex of new object. -1 if object is a root.
     */
    virtual void OnCreate(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Called when an object is deleted by another user.
     *
     * @param   sfObject::SPtr objPtr that was deleted.
     */
    virtual void OnDelete(sfObject::SPtr objPtr) override;

    /**
     * Called when an object property changes.
     *
     * @param   sfProperty::SPtr propertyPtr that changed.
     */
    virtual void OnPropertyChange(sfProperty::SPtr propertyPtr) override;

    /**
     * Called when a user leaves the session.
     *
     * @param   sfUser::SPtr& userPtr.
     */
    void OnUserLeave(sfUser::SPtr& userPtr);

    /**
     * @return  bool - true if we are in XR mode.
     */
    static bool InXRMode();

    /**
     * Moves viewport camera to the given user's camera location and rotation instantly.
     *
     * @param   uint32_t userId
     */
    void MoveViewportToUser(uint32_t userId);

    /**
     * Follows the given user's camera.
     *
     * @param   uint32_t userId
     * @return  uint32_t id of user it is following. Return 0 if it is not following any user.
     */
    uint32_t Follow(uint32_t userId);

    /**
     * Recreates all avatar actors.
     */
    void RecreateAllAvatars();

    /**
     * Destroys all avatars and their materials.
     */
    void DestroyAvatars();

    /**
     * Sets avatars' visibility.
     *
     * @param   bool showAvatar
     */
    void SetAvatarVisibility(bool showAvatar);

private:
    /**
     * Mesh id.
     */
    enum MeshId
    {
        CAMERA,
        HEAD,
        HMD,
        BODY,
        OCULUS_LEFT,
        OCULUS_RIGHT,
        VIVE
    };

    /**
     * Called when an object property changes.
     *
     * @param   sfProperty::SPtr propertyPtr
     */
    typedef std::function<void(sfProperty::SPtr propertyPtr)> PropertyChangeHandler;

    KS::ksEvent<sfUser::SPtr&>::SPtr m_userJoinEventPtr;
    KS::ksEvent<sfUser::SPtr&>::SPtr m_userLeaveEventPtr;
    KS::ksEvent<sfUser::SPtr&>::SPtr m_colorChangeEventPtr;

    std::unordered_map<sfName, PropertyChangeHandler> m_propertyChangeHandlers;

    int m_leftId;
    int m_rightId;
    sfSession::SPtr m_sessionPtr;
    sfObject::SPtr m_cameraObjPtr;
    sfObject::SPtr m_leftObjPtr;
    sfObject::SPtr m_rightObjPtr;
    UMaterialInterface* m_materialPtr;
    TMap<uint32_t, UMaterialInstanceDynamic*> m_userIdToMaterial;
    TMap<uint32_t, AsfAvatarActor*> m_userIdToCamera;
    TMap<uint32_t, AsfAvatarActor*> m_sfObjToActor;
    TArray<UStaticMesh*> m_meshPtrs;
    bool m_isInXRMode;
    float m_worldScaleFactor;
    bool m_flashlightOn;

    AsfAvatarActor* m_followingCameraPtr;
    int m_interpolatingFrame;
    FVector m_oldCameraLocation;
    FQuat m_oldCameraRotation;

    FDelegateHandle m_onCameraMovedHandle;
    FDelegateHandle m_tickHandle;

    bool m_showAvatar;

    /**
     * Tick function.
     *
     * @param   float deltaTime in seconds since last tick.
     */
    void Tick(float deltaTime);

    /**
     * Loads materials and meshes for avatars.
     */
    void LoadMaterialAndMeshes();

    /**
     * Register property change handlers.
     */
    void RegisterPropertyChangeHandlers();

    /**
     * Called when a user's color changes.
     *
     * @param   sfUser::SPtr userPtr
     */
    void OnUserColorChange(sfUser::SPtr userPtr);

    /**
     * Called when a camera moves.
     *
     * @param   const FVector& location
     * @param   const FRotator& rotation
     * @param   ELevelViewportType viewportType
     * @param   int viewportIndex
     */
    void OnCameraMoved(const FVector& location, 
        const FRotator& rotation, 
        ELevelViewportType viewportType, 
        int viewportIndex);

    /**
     * Creates a dictionary property and adds mesh, location and rotation properties.
     *
     * @param   int meshId
     * @param   const FVector& location
     * @param   const FQuat& rotation
     * @return  sfDictionaryProperty::SPtr
     */
    sfDictionaryProperty::SPtr CreateAvatarProperty(int meshId, const FVector& location, const FQuat& rotation);

    /**
     * Gets camera location and rotation.
     *
     * @param   FVector& location
     * @param   FQuat& rotation
     * @return  bool - true if we got location and rotation successfully
     */
    bool GetCameraLocationAndRotation(FVector& location, FQuat& rotation);

    /**
     * Gets XR device location and rotation.
     *
     * @param   int device id of head set or controllers to get device location and rotation from Unreal.
     * @param   FVector& location
     * @param   FQuat& rotation
     */
    void GetXRDeviceLocationAndRotation(int deviceId, FVector& location, FQuat& rotation);

    /**
     * Gets owner's id of the given object.
     *
     * @param   sfObject::SPtr objPtr
     * @return  uint32_t
     */
    uint32_t GetOwnerId(sfObject::SPtr objPtr);

    /**
     * Hides user cameras if they are too close to the local camera.
     */
    void HideUserAvatar();

    /**
     * Sends camera and controllers info change.
     */
    void SendChange();

    /**
     * Sends XR controllers' transform to server.
     */
    void SendControllerTransformToServer();

    /**
     * Sets location and rotation properties on the given dictionary property.
     *
     * @param   sfDictionaryProperty::SPtr propertiesPtr
     * @param   const FVector& location
     * @param   const FQuat& rotation
     */
    void SendTransform(sfDictionaryProperty::SPtr propertiesPtr, const FVector& location, const FQuat& rotation);

    /**
     * Toggles flashlight on controllerActorPtr.
     *
     * @param   AsfAvatarActor* controllerActorPtr avatar actor
     * @param   bool flashlightOn
     */
    void ToggleFlashlightOnController(AsfAvatarActor* controllerActorPtr, bool flashlightOn);

    /**
     * @return  bool - true if the given actor pointer is valid and the actor is not pending kill.
     */
    bool IsActorValid(AsfAvatarActor* actorPtr);

    /**
     * Starts camera following. Sets interpolation frame number and record old location and rotation.
     */
    void StartFollowing();

    /**
     * Moves viewport towards followed camera.
     */
    void MoveViewportTowardsFollowedCamera();
};