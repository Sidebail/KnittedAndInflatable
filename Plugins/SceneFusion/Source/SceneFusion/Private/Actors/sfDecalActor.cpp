#include "sfDecalActor.h"

AsfDecalActor::AsfDecalActor(const FObjectInitializer& objectInitializer)
    : Super(objectInitializer)
{
    // Destroy sprite and arrow components so they don't render in the viewport
    if (GetSpriteComponent() != nullptr)
    {
        GetSpriteComponent()->DestroyComponent();
    }
    if (GetArrowComponent() != nullptr)
    {
        GetArrowComponent()->DestroyComponent();
    }
}

bool AsfDecalActor::IsSelectable() const
{
    return false;
}