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

#include "sfIStandInGenerator.h"
#include <sfProperty.h>
#include <sfObject.h>
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>
#include <CoreMinimal.h>
#include <Framework/Application/IInputProcessor.h>
#include <Framework/Application/SlateApplication.h>
#include <Engine/AssetManager.h>

using namespace KS;
using namespace KS::SceneFusion2;

/**
 * Utility for loading assets from memory, or from disc when the user is idle. Loading from disc may trigger assets to
 * be baked which can block the main thread (and cannot be done from another thread) for several seconds and interrupt
 * the user, so we wait till the user is idle.
 */
class sfLoader : public IInputProcessor
{
public:
    /**
     * Stand-in generator.
     *
     * @param   const FString& path to missing asset to generate stand-in for.
     * @param   UObject* stand-in to generate data for.
     */
    typedef std::function<void(const FString&, UObject*)> StandInGenerator;

    /**
     * Delegate for on create asset.
     *
     * @param   UObject* asset that was created.
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(OnCreateMissingAssetDelegate, UObject*);

    /**
     * Delegate for on create stand-in.
     *
     * @param   const FString& path to missing asset the stand-in was created for.
     * @param   UObject* stand-in
     */
    DECLARE_MULTICAST_DELEGATE_TwoParams(OnCreateStandInDelegate, const FString&, UObject*);

    /**
     * Delegate for on replace stand-in.
     *
     * @param   const FString& path to asset that replaced the stand-in.
     * @param   UObject* stand-in that was replaced.
     */
    DECLARE_MULTICAST_DELEGATE_TwoParams(OnReplaceStandInDelegate, const FString&, UObject*);

    /**
     * Invoked when the loader creates a missing asset.
     */
    OnCreateMissingAssetDelegate OnCreateMissingAsset;

    /**
     * Invoked when a stand-in is created for a missing asset.
     */
    OnCreateStandInDelegate OnCreateStandIn;

    /**
     * Invoked when a new asset replaces a stand-in.
     */
    OnReplaceStandInDelegate OnReplaceStandIn;

    /**
     * @return  sfLoader& singleton instance.
     */
    static sfLoader& Get();

    /**
     * Constructor
     */
    sfLoader();

    /**
     * Constructor
     */
    virtual ~sfLoader();

    /**
     * Starts monitoring user activity and loads assets when the user becomes idle.
     */
    void Start();

    /**
     * Stops monitoring user activity and stops loading.
     */
    void Stop();

    /**
     * Registers a generator to generate data for stand-ins of a given class.
     *
     * @param   UClass* classPtr to register generator for.
     * @param   TSharedPtr<sfIStandInGenerator> generatorPtr to register.
     */
    void RegisterStandInGenerator(UClass* classPtr, TSharedPtr<sfIStandInGenerator> generatorPtr);

    /**
     * Registers a generator to generate data for stand-ins of a given class.
     *
     * @param   UClass* classPtr to register generator for.
     * @param   StandInGenerator generator to register.
     */
    void RegisterStandInGenerator(UClass* classPtr, StandInGenerator generator);

    /**
     * Checks if the user is idle.
     *
     * @return  bool true if the user is idle.
     */
    bool IsUserIdle();

    /**
     * Loads the asset for a property when the user becomes idle.
     *
     * @param   sfProperty::SPtr propPtr to load asset for.
     */
    void LoadWhenIdle(sfProperty::SPtr propPtr);

    /**
     * Loads delayed assets referenced by an object or its component children.
     *
     * @param   sfObject::SPtr objPtr to load assets for.
     */
    void LoadAssetsFor(sfObject::SPtr objPtr);

    /**
     * Loads an asset. If the asset could not be found, creates a stand-in to represent the asset.
     *
     * @param   const FString& path of asset to load.
     * @param   const FString& className of asset to load.
     * @return  UObject* asset or stand-in for the asset. nullptr if the asset was not found and a stand-in could not
     *          be created.
     */
    UObject* Load(const FString& path, const FString& className);

    /**
     * Gets the class name and path of the asset a stand-in is representing.
     *
     * @param   UObject* standInPtr
     * @return  FString class name and path seperated by a ';'.
     */
    FString GetPathFromStandIn(UObject* standInPtr);

    /**
     * Adds a property to the stand-in property map. When the stand-in is replaced with the missing asset, the property
     * will be reloaded so it references the asset instead of the stand-in. Asset reference properties do not need to
     * be passed to this function as they are detected and updated automatically, but more complex properties that
     * reference assets (such as serialized binary data which may contain asset references and other data) need to be
     * added to the map.
     *
     * @param   uint32_t pathId - string table id of the stand-in class and path seperated by a ';'.
     * @param   sfProperty::SPtr propertyPtr that references the stand-in.
     */
    void AddStandInReference(uint32_t pathId, sfProperty::SPtr propertyPtr);

    /**
     * Loads an asset from memory. Returns null if the asset was not found in memory.
     *
     * @param   FString path to the asset.
     * @return  UObject* loaded asset or nullptr.
     */
    UObject* LoadFromCache(FString path);

    /**
     * Called every tick while the sfLoader is running. Loads assets if the user is idle and sets references to them,
     * and replaces stand-ins with their proper assets if they are available.
     *
     * @param   const float deltaTime
     * @param   FSlateApplication& slateApp
     * @param   TSharedRef<ICursor> cursor
     */
    virtual void Tick(const float deltaTime, FSlateApplication& slateApp, TSharedRef<ICursor> cursor) override;

    /**
     * Called when a mouse button is pressed.
     *
     * @param   FSlateApplication& slateApp
     * @param   const FPointerEvent& mouseEvent
     * @return  bool false to indicate the event was not handled.
     */
    virtual bool HandleMouseButtonDownEvent(FSlateApplication& slateApp, const FPointerEvent& mouseEvent) override;

    /**
     * Called when a mouse button is released.
     *
     * @param   FSlateApplication& slateApp
     * @param   const FPointerEvent& mouseEvent
     * @return  bool false to indicate the event was not handled.
     */
    virtual bool HandleMouseButtonUpEvent(FSlateApplication& slateApp, const FPointerEvent& mouseEvent) override;

    /**
     * Checks if an asset type should be created if it is missing.
     *
     * @param   UClass* classPtr
     * @return  bool true if the asset type should be created if it is missing.
     */
    bool IsCreatableAssetType(UClass* classPtr);

    /**
     * Checks if an asset was a missing asset we created when we tried to load it.
     *
     * @param   UObject* assetPtr
     * @return  bool true if the asset was created when we tried to load it.
     */
    bool WasCreatedOnLoad(UObject* assetPtr);

    /**
     * Registers an asset type to be created if it is missing when we try to load it.
     */
    template<typename T>
    void RegisterCreatableAssetType()
    {
        m_createTypes.Add(T::StaticClass());
    }

    /**
     * Unregisters a creatable asset type.
     */
    template<typename T>
    void UnregisterCreatableAssetType()
    {
        m_createTypes.Remove(T::StaticClass());
    }

private:
    static TSharedPtr<sfLoader> m_instancePtr;

    /**
     * sfIStandInGenerator implementation that wraps a delegate.
     */
    class Generator : public sfIStandInGenerator
    {
    public:
        /**
         * Constructor
         *
         * @param StandInGenerator generator
         */
        Generator(StandInGenerator generator) :
            m_generator{ generator }
        {

        }

        /**
         * Destructor
         */
        virtual ~Generator() {

        }

        /**
         * Calls the generator delegate.
         *
         * @param   const FString* path of missing asset.
         * @param   UObject* uobjPtr stand-in to generate data for.
         */
        virtual void Generate(const FString& path, UObject* uobjPtr) override
        {
            m_generator(path, uobjPtr);
        }

    private:
        StandInGenerator m_generator;
    };

    // Maps objects to a list of their properties that referenced assets to be loaded when the user is idle.
    std::unordered_map<sfObject::SPtr, std::vector<sfProperty::SPtr>> m_delayedAssets;
    // Maps classes to the path to their stand-in asset. If a class is not in the map, a stand-in is created using
    // NewObject.
    TMap<UClass*, FString> m_standInPaths;
    TMap<UClass*, TSharedPtr<sfIStandInGenerator>> m_standInGenerators;
    // Maps missing asset paths to their stand-ins.
    TMap<FString, UObject*> m_standIns;
    // Maps stand-in path string table ids to properties that contain references to the stand-in. Properties that are
    // just an asset reference aren't in this map as they are detected automatically, but more complex properties (such
    // as binary data containing asset references) need to be in this map so the asset reference can be updated when
    // the missing asset becomes available.
    std::unordered_map<uint32_t, std::vector<std::weak_ptr<sfProperty>>> m_standInPropertyMap;
    TArray<UObject*> m_standInsToReplace;
    // Will create assets of these types when loading if the asset does not exist
    TSet<UClass*> m_createTypes;
    TSet<UObject*> m_createdAssets;
    float m_replaceTimer;
    bool m_isMouseDown;
    bool m_overrideIdle;// if true, IsUserIdle() returns true even if the user isn't idle
    FDelegateHandle m_onNewAssetHandle;

    /**
     * Replaces references to stand-ins with the proper assets.
     */
    void ReplaceStandIns();

    /**
     * Checks if a property is a value property referencing one of the given stand-in path ids.
     *
     * @param   sfProperty::SPtr propertyPtr to check.
     * @param   const TSet<uint32_t>& pathIds - string table ids of paths to stand-in assets.
     * @return  bool true if the property is one of the stand-in asset path ids.
     */
    bool ReferencesStandIn(sfProperty::SPtr propertyPtr, const TSet<uint32_t>& pathIds);

    /**
     * Loads delayed assets and sets references to them.
     */
    void LoadDelayedAssets();

    /**
     * Loads the asset for a property and sets the reference to it.
     *
     * @param   sfProperty::SPtr propPtr
     */
    void LoadProperty(sfProperty::SPtr propPtr);

    /**
     * Called when a new asset is created. If the asset has a stand-in, adds the stand-in to a list to replaced with
     * the new asset.
     *
     * @param   const FAssetData& assetData
     */
    void OnNewAsset(const FAssetData& assetData);
};
