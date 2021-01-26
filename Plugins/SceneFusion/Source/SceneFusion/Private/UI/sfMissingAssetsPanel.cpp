#include "sfMissingAssetsPanel.h"
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Views/SListView.h>
#include <Log.h>

sfMissingAssetsPanel::sfMissingAssetsPanel() : sfUIPanel("Missing Assets")
{
    m_contentPtr->AddSlot()
    .HAlign(HAlign_Fill)
    .VAlign(VAlign_Center)
    .Padding(5, 2)
    .AutoHeight()
    [
        SAssignNew(m_messageBoxPtr, sfUIMessageBox)
    ];

    m_contentPtr->AddSlot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Center)
        .Padding(5, 2)
        .AutoHeight()
        [
            SAssignNew(m_missingAssetsListPtr, SListView<TSharedPtr<FString>>)
            .ItemHeight(24)
            .ListItemsSource(&m_missingAssets)
            .OnGenerateRow(TSlateDelegates<TSharedPtr<FString>>::FOnGenerateRow::CreateLambda(
                [this](TSharedPtr<FString> pathPtr, const TSharedRef<STableViewBase>& owner)->TSharedRef<ITableRow> 
            {
                return
                    SNew(STableRow<TSharedPtr<FString>>, owner)
                    .Padding(2.0f)
                    [
                        SNew(STextBlock).Text(FText::FromString(*pathPtr))
                    ];
            })
        )
        .SelectionMode(ESelectionMode::None)
    ];
}

int sfMissingAssetsPanel::NumMissingAssets()
{
    return m_missingAssets.Num();
}

void sfMissingAssetsPanel::AddMissingPath(const FString& path)
{
    m_missingAssets.Add(MakeShareable(new FString(path)));
    m_missingAssetsListPtr->RequestListRefresh();
    RefreshMessageBox();
    m_widgetPtr->Invalidate(EInvalidateWidget::Layout);
}

void sfMissingAssetsPanel::RemoveMissingPath(const FString& path)
{
    for (auto iter = m_missingAssets.CreateIterator(); iter; ++iter)
    {
        if (**iter == path)
        {
            iter.RemoveCurrent();
            m_missingAssetsListPtr->RequestListRefresh();
            RefreshMessageBox();
            m_widgetPtr->Invalidate(EInvalidateWidget::Layout);
            return;
        }
    }
}

void sfMissingAssetsPanel::RefreshMessageBox()
{
    FString str;
    if (m_missingAssets.Num() == 1)
    {
        str = "1 missing asset.";
    }
    else
    {
        str = FString::FromInt(m_missingAssets.Num()) + " missing assets.";
    }
    sfUIMessageBox::Icon icon = m_missingAssets.Num() == 0 ? 
        sfUIMessageBox::Icon::INFO : sfUIMessageBox::Icon::WARNING;
    m_messageBoxPtr->SetMessage(str, icon);
}

void sfMissingAssetsPanel::Clear()
{
    m_missingAssets.Empty();
    m_missingAssetsListPtr->RequestListRefresh();
    RefreshMessageBox();
    m_widgetPtr->Invalidate(EInvalidateWidget::Layout);
}