#include "sfUIPanel.h"

sfUIPanel::sfUIPanel(const FString& title)
{
    m_widgetPtr = SNew(SScrollBox);

    // Title
    if (!title.IsEmpty()) {
        m_widgetPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 10)
        [
            SAssignNew(m_titlePtr, STextBlock)
            .Text(FText::FromString(title))
            .Font(sfUIStyles::GetDefaultFontStyle("Bold", 16))
        ];
    }

    // Content
    m_widgetPtr->AddSlot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
    [
        SAssignNew(m_contentPtr, SVerticalBox)
    ];

    // Message Box
    m_widgetPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 10)
    [
        SAssignNew(m_msgPtr, sfUIMessageBox)
    ];

    // Panels are hidden by default.
    m_msgPtr->SetVisibility(EVisibility::Collapsed);
}

TSharedRef<SScrollBox> sfUIPanel::Widget()
{
    return m_widgetPtr.ToSharedRef();
}

void sfUIPanel::Show()
{
    Enable();
    if (m_widgetPtr.IsValid()) {
        m_widgetPtr->SetVisibility(EVisibility::Visible);
        DisplayMessage("", sfUIMessageBox::INFO);
    }
}

void sfUIPanel::Hide()
{
    if (m_widgetPtr.IsValid()) {
        m_widgetPtr->SetVisibility(EVisibility::Hidden);
    }
}

void sfUIPanel::Enable()
{
    if (m_contentPtr.IsValid())
    {
        m_contentPtr->SetEnabled(true);
    }
}

void sfUIPanel::Disable()
{
    if (m_contentPtr.IsValid())
    {
        m_contentPtr->SetEnabled(false);
    }
}

void sfUIPanel::DisplayMessage(
    const FString& error,
    sfUIMessageBox::Icon icon,
    sfUIMessageBox::OnClickDelegate onClick)
{
    if (m_msgPtr.IsValid())
    {
        m_msgPtr->SetMessage(error, icon, onClick);
    }
}

void sfUIPanel::ClearMessage()
{
    if (m_msgPtr.IsValid())
    {
        m_msgPtr->SetVisibility(EVisibility::Collapsed);
    }
}