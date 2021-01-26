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

#include "sfObjectEventDispatcher.h"
#include "sfUndoManager.h"
#include "sfMissingObjectManager.h"
#include "sfSessionInfo.h"

#include <sfService.h>
#include <Log.h>
#include <CoreMinimal.h>
#include <LandscapeMaterialInstanceConstant.h>

// Log setup
DECLARE_LOG_CATEGORY_EXTERN(LogSceneFusion, Log, All)

class sfUI;
class sfBaseWebService;

/**
 * Scene Fusion Plugin Module
 */
class SceneFusion : public IModuleInterface
{
public:
    static TSharedPtr<sfBaseWebService> WebService;
    static sfService::SPtr Service;
    static sfObjectEventDispatcher::SPtr ObjectEventDispatcher;
    static TSharedPtr<sfMissingObjectManager> MissingObjectManager;
    static bool IsSessionCreator;

    enum RestrictedFeature
    {
        NONE,
        ACTOR_LIMIT,
        LANDSCAPE,
        FOLIAGE,
        MODEL,
        MULTI_LEVEL
    };

    /**
     * Tick delegate.
     *
     * @param   float deltaTime in seconds since the last tick.
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(TickDelegate, float);

    /**
     * Invoked at the start of each tick before processing server RPCs.
     */
    TickDelegate PreTick;

    /**
     * Invoked every tick after processing server RPCs.
     */
    TickDelegate OnTick;

    /**
     * Constructor
     */
    SceneFusion();

    /**
     * Find the loaded Scene Fusion module
     *
     * @return  ISceneFusion&
     */
    static inline SceneFusion& Get()
    {
        return FModuleManager::LoadModuleChecked<SceneFusion>("SceneFusion");
    }

    /**
     * Check if the Scene Fusion module is loaded.
     *
     * @return  bool - true if the module is available
     */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("SceneFusion");
    }

    /**
     * @return  Scene Fusion version string
     */
    static inline FString Version()
    {
        return "2.2.1";
    }

    /**
     * Module entry point
     */
    void StartupModule();
    
    /**
     * Module cleanup
     */
    void ShutdownModule();

    /**
     * Initialize the webservice and associated console commands
     */
    void InitializeWebService();

    /**
     * Writes a log message to Unreal's log system.
     *
     * @param   KS::LogLevel level
     * @param   const char* channel that was logged to.
     * @param   const char* message
     */
    static void HandleLog(KS::LogLevel level, const char* channel, const char* message);

    /**
     * Flags the active viewport to be redrawn during the next update SceneFusion tick.
     */
    static void RedrawActiveViewport();

    /**
     * Gets the lock material for a user. Creates the material if it does not already exist.
     *
     * @param   sfUser::SPtr userPtr to get lock material for. May be nullptr.
     * @return  UMaterialInterface* lock material for the user.
     */
    static UMaterialInterface* GetLockMaterial(sfUser::SPtr userPtr);

    /**
     * Gets the landscape lock material for a user. Creates the material if it does not already exist.
     *
     * @param   sfUser::SPtr userPtr to get lock material for. May be nullptr.
     * @return  UMaterialInterface* lock material for the user.
     */
    static UMaterialInterface* GetLandscapeLockMaterial(sfUser::SPtr userPtr);

    /**
     * Checks if the frame time has exceeded the maximum time for creating objects.
     *
     * @return  bool true if the frame time has exceeded the maximum create time.
     */
    static bool CreateTimeExceeded();

    /**
     * Connects to a session.
     *
     * @param   TSharedPtr<sfSessionInfo> sessionInfoPtr - determines where to connect.
     */
    static void JoinSession(TSharedPtr<sfSessionInfo> sessionInfoPtr);

    /**
     * Called after connecting to a session.
     */
    void OnConnect();

    /**
     * Called after disconnecting from a session. Performs clean up.
     */
    void CleanUp();

    /**
     * Shows a link in the UI to upgrade the user's account.
     *
     * @param   RestrictedFeature feature the user tried to use that was restricted.
     */
    void ShowUpgradeLink(RestrictedFeature feature);

    /**
     * Gets the translator for the given type.
     *
     * @param   const sfName& type
     * @return  TSharedPtr<T> translator for the object, or nullptr if there is no translator for the given type.
     */
    template<typename T>
    TSharedPtr<T> GetTranslator(const sfName& type)
    {
        if (ObjectEventDispatcher == nullptr)
        {
            return nullptr;
        }
        return StaticCastSharedPtr<T>(ObjectEventDispatcher->GetTranslator(type));
    }

private:
    static IConsoleCommand* m_mockWebServiceCommand;
    static IConsoleCommand* m_bpSyncingCommand;
    static IConsoleCommand* m_landscapeSyncingCommand;
    static bool m_redrawActiveViewport;
    static TSharedPtr<sfUI> m_sfUIPtr;
    static TMap<uint32_t, UMaterialInstanceDynamic*> m_lockMaterials;
    static TMap<uint32_t, UMaterialInstanceDynamic*> m_landscapeLockMaterials;
    static UMaterialInterface* m_lockMaterialPtr;
    static ULandscapeMaterialInstanceConstant* m_landscapeLockMaterialPtr;
    static TArray<UObject*> m_replacedObjects;
    static ksEvent<sfUser::SPtr&>::SPtr m_onUserColorChangeEventPtr;
    static ksEvent<sfUser::SPtr&>::SPtr m_onUserLeaveEventPtr;
    static FDelegateHandle m_onObjectsReplacedHandle;
    static FDelegateHandle m_onHotReloadHandle;
    static int64_t m_frameStartTime;

    bool m_running;
    bool m_isFirstTick;
    FDelegateHandle m_updateHandle;

    /**
     * Updates the service
     *
     * @param   float deltaTime since the last tick
     * @return  bool true to keep the Tick function registered
     */
    bool Tick(float deltaTime);

    /**
     * Called when a user's color changes.
     *
     * @param   sfUser::SPtr userPtr
     */
    static void OnUserColorChange(sfUser::SPtr userPtr);

    /**
     * Called when a user disconnects.
     *
     * @param   sfUser::SPtr userPtr
     */
    static void OnUserLeave(sfUser::SPtr userPtr);

    /**
     * Called when Unreal deletes objects and replaces them with new ones, such as when rerunning the construction
     * script to recreate components in a blueprint whenever a property changes. If old objects were in the
     * sfObjectMap, replaces them with the new ones.
     *
     * @param   const TMap<UObject*, UObject*>& replacementMap mapping old objects to new objects.
     */
    void OnObjectsReplaced(const TMap<UObject*, UObject*>& replacementMap);

    /**
     * Called when a uobject is replaced. Updates maps to point to the new uobject. Recursively called on replaced
     * child uobjects.
     *
     * @param   UObject* oldPtr to be replaced.
     * @param   UObject* newPtr to replace with.
     * @return  bool true if the uobject was replaced in the sfObjectMap.
     */
    static bool Replace(UObject* oldPtr, UObject* newPtr);

    /**
     * Called after a hot reload. Syncs actors/components that were changed by the hot reload.
     *
     * @param   bool automatic - true if the hot reload was triggered automatically.
     */
    void OnHotReload(bool automatic);
};