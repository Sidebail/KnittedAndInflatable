#include "../../Public/Actors/sfMissingActor.h"
#include "../../Public/sfUnrealUtils.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/Consts.h"
#include "../../Public/Translators/sfActorTranslator.h"
#include "../../Public/SceneFusion.h"
#include <Developer/HotReload/Public/IHotReload.h>

FString& AsfMissingActor::MissingClass()
{
    return ClassName;
}

void AsfMissingActor::Reload()
{
    sfObject::SPtr objPtr = sfObjectMap::Remove(this);
    if (objPtr == nullptr)
    {
        return;
    }
    // Remove child component objects from the sfObjectMap
    objPtr->ForEachDescendant([](sfObject::SPtr childPtr)
    {
        if (childPtr->Type() != sfType::Component)
        {
            return false;
        }
        sfObjectMap::Remove(childPtr);
        return true;
    });
    // Rename this actor so the replacement can use its name
    sfUnrealUtils::Rename(this, GetName() + " (deleted)");
    // Create a new actor of the correct class for this sfObject and destroy this actor
    TSharedPtr<sfActorTranslator> actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(
        sfType::Actor);
    if (actorTranslatorPtr.IsValid())
    {
        actorTranslatorPtr->OnCreate(objPtr, 0);
    }
    if (IsSelected())
    {
        // Unselect this actor and select the replacement actor
        GEditor->SelectActor(this, false, true);
        AActor* actorPtr = sfObjectMap::Get<AActor>(objPtr);
        if (actorPtr != nullptr)
        {
            GEditor->SelectActor(actorPtr, true, true);
        }
    }
    if (actorTranslatorPtr.IsValid())
    {
        actorTranslatorPtr->DestroyActor(this);
    }
}

void AsfMissingActor::BeginDestroy()
{
    if (SceneFusion::MissingObjectManager.IsValid())
    {
        SceneFusion::MissingObjectManager->RemoveStandIn(this);
    }
    Super::BeginDestroy();
}
