#include "sfUIStyles.h"
#include "../../Public/SceneFusion.h"

#include <Framework/Application/SlateApplication.h>
#include <Styling/SlateStyleRegistry.h>
#include <Interfaces/IPluginManager.h>
#include <Runtime/Engine/Classes/Engine/Font.h>

TSharedPtr<FSlateStyleSet> sfUIStyles::StyleInstancePtr = NULL;

// If the style instance has not been created, then create a new 
// style set instance and register it in the slate style registry.
void sfUIStyles::Initialize()
{
    if (!StyleInstancePtr.IsValid()) {
        StyleInstancePtr = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstancePtr);
    }
}

// Remove the style set instance from the slate style registry and dispose of it.
void sfUIStyles::Shutdown()
{
    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstancePtr);
    ensure(StyleInstancePtr.IsUnique());
    StyleInstancePtr.Reset();
}

// Get the name of this style set.
FName sfUIStyles::GetStyleSetName()
{
    static FName StyleSetName(TEXT("sfUIStyles"));
    return StyleSetName;
}

// Create a slate style set.
TSharedRef<FSlateStyleSet> sfUIStyles::Create()
{
    // Style initialization
    TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("sfUIStyles"));
    Style->SetContentRoot(IPluginManager::Get().FindPlugin("SceneFusion")->GetBaseDir() / TEXT("Resources"));

    // Add tool bar icon to style
    const FString toolBarIconPath = Style->RootToContentDir("Icon128.png");
    Style->Set("SceneFusion.ToolBarClickPtr", new FSlateImageBrush{ toolBarIconPath, FVector2D{ 40.0f, 40.0f } });
    Style->Set("SceneFusion.ToolBarClickPtr.small", new FSlateImageBrush{ toolBarIconPath, FVector2D{ 20.0f, 20.0f } });
    Style->Set("SceneFusion.TabIcon", new FSlateImageBrush{ toolBarIconPath, FVector2D{ 16.0f, 16.0f } });

    Style->Set("SceneFusion.Error",
        new FSlateImageBrush{ Style->RootToContentDir("Error96.png"),
        FVector2D{ 18.0f, 18.0f } });
    Style->Set("SceneFusion.Warning",
        new FSlateImageBrush{ Style->RootToContentDir("Warning96.png"),
        FVector2D{ 18.0f, 18.0f } });
    Style->Set("SceneFusion.Info",
        new FSlateImageBrush{ Style->RootToContentDir("Info96.png"),
        FVector2D{ 18.0f, 18.0f } });
    Style->Set("SceneFusion.Error.small",
        new FSlateImageBrush{ Style->RootToContentDir("Error96.png"),
        FVector2D{ 12.0f, 12.0f } });
    Style->Set("SceneFusion.Warning.small",
        new FSlateImageBrush{ Style->RootToContentDir("Warning96.png"),
        FVector2D{ 12.0f, 12.0f } });
    Style->Set("SceneFusion.Info.small",
        new FSlateImageBrush{ Style->RootToContentDir("Info96.png"),
        FVector2D{ 12.0f, 12.0f } });
    Style->Set("SceneFusion.User",
        new FSlateImageBrush{ Style->RootToContentDir("User128.png"),
        FVector2D{ 16.0f, 16.0f } });

    // Add world outliner icon to style
    Style->Set(
        "SceneFusion.Unlocked",
        new FSlateImageBrush{ Style->RootToContentDir("Check128.png"),
        FVector2D{ 16.0f, 16.0f } });
    Style->Set(
        "SceneFusion.Locked",
        new FSlateImageBrush{ Style->RootToContentDir("Lock128.png"),
        FVector2D{ 16.0f, 16.0f } });
        
    // Add viewport icons to style
    Style->Set("SceneFusion.Online",
        new FSlateImageBrush{ Style->RootToContentDir("Online128.png"),
        FVector2D{ 24.0f, 24.0f } });
    Style->Set("SceneFusion.Offline",
        new FSlateImageBrush{ Style->RootToContentDir("Offline128.png"),
        FVector2D{ 24.0f, 24.0f } });
    Style->Set("SceneFusion.Camera",
        new FSlateImageBrush{ Style->RootToContentDir("Camera128.png"),
        FVector2D{ 24.0f, 24.0f } });
    Style->Set("SceneFusion.CameraOff",
        new FSlateImageBrush{ Style->RootToContentDir("CameraOff128.png"),
        FVector2D{ 24.0f, 24.0f } });

    // Add class icons and thumbnails for our actors/components to style
    Style->Set("ClassThumbnail.sfMissingActor",
        new FSlateImageBrush{ Style->RootToContentDir("Question64.png"),
        FVector2D{ 64.0f, 64.0f } });
    Style->Set("ClassThumbnail.sfMissingComponent",
        new FSlateImageBrush{ Style->RootToContentDir("Question64.png"),
        FVector2D{ 64.0f, 64.0f } });
    Style->Set("ClassThumbnail.sfMissingSceneComponent",
        new FSlateImageBrush{ Style->RootToContentDir("Question64.png"),
        FVector2D{ 64.0f, 64.0f } });
    Style->Set("ClassIcon.sfMissingActor",
        new FSlateImageBrush{ Style->RootToContentDir("Question16.png"),
        FVector2D{ 16.0f, 16.0f } });
    Style->Set("ClassIcon.sfMissingComponent",
        new FSlateImageBrush{ Style->RootToContentDir("Question16.png"),
        FVector2D{ 16.0f, 16.0f } });
    Style->Set("ClassIcon.sfMissingSceneComponent",
        new FSlateImageBrush{ Style->RootToContentDir("Question16.png"),
        FVector2D{ 16.0f, 16.0f } });

    // Add Scene Fusion log
    Style->Set("SceneFusion.LogoDark",
        new FSlateImageBrush{ Style->RootToContentDir("SFLogoDark.png"),
        FVector2D{ 500.0f, 100.0f } });

    return Style;
}

FSlateImageBrush* sfUIStyles::CreateIcon(const FString& iconName, const FVector2D& iconSize)
{
    FString path = IPluginManager::Get().FindPlugin("SceneFusion")->GetBaseDir() / TEXT("Resources") / iconName;
    return new FSlateImageBrush{ path, iconSize };
}

// Reload slate application textures
void sfUIStyles::ReloadTextures()
{
    if (FSlateApplication::IsInitialized()) {
        FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
    }
}

// Get a reference to the slate style set.
const ISlateStyle& sfUIStyles::Get()
{
    return *StyleInstancePtr;
}

FSlateFontInfo sfUIStyles::GetDefaultFontStyle(const FName typefaceFontName, const int size)
{
    // Since FCoreStyle::GetDefaultFontStyle does not exist on Unreal 4.18,
    // we use the code below to create the default font.
    UFont* robotoFontPtr = LoadObject<UFont>(NULL, TEXT("/Engine/EngineFonts/Roboto"));
    return FSlateFontInfo((UObject*)(robotoFontPtr), size, typefaceFontName);
}