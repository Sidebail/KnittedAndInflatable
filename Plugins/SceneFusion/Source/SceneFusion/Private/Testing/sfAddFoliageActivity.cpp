#include "sfAddFoliageActivity.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfObjectMap.h"
#include "../Translators/sfFoliageTranslator.h"
#include "../../Public/sfUnrealUtils.h"

#include <Editor.h>
#include <Model.h>
#include <Components/ModelComponent.h>

sfAddFoliageActivity::sfAddFoliageActivity(const FString& name, float weight)
    : sfBaseActivity{ name, weight }
{}

void sfAddFoliageActivity::Start()
{
    m_typePtr = nullptr;

    UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
    m_actorPtr = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(worldPtr);
    if (m_actorPtr == nullptr || sfFoliageTranslator::GetFoliageInfos(m_actorPtr).Num() == 0)
    {
        return;
    }
    TArray<UFoliageType*> types;
    sfFoliageTranslator::GetFoliageInfos(m_actorPtr).GetKeys(types);
    m_typePtr = types[FMath::RandRange(0, types.Num() - 1)];

    for (int i = 0; i < 5; i++)
    {
        AActor* actorPtr = RandomActor();
        if (actorPtr == nullptr)
        {
            return;
        }
        TArray<UPrimitiveComponent*> primitives;
        actorPtr->GetComponents(primitives);
        if (primitives.Num() > 0)
        {
            m_bounds = primitives[0]->Bounds.GetBox();
            float sizeX = m_bounds.Max.X - m_bounds.Min.X;
            float pad = (1000.0f - sizeX) / 2.0f;
            if (pad > 0.0f)
            {
                m_bounds.Max.X += pad;
                m_bounds.Min.X -= pad;
            }
            float sizeY = m_bounds.Max.Y - m_bounds.Min.Y;
            pad = (1000.0f - sizeY) / 2.0f;
            if (pad > 0.0f)
            {
                m_bounds.Max.Y += pad;
                m_bounds.Min.Y -= pad;
            }
            return;
        }
    }
}

void sfAddFoliageActivity::Tick(float deltaTime)
{
    if (m_actorPtr == nullptr || m_actorPtr->IsPendingKill() || m_typePtr == nullptr)
    {
        return;
    }

    for (int i = 0; i < 10; i++)
    {
        FVector start;
        start.X = FMath::RandRange(m_bounds.Min.X, m_bounds.Max.X);
        start.Y = FMath::RandRange(m_bounds.Min.Y, m_bounds.Max.Y);
        start.Z = m_bounds.Max.Z + 100.0f;
        FVector end = start;
        end.Z = m_bounds.Min.Z - 100.0f;

        UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
        FHitResult hit;
        if (AInstancedFoliageActor::FoliageTrace(worldPtr, hit, FDesiredFoliageInstance(start, end), NAME_None, false,
            &Filter))
        {
            UActorComponent* componentPtr = hit.Component.Get();
            if (componentPtr != nullptr)
            {
                FFoliageInfo* infoPtr = m_actorPtr->FindOrAddMesh(m_typePtr);
                FFoliageInstance instance;
                instance.BaseComponent = componentPtr;
                instance.Location = hit.Location;
                instance.Rotation = FRotator{ 0.0f, FMath::FRandRange(-180.0f, 180.0f), 0.0f };
                instance.DrawScale3D = FVector::OneVector;
                instance.PreAlignRotation = instance.Rotation;
                instance.ZOffset = 0.0f;
                instance.Flags = 0;
                sfFoliageTranslator::AddInstanceToFoliageInfo(
                    infoPtr, m_actorPtr, m_typePtr, instance, instance.BaseComponent, true);
                break;
            }
        }
    }
}

void sfAddFoliageActivity::OnActorDeleted(AActor* actorPtr)
{
    if (actorPtr == m_actorPtr)
    {
        m_actorPtr = nullptr;
    }
}

bool sfAddFoliageActivity::Filter(const UPrimitiveComponent* componentPtr)
{
    return !componentPtr->IsA<UModelComponent>();
}