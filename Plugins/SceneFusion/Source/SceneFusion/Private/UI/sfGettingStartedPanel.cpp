#include "sfGettingStartedPanel.h"
#include "sfUIStyles.h"
#include "../../Public/sfConfig.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SHyperlink.h"

TSharedRef<SDockTab> sfGettingStartedPanel::CreateGettingStartedTab()
{
    return SNew(SDockTab)
        .Icon(sfUIStyles::Get().GetBrush("SceneFusion.TabIcon"))
        .TabRole(NomadTab)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 10)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).AutoHeight().Padding(2)
                [
                    SNew(SImage)
                    .Image(sfUIStyles::Get().GetBrush("SceneFusion.LogoDark"))
                ]
                + SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight().Padding(2, 20, 2, 10)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString("Getting Started"))
                    .Font(sfUIStyles::GetDefaultFontStyle("Regular", 12))
                ]
                + SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight().Padding(2, 10, 2, 10)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString("One team member creates a Scene Fusion project. After creation they can "
                        "use the web console to send project invitations\nto the other team members. The team member "
                        "who completes this process is the project owner."))
                ]
                + SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight().Padding(2)
                [
                    SNew(SButton)
                    .Text(FText::FromString("Set Up Project"))
                    .OnClicked(FOnClicked::CreateLambda([]()->FReply {
                        FString error = "";
                        FPlatformProcess::LaunchURL(
                            *FString("https://www.kinematicsoup.com/scene-fusion/pricing"), nullptr, &error);
                        return FReply::Handled();
                    }))
                ]
                + SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight().Padding(2, 10, 2, 10)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString("If you are not going to set up or manage the project, check your "
                        "email for the invite. Be sure to check your junk folder\nif it does not arrive within a few "
                        "minutes of being sent by the project owner."))
                ]
                + SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight().Padding(2)
                [
                    SNew(SButton)
                    .Text(FText::FromString("Open Scene Fusion Window"))
                    .OnClicked(FOnClicked::CreateLambda([]()->FReply {
                            TSharedRef<SDockTab> tab = FGlobalTabmanager::Get()->InvokeTab(FName("Scene Fusion"));
                            tab->FlashTab();
                            return FReply::Handled();
                    }))
                ]
                + SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight().Padding(2, 10, 2, 10)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString("You can find instructions how to use Scene Fusion here:"))
                ]
                + SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight().Padding(2)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
                    [
                        SNew(SButton)
                        .Text(FText::FromString("Troubleshooting"))
                        .OnClicked(FOnClicked::CreateLambda([]()->FReply {
                            FString error = "";
                            FPlatformProcess::LaunchURL(
                                *FString("https://docs.kinematicsoup.com/SceneFusion/unreal/trouble_shooting.html"),
                                nullptr,
                                &error);
                            return FReply::Handled();
                        }))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
                    [
                        SNew(SButton)
                        .Text(FText::FromString("Discord"))
                        .OnClicked(FOnClicked::CreateLambda([]()->FReply {
                            FString error = "";
                            FPlatformProcess::LaunchURL(
                                *FString("https://discord.gg/T2apBP"), nullptr, &error);
                            return FReply::Handled();
                        }))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
                    [
                        SNew(SButton)
                        .Text(FText::FromString("Youtube"))
                        .OnClicked(FOnClicked::CreateLambda([]()->FReply {
                            FString error = "";
                            FPlatformProcess::LaunchURL(
                                *FString("https://www.youtube.com/KinematicSoup"
                                    "?utm_source=Store&utm_medium=Store&utm_campaign=ES"), nullptr, &error);
                            return FReply::Handled();
                        }))
                    ]
                ]
            ]
            + SScrollBox::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(5, 10)
            [
                SNew(SCheckBox)
                .HAlign(HAlign_Left)
                .OnCheckStateChanged_Lambda([](ECheckBoxState newCheckedState){
                    sfConfig::Get().ShowGettingStartedScreen = newCheckedState == ECheckBoxState::Unchecked;
                    sfConfig::Get().Save();
                })
                .IsChecked_Lambda([]()-> const ECheckBoxState {
                    return sfConfig::Get().ShowGettingStartedScreen ?
                        ECheckBoxState::Unchecked :
                        ECheckBoxState::Checked;
                })
                .ToolTipText(FText::FromString(""))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString("Do not show this screen on startup"))
                ]
            ]
        ];
}