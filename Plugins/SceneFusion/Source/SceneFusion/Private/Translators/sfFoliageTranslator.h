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
#include <CoreMinimal.h>
#include <InstancedFoliageActor.h>
#include <sfListProperty.h>

#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 22
typedef FFoliageMeshInfo FFoliageInfo;
#endif

/**
 * Manages foliage syncing. Each level with foliage contains an AInstancedFoliageActor which has a map of UFoliageTypes
 * to FFoliageInfo. We create an sfType::Foliage sfObject for each AInstancedFoliageActor and add it as a child to
 * the sfType::Actor sfObject for that foliage actor. The foliage sfObject has a list property with one dictionary
 * element for each UFoliageType-FFoliageInfo pair. The FMeshInfo contains a list of all foliage instances of the
 * corresponding foliage type, which we sync in a child list property of the dictionary property. A UFoliageType is a
 * mesh and associated data for creating foliage instances, which can either be saved in the level or as an asset. If
 * it is saved in the level, we sync it through the UObjectTranslator. Otherwise it is synced as an asset path
 * reference.
 */
class sfFoliageTranslator : public sfBaseTranslator
{
public:
    sfFoliageTranslator();

    /**
     * Sets the component On the given foliage info.
     *
     * @param   FFoliageInfo* infoPtr
     * @param   UHierarchicalInstancedStaticMeshComponent*
     * @return  bool - true if the component is set successfully
     */
    static bool SetComponentOnInfo(
        FFoliageInfo* infoPtr,
        UHierarchicalInstancedStaticMeshComponent* componentPtr);

    /**
     * Adds the given foliage instance to the given foliage info.
     *
     * @param   FFoliageInfo* infoPtr
     * @param   AInstancedFoliageActor* actorPtr
     * @param   const UFoliageType* typePtr
     * @param   const FFoliageInstance& instance
     * @param   UActorComponent* baseComponent
     * @param   bool rebuildFoliageTree
     */
    static void AddInstanceToFoliageInfo(
        FFoliageInfo* infoPtr,
        AInstancedFoliageActor* actorPtr,
        const UFoliageType* typePtr,
        const FFoliageInstance& instance,
        UActorComponent* baseComponent,
        bool rebuildFoliageTree);

    /**
     * Returns the given instanced foliage actor's foliage infos.
     *
     * @param   AInstancedFoliageActor* actorPtr
     * @return  TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>&
     */
    static inline TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& GetFoliageInfos(AInstancedFoliageActor* actorPtr)
    {
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
        return actorPtr->FoliageInfos;
#else
        return actorPtr->FoliageMeshes;
#endif
    }

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
     * Called when a foliage object is created by another user.
     *
     * @param   sfObject::SPtr objPtr that was created.
     * @param   int childIndex of new object. -1 if object is a root
     */
    virtual void OnCreate(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Called when a foliage object property changes.
     *
     * @param   sfProperty::SPtr propertyPtr that changed.
     */
    virtual void OnPropertyChange(sfProperty::SPtr propertyPtr) override;

    /**
     * Called when one or more elements are added to a list property.
     *
     * @param   sfListProperty::SPtr listPtr that elements were added to.
     * @param   int index elements were inserted at.
     * @param   int count - number of elements added.
     */
    virtual void OnListAdd(sfListProperty::SPtr listPtr, int index, int count) override;

    /**
     * Called when one or more elements are removed from a list property.
     *
     * @param   sfListProperty::SPtr listPtr that elements were removed from.
     * @param   int index elements were removed from.
     * @param   int count - number of elements removed.
     */
    virtual void OnListRemove(sfListProperty::SPtr listPtr, int index, int count) override;

private:
    /**
     * Hack to expose private variables and functions from FFoliageInstanceHash
     */
    struct FoliageHashHack
    {
    public:
        const int HashCellBits;
        TMap<uint64_t, TSet<int>> CellMap;

        /**
         * Makes a hash key from cell coordinates.
         *
         * @param   int cellX
         * @param   int cellY
         * @return  uint64_t key
         */
        uint64_t MakeKey(int cellX, int cellY) const
        {
            return ((uint64_t)(*(uint32_t*)(&cellX)) << 32) | (*(uint32_t*)(&cellY) & 0xffffffff);
        }

        /**
         * Makes a hash key from a location.
         *
         * @param   const FVector& location
         * @return  uint64_t key
         */
        uint64_t MakeKey(const FVector& location) const
        {
            return  MakeKey(FMath::FloorToInt(location.X) >> 9, FMath::FloorToInt(location.Y) >> 9);
        }
    };

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
    /**
     * Hack to expose private struct FFoliageStaticMesh
     */
    struct FFoliageStaticMeshHack : public FFoliageImpl
    {
        UHierarchicalInstancedStaticMeshComponent* Component;

#if WITH_EDITORONLY_DATA
        int32 UpdateDepth;
        bool bPreviousValue;

        bool bInvalidateLightingCache;
#endif
    };
#endif

    TSet<UFoliageInstancedStaticMeshComponent*> m_modifiedComponents;
    TSet<AInstancedFoliageActor*> m_modifiedFoliageActors;
    // Maps foliage actor and foliage type pairs to sets of sfObject ids for base components that aren't loaded yet
    // that we need to attach foliage to once they are loaded.
    TMap<TPair<AInstancedFoliageActor*, UFoliageType*>, TSet<uint32_t>> m_pendingBases;
    TMap<TPair<AInstancedFoliageActor*, UFoliageType*>, TSet<int>> m_possibleDuplicates;
    FDelegateHandle m_preTickHandle;
    bool m_uiStale;
    bool m_showedDisabledMessage;

    /**
     * Called before the scene fusion service update.
     *
     * @param   float deltaTime in seconds since the last tick.
     */
    void PreTick(float deltaTime);

    /**
     * Checks for and syncs added and removed foliage types for a foliage actor. If the actor is locked, reverts
     * foliage types to the server state.
     *
     * @param   AInstancedFoliageActor* actorPtr to sync foliage types for.
     */
    void SyncFoliageTypes(AInstancedFoliageActor* actorPtr);

    /**
     * Checks if base components we need to attach foliage to have been loaded. If all required bases are loaded,
     * creates the foliage.
     */
    void UpdatePendingBases();

    /**
     * Checks for and removes duplicate foliage instances at indexes that may have been duplicated.
     */
    void RemoveDuplicates();

    /**
     * Creates an sfObject for a foliage actor.
     *
     * @param   AInstancedFoliageActor* actorPtr to create sfObject for.
     * @return  sfObject::SPtr
     */
    sfObject::SPtr CreateObject(AInstancedFoliageActor* actorPtr);

    /**
     * Creates a property for a foliage type and its mesh info. The property contains a list of all instances for the
     * foliage type.
     *
     * @param   UFoliageType* typePtr to create property for.
     * @param   FFoliageInfo& meshInfo for the type.
     * @param   sfDictionaryProperty::SPtr
     */
    sfDictionaryProperty::SPtr CreateFoliageProperty(UFoliageType* typePtr, FFoliageInfo& meshInfo);

    /**
     * Deserializes a property for a foliage type and creates all its instances.
     *
     * @param   AInstancedFoliageActor* actorPtr to create foliage instances for.
     * @param   sfDictionaryProperty::SPtr propPtr for the foliage type.
     */
    UFoliageType* OnCreateFoliageProperty(AInstancedFoliageActor* actorPtr, sfDictionaryProperty::SPtr propPtr);

    /**
     * Sends foliage instances changes to the server, or reverts the the server state if the actor is locked.
     *
     * @param   UFoliageInstnacedStaticMeshComponent* componentPtr to sync foliage instances for.
     */
    void SyncInstances(UFoliageInstancedStaticMeshComponent* componentPtr);

    /**
     * Serializes an array of foliage instances to a list property.
     *
     * @param   TArray<FFoliageInstance>& instances to serialize.
     * @return  sfListProperty::SPtr serialized instances.
     */
    sfListProperty::SPtr Serialize(TArray<FFoliageInstance>& instances);

    /**
     * Deserializes a list property of foliage instances.
     *
     * @param   AInstancedFoliageActor* actorPtr to deserialize foliage instances for.
     * @param   UFoliageType* typePtr to deserialize instances for.
     * @param   FFoliageInfo& meshInfo to deserialize instances for.
     * @param   sfListProperty::SPtr listPtr to deserialize instances from.
     */
    void Deserialize(
        AInstancedFoliageActor* actorPtr,
        UFoliageType* typePtr,
        FFoliageInfo& meshInfo,
        sfListProperty::SPtr listPtr);

    /**
     * Serializes or deserializes a foliage instance.
     *
     * @param   FArchive& archive to serialize to or deserialize from.
     * @param   FFoliageInstance& instance to serialize or deserialize.
     */
    uint32_t Serialize(FArchive& archive, FFoliageInstance& instance);

    /**
     * Adds a sfObject id to the map of base components we need to attach foliage to that haven't been loaded yet.
     *
     * @param   AInstancedFoliageActor* actorPtr the foliage with the pending base belongs to.
     * @param   UFoliageType* typePtr for the foliage the pending base belongs to.
     * @param   uint32_t baseId of the sfObject for the pending base component.
     */
    void AddPendingBase(AInstancedFoliageActor* actorPtr, UFoliageType* typePtr, uint32_t baseId);

    /**
     * Checks if two foliage instances are the same.
     *
     * @param   FFoliageInstance& lhs
     * @param   FFoliageInstance& rhs
     * @return  bool true if the instances are the same.
     */
    bool Compare(const FFoliageInstance& lhs, const FFoliageInstance& rhs);

    /**
     * Rebuilds the foliage tree for a component if the instanced reorder table is empty. This prevents a crash when
     * adding instances.
     *
     * @param   UHierarchicalInstancedStaticMeshComponent* componentPtr to rebuild foliage tree for.
     */
    static void RebuildTreeIfInvalid(UHierarchicalInstancedStaticMeshComponent* componentPtr);

    /**
     * Refreshes the foliage counts in the foliage UI.
     */
    void RefreshFoliageUI();

    /**
     * Finds the foliage info of the given type on the given instanced foliage actor.
     *
     * @param   AInstancedFoliageActor* actorPtr
     * @param   const UFoliageType* type
     * @return  FFoliageInfo*
     */
    static inline FFoliageInfo* FindFoliageInfo(AInstancedFoliageActor* actorPtr, const UFoliageType* type);

    /**
     * Returns the given foliage info's hierarchical instanced static mesh component.
     *
     * @param   FFoliageInfo* infoPtr
     * @return  UHierarchicalInstancedStaticMeshComponent*
     */
    static inline UHierarchicalInstancedStaticMeshComponent* GetComponentFromInfo(FFoliageInfo* infoPtr);
};