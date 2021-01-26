#include "../Public/sfObjectEventDispatcher.h"
#include "../Public/SceneFusion.h"
#include "../Public/sfObjectMap.h"
#include "../Public/sfConfig.h"

#define LOG_CHANNEL "sfObjectEventDispatcher"

sfObjectEventDispatcher::SPtr sfObjectEventDispatcher::CreateSPtr()
{
    return std::make_shared<sfObjectEventDispatcher>();
}

sfObjectEventDispatcher::sfObjectEventDispatcher() :
    m_active{ false },
    m_numFallbackTranslators{ 0 }
{

}

sfObjectEventDispatcher::~sfObjectEventDispatcher()
{

}

void sfObjectEventDispatcher::Register(
    const sfName& objectType,
    TSharedPtr<sfBaseTranslator> translatorPtr,
    bool isFallback)
{
    m_translatorMap[objectType] = translatorPtr;
    if (!m_translators.Contains(translatorPtr))
    {
        if (isFallback)
        {
            // Add the fallback translator at the end of the list.
            m_translators.Add(translatorPtr);
            m_numFallbackTranslators++;
        }
        else
        {
            // Insert the translator before the fallback translators.
            m_translators.Insert(translatorPtr, m_translators.Num() - m_numFallbackTranslators);
        }
    }
}

void sfObjectEventDispatcher::Initialize()
{
    if (m_active)
    {
        return;
    }
    m_active = true;
    sfSession::SPtr sessionPtr = SceneFusion::Service->Session();
    m_createEventPtr = sessionPtr->RegisterOnCreateHandler([this](sfObject::SPtr objPtr, int childIndex)
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
        if (translatorPtr.IsValid())
        {
            DisableOnUObjectModified();
            translatorPtr->OnCreate(objPtr, childIndex);
            EnableOnUObjectModified();
        }
    });
    m_deleteEventPtr = sessionPtr->RegisterOnDeleteHandler([this](sfObject::SPtr objPtr)
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
        if (translatorPtr.IsValid())
        {
            DisableOnUObjectModified();
            translatorPtr->OnDelete(objPtr);
            EnableOnUObjectModified();
        }
    });
    m_confirmDeleteEventPtr = sessionPtr->RegisterOnConfirmDeleteHandler([this](sfObject::SPtr objPtr)
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
        if (translatorPtr.IsValid())
        {
            translatorPtr->OnConfirmDelete(objPtr);
        }
    });
    m_lockEventPtr = sessionPtr->RegisterOnLockHandler([this](sfObject::SPtr objPtr)
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
        if (translatorPtr.IsValid())
        {
            translatorPtr->OnLock(objPtr);
        }
    });
    m_unlockEventPtr = sessionPtr->RegisterOnUnlockHandler([this](sfObject::SPtr objPtr)
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
        if (translatorPtr.IsValid())
        {
            translatorPtr->OnUnlock(objPtr);
        }
    });
    m_lockOwnerChangeEventPtr = sessionPtr->RegisterOnLockOwnerChangeHandler([this](sfObject::SPtr objPtr)
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
        if (translatorPtr.IsValid())
        {
            translatorPtr->OnLockOwnerChange(objPtr);
        }
    });
    m_directLockChangeEventPtr = sessionPtr->RegisterOnDirectLockChangeHandler([this](sfObject::SPtr objPtr)
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
        if (translatorPtr.IsValid())
        {
            translatorPtr->OnDirectLockChange(objPtr);
        }
    });
    m_parentChangeEventPtr = sessionPtr->RegisterOnParentChangeHandler([this](sfObject::SPtr objPtr, int childIndex)
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
        if (translatorPtr.IsValid())
        {
            DisableOnUObjectModified();
            translatorPtr->OnParentChange(objPtr, childIndex);
            EnableOnUObjectModified();
        }
    });
    m_propertyChangeEventPtr = sessionPtr->RegisterOnPropertyChangeHandler(
        [this](sfProperty::SPtr propertyPtr)
    {
        OnPropertyChange(propertyPtr);
    });
    m_removeFieldEventPtr = sessionPtr->RegisterOnDictionaryRemoveHandler(
        [this](sfDictionaryProperty::SPtr dictPtr, sfName name)
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(dictPtr->GetContainerObject());
        if (translatorPtr.IsValid())
        {
            translatorPtr->OnRemoveField(dictPtr, name);
        }
    });
    m_listAddEventPtr = sessionPtr->RegisterOnListAddHandler(
        [this](sfListProperty::SPtr listPtr, int index, int count)
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(listPtr->GetContainerObject());
        if (translatorPtr.IsValid())
        {
            translatorPtr->OnListAdd(listPtr, index, count);
        }
    });
    m_listRemoveEventPtr = sessionPtr->RegisterOnListRemoveHandler(
        [this](sfListProperty::SPtr listPtr, int index, int count)
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(listPtr->GetContainerObject());
        if (translatorPtr.IsValid())
        {
            translatorPtr->OnListRemove(listPtr, index, count);
        }
    });
    EnableOnUObjectModified();

    for (TSharedPtr<sfBaseTranslator> translatorPtr : m_translators)
    {
        translatorPtr->Initialize();
    }
}

void sfObjectEventDispatcher::CleanUp()
{
    if (!m_active)
    {
        return;
    }
    m_active = false;
    m_createQueue.Empty();
    m_createSet.clear();
    sfSession::SPtr sessionPtr = SceneFusion::Service->Session();
    sessionPtr->UnregisterOnCreateHandler(m_createEventPtr);
    sessionPtr->UnregisterOnDeleteHandler(m_deleteEventPtr);
    sessionPtr->UnregisterOnLockHandler(m_lockEventPtr);
    sessionPtr->UnregisterOnUnlockHandler(m_unlockEventPtr);
    sessionPtr->UnregisterOnLockOwnerChangeHandler(m_lockOwnerChangeEventPtr);
    sessionPtr->UnregisterOnDirectLockChangeHandler(m_directLockChangeEventPtr);
    sessionPtr->UnregisterOnParentChangeHandler(m_parentChangeEventPtr);
    sessionPtr->UnregisterOnPropertyChangeHandler(m_propertyChangeEventPtr);
    sessionPtr->UnregisterOnDictionaryRemoveHandler(m_removeFieldEventPtr);
    sessionPtr->UnregisterOnListAddHandler(m_listAddEventPtr);
    sessionPtr->UnregisterOnListRemoveHandler(m_listRemoveEventPtr);
    DisableOnUObjectModified();

    for (TSharedPtr<sfBaseTranslator> translatorPtr : m_translators)
    {
        translatorPtr->CleanUp();
    }
}

void sfObjectEventDispatcher::EnableOnUObjectModified()
{
    if (m_onObjectModifiedHandle.IsValid())
    {
        return;
    }
    m_onObjectModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddLambda(
        [this](UObject* uobjPtr)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(uobjPtr);
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
        if (translatorPtr.IsValid())
        {
            translatorPtr->OnUObjectModified(objPtr, uobjPtr);
        }
    });
}

void sfObjectEventDispatcher::DisableOnUObjectModified()
{
    if (m_onObjectModifiedHandle.IsValid())
    {
        FCoreUObjectDelegates::OnObjectModified.Remove(m_onObjectModifiedHandle);
        m_onObjectModifiedHandle.Reset();
    }
}

sfObject::SPtr sfObjectEventDispatcher::Create(UObject* uobjPtr)
{
    if (uobjPtr == nullptr)
    {
        return nullptr;
    }
    sfObject::SPtr objPtr = nullptr;
    for (TSharedPtr<sfBaseTranslator> translatorPtr : m_translators)
    {
        if (translatorPtr->Create(uobjPtr, objPtr))
        {
            break;
        }
    }
    return objPtr;
}

bool sfObjectEventDispatcher::IsCreateQueued(sfObject::SPtr objPtr)
{
    return m_createSet.find(objPtr) != m_createSet.end();
}

void sfObjectEventDispatcher::QueueCreate(sfObject::SPtr objPtr)
{
    if (!IsCreateQueued(objPtr))
    {
        m_createSet.emplace(objPtr);
        m_createQueue.Enqueue(objPtr);
    }
}

void sfObjectEventDispatcher::ProcessCreateQueue()
{
    while (!m_createQueue.IsEmpty() && !SceneFusion::CreateTimeExceeded())
    {
        sfObject::SPtr objPtr;
        m_createQueue.Dequeue(objPtr);
        m_createSet.erase(m_createSet.find(objPtr));
        if (objPtr->IsCreated() && !objPtr->IsDeletePending())
        {
            OnCreate(objPtr, 0);
        }
    }
}

void sfObjectEventDispatcher::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
    if (translatorPtr.IsValid())
    {
        translatorPtr->OnCreate(objPtr, childIndex);
    }
}

void sfObjectEventDispatcher::OnPropertyChange(sfProperty::SPtr propPtr)
{
    if (propPtr == nullptr)
    {
        return;
    }
    if (propPtr->GetContainerObject() == nullptr)
    {
        KS::Log::Error("Container object is null. Property path: " + propPtr->GetPath(), LOG_CHANNEL);
        return;
    }
    TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(propPtr->GetContainerObject());
    if (translatorPtr.IsValid())
    {
        translatorPtr->OnPropertyChange(propPtr);
    }
}

bool sfObjectEventDispatcher::OnUPropertyChange(sfObject::SPtr objPtr, UObject* uobjPtr, UnrealProperty* upropPtr)
{
    if (objPtr == nullptr)
    {
        return false;
    }
    TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
    return translatorPtr.IsValid() && translatorPtr->OnUPropertyChange(objPtr, uobjPtr, upropPtr);
}

void sfObjectEventDispatcher::PostPropertyChange(UObject* uobjPtr, UnrealProperty* upropPtr)
{
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(uobjPtr);
    if (upropPtr == nullptr || objPtr == nullptr)
    {
        return;
    }
    TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
    if (translatorPtr.IsValid())
    {
        translatorPtr->PostPropertyChange(uobjPtr, upropPtr);
    }
}

void sfObjectEventDispatcher::OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    if (objPtr == nullptr)
    {
        // Call OnUndoRedo on all translators until one of them handles it.
        for (TSharedPtr<sfBaseTranslator> translatorPtr : m_translators)
        {
            if (translatorPtr->OnUndoRedo(nullptr, uobjPtr))
            {
                return;
            }
        }
    }
    else
    {
        TSharedPtr<sfBaseTranslator> translatorPtr = GetTranslator(objPtr);
        if (translatorPtr.IsValid())
        {
            translatorPtr->OnUndoRedo(objPtr, uobjPtr);
        }
    }
}

TSharedPtr<sfBaseTranslator> sfObjectEventDispatcher::GetTranslator(sfObject::SPtr objPtr)
{
    if (objPtr == nullptr)
    {
        return nullptr;
    }
    return GetTranslator(objPtr->Type());
}

TSharedPtr<sfBaseTranslator> sfObjectEventDispatcher::GetTranslator(const sfName& type)
{
    auto iter = m_translatorMap.find(type);
    if (iter == m_translatorMap.end())
    {
        KS::Log::Error("Unknown object type '" + *type + "'.", LOG_CHANNEL);
        return nullptr;
    }
    return iter->second;
}

#undef LOG_CHANNEL