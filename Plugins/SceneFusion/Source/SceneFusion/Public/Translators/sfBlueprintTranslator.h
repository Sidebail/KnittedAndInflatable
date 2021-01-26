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
#include <Engine/SCS_Node.h>

#include <sfSession.h>

#include "sfBaseUObjectTranslator.h"

using namespace KS::SceneFusion2;
using namespace KS;

/**
 * Manages blueprint syncing.
 */
class sfBlueprintTranslator : public sfBaseUObjectTranslator
{
public:
    /**
     * Constructor
     */
    sfBlueprintTranslator();

    /**
     * Destructor
     */
    virtual ~sfBlueprintTranslator();

    /**
     * Initialization. Called after connecting to a session.
     */
    virtual void Initialize() override;

    /**
     * Deinitialization. Called after disconnecting from a session.
     */
    virtual void CleanUp() override;
    
    /**
     * Uploads the given UBlueprint and its generated class's default object if found any.
     *
     * @param   UBlueprint* blueprintPtr
     * @return  sfObject::SPtr
     */
    void UploadBlueprint(UBlueprint* blueprintPtr);

private:
    sfSession::SPtr m_sessionPtr;
    FDelegateHandle m_onPropertyChangeHandle;
    TSet<UBlueprint*> m_dirtyBlueprints;
    TSet<UBlueprint*> m_structurallyModifiedBlueprints;
    FDelegateHandle m_tickHandle;

    /**
     * Updates the blueprint translator.
     *
     * @param   float deltaTime in seconds since the last tick.
     */
    void Tick(float deltaTime);

    /**
     * Called when an actor is created by another user.
     *
     * @param   sfObject::SPtr objPtr that was created.
     * @param   int childIndex of new object. -1 if object is a root
     */
    virtual void OnCreate(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Called when an actor is deleted by another user.
     *
     * @param   sfObject::SPtr objPtr that was deleted.
     */
    virtual void OnDelete(sfObject::SPtr objPtr) override;

    /**
     * Called when a template component's parent is changed by another user.
     *
     * @param   sfObject::SPtr objPtr whose parent changed.
     * @param   int childIndex of the object. -1 if the object is a root.
     */
    virtual void OnParentChange(sfObject::SPtr objPtr, int childIndex) override;

    /**
     * Registers property change handlers for server events.
     */
    void RegisterPropertyChangeHandlers();

    /**
     * Called when a blueprint object is created.
     *
     * @param   sfObject::SPtr objPtr
     */
    void OnCreateBlueprint(sfObject::SPtr objPtr);

    /**
     * Called when a template component object is created.
     *
     * @param   sfObject::SPtr objPtr
     * @param   bool refreshAllNodes - if true, refresh all nodes
     */
    bool OnCreateTemplateComponent(sfObject::SPtr objPtr, bool refreshAllNodes = false);

    /**
     * Recursively creates an sfObject for the nodes through hierarchy. Returns the created sfObject.
     *
     * @param   sfObject::SPtr parentObjPtr
     * @param   USC_Node* nodePtr - node of component to create sfObject for.
     * @param   UBlueprintGeneratedClass* bgcPtr
     * @return  sfObject::SPtr
     */
    sfObject::SPtr CreateComponentObjectRecursive(
        sfObject::SPtr parentObjPtr,
        USCS_Node* nodePtr,
        UBlueprintGeneratedClass* bgcPtr);

    /**
     * Creates an sfObject for the non-native inherited component. Returns the created sfObject.
     *
     * @param   UActorComponent* templateComponentPtr
     * @param   UBlueprint* blueprintPtr
     * @return  sfObject::SPtr
     */
    sfObject::SPtr CreateNonNativeInheritedComponentObject(
        UActorComponent* templateComponentPtr,
        UBlueprint* blueprintPtr);

    /**
     * Called when a property is changed.
     *
     * @param   UObject* uobjPtr whose property changed.
     * @param   FPropertyChangedEvent& ev with information on what property changed.
     */
    void OnUObjectPropertyChange(UObject* uobjPtr, FPropertyChangedEvent& ev);

    /**
     * Checks and sends blueprint changes to server.
     */
    void SendChanges();

    /**
     * Recursively checks and sends changes for all non-native template components.
     *
     * @param   sfObject::SPtr currentParentObjPtr
     * @param   UBlueprint* blueprintPtr
     * @param   const TArray<USCS_Node*>& nodesToCheck
     */
    void SendChangesRecursive(
        sfObject::SPtr currentParentObjPtr,
        UBlueprint* blueprintPtr,
        const TArray<USCS_Node*>& nodesToCheck);

    /**
     * Sets parent of the given template component to be the given parent.
     *
     * @param   USceneComponent* componentPtr
     * @param   sfObject::SPtr parentObjPtr
     * @param   USCS_Node* nodePtr
     * @return  bool - true if the component's parent was changed
     */
    bool SetParent(USceneComponent* componentPtr, sfObject::SPtr parentObjPtr, USCS_Node* nodePtr);

    /**
     * Gets nodes for the given sfObject. If not found, creates one.
     *
     * @param   USimpleConstructionScript* scsPtr
     * @param   sfObject::SPtr objPtr
     * @param   bool& created - set to be true if we created a new node.
     * @return  USCS_Node*
     */
    USCS_Node* GetOrCreateNode(USimpleConstructionScript* scsPtr, sfObject::SPtr objPtr, bool& created);

    /**
     * Removes node for the given sfObject.
     *
     * @param   sfObject::SPtr objPtr
     * @param   bool refreshAllNodes - if true, refresh all nodes after node is removed.
     */
    void RemoveSCSNode(sfObject::SPtr objPtr, bool refreshAllNodes);

    /**
     * Removes the given node from the blueprint.
     *
     * @param   UBlueprint* blueprintPtr
     * @param   USCS_Node* nodePtr
     * @param   bool isSceneRoot - if true, the node is a root scene component
     */
    void RemoveSCSNode(UBlueprint* blueprintPtr, USCS_Node* nodePtr, bool isSceneRoot);

    /**
     * Removes unsynced components from the blueprint.
     *
     * @param   UBlueprint* blueprintPtr
     * @return  bool - true if any component is removed
     */
    bool RemoveUnsyncedComponents(UBlueprint* blueprintPtr);

    /**
     * Gets the node for the given UObject.
     *
     * @param   UObject* uobjPtr
     * @return  USCS_Node*
     */
    USCS_Node* GetSCSNode(UObject* uobjPtr);

    /**
     * Sets the given node's parent.
     *
     * @param   USCS_Node* nodePtr
     * @param   sfProperty::SPtr propertyPtr - property referenced to the parent
     * @return  bool - true if the parent is changed
     */
    bool SetInheritedParent(USCS_Node* nodePtr, sfProperty::SPtr propertyPtr);

    /**
     * Tries to find parent component of the given node in the ancestor blueprints. If not found, returns nullptr.
     *
     * @param   USCS_Node* nodePtr
     * @return  USceneComponent*
     */
    USceneComponent* FindParentComponentInAncestors(USCS_Node* nodePtr);
};