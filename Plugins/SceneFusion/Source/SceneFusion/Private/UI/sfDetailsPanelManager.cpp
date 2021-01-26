#include "sfDetailsPanelManager.h"
#include "../../Public/sfUnrealUtils.h"
#include "../../Public/Translators/sfActorTranslator.h"
#include "../../Public/SceneFusion.h"

#include <Widgets/SWidget.h>
#include <Widgets/Docking/SDockTab.h>
#include <Widgets/Views/STreeView.h>
#include <EditorModeRegistry.h>
#include <EditorModeManager.h>

TSharedPtr<sfDetailsPanelManager> sfDetailsPanelManager::m_instancePtr = nullptr;

sfDetailsPanelManager& sfDetailsPanelManager::Get()
{
    if (!m_instancePtr.IsValid())
    {
        FPropertyEditorModule& propertyEditorModule
            = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
        m_instancePtr = MakeShareable(new sfDetailsPanelManager(propertyEditorModule));
    }
    return *m_instancePtr;
}

sfDetailsPanelManager::sfDetailsPanelManager(FPropertyEditorModule& propertyEditorModule) :
    m_sessionPtr { nullptr },
    m_propertyEditorModule { propertyEditorModule }
{
    if (FSlateApplication::IsInitialized())
    {
        RegisterEditableObjectPredicates();
    }
}

sfDetailsPanelManager::~sfDetailsPanelManager()
{
    if (FSlateApplication::IsInitialized())
    {
        UnregisterEditableObjectPredicates();
    }
}

void sfDetailsPanelManager::Initialize()
{
    m_sessionPtr = SceneFusion::Service->Session();
    FetchDetailsViews(); //Fetch all details views
    OverrideAllowEditingAndIsEnabled();
    m_onPropertyEditorOpened = m_propertyEditorModule.OnPropertyEditorOpened().AddRaw(
        this, &sfDetailsPanelManager::OnPropertyEditorOpened);
}

void sfDetailsPanelManager::CleanUp()
{
    m_sessionPtr = nullptr;
    m_propertyEditorModule.OnPropertyEditorOpened().Remove(m_onPropertyEditorOpened);
}

void sfDetailsPanelManager::RegisterEditableObjectPredicates()
{
    FLevelEditorModule& module = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
    m_editableObjectPredicate.BindRaw(this, &sfDetailsPanelManager::AreObjectsEditable);
    module.AddEditableObjectPredicate(m_editableObjectPredicate);
}

void sfDetailsPanelManager::UnregisterEditableObjectPredicates()
{
    FLevelEditorModule& module = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
    module.RemoveEditableObjectPredicate(m_editableObjectPredicate.GetHandle());
}

void sfDetailsPanelManager::FetchDetailsViews()
{
    static const TArray<FName> detailsTabIdentifiers =
    {
        "LevelEditorSelectionDetails",
        "LevelEditorSelectionDetails2",
        "LevelEditorSelectionDetails3",
        "LevelEditorSelectionDetails4"
    };
    m_detailsViews.Empty();
    for (const FName& detailsTabIdentifier : detailsTabIdentifiers)
    {
        TSharedPtr<IDetailsView> detailsViewPtr = m_propertyEditorModule.FindDetailView(detailsTabIdentifier);
        if (detailsViewPtr.IsValid())
        {
            m_detailsViews.Add(detailsViewPtr);
        }
    }
}

void sfDetailsPanelManager::ForEachSSCSEditor(ForEachSSCSEditorCallback callback)
{
    for (TSharedPtr<IDetailsView> detailsViewPtr : m_detailsViews)
    {
        if (!detailsViewPtr.IsValid())
        {
            continue;
        }
        TSharedPtr<FTabManager> tabManagerPtr = detailsViewPtr->GetHostTabManager();
        if (tabManagerPtr.IsValid())
        {
            TSharedPtr<SDockTab> tabPtr = tabManagerPtr->FindExistingLiveTab(detailsViewPtr->GetIdentifier());
            if (tabPtr.IsValid())
            {
                TSharedPtr<SSCSEditor> editorPtr = StaticCastSharedPtr<SSCSEditor>(
                    sfUnrealUtils::FindWidget(tabPtr->GetContent(), "SSCSEditor"));
                if (editorPtr.IsValid())
                {
                    callback(detailsViewPtr, editorPtr);
                }
            }
        }
    }
}

void sfDetailsPanelManager::UpdateDetailsPanelTree()
{
    ForEachSSCSEditor([](TSharedPtr<IDetailsView> detailsViewPtr, TSharedPtr<SSCSEditor> editorPtr)
    {
        editorPtr->UpdateTree(true);
    });
}

void sfDetailsPanelManager::OverrideAllowEditingAndIsEnabled()
{
    ForEachSSCSEditor([this](TSharedPtr<IDetailsView> detailsViewPtr, TSharedPtr<SSCSEditor> editorPtr)
    {
        // Create TAttribute to return true when all selected actors in the details view are unlocked
        TAttribute<bool> enabledState;
        enabledState.Bind(TAttribute<bool>::FGetter::CreateLambda([detailsViewPtr]()
        {
            return Get().AreObjectsEditable(detailsViewPtr->GetSelectedObjects());
        }));

        // Set enabled on the name area
        detailsViewPtr->GetNameAreaWidget()->SetEnabled(enabledState);

        // Set enabled on the AddComponent button
        TSharedPtr<SWidget> addComponentButton = FindWidgetRecursive(editorPtr, "SComponentClassCombo");
        if (addComponentButton.IsValid())
        {
            addComponentButton->SetEnabled(enabledState);
        }

        // Override SSCSEditor's AllowEditing attribute with our function,
        // which returns true when any selected actors in the details view are unlocked.
        editorPtr->AllowEditing.Bind(TAttribute<bool>::FGetter::CreateLambda([detailsViewPtr]()
        {
            return Get().AllowComponentTreeEditing(detailsViewPtr->GetSelectedObjects());
        }));
    });
}

bool sfDetailsPanelManager::AreObjectsEditable(const TArray<TWeakObjectPtr<UObject>>& objects)
{
    bool editable = true;
    TSharedPtr<sfActorTranslator> actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(
        sfType::Actor);
    if (m_sessionPtr != nullptr && actorTranslatorPtr.IsValid() && objects.Num() > 0)
    {
        editable = CanEdit(objects);
        SetDetailsPanelEnabled(editable);
    }
    return editable;
}

bool sfDetailsPanelManager::AllowComponentTreeEditing(const TArray<TWeakObjectPtr<UObject>>& objects)
{
    if (GEditor->PlayWorld != nullptr)
    {
        return false;
    }

    TSharedPtr<sfActorTranslator> actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(
        sfType::Actor);
    if (m_sessionPtr != nullptr && actorTranslatorPtr.IsValid() && objects.Num() > 0)
    {
        return CanEdit(objects);
    }
    return true;

}

void sfDetailsPanelManager::SetDetailsPanelEnabled(bool enabled)
{
    // Grey out component name
    static const TArray<FName> disabledWidgetTypes =
    {
        "SInlineEditableTextBlock"
    };
    ForEachSSCSEditor([this, enabled](TSharedPtr<IDetailsView> detailsViewPtr, TSharedPtr<SSCSEditor> editorPtr)
    {
        SetEnabledRecursive(editorPtr, disabledWidgetTypes, enabled);
    });
}

void sfDetailsPanelManager::SetEnabledRecursive(
    TSharedPtr<SWidget> widget,
    TArray<FName> disabledWidgetTypes,
    bool enabled)
{
    if (disabledWidgetTypes.Contains(widget->GetType()))
    {
        widget->SetEnabled(enabled);
        return;
    }

    FChildren* children = widget->GetChildren();
    if (children != nullptr)
    {
        for (int i = 0; i < children->Num(); ++i)
        {
            SetEnabledRecursive(children->GetChildAt(i), disabledWidgetTypes, enabled);
        }
    }
}

TSharedPtr<SWidget> sfDetailsPanelManager::FindWidgetRecursive(TSharedPtr<SWidget> widget, FName widgetType)
{
    if (!widget.IsValid())
    {
        return nullptr;
    }

    if (widget->GetType() == widgetType)
    {
        return widget;
    }

    TSharedPtr<SWidget> result = nullptr;
    FChildren* children = widget->GetChildren();
    if (children != nullptr)
    {
        for (int i = 0; i < children->Num(); ++i)
        {
            result = FindWidgetRecursive(children->GetChildAt(i), widgetType);
            if (result.IsValid())
            {
                break;
            }
        }
    }
    return result;
}

void sfDetailsPanelManager::OnPropertyEditorOpened()
{
    FetchDetailsViews();
    FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this](float deltaTime)
    {
        // New details panel tab is not fully constructed yet. Call function in the next frame.
        OverrideAllowEditingAndIsEnabled();
        return false;
    }));
}

void sfDetailsPanelManager::GetSelectedActors(TSet<AActor*>& outActorSet)
{
    outActorSet.Empty();
    AActor* actorPtr;
    // Get Selected actors
    for (auto iter = GEditor->GetSelectedActorIterator(); iter; ++iter)
    {
        actorPtr = Cast<AActor>(*iter);
        if (actorPtr != nullptr)
        {
            outActorSet.Add(actorPtr);
        }
    }

    // Add actors inspected by locked details panels into the selected actors
    for (TSharedPtr<IDetailsView> detailsViewPtr : m_detailsViews)
    {
        if (!detailsViewPtr.IsValid())
        {
            continue;
        }

        TArray<TWeakObjectPtr<AActor>> detailsPanelSelectedActors = detailsViewPtr->GetSelectedActors();
        for (TWeakObjectPtr<AActor> actorWeakPtr : detailsPanelSelectedActors)
        {
            if (!actorWeakPtr.IsValid())
            {
                continue;
            }
            actorPtr = actorWeakPtr.Get();
            if (actorPtr != nullptr)
            {
                outActorSet.Add(actorPtr);
            }
        }
    }
}

bool sfDetailsPanelManager::CanEdit(const TArray<TWeakObjectPtr<UObject>>& objects)
{
    AActor* actorPtr;
    for (const TWeakObjectPtr<UObject>& objPtr : objects)
    {
        if (objPtr.IsValid())
        {
            actorPtr = Cast<AActor>(objPtr.Get());
            // If we did not get an actor pointer, then try to get the actor owning this UObject.
            if (!actorPtr)
            {
                UActorComponent* actorComponent = Cast<UActorComponent>(objPtr.Get());
                if (actorComponent != nullptr)
                {
                    actorPtr = Cast<AActor>(actorComponent->GetOuter());
                }
            }
            // Check the locked state of the sfObject that maps to the actor.
            if (actorPtr)
            {
                sfObject::SPtr sfobjPtr = sfObjectMap::GetSFObject(actorPtr);
                if (sfobjPtr != nullptr && sfobjPtr->IsLocked())
                {
                    return false;
                }
            }
        }
    }
    return true;
}