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

#include <sfUser.h>
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Runtime/Engine/Classes/Components/MeshComponent.h"
#include "sfLockComponent.generated.h"

using namespace KS::SceneFusion2;

/**
 * Lock component for indicating an actor cannot be edited. This is added to each mesh component of the actor, and
 * adds a copy of the mesh as a child with a lock shader. It also deletes itself and unlocks the actor when copied.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UsfLockComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    /**
     * Constructor
     */
    UsfLockComponent();

    /**
     * Destructor
     */
    virtual ~UsfLockComponent();

    /**
     * Initialization
     */
    virtual void InitializeComponent() override;

    /**
     * Called after being duplicated. Destroys this component, its children if any, and unlocks the actor.
     */
    virtual void PostEditImport() override;

    /**
     * Called when the attach parent changes. If the DuplicateParentMesh was never called, does nothing. Otherwise if
     * this is the only lock component on the actor, destroys the children of this component. Otherwise destroys this
     * component.
     */
    virtual void OnAttachmentChanged() override;

    /**
     * Called when the component is destroyed. Destroys children components.
     *
     * @param   bool bDestroyingHierarchy
     */
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

    /**
     * Duplicates the parent mesh component and adds the duplicate as a child.
     *
     * @param   UMaterialInterface* materialPtr to use on the duplicate mesh. If nullptr, will use the current
     *          material.
     */
    void DuplicateParentMesh(UMaterialInterface* materialPtr = nullptr);

    /**
     * If attached to a brush, creates or finds the mesh for the brush's model and adds it as a child mesh component.
     *
     * @param   UMaterialInterface* materialPtr to use on the mesh. If nullptr, will use the current material.
     */
    void CreateOrFindModelMesh(UMaterialInterface* materialPtr = nullptr);

    /**
     * Sets the landscape lock material on all landscape components attached to the landscape this component is
     * attached to.
     *
     * @param   UMaterialInterface* materialPtr
     */
    void SetLandscapeMaterial(UMaterialInterface* materialPtr);

    /**
     * Destroys the children of this component.
     */
    void DestroyChildren();

    /**
     * Sets the material of all child meshes.
     *
     * @param   UMaterialInterface* materialPtr. If nullptr, uses the current material.
     */
    void SetMaterial(UMaterialInterface* materialPtr = nullptr);

    /**
     * Allows the mesh generated from a brush's model to be garbage collected and removes it from the model mesh map.
     *
     * @param   ABrush* brushPtr to destroy generated mesh for.
     */
    static void DestroyModelMesh(ABrush* brushPtr);

    /**
     * Allows all meshes generated from brush models to be garbage collected and clears the model mesh map.
     */
    static void DestroyModelMeshes();

private:
    /**
     * Mesh and transform data for a mesh generated from a brush model.
     */
    struct MeshData
    {
    public:
        UStaticMesh* MeshPtr;
        FQuat Rotation;
        FVector Scale;

        /**
         * Constructor
         */
        MeshData() :
            MeshPtr{ nullptr },
            Rotation{ FQuat::Identity },
            Scale{ FVector::OneVector }
        { }

        /**
         * Constructor
         *
         * @param   UStaticMesh* meshPtr
         * @param   const FQuat& rotation
         * @param   const FVector& scale
         */
        MeshData(UStaticMesh* meshPtr, const FQuat& rotation, const FVector& scale) :
            MeshPtr{ meshPtr },
            Rotation{ rotation },
            Scale{ scale }
        { }
    };

    bool m_copied;
    bool m_initialized;
    FDelegateHandle m_tickerHandle;
    UMaterialInterface* m_materialPtr;

    // Maps brushes to lock meshes generated from their models
    static TMap<ABrush*, MeshData> m_modelMeshes;
    
    /**
     * Initializes a mesh component and attaches it to this component.
     * 
     * @param   UMeshComponent* componentPtr to initialize.
     */
    void InitializeMeshComponent(UMeshComponent* componentPtr);

    /**
     * Called when a UnrealProperty changes. If the property belongs to the parent object and contains the string "mesh",
     * replaces the child mesh component with a new duplicate of the parent.
     *
     * @param   UObject* uobjPtr whose property changed.
     * @param   FPropertyChangedEvent& ev
     */
    void OnUPropertyChange(UObject* uobjPtr, FPropertyChangedEvent& ev);

    /**
     * Creates a static mesh from a brush's model. The brush's rotation and scale is applied to the generated mesh, so
     * the mesh will be aligned with the brush when the mesh has no rotation or scale.
     *
     * @param   UObject* outerPtr for the mesh.
     * @param   FName name of the mesh.
     * @param   ABrush* brushPtr
     * @return  UStaticMesh* mesh generated from the model.
     */
    UStaticMesh* CreateMeshFromBrush(UObject* outerPtr, FName name, ABrush* brushPtr);

    /**
     * Updates the material on all landscape components that have a different material from the lock material.
     *
     * @return  bool true. This function is registered as a ticker and it returns true to keep it registered. Returns
     *      false if not attached to a landscape.
     */
    bool UpdateLandscapeMaterials();
};
