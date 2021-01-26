#include "sfUILoginPanel.h"
#include "../Web/sfBaseWebService.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfConfig.h"

#include <functional>

#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SHyperlink.h>

#define LOG_CHANNEL "sfUILoginPanel"

sfUILoginPanel::sfUILoginPanel() : sfUIPanel("Login")
{
    // Email Input
    m_contentPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 2).AutoHeight()
    [
        SAssignNew(m_emailPtr, SEditableTextBox)
        .MinDesiredWidth(200)
        .Text(FText::FromString(sfConfig::Get().Email))
        .HintText(FText::FromString("Email"))
        .OnTextCommitted(FOnTextCommitted::CreateRaw(this, &sfUILoginPanel::OnCommit))
    ];

    // Password Input
    m_contentPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 2).AutoHeight()
    [
        SAssignNew(m_passPtr, SEditableTextBox)
        .MinDesiredWidth(200)
        .HintText(FText::FromString("Password"))
        .IsPassword(true)
        .OnTextCommitted(FOnTextCommitted::CreateRaw(this, &sfUILoginPanel::OnCommit))
    ];

    // Login Button
    m_contentPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 10, 5, 2).AutoHeight()
    [
        SNew(SBox).WidthOverride(100).HAlign(HAlign_Fill).VAlign(VAlign_Center)
        [
            SNew(SButton)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .Text(FText::FromString("Login"))
            .OnClicked(FOnClicked::CreateRaw(this, &sfUILoginPanel::Login))
        ]
    ];

    // Register Link
    m_contentPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 5).AutoHeight()
    [
        SNew(SHyperlink)
        .Text(FText::FromString("Register"))
        .OnNavigate(FSimpleDelegate::CreateLambda([]() {
            FString error = "";
            FPlatformProcess::LaunchURL(
                *(sfConfig::Get().WebURL / "ksauthentication/login?register=1"), nullptr, &error);
        }))
    ];

    // Forgot Password Link
    m_contentPtr->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(5, 5).AutoHeight()
    [
        SNew(SHyperlink)
        .Text(FText::FromString("Forgot Password"))
        .OnNavigate(FSimpleDelegate::CreateLambda([]() {
            FString error = "";
            FPlatformProcess::LaunchURL(
                *(sfConfig::Get().WebURL / "ksauthentication/login?forgotpassword=1"), nullptr, &error);
        }))
    ];
}

void sfUILoginPanel::OnCommit(const FText& text, ETextCommit::Type commitType)
{
    if (commitType == ETextCommit::Type::OnEnter)
    {
        Login();
    }
}

FReply sfUILoginPanel::Login()
{
    DisplayMessage("Logging in...", sfUIMessageBox::INFO);
    Disable();
    SceneFusion::WebService->Login(
        m_emailPtr->GetText().ToString(),
        m_passPtr->GetText().ToString(),
        sfBaseWebService::OnLoginDelegate::CreateRaw(this, &sfUILoginPanel::LoginReply)
    );
    m_passPtr->SetText(FText::FromString(""));
    return FReply::Handled();
}

void sfUILoginPanel::Authenticate()
{
    // Login if we have an email and token
    sfConfig& settings = sfConfig::Get();
    if (!settings.Email.IsEmpty() && !settings.Token.IsEmpty()) {
        DisplayMessage("Authenticating...", sfUIMessageBox::INFO);
        Disable();
        SceneFusion::WebService->Authenticate(
            sfBaseWebService::OnLoginDelegate::CreateRaw(this, &sfUILoginPanel::LoginReply)
        );
    }
}

void sfUILoginPanel::LoginReply(const FString& error)
{
    Enable();
    if (error.IsEmpty()) {
        DisplayMessage("", sfUIMessageBox::INFO);
        OnLogin.Execute();
    }
    else {
        KS::Log::Error("HTTP Login failed: " + std::string(TCHAR_TO_UTF8(*error)), LOG_CHANNEL);
        DisplayMessage(error, sfUIMessageBox::ERROR);
    }
}

void sfUILoginPanel::AuthenticateReply(const FString& error)
{
    Enable();
    if (error.IsEmpty()) {
        DisplayMessage("", sfUIMessageBox::INFO);
        OnLogin.Execute();
    }
    else {
        KS::Log::Error("HTTP Authenticate failed: " + std::string(TCHAR_TO_UTF8(*error)), LOG_CHANNEL);
    }
}

#undef LOG_CHANNEL