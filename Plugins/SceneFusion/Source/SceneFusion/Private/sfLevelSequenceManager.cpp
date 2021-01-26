#include "sfLevelSequenceManager.h"
#include "../Public/sfPropertyManager.h"
#include "../Public/sfObjectMap.h"
#include "../Public/SceneFusion.h"
#include <Log.h>
#include <ISequencerModule.h>
#include <Modules/ModuleManager.h>
#include <Editor.h>
#include <EngineUtils.h>

sfLevelSequenceManager& sfLevelSequenceManager::Get()
{
    static sfLevelSequenceManager instance;
    return instance;
}

sfLevelSequenceManager::sfLevelSequenceManager()
{
    
}

void sfLevelSequenceManager::Initialize()
{
    GUObjectArray.AddUObjectDeleteListener((FUObjectArray::FUObjectDeleteListener*)&m_deleteListener);

    ISequencerModule& sequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
    sequencerModule.RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate::CreateLambda(
        [this](TSharedRef<ISequencer> sequencerPtr)
    {
        // Unreal doesn't allow multiple persistent TSharedRefs to a sequencer, so we use raw pointers.
        OnCreateSequencer(&sequencerPtr.Get());
    }));
}

void sfLevelSequenceManager::Update()
{
    // There is no way to get the controlled actors from a sequencer, but it is possible to check if an actor is
    // controlled by a sequencer. We iterate all actors to find out which ones are controlled by the sequencer and
    // store the list of controlled actors in a map for that sequencer.
    for (ISequencer* sequencerPtr : m_newSequencers)
    {
        UWorld* world = GEditor->GetEditorWorldContext().World();
        TArray<AActor*>* actorsPtr = nullptr;
        for (TActorIterator<AActor> iter(world); iter; ++iter)
        {
            // We check if an actor is controlled by a sequencer by seeing if the sequencer has a guid for it.
            FGuid guid = sequencerPtr->GetHandleToObject(*iter, false);
            if (guid.IsValid())
            {
                if (actorsPtr == nullptr)
                {
                    actorsPtr = &m_sequencedActors.FindOrAdd(sequencerPtr);
                }
                actorsPtr->Add(*iter);
            }
        }
    }
    m_newSequencers.Empty();

    for (ISequencer* sequencerPtr : m_toRestore)
    {
        RestoreActors(sequencerPtr);
    }
    m_toRestore.Empty();
}

void sfLevelSequenceManager::OnCreateSequencer(ISequencer* sequencerPtr)
{
    // Add the sequencer to the array of new sequencers to have its controlled actor list built next update. We cannot
    // do it now because the controlled actors don't have guids yet.
    m_newSequencers.Add(sequencerPtr);
    // When an actor is added to a sequencer, add it to the sequenced actors list for that sequencer.
    sequencerPtr->OnActorAddedToSequencer().AddLambda([this, sequencerPtr](AActor* actorPtr, const FGuid)
    {
        TArray<AActor*>& actors = m_sequencedActors.FindOrAdd(sequencerPtr);
        actors.Add(actorPtr);
    });
    // When a sequencer is closed, restore the controlled actors for that sequencer to the server state and remove the
    // sequencer and its controlled actors from the sequenced actor map.
    sequencerPtr->OnCloseEvent().AddLambda([this, sequencerPtr](TSharedRef<ISequencer> ref)
    {
        RestoreActors(sequencerPtr);
        m_sequencedActors.Remove(sequencerPtr);
    });
    // We cannot detect when an actor is removed from a sequencer, but we can detect when an actor might have been
    // removed from a sequencer by detecting when a track is removed. Since we don't know which actors were removed
    // (if any), we restore all controlled actors to the server state.
    sequencerPtr->OnMovieSceneDataChanged().AddLambda([this, sequencerPtr](EMovieSceneDataChangeType type)
    {
        if (type == EMovieSceneDataChangeType::MovieSceneStructureItemRemoved)
        {
            // Actors haven't been reverted yet, so we need to wait a frame before restoring actors.
            m_toRestore.Add(sequencerPtr);
        }
    });
}

void sfLevelSequenceManager::RestoreActors(ISequencer* sequencerPtr)
{
    if (SceneFusion::Get().Service->Session() == nullptr)
    {
        return;
    }
    TArray<AActor*>* actorsPtr = m_sequencedActors.Find(sequencerPtr);
    if (actorsPtr == nullptr)
    {
        return;
    }
    for (AActor* actorPtr : *actorsPtr)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(actorPtr);
        if (objPtr != nullptr)
        {
            Restore(objPtr, actorPtr);
        }
    }
}

void sfLevelSequenceManager::Restore(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    sfPropertyManager::Get().ApplyProperties(uobjPtr, objPtr->Property()->AsDict());
    for (sfObject::SPtr childPtr : objPtr->Children())
    {
        if (childPtr->Type() == sfType::Component)
        {
            UActorComponent* componentPtr = sfObjectMap::Get<UActorComponent>(childPtr);
            if (componentPtr != nullptr)
            {
                Restore(childPtr, componentPtr);
            }
        }
    }
}

void sfLevelSequenceManager::DeleteEventListener::NotifyUObjectDeleted(const class UObjectBase *uobjPtr, int index)
{
    const AActor* actorPtr = Cast<AActor>(static_cast<const UObject*>(uobjPtr));
    if (actorPtr == nullptr)
    {
        return;
    }
    for (auto iter : sfLevelSequenceManager::Get().m_sequencedActors)
    {
        iter.Value.Remove(const_cast<AActor*>(actorPtr));
    }
}

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
void sfLevelSequenceManager::DeleteEventListener::OnUObjectArrayShutdown()
{
    GUObjectArray.RemoveUObjectDeleteListener(this);
}
#endif