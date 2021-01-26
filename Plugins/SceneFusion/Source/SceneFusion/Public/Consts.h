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

#include <sfName.h>

using namespace KS::SceneFusion2;

/**
 * Property names
 */
class sfProp
{
public:
    static const sfName Location;
    static const sfName Rotation;
    static const sfName Scale;
    static const sfName Name;
    static const sfName Class;
    static const sfName Label;
    static const sfName Folder;
    static const sfName Mesh;
    static const sfName Id;
    static const sfName Flashlight;
    static const sfName Level;
    static const sfName IsPersistentLevel;
    static const sfName WorldComposition;
    static const sfName PackageName;
    static const sfName TilePosition;
    static const sfName Actors;
    static const sfName CreationMethod;
    static const sfName Visualize;
    static const sfName IsRoot;
    static const sfName Flags;
    static const sfName DefaultGameMode;
    static const sfName HierarchicalLODSetup;
    static const sfName NodeName;
    static const sfName Tooltip;
    static const sfName Parent;
    static const sfName Category;
    static const sfName Heightmap;
    static const sfName Offsetmap;
    static const sfName Weightmap;
    static const sfName ControlPoints;
    static const sfName Segments;
    static const sfName SplineComponent;
    static const sfName EditorLayerSettings;
    static const sfName SyncBlueprint;
    static const sfName SyncLandscape;
    static const sfName Type;
    static const sfName Instances;
    static const sfName Component;
    static const sfName Checksum;
    static const sfName LockLocation;
    static const sfName Layers;
};

/**
 * Object types
 */
class sfType
{
public:
    static const sfName Actor;
    static const sfName Avatar;
    static const sfName Level;
    static const sfName LevelLock;
    static const sfName LevelProperties;
    static const sfName Component;
    static const sfName MeshBounds;
    static const sfName GameMode;
    static const sfName UObject;
    static const sfName Blueprint;
    static const sfName TemplateComponent;
    static const sfName Model;
    static const sfName Landscape;
    static const sfName LandscapeBrush;
    static const sfName Config;
    static const sfName Foliage;
    static const sfName Asset;
};