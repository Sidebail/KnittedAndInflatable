#include "sfOutlinerManager.h"

#include <Editor.h>
#include <Editor/SceneOutliner/Public/SceneOutlinerModule.h>
#include <Editor/LevelEditor/Public/LevelEditor.h>
#include <Widgets/Docking/SDockTab.h>

#include "sfUIStyles.h"
#include "sfLockColumn.h"

#define SCENE_OUTLINER_MODULE "SceneOutliner"
#define LEVEL_EDITOR "LevelEditor"
#define WORLD_OUTLINER "LevelEditorSceneOutliner"

sfOutlinerManager::sfOutlinerManager() :
    m_tabManager{ nullptr }
{

}

sfOutlinerManager::~sfOutlinerManager()
{

}

void sfOutlinerManager::Initialize()
{
    m_onActorDeletedHandle = GEngine->OnLevelActorDeleted().AddRaw(this, &sfOutlinerManager::OnActorDeleted);
    m_onObjectsReplacedHandle = GEditor->OnObjectsReplaced().AddRaw(this, &sfOutlinerManager::OnObjectsReplaced);
    if (!m_tabManager.IsValid())
    {
        FLevelEditorModule& levelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(LEVEL_EDITOR);
        m_tabManager = levelEditorModule.GetLevelEditorTabManager();
    }

    // Register scene fusion lock icon column
    FSceneOutlinerModule& sceneOutlinerModule
        = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>(SCENE_OUTLINER_MODULE);
    SceneOutliner::FColumnInfo columnInfo(SceneOutliner::EColumnVisibility::Visible, 15,
        FCreateSceneOutlinerColumn::CreateRaw(this, &sfOutlinerManager::CreateLockColumn));
    sceneOutlinerModule.RegisterDefaultColumnType<FsfLockColumn>(SceneOutliner::FDefaultColumnInfo(columnInfo));
    //Reconstruct the world outliner tab to show our column.
    ReconstructWorldOutliner();
}

void sfOutlinerManager::CleanUp()
{
    // Unregister scene fusion lock icon column
    FSceneOutlinerModule& SceneOutlinerModule
        = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>(SCENE_OUTLINER_MODULE);
    SceneOutlinerModule.UnRegisterColumnType<FsfLockColumn>();

    //Reconstruct the world outliner tab to remove our column.
    ReconstructWorldOutliner();

    GEngine->OnLevelActorDeleted().Remove(m_onActorDeletedHandle);
    GEditor->OnObjectsReplaced().Remove(m_onObjectsReplacedHandle);
    m_actorLockInfos.Empty();
}

TSharedRef<ISceneOutlinerColumn> sfOutlinerManager::CreateLockColumn(ISceneOutliner& SceneOutliner)
{
    TSharedRef<sfOutlinerManager> outlinerManagerPtr = AsShared();
    return MakeShareable(new FsfLockColumn(outlinerManagerPtr));
}

void sfOutlinerManager::ReconstructWorldOutliner()
{
    if (m_tabManager.IsValid())
    {
        TSharedPtr<SDockTab> worldOutlinerTab = m_tabManager->FindExistingLiveTab(FName(WORLD_OUTLINER));
        if (worldOutlinerTab.IsValid())
        {
            worldOutlinerTab->RequestCloseTab();
            FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float delta) {
                if (m_tabManager.IsValid())
                {
                    m_tabManager->InvokeTab(FName(WORLD_OUTLINER));
                }
                return false;
            }));
        }
    }
}

void sfOutlinerManager::SetLockState(AActor* actorPtr, sfActorTranslator::LockType lockType, sfUser::SPtr lockOwner)
{
    TSharedPtr<sfLockInfo> lockInfoPtr = FindOrAddLockInfo(actorPtr);
    lockInfoPtr->LockType = lockType;
    lockInfoPtr->LockOwner = lockOwner;
}

void sfOutlinerManager::OnActorDeleted(AActor* actorPtr)
{
    m_actorLockInfos.Remove(actorPtr);
}

void sfOutlinerManager::OnObjectsReplaced(const TMap<UObject*, UObject*>& replacementMap)
{
    for (auto iter : replacementMap)
    {
        AActor* oldActorPtr = Cast<AActor>(iter.Key);
        if (oldActorPtr == nullptr)
        {
            continue;
        }
        TSharedPtr<sfLockInfo> lockInfoPtr;
        if (m_actorLockInfos.RemoveAndCopyValue(oldActorPtr, lockInfoPtr))
        {
            m_actorLockInfos.Add(Cast<AActor>(iter.Value), lockInfoPtr);
        }
    }
}

const TSharedRef<SWidget> sfOutlinerManager::ConstructRowWidget(AActor* actorPtr)
{
    return StaticCastSharedPtr<SWidget>(FindOrAddLockInfo(actorPtr)->Icon).ToSharedRef();
}

TSharedPtr<sfLockInfo> sfOutlinerManager::FindOrAddLockInfo(AActor* actorPtr)
{
    TSharedPtr<sfLockInfo> lockInfoPtr = m_actorLockInfos.FindRef(actorPtr);
    if (!lockInfoPtr.IsValid())
    {
        lockInfoPtr = MakeShareable(new sfLockInfo);
        m_actorLockInfos.Add(actorPtr, lockInfoPtr);
    }
    return lockInfoPtr;
}

#undef SCENE_OUTLINER_MODULE
#undef LEVEL_EDITOR
#undef WORLD_OUTLINER