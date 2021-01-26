#include "sfDeleteActivity.h"
#include "../../Public/SceneFusion.h"

#include <Engine/Brush.h>
#include <InstancedFoliageActor.h>

sfDeleteActivity::sfDeleteActivity(const FString& name, float weight)
    : sfBaseActivity{ name, weight }
{}

void sfDeleteActivity::Start()
{
    AActor* actorPtr = RandomActor();
    if (actorPtr != nullptr && !actorPtr->IsA<AInstancedFoliageActor>())
    {
        GEditor->GetEditorWorldContext().World()->EditorDestroyActor(actorPtr, true);
        if (actorPtr->IsA<ABrush>())
        {
            SceneFusion::RedrawActiveViewport();
            GEditor->RebuildAlteredBSP();
        }
    }
}