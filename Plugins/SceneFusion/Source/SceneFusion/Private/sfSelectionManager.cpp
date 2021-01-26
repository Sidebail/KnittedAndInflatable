#include "../Public/sfSelectionManager.h"
#include "../Public/sfPropertyManager.h"
#include "../Public/sfLoader.h"
#include "UI/sfDetailsPanelManager.h"
#include <Editor.h>
#include <LevelEditorViewport.h>

sfSelectionManager::sfSelectionManager() :
    m_isMovingActors{ false },
    m_isMovingBrush{ false }
{

}

sfSelectionManager& sfSelectionManager::Get()
{
    static sfSelectionManager m_selectionManager;
    return m_selectionManager;
}

const TSet<AActor*>& sfSelectionManager::SelectedActors()
{
    return m_selectedActors;
}

bool sfSelectionManager::MovingActors()
{
    return m_isMovingActors;
}

bool sfSelectionManager::MovingBrush()
{
    return m_isMovingBrush;
}

void sfSelectionManager::Start()
{
    m_onMoveStartHandle = GEditor->OnBeginObjectMovement().AddRaw(this, &sfSelectionManager::OnMoveStart);
    m_onMoveEndHandle = GEditor->OnEndObjectMovement().AddRaw(this, &sfSelectionManager::OnMoveEnd);
    m_onActorMovedHandle = GEditor->OnActorMoved().AddRaw(this, &sfSelectionManager::OnActorMoved);
}

void sfSelectionManager::Stop()
{
    GEditor->OnBeginObjectMovement().Remove(m_onMoveStartHandle);
    GEditor->OnEndObjectMovement().Remove(m_onMoveEndHandle);
    GEditor->OnActorMoved().Remove(m_onActorMovedHandle);
    m_selectedActors.Empty();
    m_movedActors.Empty();
    m_isMovingActors = false;
    m_isMovingBrush = false;
}

void sfSelectionManager::Clear()
{
    m_selectedActors.Empty();
    m_movedActors.Empty();
}

void sfSelectionManager::Update()
{
    TSet<AActor*> selectedActors;
    sfDetailsPanelManager::Get().GetSelectedActors(selectedActors);
    // Unreal doesn't have deselect events and doesn't fire select events when selecting through the World Outliner so
    // we have to iterate the selection to check for changes
    for (auto iter = m_selectedActors.CreateIterator(); iter; ++iter)
    {
        if (m_isMovingActors)
        {
            OnMove.Broadcast(*iter);
            m_movedActors.Remove(*iter);
        }
        if (!selectedActors.Contains(*iter))
        {
            OnDeselect.Broadcast(*iter);
            iter.RemoveCurrent();
        }
    }

    for (AActor* actorPtr : selectedActors)
    {
        if (!m_selectedActors.Contains(actorPtr))
        {
            m_selectedActors.Add(actorPtr);
            OnSelect.Broadcast(actorPtr);
        }
    }

    for (AActor* actorPtr : m_movedActors)
    {
        OnMove.Broadcast(actorPtr);
    }
    m_movedActors.Empty();
}

void sfSelectionManager::RemoveActor(AActor* actorPtr)
{
    m_movedActors.Remove(actorPtr);
    m_selectedActors.Remove(actorPtr);
    if (m_selectedActors.Num() == 0)
    {
        m_isMovingActors = false;
        m_isMovingBrush = false;
    }
}

void sfSelectionManager::OnMoveStart(UObject& uobj)
{
    m_isMovingActors = GCurrentLevelEditingViewportClient &&
        GCurrentLevelEditingViewportClient->bWidgetAxisControlledByDrag;
    m_isMovingBrush = m_isMovingActors && uobj.IsA<ABrush>();
}

void sfSelectionManager::OnMoveEnd(UObject& uobj)
{
    if (m_isMovingActors)
    {
        m_isMovingActors = false;
        m_isMovingBrush = false;
        for (AActor* actorPtr : m_selectedActors)
        {
            OnMove.Broadcast(actorPtr);
        }
    }
}

void sfSelectionManager::OnActorMoved(AActor* actorPtr)
{
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
    if (sfPropertyManager::Get().ListeningForPropertyChanges() &&
        actorPtr->GetWorld() == GEditor->GetEditorWorldContext().World() &&
        objPtr != nullptr && objPtr->IsSyncing())
    {
        m_movedActors.Add(actorPtr);
    }
}