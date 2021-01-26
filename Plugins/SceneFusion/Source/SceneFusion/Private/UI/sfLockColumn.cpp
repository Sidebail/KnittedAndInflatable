#include "sfLockColumn.h"

#include <Editor/SceneOutliner/Public/ISceneOutliner.h>
#include <Editor/SceneOutliner/Public/ITreeItem.h>

#include <Widgets/SWidget.h>
#include <Widgets/Views/STableViewBase.h>
#include <Widgets/Views/SHeaderRow.h>
#include <Widgets/Views/STableRow.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Images/SImage.h>

#include <sfObject.h>

FsfLockColumn::FsfLockColumn(TSharedPtr<sfOutlinerManager> outlinerManagerPtr) :
    m_outlinerManagerPtr { outlinerManagerPtr }
{

}

FsfLockColumn::~FsfLockColumn()
{
}

FName FsfLockColumn::GetID()
{
    static FName IDName("sfLockColumn");
    return IDName;
}


FName FsfLockColumn::GetColumnID()
{
    return GetID();
}


SHeaderRow::FColumn::FArguments FsfLockColumn::ConstructHeaderRowColumn()
{
    return SHeaderRow::Column(GetColumnID())
        .DefaultLabel(FText::FromString("SF"))
        .ToolTipText(FText::FromString("Scene Fusion"))
        .FillWidth(0.5f);
}

const TSharedRef<SWidget> FsfLockColumn::ConstructRowWidget(
    SceneOutliner::FTreeItemRef TreeItem,
    const STableRow<SceneOutliner::FTreeItemPtr>& Row)
{
    FColumnGenerator Generator(*this);
    TreeItem->Visit(Generator);
    return Generator.Widget.ToSharedRef();
}

TSharedRef<SWidget> FsfLockColumn::ConstructRowWidget(const TWeakObjectPtr<AActor>& actor)
{
    return m_outlinerManagerPtr->ConstructRowWidget(actor.Get());
}