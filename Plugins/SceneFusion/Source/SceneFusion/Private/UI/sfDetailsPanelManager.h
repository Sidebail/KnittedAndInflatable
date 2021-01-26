/*************************************************************************
 *
 * KINEMATICOUP CONFIDENTIAL
 * __________________
 *
 *  Copyright (2017-2020) KinematicSoup Technologies Incorporated
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of KinematicSoup Technologies Incorporated and its
 * suppliers, if any.  The intellectual and technical concepts contained
 * herein are proprietary to KinematicSoup Technologies Incorporated
 * and its suppliers and may be covered by Canadian and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from KinematicSoup Technologies Incorporated.
 */
#pragma once

#include <CoreMinimal.h>
#include <IDetailsView.h>
#include <PropertyEditorModule.h>
#include <EdMode.h>
#include <Engine/Selection.h>
#include <LevelEditor.h>
#include <SSCSEditor.h>

#include "../../Public/sfObjectMap.h"

#include <sfSession.h>

/**
 * Manages details panels. Disable editing when selected objects are locked.
 */
class sfDetailsPanelManager
{
public:
    /**
     * @return  sfDetailsPanelManager& singleton instance.
     */
    static sfDetailsPanelManager& Get();

    /**
     * Constructor
     *
     * @param   FPropertyEditorModule& propertyEditorModule
     */
    sfDetailsPanelManager(FPropertyEditorModule& propertyEditorModule);

    /**
     * Destructor
     */
    virtual ~sfDetailsPanelManager();

    /**
     * Initialization. Called after connecting to a session.
     */
    virtual void Initialize();

    /**
     * Deinitialization. Called after disconnecting from a session.
     */
    virtual void CleanUp();
    
    /**
     * Updates the tree of the SSCSEditor in the details panel.
     */
    void UpdateDetailsPanelTree();

    /**
     * Gets all selected actors including actors inspected by locked detail panels.
     *
     * @param   TSet<AActor*>& outActorSet
     */
    void GetSelectedActors(TSet<AActor*>& outActorSet);

private:
    /**
     * Callback for iterating SSCSEditor.
     *
     * @param   TSharedPtr<IDetailsView> pointer of the details view
     * @param   TSharedPtr<SSCSEditor> pointer of the SSCSEditor
     */
    typedef std::function<void(
        TSharedPtr<IDetailsView> detailsViewPtr,
        TSharedPtr<SSCSEditor> sscsEditorPtr)> ForEachSSCSEditorCallback;

    static TSharedPtr<sfDetailsPanelManager> m_instancePtr;

    sfSession::SPtr m_sessionPtr;
    FPropertyEditorModule& m_propertyEditorModule;
    TArray<TSharedPtr<IDetailsView>> m_detailsViews;

    FDelegateHandle m_onPropertyEditorOpened;
    FAreObjectsEditable m_editableObjectPredicate;

    /**
     * Register selection predicate for detail panel.
     */
    void RegisterEditableObjectPredicates();

    /**
     * Unregister selection predicate for detail panel.
     */
    void UnregisterEditableObjectPredicates();

    /**
     * Fetches details views in all open details panels.
     */
    void FetchDetailsViews();

    /**
     * Calls the given callback function for every SSCSEditor.
     *
     * @param   ForEachSSCSEditorCallback callback
     */
    void ForEachSSCSEditor(ForEachSSCSEditorCallback callback);

    /**
     * Overrides SSCSEditor's AllowEditing, name area and AddComponent button's IsEnabled.
     */
    void OverrideAllowEditingAndIsEnabled();

    /**
     * Check if a selection of UObjects is editable.
     *
     * @param   const TArray<TWeakObjectPtr<UObject>>& objects
     */
    bool AreObjectsEditable(const TArray<TWeakObjectPtr<UObject>>& objects);

    /**
     * Returns true if all actors in the given objects are unlocked.
     *
     * @return  const TArray<TWeakObjectPtr<UObject>>& objects
     */
    bool CanEdit(const TArray<TWeakObjectPtr<UObject>>& objects);

    /**
     * Allows component tree editing if all actors in the given objects are unlocked.
     *
     * @param   const TArray<TWeakObjectPtr<UObject>>& objects
     */
    bool AllowComponentTreeEditing(const TArray<TWeakObjectPtr<UObject>>& objects);

    /**
     * Set details panel's name area and AddComponent button enabled flag.
     *
     * @param   bool enable
     */
    void SetDetailsPanelEnabled(bool enabled);
    
    /**
     * Recursively iterate through given widget and its descendants.
     * If the type is in the given types, set its enabled flag as given.
     *
     * @param   TSharedPtr<SWidget> widget - widget to iterate
     * @param   TArray<FName> disabledWidgetTypes - array of widget types to set enable flag on
     * @param   bool enabled
     */
    void SetEnabledRecursive(TSharedPtr<SWidget> widget, TArray<FName> disabledWidgetTypes, bool enabled);

    /**
     * Recursively iterate through given widget and its descendants to find widget with of given type.
     * Returns the first widget found.
     *
     * @param   TSharedPtr<SWidget> widget - widget to iterate
     * @param   FName widgetType - widget type to look for
     */
    TSharedPtr<SWidget> FindWidgetRecursive(TSharedPtr<SWidget> widget, FName widgetType);

    /**
     * Called when a new property editor is opened.
     * Fetches details views. Overrides SSCSEditor's AllowEditing, name area and AddComponent button's IsEnabled.
     */
    void OnPropertyEditorOpened();
};