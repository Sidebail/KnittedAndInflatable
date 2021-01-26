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
#include "Consts.h"

#include <CoreMinimal.h>
#include <Editor.h>
#include <Editor/TransBuffer.h>
#include <UObject/NameTypes.h>
#include <LandscapeComponent.h>
#include <InstancedFoliageActor.h>
#include <Engine/LevelScriptActor.h>

#include <functional>

#include <Log.h>
#include <sfObject.h>
#include <sfDictionaryProperty.h>
#include <sfValueProperty.h>

#define LOG_CHANNEL "sfUnrealUtils"

#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 22
typedef int TypeHash;
#else
typedef uint32_t TypeHash;
#endif

/**
 * Utility functions.
 */
class sfUnrealUtils
{
public:
    typedef std::function<void()> Callback;

    /**
     * Calls a delegate then clears any undo transactions that were added during the delegate execution.
     *
     * @param   Callback callback
     */
    static void PreserveUndoStack(Callback callback)
    {
        UTransBuffer* undoBufferPtr = Cast<UTransBuffer>(GEditor->Trans);
        int undoCount = 0;
        int undoNum = 0;
        if (undoBufferPtr != nullptr)
        {
            undoCount = undoBufferPtr->UndoCount;
            undoBufferPtr->UndoCount = 0;
            undoNum = undoBufferPtr->UndoBuffer.Num();
        }
        callback();
        if (undoBufferPtr != nullptr)
        {
            while (undoBufferPtr->UndoBuffer.Num() > undoNum)
            {
                undoBufferPtr->UndoBuffer.Pop();
            }
            undoBufferPtr->UndoCount = undoCount;
        }
    }

    /**
     * Gets name or blueprint path for the given class.
     *
     * @param   UClass* classPtr
     * @return  FString
     */
    static FString ClassToFString(UClass* classPtr)
    {
        if (classPtr->IsInBlueprint())
        {
            if (classPtr->IsChildOf(ALevelScriptActor::StaticClass()))
            {
                return ALevelScriptActor::StaticClass()->GetName();
            }
            // Path to blueprint
            return classPtr->GetOuter()->GetName();
        }
        return classPtr->GetName();
    }

    /**
     * Loads a class by name or blueprint path.
     *
     * @param   const FString* className - name of the class or blueprint path.
     * @param   bool silent - if false, will log a warning if the class was not found.
     * @return  UClass* loaded class, or nullptr if the class was not found.
     */
    static UClass* LoadClass(const FString& className, bool silent = false)
    {
        UClass* classPtr = nullptr;
        if (className.Contains("/"))
        {
            // If it contains a '/' it's a blueprint path
            // Disable loading dialog that causes a crash if we are dragging objects
            GIsSlowTask = true;
            GIsEditorLoadingPackage = true;
            UBlueprint* blueprintPtr = LoadObject<UBlueprint>(nullptr, *className);
            GIsEditorLoadingPackage = false;
            GIsSlowTask = false;
            if (blueprintPtr == nullptr)
            {
                if (!silent)
                {
                    KS::Log::Warning("Unable to load blueprint " + std::string(TCHAR_TO_UTF8(*className)),
                        LOG_CHANNEL);
                }
                return nullptr;
            }
            classPtr = blueprintPtr->GeneratedClass;
        }
        else
        {
            classPtr = FindObject<UClass>(ANY_PACKAGE, *className);
        }
        if (classPtr == nullptr && !silent)
        {
            KS::Log::Warning("Unable to find class " + std::string(TCHAR_TO_UTF8(*className)), LOG_CHANNEL);
        }
        return classPtr;
    }

    /**
     * Renames an object. If the name is not available, appends random digits to the name until it finds a name that is
     * available.
     *
     * @param   UObject* uobjPtr to rename.
     * @param   FString name to set. If this name is already used, random digits will be appended to the name.
     */
    static void Rename(UObject* uobjPtr, FString name)
    {
        while (!uobjPtr->Rename(*name, nullptr, REN_Test))
        {
            name += FString::FromInt(rand() % 10);
        }
        uobjPtr->Rename(*name);
    }

    /**
     * Tries to rename an object. Logs a warning if the object could not be renamed because the name is already in use.
     * If a deleted object is using the name, renames the deleted object to make the name available.
     *
     * @param   UObject* uobjPtr to rename.
     * @param   const FString& name
     * @param   ERenameFlags renameFlags
     */
    static void TryRename(UObject* uobjPtr, const FString& name, ERenameFlags renameFlags = REN_None)
    {
        UObject* currentPtr = StaticFindObjectFast(UObject::StaticClass(), uobjPtr->GetOuter(), FName(*name));
        if (currentPtr == uobjPtr)
        {
            return;
        }
        if (currentPtr != nullptr && currentPtr->IsPendingKill())
        {
            // Rename the deleted actor so we can reuse its name
            Rename(currentPtr, name + " (deleted)");
            currentPtr = nullptr;
        }

        if (currentPtr == nullptr && uobjPtr->Rename(*name, nullptr, REN_Test))
        {
            uobjPtr->Rename(*name, nullptr, renameFlags);
        }
        else
        {
            KS::Log::Warning("Cannot rename object to " + std::string(TCHAR_TO_UTF8(*name)) +
                " because another object with that name already exists.", LOG_CHANNEL);
        }
    }

    /**
     * Converts FString to std::string.
     *
     * @param   const FString& inString
     * @return  std::string
     */
    static std::string FToStdString(const FString& inString)
    {
        return std::string(TCHAR_TO_UTF8(*inString));
    }

    /**
     * Returns the first widget found of the given type using depth-first search.
     *
     * @param   TSharedRef<SWidget> widgetPtr to search from.
     * @param   FName widgetType to look for.
     * @return  TSharedPtr<SWidget> widget of the given type, or invalid pointer if none was found.
     */
    static TSharedPtr<SWidget> FindWidget(TSharedRef<SWidget> widgetPtr, FName widgetType)
    {
        if (widgetPtr->GetType() == widgetType)
        {
            return widgetPtr;
        }
        FChildren* childrenPtr = widgetPtr->GetChildren();
        if (childrenPtr != nullptr)
        {
            for (int i = 0; i < childrenPtr->Num(); i++)
            {
                TSharedPtr<SWidget> resultPtr = FindWidget(childrenPtr->GetChildAt(i), widgetType);
                if (resultPtr.IsValid())
                {
                    return resultPtr;
                }
            }
        }
        return nullptr;
    }

    /**
     * Finds the child of an sfObject that is root component.
     *
     * @param   sfObject::SPtr objPtr to get root component sfObject for.
     * @return  sfObject::SPtr for the root component, or nullptr if not found.
     */
    static sfObject::SPtr FindRootComponentSFObject(sfObject::SPtr objPtr)
    {
        for (sfObject::SPtr childPtr : objPtr->Children())
        {
            if (childPtr->Type() != sfType::Component)
            {
                continue;
            }
            sfProperty::SPtr propPtr;
            if (childPtr->Property()->AsDict()->TryGet(sfProp::IsRoot, propPtr) &&
                propPtr->AsValue()->GetValue().GetBool())
            {
                return childPtr;
            }
        }
        return nullptr;
    }

    /**
     * Finds the first child of an sfObject with the given type.
     *
     * @param   sfObject::SPtr objPtr to get child from.
     * @param   const sfName& type of child to get.
     * @param   bool recursive - if true, will search descendants for the child type.
     * @return  sfObject::SPtr first child with the given type, or nullptr if none is found.
     */
    static sfObject::SPtr FindChildByType(sfObject::SPtr objPtr, const sfName& type, bool recursive = false)
    {
        if (objPtr == nullptr)
        {
            return nullptr;
        }
        for (sfObject::SPtr childPtr : objPtr->Children())
        {
            if (childPtr->Type() == type)
            {
                return childPtr;
            }
            if (recursive)
            {
                sfObject::SPtr resultPtr = FindChildByType(childPtr, type, true);
                if (resultPtr != nullptr)
                {
                    return resultPtr;
                }
            }
        }
        return nullptr;
    }

    /**
     * Finds the first ancestor of an sfObject with the given type.
     *
     * @param   sfObject::SPtr objPtr to get ancestor from.
     * @param   const sfName& type of ancestor to get.
     * @return  sfObject::SPtr first ancestor with the given type, or nullptr if none is found.
     */
    static sfObject::SPtr FindAncestorByType(sfObject::SPtr objPtr, const sfName& type)
    {
        if (objPtr == nullptr)
        {
            return nullptr;
        }
        while (objPtr->Parent() != nullptr)
        {
            objPtr = objPtr->Parent();
            if (objPtr->Type() == type)
            {
                return objPtr;
            }
        }
        return nullptr;
    }

    /**
     * Returns true if the given FString is "true" or "on" or "1".
     *
     * @param   const FString& arg
     * @return  bool
     */
    static bool IsStringTrue(const FString& arg)
    {
        return arg.Equals("true", ESearchCase::IgnoreCase) ||
            arg.Equals("on", ESearchCase::IgnoreCase) ||
            arg.Equals("1");
    }

    /**
     * Returns true if the given FString is "false" or "off" or "0".
     *
     * @param   const FString& arg
     * @return  bool
     */
    static bool IsStringFalse(const FString& arg)
    {
        return arg.Equals("false", ESearchCase::IgnoreCase) ||
            arg.Equals("off", ESearchCase::IgnoreCase) ||
            arg.Equals("0");
    }

    /**
     * Returns the given static mesh's source models.
     *
     * @param   UStaticMesh* meshPtr
     * @return  TArray<FStaticMeshSourceModel>&
     */
    static inline TArray<FStaticMeshSourceModel>& GetSourceModels(UStaticMesh* meshPtr)
    {
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
        return meshPtr->GetSourceModels();
#else
        return meshPtr->SourceModels;
#endif
    }

    /**
     * Returns the given static mesh's section info map.
     *
     * @param   UStaticMesh* meshPtr
     * @return  FMeshSectionInfoMap&
     */
    static inline FMeshSectionInfoMap& GetSectionInfoMap(UStaticMesh* meshPtr)
    {
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
        return meshPtr->GetSectionInfoMap();
#else
        return meshPtr->SectionInfoMap;
#endif
    }

    /**
     * Returns the type hash for the given name.
     *
     * @param   const FName& name
     * @return  TypeHash
     */
    static inline TypeHash GetNameTypeHash(const FName& name)
    {
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
        return GetTypeHash(name);
#else
        return name.GetComparisonIndex();
#endif
    }

    /**
     * Returns the type hash for the given class' name.
     *
     * @param   UClass* classPtr
     * @return  TypeHash
     */
    static inline TypeHash GetClassTypeHash(UClass* classPtr)
    {
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
        return GetTypeHash(classPtr->GetFName());
#else
        return classPtr->GetFName().GetComparisonIndex();
#endif
    }

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 25
    /**
     * Returns the type hash for the given class' name.
     *
     * @param   FFieldClass* classPtr
     * @return  TypeHash
     */
    static inline TypeHash GetClassTypeHash(FFieldClass* classPtr)
    {
        return GetTypeHash(classPtr->GetFName());
    }

    /**
     * Returns the given FScriptMapHelper's map.
     *
     * @param   TSharedPtr<FScriptMapHelper> scriptMapHelper
     * @return  void*
     */
    static inline void* GetMap(TSharedPtr<FScriptMapHelper> scriptMapHelper)
    {
        if (!!(scriptMapHelper->MapFlags & EMapPropertyFlags::UsesMemoryImageAllocator))
        {
            return scriptMapHelper->FreezableMap;
        }
        else
        {
            return scriptMapHelper->HeapMap;
        }
    }
#else
    /**
     * Returns the given FScriptMapHelper's map.
     *
     * @param   TSharedPtr<FScriptMapHelper> scriptMapHelper
     * @return  void*
     */
    static inline void* GetMap(TSharedPtr<FScriptMapHelper> scriptMapHelper)
    {
        return scriptMapHelper->Map;
    }
#endif
};

#undef LOG_CHANNEL
