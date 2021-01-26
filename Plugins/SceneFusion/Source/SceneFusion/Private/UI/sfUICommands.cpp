#include "sfUICommands.h"
#include "sfUIStyles.h"
#include "../../Public/SceneFusion.h"

#define LOCTEXT_NAMESPACE "SceneFusion"

sfUICommands::sfUICommands() :
    TCommands<sfUICommands>(
        TEXT("SceneFusion"), 
        NSLOCTEXT("Contexts", "Scene Fusion", "Scene Fusion Plugin"), 
        NAME_None, 
        sfUIStyles::GetStyleSetName()
    )
{

}

void sfUICommands::RegisterCommands()
{
    UI_COMMAND(ToolBarClickPtr, "Scene Fusion", "Open the Scene Fusion tab", 
        EUserInterfaceActionType::Button, FInputGesture());
    UI_COMMAND(MissingAssetsPtr, "Missing Assets", "Open the Scene Fusion Missing Assets tab",
        EUserInterfaceActionType::Button, FInputGesture());
    UI_COMMAND(GettingStartedPtr, "Getting Started", "Open the Scene Fusion Getting Started tab",
        EUserInterfaceActionType::Button, FInputGesture());
    UI_COMMAND(WebConsolePtr, "Web Console", "Open the Scene Fusion Web Console page",
        EUserInterfaceActionType::Button, FInputGesture());
    UI_COMMAND(DocumentPtr, "Documentation", "Open the Scene Fusion Documentation page",
        EUserInterfaceActionType::Button, FInputGesture());
    UI_COMMAND(EmailSupportPtr, "Email Support", "Open the Scene Fusion Email Support",
        EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE