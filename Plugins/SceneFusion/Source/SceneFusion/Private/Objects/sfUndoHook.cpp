#include "sfUndoHook.h"
#include <Log.h>
#include <Editor.h>
#include <Components/ModelComponent.h>

void UsfUndoHook::PostEditUndo()
{
    TArray<TWeakObjectPtr<ULevel>> levelsToRebuild;
    ABrush::NeedsRebuild(&levelsToRebuild);
    for (TWeakObjectPtr<ULevel> levelPtr : levelsToRebuild)
    {
        // Remove unregistered model components from the levels we are rebuilding BSP for to avoid log spam when Unreal
        // tries to unregister them.
        for (auto iter = levelPtr->ModelComponents.CreateIterator(); iter; ++iter)
        {
            if (!(*iter)->IsRegistered())
            {
                iter.RemoveCurrent();
            }
        }
        // Brushes deleted by other users will be in the level but have a null brush component. We need to remove them
        // to avoid a crash
        for (int i = 0; i < levelPtr->Actors.Num(); i++)
        {
            ABrush* brushPtr = Cast<ABrush>(levelPtr->Actors[i]);
            if (brushPtr != nullptr && brushPtr->GetBrushComponent() == nullptr)
            {
                levelPtr->Actors[i] = nullptr;
            }
        }
    }
    GIsTransacting = false;
    GEditor->RebuildAlteredBSP();
    GIsTransacting = true;
}