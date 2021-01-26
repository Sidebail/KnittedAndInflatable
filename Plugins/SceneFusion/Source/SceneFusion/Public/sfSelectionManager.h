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
#include <GameFramework/Actor.h>
#include <sfObject.h>

using namespace KS::SceneFusion2;

/**
 * Detects actor selection/movement and broadcasts events.
 */
class sfSelectionManager
{
public:
    /**
     * @return  sfSelectionManager& static singleton instance.
     */
    static sfSelectionManager& Get();

    /**
     * Constructor
     */
    sfSelectionManager();

    /**
     * Delegate for selection events.
     *
     * @param   AActor* actorPtr
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(SelectionDelegate, AActor*);

    /**
     * Invoked when an actor is selected.
     */
    SelectionDelegate OnSelect;

    /**
     * Invoked when an actor is deselected.
     */
    SelectionDelegate OnDeselect;

    /**
     * Invoked when an actor is moved.
     */
    SelectionDelegate OnMove;

    /**
     * @return  TSet<AActor*>& the set of selected actors.
     */
    const TSet<AActor*>& SelectedActors();

    /**
     * @return  bool true if the selected actors are currently being moved.
     */
    bool MovingActors();

    /**
     * @return  bool true if at least one brush actor is currently being moved.
     */
    bool MovingBrush();
    
    /**
     * Starts monitoring for selection/movement changes.
     */
    void Start();

    /**
     * Stops monitoring for selection/movement changes.
     */
    void Stop();

    /**
     * Clears the sets of selected and moved actors.
     */
    void Clear();

    /**
     * Updates the selection manager. Checks for selection changes and broadcasts events.
     */
    void Update();

    /**
     * Removes an actor from the sets of selected and moved actors.
     *
     * @param   AActor* actorPtr to remove.
     */
    void RemoveActor(AActor* actorPtr);

private:
    TSet<AActor*> m_selectedActors;
    TSet<AActor*> m_movedActors;
    FDelegateHandle m_onMoveStartHandle;
    FDelegateHandle m_onMoveEndHandle;
    FDelegateHandle m_onActorMovedHandle;
    bool m_isMovingActors;
    bool m_isMovingBrush;

    /**
     * Called when an object starts being dragged in the viewport.
     *
     * @param   UObject& obj
     */
    void OnMoveStart(UObject& uobj);

    /**
     * Called when an object stops being dragged in the viewport.
     *
     * @param   UObject& object
     */
    void OnMoveEnd(UObject& uobj);

    /**
     * Called when an actor moves.
     *
     * @param   AActor* actorPtr
     */
    void OnActorMoved(AActor* actorPtr);
};