#include "sfAction.h"
#include "Log.h"
#include "../../Public/sfUnrealUtils.h"
#include "../../Public/SceneFusion.h"

#include <Editor.h>
#include <EditorLevelUtils.h>
#include <LevelUtils.h>
#include <Settings/LevelEditorMiscSettings.h>
#include <UObject/UObjectGlobals.h>
#include <EngineUtils.h>
#include <PropertyEditorModule.h>
#include <Widgets/Docking/SDockTab.h>

#define LOG_CHANNEL "sfAction"

sfAction::sfAction()
{
    Register("TestExample", [](const TArray<FString>& args)
    {
        for (FString str : args)
        {
            KS::Log::Debug(sfUnrealUtils::FToStdString(str));
        }
    });

    Register("LoadLevel", [](const TArray<FString>& args)
    {
        if (args.Num() == 1)
        {
            FString levelPath = args[0];
            UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
            if (worldPtr->PersistentLevel->GetOutermost()->GetName() == levelPath ||
                FLevelUtils::FindStreamingLevel(worldPtr, *levelPath) != nullptr)
            {
                KS::Log::Warning("Level " + sfUnrealUtils::FToStdString(levelPath));
                return;
            }
            else if (FPackageName::DoesPackageExist(levelPath))
            {
                UEditorLevelUtils::AddLevelToWorld(worldPtr,
                    *levelPath,
                    GetDefault<ULevelEditorMiscSettings>()->DefaultLevelStreamingClass);
                FEditorDelegates::RefreshLevelBrowser.Broadcast();// Refresh levels window
                SceneFusion::RedrawActiveViewport();//Redraw viewport
            }
            return;
        }
        KS::Log::Warning("Wrong arguments number. Expecting 1. Got " + std::to_string(args.Num()) + ".", LOG_CHANNEL);
    });

    Register("SelectTestActor", [](const TArray<FString>& args)
    {
        for (ULevel* levelPtr : GEditor->GetEditorWorldContext().World()->GetLevels())
        {
            for (AActor* actorPtr : levelPtr->Actors)
            {
                if (actorPtr != nullptr && actorPtr->GetName() == "Test")
                {
                    GEditor->SelectActor(actorPtr, true, true, true);
                    return;
                }
            }
        }
    });
}

sfAction::~sfAction()
{

}

bool sfAction::Register(FString actionName, Action action)
{
    if (m_actions.Contains(actionName))
    {
        KS::Log::Warning("An action with the name " + sfUnrealUtils::FToStdString(actionName) + " already exists.");
        return false;
    }
    m_actions.Add(actionName, action);
    return true;
}

bool sfAction::Unregister(FString actionName)
{
    return m_actions.Remove(actionName) != 0;
}

sfAction::Action sfAction::Get(FString actionName)
{
    if (m_actions.Contains(actionName))
    {
        return m_actions[actionName];
    }
    KS::Log::Warning("Could not find action " + sfUnrealUtils::FToStdString(actionName), LOG_CHANNEL);
    return nullptr;
}

#undef LOG_CHANNEL