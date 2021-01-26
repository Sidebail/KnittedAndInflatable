#include "../Public/SceneFusion.h"
#include "Web/sfWebService.h"
#include "Web/sfMockWebService.h"
#include "Web/sfBaseWebService.h"
#include "UI/sfUI.h"
#include "Testing/sfTestUtil.h"
#include "../Public/Consts.h"
#include "../Public/sfConfig.h"
#include "../Public/sfObjectMap.h"
#include "../Public/sfPropertyManager.h"
#include "../Public/sfPropertyUtil.h"
#include "../Public/sfSelectionManager.h"
#include "sfLevelSequenceManager.h"
#include "../Public/sfLoader.h"
#include "../Public/sfUnrealUtils.h"
#include "SceneFusionEdMode.h"
#include "../Public/Objects/sfReferenceTracker.h"
#include "../Public/Components/sfLockComponent.h"
#include "Translators/sfConfigTranslator.h"
#include "../Public/Translators/sfActorTranslator.h"
#include "Translators/sfAvatarTranslator.h"
#include "../Public/Translators/sfComponentTranslator.h"
#include "../Public/Translators/sfLevelTranslator.h"
#include "Translators/sfMeshStandInTranslator.h"
#include "Translators/sfUObjectTranslator.h"
#include "../Public/Translators/sfBlueprintTranslator.h"
#include "../Public/Translators/sfModelTranslator.h"
#include "Translators/sfLandscapeTranslator.h"
#include "Translators/sfLandscapeBrushTranslator.h"
#include "Translators/sfFoliageTranslator.h"
#include "Translators/sfAssetTranslator.h"
#include "Adapters/sfGroupActorAdapter.h"

#include <Developer/HotReload/Public/IHotReload.h>
#include <EditorModeManager.h>
#include <Materials/MaterialInstanceConstant.h>
#include <LandscapeMaterialInstanceConstant.h>

// Log setup
DEFINE_LOG_CATEGORY(LogSceneFusion)

#define LOG_CHANNEL "SceneFusion"

TSharedPtr<sfBaseWebService> SceneFusion::WebService = MakeShareable(new sfWebService());
sfService::SPtr SceneFusion::Service = nullptr;
IConsoleCommand* SceneFusion::m_mockWebServiceCommand = nullptr;
IConsoleCommand* SceneFusion::m_bpSyncingCommand = nullptr;
IConsoleCommand* SceneFusion::m_landscapeSyncingCommand = nullptr;
sfObjectEventDispatcher::SPtr SceneFusion::ObjectEventDispatcher = nullptr;
TSharedPtr<sfMissingObjectManager> SceneFusion::MissingObjectManager = nullptr;
TSharedPtr<sfUI> SceneFusion::m_sfUIPtr = nullptr;
ksEvent<sfUser::SPtr&>::SPtr SceneFusion::m_onUserColorChangeEventPtr = nullptr;
ksEvent<sfUser::SPtr&>::SPtr SceneFusion::m_onUserLeaveEventPtr = nullptr;
UMaterialInterface* SceneFusion::m_lockMaterialPtr = nullptr;
ULandscapeMaterialInstanceConstant* SceneFusion::m_landscapeLockMaterialPtr = nullptr;
FDelegateHandle SceneFusion::m_onObjectsReplacedHandle;
FDelegateHandle SceneFusion::m_onHotReloadHandle;
TMap<uint32_t, UMaterialInstanceDynamic*> SceneFusion::m_lockMaterials;
TMap<uint32_t, UMaterialInstanceDynamic*> SceneFusion::m_landscapeLockMaterials;
TArray<UObject*> SceneFusion::m_replacedObjects;
bool SceneFusion::IsSessionCreator = false;
bool SceneFusion::m_redrawActiveViewport = false;
int64_t SceneFusion::m_frameStartTime = 0;

SceneFusion::SceneFusion() :
    m_running{ false },
    m_isFirstTick{ true }
{

}

void SceneFusion::StartupModule()
{
    KS::Log::Instance()->RegisterHandler("Root", &HandleLog, KS::LogLevel::LOG_ALL & ~KS::LogLevel::LOG_DEBUG, true);
    KS::Log::Info("Scene Fusion Client: " + std::string(TCHAR_TO_UTF8(*Version())), LOG_CHANNEL);
    sfConfig::Get().Load();
    sfConfig& config = sfConfig::Get();
    if (config.LogDebug)
    {
        KS::Log::Instance()->RegisterHandler("Root", &HandleLog, KS::LogLevel::LOG_DEBUG, true);
    }
    // Check Scene Fusion version change
    if (config.LastSFVersion != Version())
    {
        config.LastSFVersion = Version();
        config.ShowGettingStartedScreen = true;
    }

    InitializeWebService();
    SceneFusion::WebService->RegisterInstall();

    // Register SceneFusion edit mode
    FEditorModeRegistry::Get().RegisterMode<SceneFusionEdMode>(
        "SceneFusion", FText::FromString("SceneFusion"), FSlateIcon(), false);

    m_lockMaterialPtr = LoadObject<UMaterialInterface>(nullptr, TEXT("/SceneFusion/LockMaterial"));

    Service = sfService::Create();
    ObjectEventDispatcher = sfObjectEventDispatcher::CreateSPtr();
    MissingObjectManager = MakeShareable(new sfMissingObjectManager);

    ObjectEventDispatcher->Register(sfType::Config, MakeShareable(new sfConfigTranslator));
    ObjectEventDispatcher->Register(sfType::Actor, MakeShareable(new sfActorTranslator));

    TSharedPtr<sfLevelTranslator> levelTranslatorPtr = MakeShareable(new sfLevelTranslator);
    ObjectEventDispatcher->Register(sfType::Level, levelTranslatorPtr);
    ObjectEventDispatcher->Register(sfType::LevelLock, levelTranslatorPtr);
    ObjectEventDispatcher->Register(sfType::LevelProperties, levelTranslatorPtr);
    ObjectEventDispatcher->Register(sfType::GameMode, levelTranslatorPtr);

    TSharedPtr<sfBlueprintTranslator> blueprintTranslatorPtr = MakeShareable(new sfBlueprintTranslator);
    ObjectEventDispatcher->Register(sfType::Blueprint, blueprintTranslatorPtr);
    ObjectEventDispatcher->Register(sfType::TemplateComponent, blueprintTranslatorPtr);

    TSharedPtr<sfAvatarTranslator> avatarTranslatorPtr = MakeShareable(new sfAvatarTranslator);
    ObjectEventDispatcher->Register(sfType::Avatar, avatarTranslatorPtr);

    ObjectEventDispatcher->Register(sfType::Component, MakeShareable(new sfComponentTranslator));
    ObjectEventDispatcher->Register(sfType::Model, MakeShareable(new sfModelTranslator));
    ObjectEventDispatcher->Register(sfType::Landscape, MakeShareable(new sfLandscapeTranslator));
    ObjectEventDispatcher->Register(sfType::LandscapeBrush, MakeShareable(new sfLandscapeBrushTranslator));
    ObjectEventDispatcher->Register(sfType::Foliage, MakeShareable(new sfFoliageTranslator));
    ObjectEventDispatcher->Register(sfType::Asset, MakeShareable(new sfAssetTranslator), true);
    ObjectEventDispatcher->Register(sfType::UObject, MakeShareable(new sfUObjectTranslator), true);

    TSharedPtr<sfMeshStandInTranslator> meshStandInTranslatorPtr = MakeShareable(new sfMeshStandInTranslator);
    ObjectEventDispatcher->Register(sfType::MeshBounds, meshStandInTranslatorPtr);
    sfLoader::Get().RegisterStandInGenerator(UStaticMesh::StaticClass(), meshStandInTranslatorPtr);

    // The level sequence manager is always running even when not in a session because we have no way of getting all
    // open sequencers (which we would need to do when starting a session), so we need to always be listening for
    // sequencer create and close events.
    sfLevelSequenceManager::Get().Initialize();

    sfGroupActorAdapter::Initialize();

    if (FSlateApplication::IsInitialized())
    {
        m_sfUIPtr = MakeShareable(new sfUI);
        m_sfUIPtr->Initialize();
        m_sfUIPtr->OnGoToUser().BindRaw(avatarTranslatorPtr.Get(), &sfAvatarTranslator::MoveViewportToUser);
        m_sfUIPtr->OnFollowUser().BindRaw(avatarTranslatorPtr.Get(), &sfAvatarTranslator::Follow);
        avatarTranslatorPtr->OnUnfollow.BindRaw(m_sfUIPtr.Get(), &sfUI::UnfollowCamera);
    }

    sfTestUtil::RegisterCommands();

    // Register an FTickerDelegate to be called 60 times per second.
    m_updateHandle = FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &SceneFusion::Tick), 1.0f / 60.0f);
}

void SceneFusion::ShutdownModule()
{
    KS::Log::Info("Scene Fusion shut down module.", LOG_CHANNEL);
    Service->LeaveSession();
    if (m_sfUIPtr.IsValid())
    {
        m_sfUIPtr->Cleanup();
        m_sfUIPtr.Reset();
    }
    sfTestUtil::CleanUp();
    IConsoleManager::Get().UnregisterConsoleObject(m_mockWebServiceCommand);
    IConsoleManager::Get().UnregisterConsoleObject(m_bpSyncingCommand);
    IConsoleManager::Get().UnregisterConsoleObject(m_landscapeSyncingCommand);
    FTicker::GetCoreTicker().RemoveTicker(m_updateHandle);
}

void SceneFusion::OnConnect()
{
    if (m_running)
    {
        return;
    }
    m_running = true;
    // Activate SceneFusion edit mode to prevent locked objects being deleted.
    // Also prevent locked components being duplicated or pasted.
    GLevelEditorModeTools().ActivateMode("SceneFusion", false);
    // Reinterpret to our hack class to expose the protected array of active editor modes.
    sfEditorModeToolsHack* hackPtr = static_cast<sfEditorModeToolsHack*>(&GLevelEditorModeTools());
    // Move our editor mode to the first position in the modes array so it processes events first
    if (hackPtr->GetModes().Num() > 1)
    {
        TSharedPtr<FEdMode> modePtr = hackPtr->GetModes().Pop();
        hackPtr->GetModes().Insert(modePtr, 0);

        for (TSharedPtr<FEdMode> iter : hackPtr->GetModes())
        {
            KS::Log::Info("mode: " + std::string(TCHAR_TO_UTF8(*iter->GetID().ToString())));
        }
    }

    ObjectEventDispatcher->Initialize();
    MissingObjectManager->Initialize();
    sfUndoManager::Get().Initialize();
    sfPropertyManager::Get().EnablePropertyChangeHandler();
    sfSelectionManager::Get().Start();
    sfObjectMap::EnableDeleteListener();
    sfLoader::Get().Start();
    m_onUserColorChangeEventPtr = Service->Session()->RegisterOnUserColorChangeHandler(&OnUserColorChange);
    m_onUserLeaveEventPtr = Service->Session()->RegisterOnUserLeaveHandler(&OnUserLeave);
    m_onObjectsReplacedHandle = GEditor->OnObjectsReplaced().AddRaw(this, &SceneFusion::OnObjectsReplaced);
    m_onHotReloadHandle = IHotReloadModule::Get().OnHotReload().AddRaw(this, &SceneFusion::OnHotReload);
}

void SceneFusion::CleanUp()
{
    if (!m_running)
    {
        return;
    }
    m_running = false;
    GLevelEditorModeTools().DeactivateMode("SceneFusion");
    for (auto iter : m_lockMaterials)
    {
        iter.Value->ClearFlags(RF_Standalone);// Allow unreal to destroy the material instances
    }
    m_lockMaterials.Empty();
    for (auto iter : m_landscapeLockMaterials)
    {
        iter.Value->ClearFlags(RF_Standalone);
    }
    m_landscapeLockMaterials.Empty();
    m_onUserColorChangeEventPtr.reset();
    m_onUserLeaveEventPtr.reset();
    GEditor->OnObjectsReplaced().Remove(m_onObjectsReplacedHandle);
    IHotReloadModule::Get().OnHotReload().Remove(m_onHotReloadHandle);
    ObjectEventDispatcher->CleanUp();
    MissingObjectManager->CleanUp();
    sfUndoManager::Get().CleanUp();
    sfPropertyManager::Get().CleanUp();
    sfPropertyManager::Get().DisablePropertyChangeHandler();
    sfSelectionManager::Get().Stop();
    sfObjectMap::DisableDeleteListener();
    sfObjectMap::Clear();
    sfLoader::Get().Stop();
    UsfReferenceTracker::CleanUp();
    UsfLockComponent::DestroyModelMeshes();
}

void SceneFusion::ShowUpgradeLink(RestrictedFeature feature)
{
    m_sfUIPtr->ShowUpgradeLink(feature);
}

bool SceneFusion::Tick(float deltaTime)
{
    m_frameStartTime = FDateTime().Now().GetTicks();
    if (Service->Session() != nullptr && Service->Session()->IsConnected())
    {
        PreTick.Broadcast(deltaTime);
        ObjectEventDispatcher->ProcessCreateQueue();
    }
    Service->Update(deltaTime);
    sfLevelSequenceManager::Get().Update();
    m_replacedObjects.Empty();
    if (Service->Session() != nullptr && Service->Session()->IsConnected())
    {
        GLevelEditorModeTools().ActivateMode("SceneFusion", false);
        sfPropertyManager::Get().RehashProperties();// rehash to make sure state is valid before broadcasting events
        sfPropertyManager::Get().BroadcastChangeEvents();
        sfPropertyManager::Get().SyncProperties();
        sfPropertyManager::Get().RehashProperties();// rehash in case properties were reverted on locked objects
        sfSelectionManager::Get().Update();
        OnTick.Broadcast(deltaTime);
    }

    // Redraw the active viewport
    if (m_redrawActiveViewport)
    {
        m_redrawActiveViewport = false;
        FViewport* viewport = GEditor->GetActiveViewport();
        if (viewport != nullptr)
        {
            viewport->Draw();
        }
    }

    // We need to empty this or we may miss some modify events before the end of the Unreal frame.
    FCoreUObjectDelegates::ObjectsModifiedThisFrame.Empty();

    if (m_isFirstTick)
    {
        m_isFirstTick = false;

        // Show Scene Fusion Getting Started tab.
        // If we do this in the StartupModule function, this tab will be behind the main window.
        if (sfConfig::Get().ShowGettingStartedScreen)
        {
            FGlobalTabmanager::Get()->InvokeTab(FName("SF Getting Started"));
        }
    }
    return true;
}

void SceneFusion::HandleLog(KS::LogLevel level, const char* channel, const char* message)
{
    std::string str = "[" + KS::Log::GetLevelString(level) + ";" + channel + "] " + message;
    FString fstr(UTF8_TO_TCHAR(str.c_str()));
    switch (level)
    {
        case KS::LogLevel::LOG_DEBUG:
        {
            UE_LOG(LogSceneFusion, Log, TEXT("%s"), *fstr);
            break;
        }
        case KS::LogLevel::LOG_INFO:
        {
            UE_LOG(LogSceneFusion, Log, TEXT("%s"), *fstr);
            break;
        }
        case KS::LogLevel::LOG_WARNING:
        {
            UE_LOG(LogSceneFusion, Warning, TEXT("%s"), *fstr);
            m_sfUIPtr->HandleLog(level, channel, message);
            break;
        }
        case KS::LogLevel::LOG_ERROR:
        {
            UE_LOG(LogSceneFusion, Error, TEXT("%s"), *fstr);
            m_sfUIPtr->HandleLog(level, channel, message);
            break;
        }
        case KS::LogLevel::LOG_FATAL:
        {
            UE_LOG(LogSceneFusion, Fatal, TEXT("%s"), *fstr);
            break;
        }
    }
}

void SceneFusion::InitializeWebService()
{
    // Load Mock Web Service configs
    sfConfig& config = sfConfig::Get();
    if (!config.MockWebServerAddress.IsEmpty() && !config.MockWebServerPort.IsEmpty()) {
        KS::Log::Info("Mock Web Service enabled: " +
            std::string(TCHAR_TO_UTF8(*config.MockWebServerAddress)) + " " +
            std::string(TCHAR_TO_UTF8(*config.MockWebServerPort)), LOG_CHANNEL);
        WebService = MakeShareable(new sfMockWebService(config.MockWebServerAddress, config.MockWebServerPort));
    }

    // Register Mock Web Service Command
    m_mockWebServiceCommand = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("SFMockWebService"),
        TEXT("Usage: SFMockWebService [host port]. If a host or port are ommitted then the mock web service will be disabled."),
        FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& args) {
        sfConfig& config = sfConfig::Get();
        if (args.Num() == 2) {
            KS::Log::Info("Mock Web Service enabled: " +
                std::string(TCHAR_TO_UTF8(*args[0])) + " " +
                std::string(TCHAR_TO_UTF8(*args[1])), LOG_CHANNEL);
            WebService = MakeShareable(new sfMockWebService(args[0], args[1]));
            config.MockWebServerAddress = args[0];
            config.MockWebServerPort = args[1];
        }
        else {
            KS::Log::Info("Mock Web Service disabled", LOG_CHANNEL);
            WebService = MakeShareable(new sfWebService());
            config.MockWebServerAddress.Empty();
            config.MockWebServerPort.Empty();
        }
        config.Save();
    }));

    // Register Blueprint Syncing Command
    m_bpSyncingCommand = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("SFBlueprintSyncing"),
        TEXT("Usage: SFBlueprintSyncing [on | off | true | false | 1 | 0]. Toggle blueprint syncing."),
        FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& args) {
        if (SceneFusion::Get().Service->Session() != nullptr)
        {
            KS::Log::Warning("Cannot toggle blueprint syncing while you are in a session", LOG_CHANNEL);
            return;
        }
        sfConfig& config = sfConfig::Get();
        bool syncBlueprint = !config.SyncBlueprint;
        if (args.Num() > 0)
        {
            const FString& arg = args[0];
            if (sfUnrealUtils::IsStringFalse(arg))
            {
                syncBlueprint = false;
            }
            else if (sfUnrealUtils::IsStringTrue(arg))
            {
                syncBlueprint = true;
            }
        }
        if (syncBlueprint != config.SyncBlueprint)
        {
            config.SyncBlueprint = syncBlueprint;
            config.Save();
        }
        KS::Log::Info(std::string("Blueprint syncing ") + (syncBlueprint ? "enabled" : "disabled"), LOG_CHANNEL);
    }));

    // Register Landscape Syncing Command
    m_landscapeSyncingCommand = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("SFLandscapeSyncing"),
        TEXT("Usage: SFLandscapeSyncing [on | off | true | false | 1 | 0]. Toggle landscape syncing."),
        FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& args) {
        if (SceneFusion::Get().Service->Session() != nullptr)
        {
            KS::Log::Warning("Cannot toggle landscape syncing while you are in a session", LOG_CHANNEL);
            return;
        }
        sfConfig& config = sfConfig::Get();
        bool syncLandscape = !config.SyncLandscape;
        if (args.Num() > 0)
        {
            const FString& arg = args[0];
            if (sfUnrealUtils::IsStringFalse(arg))
            {
                syncLandscape = false;
            }
            else if (sfUnrealUtils::IsStringTrue(arg))
            {
                syncLandscape = true;
            }
        }
        if (syncLandscape != config.SyncLandscape)
        {
            config.SyncLandscape = syncLandscape;
            config.Save();
        }
        KS::Log::Info(std::string("Landscape syncing ") + (syncLandscape ? "enabled" : "disabled"), LOG_CHANNEL);
    }));
}

void SceneFusion::RedrawActiveViewport()
{
    m_redrawActiveViewport = true;
}

bool SceneFusion::CreateTimeExceeded()
{
    return FTimespan(FDateTime().Now().GetTicks() - m_frameStartTime).GetTotalMilliseconds() > 
        sfConfig::Get().MaxCreateTimeMS;
}

UMaterialInterface* SceneFusion::GetLockMaterial(sfUser::SPtr userPtr)
{
    if (userPtr == nullptr || m_lockMaterialPtr == nullptr)
    {
        return m_lockMaterialPtr;
    }
    UMaterialInstanceDynamic* materialPtr = m_lockMaterials.FindRef(userPtr->Id());
    if (materialPtr != nullptr)
    {
        return materialPtr;
    }
    materialPtr = UMaterialInstanceDynamic::Create(m_lockMaterialPtr, GEditor->GetEditorWorldContext().World());
    materialPtr->SetFlags(RF_Standalone | RF_Transient); // prevent material from being destroyed or saved
    ksColor color = userPtr->Color();
    FLinearColor ucolor(color.R(), color.G(), color.B());
    materialPtr->SetVectorParameterValue("Color", ucolor);
    m_lockMaterials.Add(userPtr->Id(), materialPtr);
    return materialPtr;
}

UMaterialInterface* SceneFusion::GetLandscapeLockMaterial(sfUser::SPtr userPtr)
{
    if (m_lockMaterialPtr == nullptr)
    {
        return nullptr;
    }
    if (m_landscapeLockMaterialPtr == nullptr)
    {
        m_landscapeLockMaterialPtr = NewObject<ULandscapeMaterialInstanceConstant>(GetTransientPackage());
        m_landscapeLockMaterialPtr->bEditorToolUsage = true;
        m_landscapeLockMaterialPtr->SetParentEditorOnly(m_lockMaterialPtr);
        m_landscapeLockMaterialPtr->PostEditChange();
        m_landscapeLockMaterialPtr->SetFlags(RF_Standalone);
    }
    if (userPtr == nullptr)
    {
        return m_landscapeLockMaterialPtr;
    }
    UMaterialInstanceDynamic* materialPtr = m_landscapeLockMaterials.FindRef(userPtr->Id());
    if (materialPtr != nullptr)
    {
        return materialPtr;
    }
    materialPtr = UMaterialInstanceDynamic::Create(m_landscapeLockMaterialPtr, nullptr);
    materialPtr->SetFlags(RF_Standalone | RF_Transient); // prevent material from being destroyed or saved
    ksColor color = userPtr->Color();
    FLinearColor ucolor(color.R(), color.G(), color.B());
    materialPtr->SetVectorParameterValue("Color", ucolor);
    m_landscapeLockMaterials.Add(userPtr->Id(), materialPtr);
    return materialPtr;
}

void SceneFusion::OnObjectsReplaced(const TMap<UObject*, UObject*>& replacementMap)
{
    TSet<AActor*> actorPtrs;
    for (auto iter : replacementMap)
    {
        if (Replace(iter.Key, iter.Value))
        {
            // Record the affected actors
            UActorComponent* componentPtr = Cast<UActorComponent>(iter.Value);
            if (componentPtr != nullptr)
            {
                AActor* actorPtr = componentPtr->GetOwner();
                if (actorPtr != nullptr)
                {
                    actorPtrs.Add(actorPtr);
                }
            }
        }
    }
    
    // Relock the affected actors
    for (auto actorPtr : actorPtrs)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
        if (objPtr != nullptr && objPtr->IsLocked())
        {
            TSharedPtr<sfActorTranslator> actorTranslatorPtr = GetTranslator<sfActorTranslator>(sfType::Actor);
            actorTranslatorPtr->Unlock(actorPtr);
            actorTranslatorPtr->Lock(actorPtr, objPtr);
        }
    }
}

bool SceneFusion::Replace(UObject* oldPtr, UObject* newPtr)
{
    // This fixes a complicated bug with blueprint syncing. For a description of the bug see #6440
    if (oldPtr->GetOutermost() != GetTransientPackage() && 
        !oldPtr->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists) && !oldPtr->IsPendingKill())
    {
        return false;
    }

    sfObject::SPtr objPtr = sfObjectMap::Remove(oldPtr);
    if (objPtr == nullptr || newPtr == nullptr)
    {
        return false;
    }
    sfObjectMap::Add(objPtr, newPtr);
    m_replacedObjects.Add(newPtr);
    sfPropertyManager::Get().ReplaceUObject(oldPtr, newPtr);

    FString str = "Replace " + newPtr->GetClass()->GetName() + " " + newPtr->GetName();
    KS::Log::Debug(TCHAR_TO_UTF8(*str), LOG_CHANNEL);

    for (sfObject::SPtr childPtr : objPtr->Children())
    {
        if (childPtr->Type() != sfType::UObject)
        {
            continue;
        }
        FString name = sfPropertyUtil::ToString(childPtr->Property()->AsDict()->Get(sfProp::Name));
        UObject* innerPtr = StaticFindObjectFast(UObject::StaticClass(), newPtr, FName(*name));
        if (innerPtr != nullptr && innerPtr != sfObjectMap::GetUObject(childPtr))
        {
            oldPtr = sfObjectMap::GetUObject(childPtr);
            Replace(oldPtr, innerPtr);
        }
    }

    if (GIsTransacting)
    {
        // Objects replaced by transactions aren't counted as part of the transaction, so we need to trigger
        // an OnUndoRedo event for them.
        ObjectEventDispatcher->OnUndoRedo(objPtr, newPtr);
    }
    return true;
}

void SceneFusion::OnHotReload(bool automatic)
{
    for (UObject* uobjPtr : m_replacedObjects)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(uobjPtr);
        if (objPtr != nullptr)
        {
            // Apply server values for new properties
            sfPropertyManager::Get().ApplyProperties(uobjPtr, objPtr->Property()->AsDict());
            AActor* actorPtr = Cast<AActor>(uobjPtr);
            if (actorPtr != nullptr)
            {
                // Components may have been added or removed to so we sync components
                TSharedPtr<sfComponentTranslator> componentTranslatorPtr = GetTranslator<sfComponentTranslator>(
                    sfType::Component);
                componentTranslatorPtr->SyncComponents(actorPtr);
            }
        }
    }
    m_replacedObjects.Empty();
}

void SceneFusion::OnUserColorChange(sfUser::SPtr userPtr)
{
    UMaterialInstanceDynamic* materialPtr = m_lockMaterials.FindRef(userPtr->Id());
    if (materialPtr != nullptr)
    {
        ksColor color = userPtr->Color();
        FLinearColor ucolor(color.R(), color.G(), color.B());
        materialPtr->SetVectorParameterValue("Color", ucolor);
    }
    
    materialPtr = m_landscapeLockMaterials.FindRef(userPtr->Id());
    if (materialPtr != nullptr)
    {
        ksColor color = userPtr->Color();
        FLinearColor ucolor(color.R(), color.G(), color.B());
        materialPtr->SetVectorParameterValue("Color", ucolor);
    }
}

void SceneFusion::OnUserLeave(sfUser::SPtr userPtr)
{
    UMaterialInstanceDynamic* materialPtr;
    if (m_lockMaterials.RemoveAndCopyValue(userPtr->Id(), materialPtr))
    {
        materialPtr->ClearFlags(RF_Standalone);// Allow unreal to destroy the material instance
    }
    if (m_landscapeLockMaterials.RemoveAndCopyValue(userPtr->Id(), materialPtr))
    {
        materialPtr->ClearFlags(RF_Standalone);
    }
}

void SceneFusion::JoinSession(TSharedPtr<sfSessionInfo> sessionInfoPtr)
{
    m_sfUIPtr->JoinSession(sessionInfoPtr);
}

// Module loading
IMPLEMENT_MODULE(SceneFusion, SceneFusion)

#undef LOG_CHANNEL
