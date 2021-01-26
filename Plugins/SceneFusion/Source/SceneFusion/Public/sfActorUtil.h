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

#include <Log.h>
#include <CoreMinimal.h>
#include <GameFramework/Actor.h>
#include <Components/SceneComponent.h>
#include <EditorSupportDelegates.h>
#include <ActorEditorUtils.h>
#include <Editor.h>

#define LOG_CHANNEL "sfActorUtil"

/**
 * Actor utility functions
 */
class sfActorUtil
{
public:
    /**
     * Finds an actor with the given name in the current level.
     *
     * @param   const FString& name of actor to find.
     * @return  AActor* with the given name, or nullptr if none was found. The actor may be deleted.
     */
    static AActor* FindActorWithNameInCurrentLevel(const FString& name)
    {
        UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
        if (worldPtr == nullptr)
        {
            return nullptr;
        }
        return Cast<AActor>(StaticFindObjectFast(AActor::StaticClass(), worldPtr->GetCurrentLevel(), FName(*name)));
    }

    /**
     * Finds an actor with the given name in the given level.
     *
     * @param   ULevel* levelPtr - level to find in
     * @param   const FString& name of actor to find.
     * @return  AActor* with the given name, or nullptr if none was found. The actor may be deleted.
     */
    static AActor* FindActorWithNameInLevel(ULevel* levelPtr, const FString& name)
    {
        if (levelPtr == nullptr)
        {
            return nullptr;
        }
        return Cast<AActor>(StaticFindObjectFast(AActor::StaticClass(), levelPtr, FName(*name)));
    }

    /**
     * Unselects and reselects an actor if it is selected, along with any of the actor's components that were selected.
     * This causes Unreal to refresh the details panel and notice changes to the component hierarchy.
     * 
     * @param   AActor* actorPtr to reselect. Does nothing if the actor is not already selected.
     */
    static void Reselect(AActor* actorPtr)
    {
        if (actorPtr == nullptr || !actorPtr->IsSelected())
        {
            return;
        }
        TArray<UActorComponent*> selectedComponents;
        if (GEditor->GetSelectedComponentCount() > 0)
        {
            for (UActorComponent* componentPtr : actorPtr->GetComponents())
            {
                if (componentPtr != nullptr && componentPtr->IsSelected())
                {
                    selectedComponents.Add(componentPtr);
                }
            }
        }
        GEditor->SelectActor(actorPtr, false, true);
        GEditor->SelectActor(actorPtr, true, true);
        for (UActorComponent* componentPtr : selectedComponents)
        {
            GEditor->SelectComponent(componentPtr, true, true);
        }
    }

    /**
     * Gets all scene components of type T belonging to an actor. This will find components that aren't in the actor's
     * OwnedComponents set, which will be missed by AActor->GetComponents<T>.
     *
     * @param   AActor* actorPtr to get components from.
     * @param   TArray<T*>& components array to add components to.
     */
    template<typename T>
    static void GetSceneComponents(AActor* actorPtr, TArray<T*>& components)
    {
        actorPtr->GetComponents(components);
        GetSceneComponents(actorPtr, actorPtr->GetRootComponent(), components);
    }

    /**
     * Finds scene components of type T belonging to an actor by searching a component and its descendants.
     *
     * @param   AActor* actorPtr to get components from.
     * @param   USceneComponent* componentPtr to start depth-first search at.
     * @param   TArray<T*>& components array to add components to.
     */
    template<typename T>
    static void GetSceneComponents(AActor* actorPtr, USceneComponent* componentPtr, TArray<T*>& components)
    {
        if (componentPtr == nullptr || componentPtr->GetOwner() != actorPtr)
        {
            return;
        }
        T* tPtr = Cast<T>(componentPtr);
        if (tPtr != nullptr)
        {
            components.AddUnique(tPtr);
        }
        for (USceneComponent* childPtr : componentPtr->GetAttachChildren())
        {
            GetSceneComponents(actorPtr, childPtr, components);
        }
    }

    /**
     * Gets the first component of type T on an actor, if any.
     *
     * @param   AActor* actorPtr to get component from.
     * @return  T* component of type T, or nullptr if none was found.
     */
    template<typename T>
    static T* GetComponent(AActor* actorPtr)
    {
        if (actorPtr != nullptr)
        {
            for (UActorComponent* componentPtr : actorPtr->GetComponents())
            {
                T* tPtr = Cast<T>(componentPtr);
                if (tPtr != nullptr)
                {
                    return tPtr;
                }
            }
        }
        return nullptr;
    }

    /**
     * Updates the given actor's visibility with the level it belongs to.
     *
     * @param   AActor* actorPtr to set visibility on
     */
    static void UpdateActorVisibilityWithLevel(AActor* actorPtr)
    {
        if (actorPtr == nullptr)
        {
            return;
        }
        ULevel* levelPtr = actorPtr->GetLevel();
        bool bShouldBeVisible = levelPtr->bIsVisible;

        if (FActorEditorUtils::IsABuilderBrush(actorPtr) || actorPtr->bHiddenEdLevel != bShouldBeVisible)
        {
            return;
        }

        actorPtr->bHiddenEdLevel = !bShouldBeVisible;
        if (levelPtr->IsPersistentLevel())
        {
            actorPtr->RegisterAllComponents();
            actorPtr->MarkComponentsRenderStateDirty();
        }
        else if (bShouldBeVisible)
        {
            actorPtr->ReregisterAllComponents();
        }
        else
        {
            actorPtr->UnregisterAllComponents();
        }
    }
};

#undef LOG_CHANNEL