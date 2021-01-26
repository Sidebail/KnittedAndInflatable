#include "sfRemoveFoliageActivity.h"
#include "../Translators/sfFoliageTranslator.h"
#include "../../Public/sfUnrealUtils.h"

sfRemoveFoliageActivity::sfRemoveFoliageActivity(const FString& name, float weight)
    : sfBaseActivity{ name, weight }
{}

void sfRemoveFoliageActivity::Start()
{
    m_count = 0;
    UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
    m_actorPtr = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(worldPtr);
    if (m_actorPtr == nullptr || sfFoliageTranslator::GetFoliageInfos(m_actorPtr).Num() == 0)
    {
        return;
    }
    TArray<UFoliageType*> types;
    sfFoliageTranslator::GetFoliageInfos(m_actorPtr).GetKeys(types);
    m_typePtr = types[FMath::RandRange(0, types.Num() - 1)];
    FFoliageInfo* infoPtr = m_actorPtr->FindOrAddMesh(m_typePtr);
    if (infoPtr->Instances.Num() == 0)
    {
        m_actorPtr = nullptr;
        m_typePtr = nullptr;
        return;
    }
    m_count = FMath::RandRange(1, FMath::Min(60, infoPtr->Instances.Num() - 1));
    m_index = FMath::RandRange(0, infoPtr->Instances.Num() - m_count);
}

void sfRemoveFoliageActivity::Tick(float deltaTime)
{
    if (m_count == 0 || m_actorPtr == nullptr || m_actorPtr->IsPendingKill() || m_typePtr == nullptr)
    {
        return;
    }
    FFoliageInfo* infoPtr = m_actorPtr->FindOrAddMesh(m_typePtr);
    if (infoPtr->Instances.Num() <= m_index)
    {
        return;
    }
    TArray<int> toRemove;
    toRemove.Add(m_index);
    infoPtr->RemoveInstances(m_actorPtr, toRemove, true);
    m_count--;
}

void sfRemoveFoliageActivity::OnActorDeleted(AActor* actorPtr)
{
    if (actorPtr == m_actorPtr)
    {
        m_actorPtr = nullptr;
    }
}