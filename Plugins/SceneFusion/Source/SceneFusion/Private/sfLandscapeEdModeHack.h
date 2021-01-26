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

#include <EdMode.h>
#include <LandscapeToolInterface.h>

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
#include <LandscapeEdit.h>
#endif

/**
 * Hack to expose some members of the private class FEdModeLandscape from LandscapeEdMode.h. This class inherits
 * from the same base class as FEdModeLandscape and declares the first few members (or void* for pointers to
 * private types) of FEdModeLandscape in the same order.
 */
class sfLandscapeEdModeHack : public FEdMode
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
    , public ILandscapeEdModeInterface
#endif
{
public:
    void* UISettings;
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
    FText ErrorReasonOnMouseUp;
#endif
    void* CurrentToolMode;
    FLandscapeTool* CurrentTool;
    FLandscapeBrush* CurrentBrush;
    FLandscapeToolTarget CurrentToolTarget;
    FLandscapeBrush* GizmoBrush;
    int CurrentToolIndex;
};