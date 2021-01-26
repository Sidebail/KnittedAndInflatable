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
#include <sfObject.h>
#include <sfDictionaryProperty.h>
#include <Materials/MaterialInterface.h>
#include "../../Public/Translators/sfBaseTranslator.h"
#include "../../Public/sfIStandInGenerator.h"

using namespace KS::SceneFusion2;

/**
 * Generates bounding-box mesh stand-ins for missing meshes. Uploads mesh bounding-box data so stand-ins can be created
 * to fit the dimensions of the missing mesh.
 */
class sfMeshStandInTranslator : public sfBaseTranslator, public sfIStandInGenerator
{
public:
    /**
     * Constructor
     */
    sfMeshStandInTranslator();

    /**
     * Destructor
     */
    virtual ~sfMeshStandInTranslator();

    /**
     * Deinitialization. Called after disconnecting from a session.
     */
    virtual void CleanUp() override;

private:
    // Keys are mesh paths, values are their bounding-boxes.
    sfDictionaryProperty::SPtr m_meshBoundsPtr;
    FDelegateHandle m_onGetAssetPropertyHandle;
    UMaterialInterface* m_materialPtr;

    /**
     * Called when the mesh bounds object is created by another user.
     *
     * @param   sfObject::SPtr objPtr that was created.
     * @param   int childIndex of new object. -1 if object is a root
     */
    virtual void OnCreate(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Generates a mesh stand-in using the dimensions of the missing mesh.
     *
     * @param   const FString& path of missing mesh.
     * @param   UObject* uobjPtr - mesh to generate data for.
     */
    virtual void Generate(const FString& path, UObject* uobjPtr) override;

    /**
     * Sends mesh bounding-box data to the server if it hasn't already been sent.
     *
     * @param   UObject* uobjPtr - mesh to send bounding-box for.
     */
    void SendBounds(UObject* uobjPtr);

    /**
     * Gets the bounding-box from server data for the mesh at the given path.
     *
     * @param   const sfName& path to mesh.
     * @return  FBox bounds of the mesh.
     */
    FBox GetBounds(const sfName& path);

    /**
     * Replaces characters in a mesh path that aren't permitted in property names with "//".
     *
     * @param   FString path to sanitize.
     * @return  sfName sanitized path.
     */
    sfName SanitizePath(FString path);
};