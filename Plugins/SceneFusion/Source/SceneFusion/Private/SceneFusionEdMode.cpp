#include "SceneFusionEdMode.h"
#include "../Public/sfObjectMap.h"
#include <Log.h>
#include <LevelEditor.h>
#include <Engine/Selection.h>

SceneFusionEdMode::SceneFusionEdMode() :
    m_nextId{ 1 }
{

}

SceneFusionEdMode::~SceneFusionEdMode()
{

}

bool SceneFusionEdMode::InputDelta(
    FEditorViewportClient* viewportClient,
    FViewport* viewport,
    FVector& locationDelta,
    FRotator& rotationDelta,
    FVector& scaleDelta)
{
    if (IsEditable() == EEditAction::Halt)
    {
        GEditor->bCheckForLockActors = false;
        GEditor->bHasLockedActors = true;
    }
    return false;
}

int SceneFusionEdMode::RegisterPermitter(Permitter permitter)
{
    m_permitters.emplace_back(std::pair<int, Permitter>{ m_nextId, permitter });
    return m_nextId++;
}

void SceneFusionEdMode::UnregisterPermitter(int id)
{
    for (auto iter = m_permitters.begin(); iter != m_permitters.end(); ++iter)
    {
        if (iter->first == id)
        {
            m_permitters.erase(iter);
            return;
        }
    }
}

bool SceneFusionEdMode::IsPermitted()
{
    for (auto iter = m_permitters.begin(); iter != m_permitters.end(); ++iter)
    {
        if (!iter->second())
        {
            return false;
        }
    }
    return true;
}

EEditAction::Type SceneFusionEdMode::IsEditable()
{
    int count = 0;
    for (auto iter = GEditor->GetSelectedActorIterator(); iter; ++iter)
    {
        AActor* actorPtr = Cast<AActor>(*iter);
        if (actorPtr == nullptr)
        {
            continue;
        }
        count++;
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
        if (objPtr == nullptr || objPtr->CanEdit())
        {
            return EEditAction::Skip;
        }
    }
    return count > 0 || !IsPermitted() ? EEditAction::Halt : EEditAction::Skip;
}

void SceneFusionEdMode::StopEvent()
{
    // Reinterpret to our hack class to expose the protected array of active editor modes.
    sfEditorModeToolsHack* hackPtr = static_cast<sfEditorModeToolsHack*>(&GLevelEditorModeTools());
    // Create the dummy editor mode that will replace all editor modes after this one in the active editor modes array,
    // so our dummy mode receives events instead of the real modes.
    TSharedPtr<sfDummyEdMode> dummyPtr = MakeShareable<sfDummyEdMode>(new sfDummyEdMode());
    for (int i = hackPtr->GetModes().Num() - 1; i >= 0; i--)
    {
        if (hackPtr->GetModes()[i].Get() == this)
        {
            break;
        }
        // Add the real mode to the dummy's modes array, so it can be restored when the dummy processes the event.
        dummyPtr->Modes.Add(hackPtr->GetModes()[i]);
        hackPtr->GetModes()[i] = dummyPtr;
    }
}

void sfDummyEdMode::RestoreMode()
{
    // This is called when the dummy editor mode receives an event. Now that we've blocked the real mode from receiving
    // the event, we replace the dummy with the real mode.
    if (Modes.Num() <= 0)
    {
        return;
    }
    // Reinterpret to our hack class to expose the protected array of active editor modes.
    sfEditorModeToolsHack* hackPtr = static_cast<sfEditorModeToolsHack*>(&GLevelEditorModeTools());
    // Calculate the index of the mode that is processing this event, based on the number of modes left in our Modes
    // array.
    int index = hackPtr->GetModes().Num() - Modes.Num();
    if (index >= 0)
    {
        // Replace the dummy mode with the real mode, and remove the real mode from our Modes array.
        hackPtr->GetModes()[index] = Modes.Pop();
    }
}