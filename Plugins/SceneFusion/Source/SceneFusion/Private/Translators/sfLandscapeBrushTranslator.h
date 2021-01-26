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
#include <Math/IntRect.h>
#include <sfObject.h>
#include <sfSession.h>
#include <ksEvent.h>
#include "../../Public/Translators/sfBaseUObjectTranslator.h"
#include "sfLandscapeTranslator.h"
#include "../Actors/sfDecalActor.h"

using namespace KS::SceneFusion2;

/**
 * Manages landscape brush indicators that show where other user's landscape brushes are.
 */
class sfLandscapeBrushTranslator : public sfBaseTranslator
{
public:
    /**
     * Constructor
     */
    sfLandscapeBrushTranslator();

    static UMaterialInterface* BrushMaterial;

protected:
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

private:
    /**
     * Hack to expose some members of private class FLandscapeBrushCircle in LandscapeEdModeBrushes.cpp. This class
     * inherits from the same base class and declares the same first members in the same order.
     */
    class CircleBrushHack : public FLandscapeBrush
    {
    public:
        TSet<ULandscapeComponent*> BrushMaterialComponents;
        TArray<UMaterialInstanceDynamic*> BrushMaterialFreeInstances;
        FVector2D LastMousePosition;
        UMaterialInterface* BrushMaterial;
        TMap<ULandscapeComponent*, UMaterialInstanceDynamic*> BrushMaterialInstanceMap;
    };

    /**
     * Hack to expose some members of private class FLandscapeBrushComponent in LandscapeEdModeBrushes.cpp. This class
     * inherits from the same base class and declares the same first members in the same order.
     */
    class ComponentBrushHack : public FLandscapeBrush
    {
    public:
        TSet<ULandscapeComponent*> BrushMaterialComponents;
        FVector2D LastMousePosition;
    };

    sfObject::SPtr m_localObjPtr;
    sfSession::SPtr m_sessionPtr;
    UMaterialInterface* m_materialPtr;
    TSharedPtr<sfLandscapeTranslator> m_landscapeTranslatorPtr;
    TMap<uint32_t, UMaterialInstanceDynamic*> m_userMaterialMap;
    TMap<uint32_t, AsfDecalActor*> m_objToDecalMap;

    KS::ksEvent<sfUser::SPtr&>::SPtr m_userLeaveEventPtr;
    KS::ksEvent<sfUser::SPtr&>::SPtr m_colorChangeEventPtr;
    FDelegateHandle m_tickHandle;

    /**
     * Updates the landscape brush translator.
     *
     * @param   float deltaTime in seconds since last tick.
     */
    void Tick(float deltaTime);

    /**
     * Gets the bounds of the local user's landscape brush, relative to the landscape.
     *
     * @param   FIntRect& bounds - set to the landcape brush's bounds, if the local user has a landscape brush.
     * @return  bool true if we could get bounds from the landscape brush.
     */
    bool GetBrushBounds(FIntRect& bounds);

    /**
     * Gets the height of the landscape at a given point.
     *
     * @param   ALandscapeProxy* landscapePtr to get height from.
     * @param   float x coordinate relative to the landscape.
     * @param   float y coordinate relative to the landscape.
     * @return  float height at the given coorinates relative to the landscape. 0 if the coordinates are not on the
     *          landscape.
     */
    float GetHeight(ALandscapeProxy* landscapePtr, float x, float y);

    /**
     * Sets the location and size of a brush decal based on values from an sfProperty.
     * 
     * @param   AsfDecalActor* decalPtr to set location and size for.
     * @param   ALandscapeProxy* landscapePtr the decal is attached to.
     * @param   sfProperty::SPtr propertyPtr to get location and size from. Contains the bounds of the brush.
     */
    void SetLocationAndSize(AsfDecalActor* decalPtr, ALandscapeProxy* landscapePtr, sfProperty::SPtr propertyPtr);

    /**
     * Gets the material for a user's landscape brush.
     * 
     * @param   uint32_t userId to get material for.
     * @return  UMaterialInstanceDynamic* material for the user.
     */
    UMaterialInstanceDynamic* GetUserMaterial(uint32_t userId);

    /**
     * Called when a user's color changes.
     *
     * @param   sfUser::SPtr userPtr
     */
    void OnUserColorChange(sfUser::SPtr userPtr);

    /**
     * Called when a user leaves the session.
     *
     * @param   sfUser::SPtr userPtr.
     */
    void OnUserLeave(sfUser::SPtr userPtr);
};