#include "sfParentActivity.h"

sfParentActivity::sfParentActivity(const FString& name, float weight)
    : sfBaseActivity{ name, weight }
{}

void sfParentActivity::Start()
{
    m_actorPtr = RandomActor();
    if (m_actorPtr == nullptr || m_actorPtr->GetRootComponent() == nullptr)
    {
        return;
    }
    GEditor->SelectActor(m_actorPtr, true, true);
    if (m_actorPtr->GetAttachParentActor() != nullptr && FMath::FRand() < 0.4f)
    {
        m_actorPtr->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }
    else
    {
        AActor* parentPtr = RandomActor();
        FText reason;
        if (m_actorPtr->EditorCanAttachTo(parentPtr, reason))
        {
            m_actorPtr->AttachToActor(parentPtr, FAttachmentTransformRules::KeepWorldTransform);
        }
    }
}

void sfParentActivity::Finish()
{
    if (m_actorPtr != nullptr)
    {
        GEditor->SelectActor(m_actorPtr, false, true);
    }
}

void sfParentActivity::OnActorDeleted(AActor* actorPtr)
{
    if (m_actorPtr == actorPtr)
    {
        m_actorPtr = nullptr;
    }
}