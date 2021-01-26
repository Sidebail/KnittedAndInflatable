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

#include <functional>
#include <vector>
#include <CoreMinimal.h>
#include <EdMode.h>
#include <EditorModeManager.h>

/**
 * Edit mode class to prevent editing when selected actors contains locked actors, and can prevent other editor modes
 * from receiving events.
 */
class SceneFusionEdMode : public FEdMode
{
public:
    /**
     * Callback to determine if an edit should be permitted.
     *
     * @return  bool true to allow the edit.
     */
    typedef std::function<bool()> Permitter;

    /**
     * Constructor
     */
    SceneFusionEdMode();

    /**
     * Destructor
     */
    virtual ~SceneFusionEdMode();

    // FEdMode interface

    /**
     * Always returns true so this edit mode's existence would not affect other edit modes.
     *
     * @param   FEditorModeID otherModeID
     * @return  bool
     */
    virtual bool IsCompatibleWith(FEditorModeID otherModeID) const override { return true; }

    /**
     * If selected actors are all locked, returns EEditAction::Halt to prevent duplicating.
     *
     * @return  EEditAction::Type
     */
    virtual EEditAction::Type GetActionEditDuplicate() override { return IsEditable(); }

    /**
     * If selected actors are all locked, returns EEditAction::Halt to prevent deleting.
     *
     * @return  EEditAction::Type
     */
    virtual EEditAction::Type GetActionEditDelete() override { return IsEditable(); }

    /**
     * If selected actors are all locked, returns EEditAction::Halt to prevent cutting.
     *
     * @return  EEditAction::Type
     */
    virtual EEditAction::Type GetActionEditCut() override { return IsEditable(); }

    /**
     * If selected actors are all locked or a permitter returns false, returns EEditAction::Halt to prevent pasting.
     *
     * @return  EEditAction::Type
     */
    virtual EEditAction::Type GetActionEditPaste() override { return IsEditable(); }

    /**
     * Called to handle click events. If a permitter returns false, prevents other editor modes from receiving the
     * event.
     *
     * @param   FEditorViewportClient* viewportClientPtr
     * @param   HHitProxy* hitProxyPtr
     * @param   const FViewportClick& click
     * @return  bool false to allow the default event handling.
     */
    virtual bool HandleClick(
        FEditorViewportClient* viewportClientPtr,
        HHitProxy* hitProxyPtr,
        const FViewportClick& click) override
    {
        if (!IsPermitted())
        {
            StopEvent();
        }
        return false;
    }

    /**
     * Called to handle start tracking events. If a permitter returns false, prevents other editor modes from receiving
     * the event.
     *
     * @param   FEditorViewportClient* viewportClientPtr
     * @param   FViewport* viewportPtr
     * @return  bool false to allow the default event handling.
     */
    virtual bool StartTracking(FEditorViewportClient* viewportClientPtr, FViewport* viewportPtr) override
    {
        if (!IsPermitted())
        {
            StopEvent();
        }
        return false;
    }

    /**
     * Called to handle keyboard events. If a permitter returns false, prevents other editor modes from receiving the
     * event.
     *
     * @param   FEditorViewportClient* viewportClientPtr
     * @param   FViewport* viewportPtr
     * @param   FKey key
     * @param   EInputEvent ev
     * @return  bool false to allow the default event handling.
     */
    virtual bool InputKey(
        FEditorViewportClient* viewportClientPtr,
        FViewport* viewportPtr,
        FKey key,
        EInputEvent ev) override
    {
        if (!IsPermitted())
        {
            StopEvent();
        }
        return false;
    }

    /**
     * Called to handle viewport drag events. If there are locked actors in the selection, do nothing.
     *
     * @param   FEditorViewportClient* viewportClientPtr
     * @param   FViewport* viewportPtr
     * @param   FVector& locationDelta
     * @param   FRotator& rotationDelta
     * @param   FVector& scaleDelta
     * @return  bool
     */
    virtual bool InputDelta(
        FEditorViewportClient* viewportClient,
        FViewport* viewport,
        FVector& locationDelta,
        FRotator& rotationDelta,
        FVector& scaleDelta) override;

    /**
     * Called to handle mouse move events. If a permitter returns false, prevents other editor modes from receiving the
     * event.
     *
     * @param   FEditorViewportClient* viewportClientPtr
     * @param   FViewport* viewportPtr
     * @param   int x
     * @param   int y
     * @return  bool false to allow the default event handling.
     */
    virtual bool MouseMove(FEditorViewportClient* viewportClientPtr, FViewport* viewportPtr, int x, int y) override
    {
        if (!IsPermitted())
        {
            StopEvent();
        }
        return false;
    }

    /**
     * Called to handle captured mouse move events. If a permitter returns false, prevents other editor modes from
     * receiving the event.
     *
     * @param   FEditorViewportClient* viewportClientPtr
     * @param   FViewport* viewportPtr
     * @param   int x
     * @param   int y
     * @return  bool false to allow the default event handling.
     */
    virtual bool CapturedMouseMove(
        FEditorViewportClient* viewportClientPtr,
        FViewport* viewportPtr,
        int x,
        int y) override
    {
        if (!IsPermitted())
        {
            StopEvent();
        }
        return false;
    }

    // End of FEdMode interface

    /**
     * Registers a permitter to determine if editing should be allowed.
     *
     * @param   Permitter permitter to register.
     * @return  int id of the permitter. Pass the id to UnregisterPermitter to remove the permitter.
     */
    int RegisterPermitter(Permitter permitter);

    /**
     * Unregisters a permitter.
     *
     * @param   int id of the permitter to unregister. The id is returned when registering the permitter.
     */
    void UnregisterPermitter(int id);

private:
    std::vector<std::pair<int, Permitter>> m_permitters; // list of permitters and their ids.
    int m_nextId; // id for next permitter

    /**
     * If selected actors are all locked, returns EEditAction::Halt. Otherwise, returns EEditAction::Skip.
     *
     * @return  EEditAction::Type
     */
    EEditAction::Type IsEditable();

    /**
     * Checks if all permitters will allow an edit.
     *
     * @return  false if any permitter returned false.
     */
    bool IsPermitted();

    /**
     * Uses hacks to stop other editor modes from processing the current event.
     */
    void StopEvent();
};

/**
 * Hack to expose FEditorModeTool's protected array of active editor modes.
 */
class sfEditorModeToolsHack : public FEditorModeTools
{
public:
    /**
     * @return  TArray<TSharedPtr<FEdMode>>& the array of active editor modes.
     */
    TArray<TSharedPtr<FEdMode>>& GetModes()
    {
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
        return ActiveModes;
#else
        return Modes;
#endif
    }
};

/**
 * Part of a hack to prevents other editor modes from processing an event. We replace the other editor modes in the
 * protected Modes array we exposed with a dummy editor mode, and when the dummy editor mode processes an event,
 * it replaces itself with the mode that used to be there.
 */
class sfDummyEdMode : public FEdMode
{
public:
    /**
     * Array of modes we replaced with the dummy.
     */
    TArray<TSharedPtr<FEdMode>> Modes;

    /**
     * Called to handle click events. Replaces this dummy with the original editor mode.
     *
     * @param   FEditorViewportClient* viewportClientPtr
     * @param   HHitProxy* hitProxyPtr
     * @param   const FViewportClick& click
     * @return  bool false to allow the default event handling.
     */
    virtual bool HandleClick(
        FEditorViewportClient* viewportClientPtr,
        HHitProxy* hitProxyPtr,
        const FViewportClick& click) override
    {
        RestoreMode();
        return false;
    }

    /**
     * Called to handle start tracking events. Replaces this dummy with the original editor mode.
     *
     * @param   FEditorViewportClient* viewportClientPtr
     * @param   FViewport* viewportPtr
     * @return  bool false to allow the default event handling.
     */
    virtual bool StartTracking(FEditorViewportClient* viewportClientPtr, FViewport* viewportPtr) override
    {
        RestoreMode();
        return false;
    }

    /**
     * Called to handle keyboard events. Replaces this dummy with the original editor mode.
     *
     * @param   FEditorViewportClient* viewportClientPtr
     * @param   FViewport* viewportPtr
     * @param   FKey key
     * @param   EInputEvent ev
     * @return  bool false to allow the default event handling.
     */
    virtual bool InputKey(
        FEditorViewportClient* viewportClientPtr,
        FViewport* viewportPtr,
        FKey key,
        EInputEvent event) override
    {
        RestoreMode();
        return false;
    }

    /**
     * Called to handle mouse move events. Replaces this dummy with the original editor mode.
     *
     * @param   FEditorViewportClient* viewportClientPtr
     * @param   FViewport* viewportPtr
     * @param   int x
     * @param   int y
     * @return  bool false to allow the default event handling.
     */
    virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int x, int y) override
    {
        RestoreMode();
        return false;
    }

    /**
     * Called to handle captured mouse move events. Replaces this dummy with the original editor mode.
     *
     * @param   FEditorViewportClient* viewportClientPtr
     * @param   FViewport* viewportPtr
     * @param   int x
     * @param   int y
     * @return  bool false to allow the default event handling.
     */
    virtual bool CapturedMouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int x, int y) override
    {
        RestoreMode();
        return false;
    }

private:
    /**
     * Replaces this dummy in the protected array we exposed from FEditorModeTool with the last mode in our Modes
     * array. Removes the last mode from our Modes array.
     */
    void RestoreMode();
};