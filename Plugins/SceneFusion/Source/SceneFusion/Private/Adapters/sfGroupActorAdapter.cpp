#include "sfGroupActorAdapter.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/Translators/sfActorTranslator.h"

TSet<AGroupActor*> sfGroupActorAdapter::m_modifiedGroups;

void sfGroupActorAdapter::Initialize()
{
    // Force properties to sync even though they are not editable in the inspector.
    sfPropertyManager::Get().AddPropertyToForceSyncList("GroupActor", "bLocked");
    sfPropertyManager::Get().AddPropertyToForceSyncList("GroupActor", "GroupActors");
    sfPropertyManager::Get().AddPropertyToForceSyncList("GroupActor", "SubGroups");

    // Register tick function that is called every tick while in a SF session.
    SceneFusion::Get().OnTick.AddStatic(&sfGroupActorAdapter::Tick);

    // Get the actor translator
    TSharedPtr<sfActorTranslator> translatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(sfType::Actor);
    // Register callback to call when AGroupActor::GroupActors is modified by SF
    translatorPtr->RegisterPostPropertyChangeHandler<AGroupActor>("GroupActors",
        [](UObject* uobjPtr)
    {
        AGroupActor* groupPtr = Cast<AGroupActor>(uobjPtr);
        if (groupPtr == nullptr)
        {
            return;
        }
        // Set the GroupActor for all actors in the group
        for (AActor* actorPtr : groupPtr->GroupActors)
        {
            if (actorPtr != nullptr)
            {
                actorPtr->GroupActor = groupPtr;
            }
        }
    });
    // Register callback to call before elements are removed from AGroupActor::GroupActors by SF
    translatorPtr->RegisterRemoveElementsHandler<AGroupActor>("GroupActors",
        [](UObject* uobjPtr, int index, int count)
    {
        AGroupActor* groupPtr = Cast<AGroupActor>(uobjPtr);
        if (groupPtr == nullptr)
        {
            return;
        }
        int end = FMath::Min(index + count, groupPtr->GroupActors.Num());
        // Set the GroupActor to null for actors removed from the group
        for (int i = index; i < end; i++)
        {
            AActor* actorPtr = groupPtr->GroupActors[i];
            if (actorPtr != nullptr && actorPtr->GroupActor == groupPtr)
            {
                groupPtr->GroupActors[i]->GroupActor = nullptr;
            }
        }
    });
    // Register callback to call when GroupActors are modified by Unreal.
    translatorPtr->RegisterOnModifyHandler<AGroupActor>(
        [](sfObject::SPtr objPtr, UObject* uobjPtr)
    {
        AGroupActor* groupPtr = Cast<AGroupActor>(uobjPtr);
        if (groupPtr != nullptr)
        {
            // The modify event fires before the properties are changed, so we store the actor to check for property
            // changes on the next tick.
            m_modifiedGroups.Add(groupPtr);
        }
    });
}

void sfGroupActorAdapter::Tick(float deltaTime)
{
    // Sync changes to modified groups
    for (AGroupActor* groupPtr : m_modifiedGroups)
    {
        // Get the sfObject for the GroupActor from the sfObjectMap
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(groupPtr);
        if (objPtr == nullptr || !objPtr->IsSyncing())
        {
            continue;
        }
        sfPropertyManager::Get().SyncProperty(objPtr, groupPtr, "bLocked");
        sfPropertyManager::Get().SyncProperty(objPtr, groupPtr, "GroupActors");
        sfPropertyManager::Get().SyncProperty(objPtr, groupPtr, "SubGroups");
    }
    m_modifiedGroups.Empty();
}