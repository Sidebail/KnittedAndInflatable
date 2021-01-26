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
#include <Runtime/CoreUObject/Public/UObject/UnrealType.h>

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 25
// Property Names
#define UnrealProperty FProperty

#define UnrealBoolProperty FBoolProperty

#define UnrealFloatProperty FFloatProperty
#define UnrealDoubleProperty FDoubleProperty

#define UnrealInt8Property FInt8Property
#define UnrealInt16Property FInt16Property
#define UnrealIntProperty FIntProperty
#define UnrealInt64Property FInt64Property

#define UnrealByteProperty FByteProperty
#define UnrealUInt16Property FUInt16Property
#define UnrealUInt32Property FUInt32Property
#define UnrealUInt64Property FUInt64Property

#define UnrealStrProperty FStrProperty
#define UnrealNameProperty FNameProperty
#define UnrealTextProperty FTextProperty

#define UnrealArrayProperty FArrayProperty
#define UnrealEnumProperty FEnumProperty
#define UnrealMapProperty FMapProperty
#define UnrealSetProperty FSetProperty
#define UnrealStructProperty FStructProperty
#define UnrealInterfaceProperty FInterfaceProperty
#define UnrealClassProperty FClassProperty
#define UnrealSoftClassProperty FSoftClassProperty

#define UnrealObjectProperty FObjectProperty
#define UnrealLazyObjectProperty FLazyObjectProperty
#define UnrealSoftObjectProperty FSoftObjectProperty
#define UnrealWeakObjectProperty FWeakObjectProperty

// Cast Function
#define CastUnrealProperty CastField
#else
// Property Names
#define UnrealProperty UProperty

#define UnrealBoolProperty UBoolProperty

#define UnrealFloatProperty UFloatProperty
#define UnrealDoubleProperty UDoubleProperty

#define UnrealByteProperty UByteProperty
#define UnrealInt8Property UInt8Property
#define UnrealInt16Property UInt16Property
#define UnrealIntProperty UIntProperty
#define UnrealInt64Property UInt64Property

#define UnrealUInt16Property UUInt16Property
#define UnrealUInt32Property UUInt32Property
#define UnrealUInt64Property UUInt64Property

#define UnrealStrProperty UStrProperty
#define UnrealNameProperty UNameProperty
#define UnrealTextProperty UTextProperty

#define UnrealArrayProperty UArrayProperty
#define UnrealEnumProperty UEnumProperty
#define UnrealMapProperty UMapProperty
#define UnrealSetProperty USetProperty
#define UnrealStructProperty UStructProperty

#define UnrealObjectProperty UObjectProperty
#define UnrealLazyObjectProperty ULazyObjectProperty
#define UnrealSoftObjectProperty USoftObjectProperty
#define UnrealWeakObjectProperty UWeakObjectProperty

#define UnrealInterfaceProperty UInterfaceProperty
#define UnrealClassProperty UClassProperty
#define UnrealSoftClassProperty USoftClassProperty

// Cast Function
#define CastUnrealProperty Cast
#endif