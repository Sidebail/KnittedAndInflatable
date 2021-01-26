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
#include <Engine/Level.h>
#include <Engine/LevelStreaming.h>
#include <Layers/Layer.h>
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 23
#include <Layers/ILayers.h>
#else
#include <Layers/LayersSubsystem.h>
#endif
#include <sfSession.h>
#include <unordered_map>
#include <unordered_set>

#include "sfBaseUObjectTranslator.h"
#include "sfActorTranslator.h"

using namespace KS::SceneFusion2;
using namespace KS;

/**
 * Manages level syncing. Level relationship is not maintained.
 */
class sfLevelTranslator : public sfBaseUObjectTranslator
{
public:
    friend class sfUndoManager;

    /**
     * Delegate for on upload level.
     *
     * @param   sfObject::SPtr object for the level that will be uploaded.
     * @param   ULevel* level that will be uploaded.
     */
    DECLARE_MULTICAST_DELEGATE_TwoParams(OnUploadLevelDelegate, sfObject::SPtr, ULevel*);

    /**
     * Invoked before uploading a level.
     */
    OnUploadLevelDelegate OnUploadLevel;

    /**
     * Constructor
     */
    sfLevelTranslator();

    /**
     * Destructor
     */
    virtual ~sfLevelTranslator();

    /**
     * Initialization. Called after connecting to a session.
     */
    virtual void Initialize();

    /**
     * Deinitialization. Called after disconnecting from a session.
     */
    virtual void CleanUp();

    /**
     * Called when an sfObject of type level, level lock or world settings is created.
     *
     * @param   sfObject::SPtr objPtr that was created.
     * @param   int childIndex of new object. -1 if object is a root
     */
    virtual void OnCreate(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Called when a level is deleted by another user. Unloads the level.
     *
     * @param   sfObject::SPtr objPtr that was deleted.
     */
    virtual void OnDelete(sfObject::SPtr objPtr) override;

    /**
     * Called when an object property changes. If the property is a layer, renames the layer to the server value.
     *
     * @param   sfProperty::SPtr propertyPtr that changed.
     */
    virtual void OnPropertyChange(sfProperty::SPtr propertyPtr) override;

    /**
     * Called when one or more elements are added to a list property. If the property is a layer, creates the layer.
     *
     * @param   sfListProperty::SPtr listPtr that elements were added to.
     * @param   int index elements were inserted at.
     * @param   int count - number of elements added.
     */
    virtual void OnListAdd(sfListProperty::SPtr listPtr, int index, int count) override;

    /**
     * Called when one or more elements are removed from a list property. If the property is a layer, delete the layer.
     *
     * @param   sfListProperty::SPtr listPtr that elements were removed from.
     * @param   int index elements were removed from.
     * @param   int count - number of elements removed.
     */
    virtual void OnListRemove(sfListProperty::SPtr listPtr, int index, int count) override;

    /**
     * Gets the uobject for an sfObject, or nullptr if the sfObject has no uobject.
     *
     * @param   sfObject::SPtr objPtr to get uobject for.
     * @return  UObject* uobject for the sfObject.
     */
    virtual UObject* GetUObject(sfObject::SPtr objPtr) override;

    /**
     * Returns true if we can find sfObject for the given level and the level object has received its children from
     * the server.
     *
     * @param   ULevel* levelPtr - pointer of level to check
     * @return  bool
     */
    bool IsLevelObjectInitialized(ULevel* levelPtr);

    /**
     * Adds a level to a set to be checked for actor order changes in the next PreTick.
     *
     * @param   ULevel* levelPtr
     */
    void MarkActorOrderStale(ULevel* levelPtr);

    /**
     * Registers the layers change event handler.
     */
    void RegisterOnLayersChangeHandler();

    /**
     * Unregisters the layers change event handler.
     */
    void UnregisterOnLayersChangeHandler();

private:
    typedef std::function<void()> Callback;

    sfSession::SPtr m_sessionPtr;
    TSharedPtr<sfActorTranslator> m_actorTranslatorPtr;
    bool m_uploadUnsyncedLevels;
    bool m_downloadedInitialLevels;
    UWorld* m_worldPtr;
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 23
    TSharedPtr<ILayers> m_worldLayersPtr;
#else
    ULayersSubsystem* m_worldLayersPtr;
#endif
    sfListProperty::SPtr m_layersPropertyPtr;
    sfObject::SPtr m_worldSettingsObjPtr; // sfObject for the world setting properties
    sfObject::SPtr m_lockObject; // Request lock on this object before upload levels to the server
    bool m_worldSettingsDirty;
    bool m_hierarchicalLODSetupDirty;
    bool m_showedDisabledMessage;

    TSet<ULevel*> m_localActorOrderChangedLevels;
    TSet<ULevel*> m_serverActorOrderChangedLevels;
    std::unordered_set<ULevel*> m_movedLevels; // Levels whose offset have changed
    TSet<ULevelStreaming*> m_dirtyStreamingLevels; // Dirty streaming levels to check folder change
    std::unordered_set<sfObject::SPtr> m_levelsNeedToBeLoaded; // Levels that was deleted but locked
    std::unordered_set<ULevel*> m_levelsToUpload; // Levels to upload to server
    TMap<FString, sfObject::SPtr> m_unloadedLevelObjects; // sfObjects of unloaded levels
    // sfObjects of levels that requested server for children
    std::unordered_set<sfObject::SPtr> m_levelsWaitingForChildren;
    // Levels whose package was dirty. We need to check parent change on them
    TSet<ULevel*> m_dirtyParentLevels;
    // Levels that are just added to the world without applying server properties.
    TSet<ULevel*> m_uninitializedLevels;
    TArray<FName> m_layerNames;

    // List of properties we want sfPropertyManager class to ignore because they are handled in sfLevelTranslator
    const TSet<FString> PROPERTY_BLACKLIST{ "LevelTransform" };
    const TSet<FString> WORLD_SETTINGS_BLACKLIST{ "bEnableWorldComposition" };

    UClass* m_worldTileDetailsClassPtr;
    UnrealProperty* m_packageNamePropertyPtr;

    FDelegateHandle m_onAddLevelToWorldHandle;
    FDelegateHandle m_onPrepareToCleanseEditorObjectHandle;
    FDelegateHandle m_onWorldCompositionChangeHandle;
    FDelegateHandle m_onPackageMarkedDirtyHandle;
    FDelegateHandle m_onPropertyChangeHandle;
    FDelegateHandle m_onLevelDirtiedHandle;
    FDelegateHandle m_onLayersChangedHandle;
    FDelegateHandle m_preTickHandle;
    FDelegateHandle m_tickHandle;
    sfSession::AcknowledgeSubscriptionEventHandle m_onAcknowledgeSubscriptionHandle;
    TMap<ULevel*, FDelegateHandle> m_onLevelTransformChangeHandles;

    /**
     * Called before the scene fusion service update.
     *
     * @param   float deltaTime in seconds since the last tick.
     */
    void PreTick(float deltaTime);

    /**
     * Updates the level translator.
     *
     * @param   float deltaTime in seconds since the last tick.
     */
    void Tick(float deltaTime);

    /**
     * Tries to find level in all loaded levels. If found, returns level pointer. Otherwise, returns nullptr.
     *
     * @param   FString levelPath
     * @param   bool isPersistentLevel
     * @return  ULevel*
     */
    ULevel* FindLevelInLoadedLevels(FString levelPath, bool isPersistentLevel);

    /**
     * Tries to load level from file and return level pointer.
     *
     * @param   FString levelPath
     * @param   bool isPersistentLevel
     * @param   bool& 
     * @return  ULevel* cancel - set to true if the user cancelled.
     */
    ULevel* TryLoadLevelFromFile(FString levelPath, bool isPersistentLevel, bool& cancel);

    /**
     * Creates map file for level and returns level pointer.
     *
     * @param   FString levelPath
     * @param   bool isPersistentLevel
     * @return  ULevel*
     */
    ULevel* CreateMap(FString levelPath, bool isPersistentLevel);

    /**
     * Creates an sfObject for a uobject. Does not upload or create properties for the object.
     *
     * @param   UObject* uobjPtr to create sfObject for.
     * @param   sfObject::SPtr* outObjPtr created for the uobject.
     * @return  bool true if the uobject was handled by this translator.
     */
    virtual bool Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr) override;

    /**
     * Called when a level is added to the world. Uploads the new level.
     *
     * @param   ULevel* newLevelPtr
     * @param   UWorld* worldPtr
     */
    void OnAddLevelToWorld(ULevel* newLevelPtr, UWorld* worldPtr);

    /**
     * Called when the editor is about to cleanse an object that must be purged,
     * such as when changing the active map or level. If the object is a world object, disconnect.
     * If it is a level object, delete the sfObject on the server. Clears raw pointers of actors in
     * the level from our containers.
     *
     * @param   UObject* uobjPtr - object to be purged
     */
    void OnPrepareToCleanseEditorObject(UObject* uobjPtr);

    /**
     * Uploads levels that don't exist on the server.
     */
    void UploadUnsyncedLevels();

    /**
     * Registers property change handlers for server events.
     */
    void RegisterPropertyChangeHandlers();

    /**
     * Checks for and sends transform changes for a level to the server.
     *
     * @param   ULevel* levelPtr to send transform update for.
     */
    void SendTransformUpdate(ULevel* levelPtr);

    /**
     * Sends a new folder value to the server.
     *
     * @param   ULevelStreaming* streamingLevelPtr to send folder change for.
     */
    void SendFolderChange(ULevelStreaming* streamingLevelPtr);

    /**
     * Modifies a ULevel. Removes event handlers before and adds event handlers back after.
     * Prevents any changes to the undo stack during the call.
     *
     * @param   ULevel* levelPtr to modify
     * @param   Callback callback to modify the given level
     */
    void ModifyLevelWithoutTriggerEvent(ULevel* levelPtr, Callback callback);

    /**
     * Uploads the given level.
     *
     * @param   ULevel* levelPtr
     */
    void UploadLevel(ULevel* levelPtr);
    
    /**
     * Reuqests lock for uploading levels.
     */
    void RequestLock();

    /**
     * Updates the actor order for levels whose actor order was changed by the server.
     */
    void UpdateActorOrder();

    /**
     * Regsiters handlers on level events.
     */
    void RegisterLevelEvents();

    /**
     * Unregsiters handlers on level events.
     */
    void UnregisterLevelEvents();

    /**
     * Called when the server acknowledges the subscription for the given object's children.
     *
     * @param   bool isSubscription - if true, this acknowledgement is for a subscription.
     *          Otherwise, it is for an unsubscription.
     * @param   sfObject::SPtr objPtr
     */
    void OnAcknowledgeSubscription(bool isSubscription, sfObject::SPtr objPtr);

    /**
     * Called when a level object is created. Subscribes to children of the object if the level is loaded.
     *
     * @param   sfObject::SPtr objPtr
     */
    void OnCreateLevelObject(sfObject::SPtr objPtr);

    /**
     * Called when a level properties object is created.
     */
    void OnCreateLevelPropertiesObject(sfObject::SPtr objPtr);

    /**
     * Called when a level lock object is created.
     *
     * @param   sfObject::SPtr objPtr
     */
    void OnCreateLevelLockObject(sfObject::SPtr objPtr);

    /**
     * Called when a world settings object is created.
     *
     * @param   sfObject::SPtr objPtr
     */
    void OnCreateWorldSettingsObject(sfObject::SPtr objPtr);

    /**
     * Called when a game mode object is created.
     *
     * @param   sfObject::SPtr objPtr
     */
    void OnCreateGameModeObject(sfObject::SPtr objPtr);

    /**
     * Tries to toggle world composition. If it failed, leaves the session.
     *
     * @param   bool enableWorldComposition - if ture enable world composition. Otherwise, disable world composition.
     */
    void TryToggleWorldComposition(bool enableWorldComposition);

    /**
     * Toggles world composition. Unloads all sublevels before and loads them back after.
     *
     * @param   bool enableWorldComposition - if ture enable world composition. Otherwise, disable world composition.
     */
    void ToggleWorldComposition(bool enableWorldComposition);

    /**
     * Sets the world composition property on the server.
     *
     * @param   UWorld* worldPtr
     */
    void SetWorldCompositionOnServer(UWorld* worldPtr);

    /**
     * Gets the world composition property on the server.
     *
     * @return  bool
     */
    bool GetWorldCompositionOnServer();

    /**
     * Called when UPackage is marked dirty. If the package contains a level, put the level into the dirty parent level
     * set to check the level parent change in the Tick function.
     *
     * @param   UPackage* packagePtr - pointer to the dirty package
     * @param   bool wasDirty - If true, the package was marked as dirty before.
     */
    void OnPackageMarkedDirty(UPackage* packagePtr, bool wasDirty);

    /**
     * Called when a level is dirtied. If we are in a transaction that changed the actor order for the current level,
     * puts the level in a set to check for actor order changes in the PreTick function.
     */
    void OnLevelDirtied();

    /**
     * Loads level from file if the map file exists. Otherwise, creates a level and save it to the given path.
     * Returns the loaded or created level.
     *
     * @param   const FString& levelPath
     * @param   bool isPersistentLevel - true if the given level is the persitent level
     * @return  ULevel* - loaded level
     */
    ULevel* LoadOrCreateMap(const FString& levelPath, bool isPersistentLevel);

    /**
     * Called when a property on a ULevelStreaming object changes.
     *
     * @param   sfObject::SPtr objPtr for the uobject whose property changed.
     * @param   UObject* uobjPtr - pointer to the ULevelStreaming object whose property changed.
     * @param   UnrealProperty* upropPtr that changed.
     * @return  bool false if the property change event should be handled by the default handler.
     */
    virtual bool OnUPropertyChange(sfObject::SPtr objPtr, UObject* uobjPtr, UnrealProperty* upropPtr) override;

    /**
     * Called when an object is modified. Sends streaming level changes to server.
     *
     * @param   UObject* uobjPtr - modified object
     */
    virtual void OnUObjectModified(sfObject::SPtr objPtr, UObject* uobjPtr) override;

    /**
     * Called when an object is modified by an undo or redo transaction.
     *
     * @param   sfObject::SPtr objPtr for the uobject that was modified. nullptr if the uobjPtr is not synced.
     * @param   UObject* uobjPtr that was modified.
     * @return  bool true if event was handled and need not be processed by other handlers.
     */
    virtual bool OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr) override;

    /**
     * Called when a property on a world tile details object changes.
     *
     * @param   UObject* uobjPtr - pointer to the ULevelStreaming object whose property changed.
     * @param   UnrealProperty* upropPtr that changed.
     */
    void OnTileDetailsChange(UObject* uobjPtr, UnrealProperty* upropPtr);

    /**
     * Returns UWorldTileDetails object for the given level.
     *
     * @param   FString levelPath - path of the level to find UWorldTileDetails object for.
     * @return  UObject* - the found object. Returns nullptr if it could not be found.
     */
    UObject* FindWorldTileDetailsObject(FString levelPath);

    /**
     * Called when a property is changed through the details panel. If the changed property is the level tile position
     * and the level object is locked, reverts the position back to server value. This needs to be done before Unreal
     * moves all the level actors so we cannot do it in the sfBaseTranslator::OnUPropertyChange.
     *
     * @param   UObject* uobjPtr whose property changed.
     * @param   FPropertyChangedEvent& ev with information on what property changed.
     */
    void OnUPropertyChange(UObject* uobjPtr, FPropertyChangedEvent& ev);

    /**
     * Tries to find level object and properties for the given world tile details object.
     * Returns true if found. Otherwise, returns false.
     *
     * @param   UObject* worldTileDetailPtr
     * @param   sfObject::SPtr& levelObjPtr
     * @param   sfDictionaryProperty::SPtr& levelPropertiesPtr
     * @return  bool
     */
    bool TryGetLevelObjectAndPropertyForTileDetailObject(
        UObject* worldTileDetailPtr,
        sfObject::SPtr& levelObjPtr,
        sfDictionaryProperty::SPtr& levelPropertiesPtr);

    /**
     * Refreshes world settings tab.
     */
    void RefreshWorldSettingsTab();

    /**
     * Called when a layer is modified. Sends the layer changes to the server.
     *
     * @param   const ELayersAction::Type action
     * @param   const TWeakObjectPtr<ULayer>& changedLayer
     * @param   const FName& changedProperty
     */
    void OnLayersChanged(
        const ELayersAction::Type action,
        const TWeakObjectPtr<ULayer>& changedLayer,
        const FName& changedProperty);

    /**
     * Sends layers change to the server.
     */
    bool SyncLayers();
};
