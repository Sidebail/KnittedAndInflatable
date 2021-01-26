#include "../Public/sfUndoManager.h"
#include "../Public/sfObjectMap.h"
#include "../Public/sfUnrealUtils.h"
#include "../Public/Consts.h"
#include "../Public/SceneFusion.h"
#include "../Public/Translators/sfActorTranslator.h"
#include "../Public/Components/sfLockComponent.h"
#include <GameFramework/Actor.h>
#include <Editor.h>

using namespace KS::SceneFusion2;

sfUndoManager& sfUndoManager::Get()
{
    static sfUndoManager instance;
    return instance;
}

sfUndoManager::sfUndoManager() :
    m_undoHookPtr{ nullptr },
    m_undoText{ "" },
    m_inUndoRedo{ false }
{
}

sfUndoManager::~sfUndoManager()
{
}

void sfUndoManager::Initialize()
{
    m_undoBufferPtr = Cast<UTransBuffer>(GEditor->Trans);
    if (m_undoBufferPtr != nullptr)
    {
        m_onUndoHandle = m_undoBufferPtr->OnUndo().AddRaw(this, &sfUndoManager::OnUndo);
        m_onRedoHandle = m_undoBufferPtr->OnRedo().AddRaw(this, &sfUndoManager::OnRedo);
        m_beforeUndoRedoHandle = m_undoBufferPtr->OnBeforeRedoUndo().AddRaw(this, &sfUndoManager::BeforeUndoRedo);
    }
}

void sfUndoManager::CleanUp()
{
    if (m_undoBufferPtr != nullptr)
    {
        m_undoBufferPtr->OnUndo().Remove(m_onUndoHandle);
        m_undoBufferPtr->OnRedo().Remove(m_onRedoHandle);
        m_undoBufferPtr->OnBeforeRedoUndo().Remove(m_beforeUndoRedoHandle);
    }
    if (m_undoHookPtr != nullptr)
    {
        m_undoHookPtr->ClearFlags(RF_Standalone);
        m_undoHookPtr = nullptr;
    }
}

const FString& sfUndoManager::GetUndoText()
{
    if (!m_inUndoRedo)
    {
        if (GUndo == nullptr)
        {
            m_undoText = "";
        }
        else
        {
            m_undoText = FTextInspector::GetKey(((FTransaction*)GUndo)->GetContext().Title).Get("");
        }
    }
    return m_undoText;
}

bool sfUndoManager::InUndoRedo()
{
    return m_inUndoRedo;
}

void sfUndoManager::AddPostUndoHandler(sfIPostUndoHandler* handler)
{
    if (m_inUndoRedo)
    {
        m_postUndoHandlers.Add(handler);
    }
}

void sfUndoManager::OnUndo(TransactionContext context, bool success)
{
    if (success)
    {
        FixTransactedComponentChildren();
        OnUndoRedo(true);
    }
}

void sfUndoManager::OnRedo(TransactionContext context, bool success)
{
    if (success)
    {
        FixTransactedComponentChildren();
        OnUndoRedo(false);
    }
}

void sfUndoManager::BeforeUndoRedo(TransactionContext context)
{
    m_inUndoRedo = true;
    m_undoText = FTextInspector::GetKey(context.Title).Get("");
    // Because component child lists can be incorrect if another user changed the child list after the transaction was
    // recorded, we need to store the child components before undoing or redoing the transaction so we can correct
    // bad state after.
#if ENGINE_MAJOR_VERSION < 4 || ENGINE_MINOR_VERSION < 21
    FString action = context.Title.ToString();
#endif
    int index = m_undoBufferPtr->UndoBuffer.Num() - m_undoBufferPtr->GetUndoCount();
    const FTransaction* transactionPtr = m_undoBufferPtr->GetTransaction(index);
    // For Unreal 4.20 and lower, we don't know which transaction is being undone or redone because we don't know if
    // this is an undo or redo, so we check if the title matches the context title.
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 21
    if (transactionPtr != nullptr && context.TransactionId == transactionPtr->GetContext().TransactionId)
#else
    if (transactionPtr != nullptr && action == transactionPtr->GetContext().Title.ToString())
#endif
    {
        RecordPreTransactionState(transactionPtr);
    }
    transactionPtr = m_undoBufferPtr->GetTransaction(index - 1);
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 21
    if (transactionPtr != nullptr && context.TransactionId == transactionPtr->GetContext().TransactionId)
#else
    if (transactionPtr != nullptr && action == transactionPtr->GetContext().Title.ToString())
#endif
    {
        RecordPreTransactionState(transactionPtr);
    }
}

void sfUndoManager::RecordPreTransactionState(const FTransaction* transactionPtr)
{
    TArray<UObject*> objs;
    transactionPtr->GetTransactionObjects(objs);
    bool rebuildBSP = false;
    for (UObject* uobjPtr : objs)
    {
        UClass* classPtr = uobjPtr->GetClass();
        while (classPtr != nullptr)
        {
            auto iter = m_preUndoHandlers.find(classPtr);
            if (iter != m_preUndoHandlers.end())
            {
                iter->second(sfObjectMap::GetSFObject(uobjPtr), uobjPtr);
            }
            classPtr = classPtr->GetSuperClass();
        }

        AActor* actorPtr = Cast<AActor>(uobjPtr);
        if (actorPtr != nullptr)
        {
            if (actorPtr->IsPendingKill())
            {
                m_destroyedActorsToCheck.Add(actorPtr);
            }
            continue;
        }

        ULevel* levelPtr = Cast<ULevel>(uobjPtr);
        if (levelPtr != nullptr)
        {
            rebuildBSP = true;
            ABrush::SetNeedRebuild(levelPtr);
            continue;
        }
        UModel* modelPtr = Cast<UModel>(uobjPtr);
        if (modelPtr != nullptr)
        {
            ULevel* modelLevelPtr = modelPtr->GetTypedOuter<ULevel>();
            if (modelLevelPtr != nullptr)
            {
                rebuildBSP = true;
                ABrush::SetNeedRebuild(modelLevelPtr);
            }
            continue;
        }

        USceneComponent* componentPtr = Cast<USceneComponent>(uobjPtr);
        if (componentPtr == nullptr)
        {
            continue;
        }
        m_parentsToCheck.Add(componentPtr);
        if (componentPtr->GetAttachParent() != nullptr)
        {
            m_parentsToCheck.Add(componentPtr->GetAttachParent());
            if (!componentPtr->HasAnyFlags(RF_Transactional))
            {
                // For some reason lock mesh components are recorded in transactions for alt-drag even though they are
                // non-transactional. After the transaction they will be in a bad state. To fix this we create a new
                // lock mesh component.
                UsfLockComponent* lockPtr = Cast<UsfLockComponent>(componentPtr->GetAttachParent());
                if (lockPtr != nullptr)
                {
                    // Rename the old component so the new one can use its name.
                    sfUnrealUtils::Rename(componentPtr, componentPtr->GetName() + " (deleted)");
                    lockPtr->DuplicateParentMesh();
                }
            }
        }
        for (USceneComponent* childPtr : componentPtr->GetAttachChildren())
        {
            if (childPtr != nullptr)
            {
                m_childrenToCheck.Add(childPtr);
            }
        }
    }
    if (!rebuildBSP)
    {
        return;
    }
    // If a level in the transaction had its BSP modified since the transaction was registered, undoing it will cause
    // a crash. This is a hack to prevent the crash by rebuilding BSP after the transaction is applied but before the
    // crash can occur. We cast the transaction to our TransactionHack which lets us access the protected members and
    // insert our UsfUndoHook as the first changed object in the transaction, so its PostEditUndo is called before the
    // level's.
    TransactionHack* hackPtr = static_cast<TransactionHack*>(const_cast<FTransaction*>(transactionPtr));
    if (m_undoHookPtr == nullptr)
    {
        m_undoHookPtr = NewObject<UsfUndoHook>(GEditor->GetEditorWorldContext().World(), "Undo Hook",
            RF_Standalone | RF_Transient);
    }
    hackPtr->AddChangedObject(m_undoHookPtr);
}

// Fix bad child lists to match what the children think their parents are.
void sfUndoManager::FixTransactedComponentChildren()
{
    // Iterate components in the transaction
    for (USceneComponent* componentPtr : m_parentsToCheck)
    {
        AActor* actorPtr = componentPtr->GetOwner();
        if (!sfObjectMap::Contains(componentPtr) && componentPtr->GetOwner() != nullptr)
        {
            for (UActorComponent* otherPtr : componentPtr->GetOwner()->GetComponents())
            {
                if (otherPtr != nullptr && otherPtr != componentPtr &&
                    otherPtr->GetFName() == componentPtr->GetFName())
                {
                    // A component with the same name already exists. Rename and delete the one that was just created,
                    // and attach its children to the other component. Although we will delete it, we still need to
                    // rename it because names of deleted components are still in use.
                    USceneComponent* otherSceneCompPtr = Cast<USceneComponent>(otherPtr);
                    if (otherSceneCompPtr != nullptr)
                    {
                        // Attach the children of the duplicate component to the original component
                        for (int i = componentPtr->GetNumChildrenComponents() - 1; i >= 0; i--)
                        {
                            USceneComponent* childPtr = componentPtr->GetChildComponent(i);
                            if (childPtr->IsA<UsfLockComponent>())
                            {
                                // Do nothing with lock components
                                continue;
                            }
                            // We may be in a bad state where the child exists in both component's children list.
                            if (otherSceneCompPtr->GetAttachChildren().Contains(childPtr))
                            {
                                // Remove the child from the original's child list so we can correctly attach it.
                                TArray<USceneComponent*>& children =
                                    const_cast<TArray<USceneComponent*>&>(otherSceneCompPtr->GetAttachChildren());
                                children.Remove(childPtr);
                            }
                            childPtr->AttachToComponent(otherSceneCompPtr,
                                FAttachmentTransformRules::KeepRelativeTransform);
                        }
                        if (actorPtr->GetRootComponent() == componentPtr)
                        {
                            actorPtr->SetRootComponent(otherSceneCompPtr);
                        }
                    }
                    sfUnrealUtils::Rename(componentPtr, componentPtr->GetName() + " (deleted)");
                    componentPtr->DestroyComponent();
                    break;
                }
            }
            if (componentPtr->IsPendingKill())
            {
                continue;
            }
        }
        // Remove components in the child list that should not be there because they have a different parent.
        TArray<USceneComponent*>& children = const_cast<TArray<USceneComponent*>&>(componentPtr->GetAttachChildren());
        for (int i = children.Num() - 1; i >= 0; i--)
        {
            USceneComponent* childPtr = children[i];
            if (childPtr == nullptr || childPtr->GetAttachParent() != componentPtr)
            {
                children.RemoveAt(i);
                if (childPtr == nullptr)
                {
                    continue;
                }
                if (childPtr->GetOwner() != componentPtr->GetOwner() && componentPtr->GetOwner() != nullptr)
                {
                    componentPtr->GetOwner()->RemoveOwnedComponent(childPtr);
                }
                else if (childPtr->GetOwner() == componentPtr->GetOwner() &&
                    childPtr->GetAttachParent() == nullptr &&
                    (childPtr->GetOwner() == nullptr || childPtr->GetOwner()->GetRootComponent() != childPtr))
                {
                    childPtr->DestroyComponent();
                }
            }
        }
    }
    // Iterate the children of components we stored before the transaction and add them to their parent's child list if
    // they are missing.
    for (USceneComponent* componentPtr : m_childrenToCheck)
    {
        USceneComponent* parentPtr = componentPtr->GetAttachParent();
        if (parentPtr == nullptr || parentPtr == componentPtr)
        {
            continue;
        }
        if (parentPtr->IsPendingKill())
        {
            AActor* actorPtr = componentPtr->GetOwner();
            USceneComponent* rootPtr = actorPtr->GetRootComponent();
            if (rootPtr == nullptr)
            {
                componentPtr->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
                actorPtr->SetRootComponent(componentPtr);
            }
            else if (componentPtr == rootPtr)
            {
                UnrealObjectProperty* upropPtr = CastUnrealProperty<UnrealObjectProperty>(
                    USceneComponent::StaticClass()->FindPropertyByName(FName("AttachParent")));
                if (upropPtr != nullptr)
                {
                    upropPtr->SetObjectPropertyValue_InContainer(componentPtr, nullptr);
                }
                componentPtr->SetRelativeLocation(componentPtr->GetComponentTransform().GetTranslation());
                componentPtr->SetRelativeRotation(componentPtr->GetComponentRotation());
                componentPtr->SetRelativeScale3D(componentPtr->GetComponentScale());
            }
            else
            {
                componentPtr->AttachToComponent(rootPtr, FAttachmentTransformRules::KeepWorldTransform);
            }
        }
        else if (!parentPtr->GetAttachChildren().Contains(componentPtr))
        {
            TArray<USceneComponent*>& children = const_cast<TArray<USceneComponent*>&>(parentPtr->GetAttachChildren());
            children.Add(componentPtr);
            if (componentPtr->GetOwner() != nullptr &&
                !componentPtr->GetOwner()->GetComponents().Contains(componentPtr))
            {
                componentPtr->GetOwner()->AddOwnedComponent(componentPtr);
            }
        }
    }
    m_parentsToCheck.Empty();
    m_childrenToCheck.Empty();
}

void sfUndoManager::DestroyUnwantedActors(const TArray<UObject*>& objects)
{
    TSharedPtr<sfActorTranslator> actorTranslatorPtr = SceneFusion::Get().GetTranslator<sfActorTranslator>(
        sfType::Actor);
    if (!actorTranslatorPtr.IsValid() || m_destroyedActorsToCheck.Num() <= 0)
    {
        return;
    }

    TSet<ULevel*> modifiedLevels;
    for (UObject* uobjPtr : objects)
    {
        ULevel* levelPtr = Cast<ULevel>(uobjPtr);
        if (levelPtr != nullptr)
        {
            modifiedLevels.Add(levelPtr);
        }
    }
    for (AActor* actorPtr : m_destroyedActorsToCheck)
    {
        if (!actorPtr->IsPendingKill() && !modifiedLevels.Contains(actorPtr->GetLevel()))
        {
            actorTranslatorPtr->DestroyActor(actorPtr);
        }
    }
    m_destroyedActorsToCheck.Empty();
}

void sfUndoManager::OnUndoRedo(bool isUndo)
{
    int index = m_undoBufferPtr->UndoBuffer.Num() - m_undoBufferPtr->GetUndoCount();
    if (!isUndo)
    {
        index--;
    }
    const FTransaction* transactionPtr = m_undoBufferPtr->GetTransaction(index);
    if (transactionPtr == nullptr)
    {
        return;
    }
    TArray<UObject*> objs;
    transactionPtr->GetTransactionObjects(objs);
    DestroyUnwantedActors(objs);
    for (UObject* uobjPtr : objs)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(uobjPtr);
        if (objPtr != nullptr && !objPtr->IsSyncing())
        {
            objPtr = nullptr;
        }

        if (!uobjPtr->IsPendingKill())
        {
            UClass* classPtr = uobjPtr->GetClass();
            while (classPtr != nullptr)
            {
                auto iter = m_undoHandlers.find(classPtr);
                if (iter != m_undoHandlers.end())
                {
                    iter->second(sfObjectMap::GetSFObject(uobjPtr), uobjPtr);
                }
                classPtr = classPtr->GetSuperClass();
            }
        }

        SceneFusion::ObjectEventDispatcher->OnUndoRedo(objPtr, uobjPtr);
    }
    for (sfIPostUndoHandler* handler : m_postUndoHandlers)
    {
        handler->PostUndo();
    }
    m_postUndoHandlers.Empty();
    m_inUndoRedo = false;
}