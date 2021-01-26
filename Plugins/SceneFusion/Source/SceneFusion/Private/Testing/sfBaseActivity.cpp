#include "sfBaseActivity.h"
#include "../../Public/Translators/sfActorTranslator.h"

sfBaseActivity::sfBaseActivity(const FString& name, float weight) :
    m_name{ name },
    m_duration{ 0 },
    m_weight{ weight }
{}

sfBaseActivity::~sfBaseActivity() {};

void sfBaseActivity::RandomActors(TArray<AActor*>& actors)
{
    actors.Empty();
    UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
    if (worldPtr == nullptr)
    {
        return;
    }
    int numActors = worldPtr->GetCurrentLevel()->Actors.Num();
    if (numActors == 0)
    {
        return;
    }
    TSharedPtr<sfActorTranslator> actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(
        sfType::Actor);
    if (!actorTranslatorPtr.IsValid())
    {
        return;
    }
    int count = FMath::RandRange(1, numActors / 5 + 1);
    while (count > 0)
    {
        count--;
        AActor* actorPtr = worldPtr->GetCurrentLevel()->Actors[FMath::RandRange(0, numActors - 1)];
        if (actorTranslatorPtr->IsSyncable(actorPtr))
        {
            actors.AddUnique(actorPtr);
        }
    }
}

AActor* sfBaseActivity::RandomActor()
{
    UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
    if (worldPtr == nullptr)
    {
        return nullptr;
    }
    int numActors = worldPtr->GetCurrentLevel()->Actors.Num();
    if (numActors == 0)
    {
        return nullptr;
    }
    TSharedPtr<sfActorTranslator> actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(
        sfType::Actor);
    if (!actorTranslatorPtr.IsValid())
    {
        return nullptr;
    }
    AActor* actorPtr = worldPtr->GetCurrentLevel()->Actors[FMath::RandRange(0, numActors - 1)];
    return actorTranslatorPtr->IsSyncable(actorPtr) ? actorPtr : nullptr;
}

