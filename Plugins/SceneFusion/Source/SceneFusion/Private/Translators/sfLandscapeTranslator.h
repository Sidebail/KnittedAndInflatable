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

#include "../../Public/Translators/sfBaseTranslator.h"
#include "../../Public/Translators/sfActorTranslator.h"
#include "../../Public/sfUndoManager.h"
#include <map>
#include <sfProperty.h>
#include <sfValueProperty.h>
#include <CoreMinimal.h>
#include <Engine/Texture2D.h>
#include <LandscapeComponent.h>
#include <Landscape.h>
#include <EdMode.h>
#include <LandscapeToolInterface.h>

/**
 * Manages landscape syncing.
 */
class sfLandscapeTranslator : public sfBaseTranslator, public sfIPostUndoHandler
{
public:
    /**
     * Constructor
     */
    sfLandscapeTranslator();

    /**
     * Called after an undo or redo operation.
     */
    virtual void PostUndo() override;

    /**
     * Gets the active landcape being edited.
     *
     * @param   ALandscapeProxy* the active landcape, or nullptr if no landscape is active.
     */
    ALandscapeProxy* GetActiveLandscape();

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
     * Called when a landscape is created by another user.
     *
     * @param   sfObject::SPtr objPtr that was created.
     * @param   int childIndex of new object. -1 if object is a root
     */
    virtual void OnCreate(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Called when an object property changes.
     *
     * @param   sfProperty::SPtr propertyPtr that changed.
     */
    virtual void OnPropertyChange(sfProperty::SPtr propPtr) override;

    // Disable warning C4263
    using sfBaseTranslator::OnUObjectModified;

    /**
     * Called when an object is modified by an undo or redo transaction.
     *
     * @param   sfObject::SPtr objPtr for the uobject that was modified. nullptr if the uobjPtr is not synced.
     * @param   UObject* uobjPtr that was modified.
     * @return  bool true if event was handled and need not be processed by other handlers.
     */
    virtual bool OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr) override;

private:
    /**
     * Landscape tool ids. These are the tool indexes from Unreal.
     */
    class Tools
    {
    public:
        static const int None = -1;
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
        static const int AddComponent = 15;
        static const int DeleteComponent = 16;
        static const int MoveToLevel = 17;
        static const int Splines = 21;
#else
        static const int AddComponent = 14;
        static const int DeleteComponent = 15;
        static const int MoveToLevel = 16;
        static const int Splines = 20;
#endif
    };

    // Lanscape texture types
    enum TextureTypes : uint8_t
    {
        HEIGHTMAP = 1,
        OFFSETMAP = 1 << 1,
        WEIGHTMAP = 1 << 2,
        ALL = 255
    };

    /**
     * Stores texture type and the landscape a texture belongs to.
     */
    struct TextureInfo
    {
    public:
        ALandscapeProxy* LandscapePtr;
        TextureTypes Type;

        /**
         * Constructor
         *
         * @param   ALandscapeProxy* landscapePtr the texture belongs to.
         * @param   TextureTypes type of texture.
         */
        TextureInfo(ALandscapeProxy* landscapePtr, TextureTypes type) :
            LandscapePtr{ landscapePtr },
            Type{ type }
        {

        }
    };

    TMap<UTexture2D*, TextureInfo> m_textureInfos;
    TMap<ULandscapeComponent*, uint8_t> m_componentsToUpdate;
    TSet<ALandscapeProxy*> m_modifiedLandscapes;
    TSet<ULandscapeComponent*> m_modifiedComponents;
    TSet<UObject*> m_modifiedSplines;
    TSet<ALandscapeProxy*> m_pendingWeightmaps;
    TSet<ALandscapeProxy*> m_staleLandscapeLayers;
    TSet<ALandscapeProxy*> m_undoLandscapes;
    TSet<UTexture2D*> m_undoTextures;
    TSet<ULandscapeInfo*> m_undoLandscapeInfos;
    TSet<ALandscapeProxy*> m_landscapesToFixComponents;
    TSet<ULandscapeComponent*> m_duplicateComponents;
    TSet<ULandscapeInfo*> m_addCollisionSet;
    sfObject::SPtr m_lockedObjPtr;
    ALandscapeProxy* m_activeLandscapePtr;
    int m_tool;
    int m_canEditId;
    float m_checkChangeTimer;
    bool m_landscapeSyncEnabled;
    bool m_showedDisabledMessage;
    bool m_iteratingModifiedComponents;
    bool m_heightmapModified;
    bool m_offsetmapModified;
    bool m_weightmapModified;
    bool m_splineDeleted;
    FDelegateHandle m_onModifiedHandle;
    FDelegateHandle m_onLockHandle;
    FDelegateHandle m_tickHandle;
    FName m_landscapeInfoName;
    FName m_landscapeInfoMapName;

    /**
     * Updates the landscape translator.
     *
     * @param   float deltaTime in seconds since last tick.
     */
    void Tick(float deltaTime);

    /**
     * Creates a landscape object for a landscape component.
     *
     * @param   ULandscapeComponent* componentPtr to create landscape object for.
     * @return  sfObject::SPtr landscape object
     */
    sfObject::SPtr CreateObject(ULandscapeComponent* componentPtr);

    /**
     * Serializes heightmap data for a landscape component to a value property.
     *
     * @param   ULandscapeComponent* componentPtr to serialize heightmap data for.
     * @return  sfValueProperty::SPtr serialized heightmap data.
     */
    sfValueProperty::SPtr SerializeHeightmap(ULandscapeComponent* componentPtr);

    /**
     * Serializes offsetmap data for a landscape component to a value property or null property.
     *
     * @param   ULandscapeComponent* componentPtr to serialize offsetmap data for.
     * @return  sfProperty::SPtr serialized offsetmap data.
     */
    sfProperty::SPtr SerializeOffsetmap(ULandscapeComponent* componentPtr);

    /**
     * Serializes weightmap data for a landscape component to a property. Fixes invalid weightmap data.
     *
     * @param   ULandscapeComponent* componentPtr to serialize offsetmap data for.
     * @return  sfProperty::SPtr serialized weightmap data.
     */
    sfProperty::SPtr SerializeWeightmap(ULandscapeComponent* componentPtr);

    /**
     * Applies server heightmap data to a landscape component.
     *
     * @param   ULandscapeComponent* componentPtr to apply data to.
     * @param   sfProperty::SPtr propPtr - server data.
     */
    void ApplyServerHeightmapData(ULandscapeComponent* componentPtr, sfProperty::SPtr propPtr);

    /**
     * Applies server offsetmap data to a landscape component.
     *
     * @param   ULandscapeComponent* componentPtr to apply data to.
     * @param   sfProperty::SPtr propPtr - server data.
     */
    void ApplyServerOffsetmapData(ULandscapeComponent* componentPtr, sfProperty::SPtr propPtr);

    /**
     * Applies server weightmap data to a landscape component.
     *
     * @param   ULandscapeComponent* componentPtr to apply data to.
     * @param   sfProperty::SPtr propPtr - server data.
     */
    void ApplyServerWeightmapData(ULandscapeComponent* componentPtr, sfProperty::SPtr propPtr);

    /**
     * Applies server data to components with unapplied changes.
     */
    void ApplyServerData();

    /**
     * Sends spline changes to the server, or reverts them to the server state if the splines are locked.
     */
    void SyncSplines();

    /**
     * Syncs landscape texture changes to the server, or reverts them to the server state if the landscape is locked.
     */
    void SyncTextures();

    /**
     * Called when an undo or redo transaction changes a landscape.
     *
     * @param   ALandscapeProxy* landscapePtr changed by undo or redo.
     */
    void OnUndoLandscape(ALandscapeProxy* landscapePtr);

    /**
     * Called before an undo or redo transaction changes a landscape component.
     *
     * @param   ULandscapeComponent* componentPtr changed by undo or redo.
     */
    void PreUndoLandscapeComponent(ULandscapeComponent* componentPtr);

    /**
     * Called when an undo or redo transaction changes a landscape component.
     *
     * @param   ULandscapeComponent* componentPtr changed by undo or redo.
     */
    void OnUndoLandscapeComponent(ULandscapeComponent* componentPtr);

    /**
     * Checks if the landscape tool or target landscape changed.
     */
    void UpdateTool();

    /**
     * Called when the landscape tool or target landscape changes.
     *
     * @param   int tool id
     * @param   ALandscapeProxy* landscapePtr - target landscape
     */
    void OnToolChange(int tool, ALandscapeProxy* landscapePtr);

    /**
     * Checks if we can edit the landscape.
     *
     * @return  bool true if we can edit the landscape.
     */
    bool CanEdit();

    /**
     * Refreshes the weight layers list in the UI.
     */
    void RefreshLayers();

    /**
     * Applies sever data to weightmaps that we couldn't apply earlier because the landscape didn't have enough layers
     * set.
     */
    void ApplyPendingWeightmaps();

    /**
     * Gets the heightmap texture from a landscape component.
     *
     * @param   ULandscapeComponent* componentPtr to get heightmap from.
     * @return  UTexture2D* heightmap
     */
    UTexture2D* GetHeightmap(ULandscapeComponent* componentPtr);

    /**
     * Sets the heightmap texture on a landscape component.
     *
     * @param   ULandscapeComponent* componentPtr to set heightmap on.
     * @param   UTexture2D* heightmapPtr
     */
    void SetHeightmap(ULandscapeComponent* componentPtr, UTexture2D* heightmapPtr);

    /**
     * Called when an object is modified. If the object is a height map texture, triggers a change check after a delay.
     *
     * @param   UObject* uobjPtr - modified object
     */
    void OnUObjectModified(UObject* uobjptr);

    /**
     * Called when a spline object is modified. Triggers a change check on the next tick.
     *
     * @param   sfObject::SPtr objPtr for the spline that was modified.
     * @param   UObject* uobjPtr that was modified.
     */
    void OnSplineModified(sfObject::SPtr objPtr, UObject* uobjPtr);

    /**
     * Called when the lock state of an actor changes. If the actor is a landscape and it became fully locked, sets the
     * lock material on the landscape. If it became partially locked and we are editing splines, sets the partial lock
     * material on the landscape.
     *
     * @param   AActor* actorPtr whose lock state changed.
     * @param   sfActorTranslator::LockType lockType
     * @param   sfUser::SPtr userPtr who owns the lock. nullptr if the actor is partially locked.
     */
    void OnLockChange(AActor* actorPtr, sfActorTranslator::LockType lockType, sfUser::SPtr userPtr);

    /**
     * Checks if a landscape has at least the given number of layers and none of them are null.
     *
     * @param   ALandscapeProxy* landscapePtr to check.
     * @param   int numLayers to check for.
     * @return  bool true if the landscape has at least the given number of layers and none of them are null.
     */
    bool HasNumLayersOrMore(ALandscapeProxy* landscapePtr, int numLayers);

    /**
     * Called when a landscape component is added by another user.
     *
     * @param   ULandscapeComponent* componentPtr that was added.
     */
    void OnAddComponent(ULandscapeComponent* componentPtr);

    /**
     * Called when a landscape component is deleted by another user.
     *
     * @param   ULandscapeComponent* componentPtr that was deleted.
     */
    void OnDeleteComponent(ULandscapeComponent* componentPtr);

    /**
     * Called before a landscape component is moved from one level to another.
     *
     * @param   ULandscapeComponent* componentPtr that will be moved.
     * @param   ALandscapeProxy* newParentPtr the component will be attached to.
     */
    void PreMoveComponent(ULandscapeComponent* componentPtr, ALandscapeProxy* newParentPtr);

    /**
     * Makes a component use its own heightmap texture.
     *
     * @param   ULandscapeComponent* componentPtr that should use its own heightmap texture.
     * @param   UObject* otherPtr for the new heightmap texture. Nullptr to keep the current outer.
     */
    void SplitHeightmap(ULandscapeComponent* componentPtr, UObject* outerPtr = nullptr);

    /**
     * Returns the given landscape component's weightmap textures.
     *
     * @param   ULandscapeComponent* componentPtr
     * @return  TArray<UTexture2D*>&
     */
    static inline TArray<UTexture2D*>& GetWeightmapTextures(ULandscapeComponent* componentPtr);

    /**
     * Returns the given landscape component's weightmap layers.
     *
     * @param   ULandscapeComponent* componentPtr
     * @return  TArray<FWeightmapLayerAllocationInfo>&
     */
    static inline TArray<FWeightmapLayerAllocationInfo>& GetWeightmapLayerAllocations(
        ULandscapeComponent* componentPtr);
};
