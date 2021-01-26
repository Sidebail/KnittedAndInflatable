#include "sfRenameActivity.h"

sfRenameActivity::sfRenameActivity(const FString& name, float weight)
    : sfBaseActivity{ name, weight }
{
    m_adjectives.Add("Super");
    m_adjectives.Add("Omega");
    m_adjectives.Add("Dark");
    m_adjectives.Add("Invisible");
    m_adjectives.Add("Mythril");
    m_adjectives.Add("Secret");
    m_adjectives.Add("Shiny");
    m_adjectives.Add("Broken");
    m_adjectives.Add("Fire");
    m_adjectives.Add("Ice");
    m_adjectives.Add("Lightning");
    m_adjectives.Add("Mechanical");
    m_adjectives.Add("Mighty");
    m_adjectives.Add("Hopping");
    m_adjectives.Add("Ghost");
    m_adjectives.Add("Corrosive");
    m_adjectives.Add("Metal");
    m_adjectives.Add("Forgotten");
    m_adjectives.Add("Explosive");
    m_adjectives.Add("Chocolate");
    m_adjectives.Add("Ancient");

    m_nouns.Add("Sword");
    m_nouns.Add("Flower");
    m_nouns.Add("Mushroom");
    m_nouns.Add("Star");
    m_nouns.Add("Cape");
    m_nouns.Add("Barrel");
    m_nouns.Add("Amulet");
    m_nouns.Add("Staff");
    m_nouns.Add("Cookie");
    m_nouns.Add("Wizard");
    m_nouns.Add("Squirrel");
    m_nouns.Add("Bear");
    m_nouns.Add("Door");
    m_nouns.Add("Dragon");
    m_nouns.Add("Robot");
    m_nouns.Add("Tree");
    m_nouns.Add("Quiche");
    m_nouns.Add("Boots");
    m_nouns.Add("Scroll");
    m_nouns.Add("Chair");
    m_nouns.Add(L"Café");
}

void sfRenameActivity::Start()
{
    RandomActors(m_actors);
    for (AActor* actorPtr : m_actors)
    {
        GEditor->SelectActor(actorPtr, true, true);
        FString name = m_adjectives[FMath::RandRange(0, m_adjectives.Num() - 1)] + "_" +
            m_nouns[FMath::RandRange(0, m_nouns.Num() - 1)];
        actorPtr->SetActorLabel(name);
    }
}

void sfRenameActivity::Finish()
{
    for (AActor* actorPtr : m_actors)
    {
        GEditor->SelectActor(actorPtr, false, true);
    }
}

void sfRenameActivity::OnActorDeleted(AActor* actorPtr)
{
    m_actors.Remove(actorPtr);
}