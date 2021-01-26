#include "../../Public/Objects/sfReferenceTracker.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/SceneFusion.h"
#include <Editor.h>

#define LOG_CHANNEL "UsfReferenceTracker"

UsfReferenceTracker* UsfReferenceTracker::m_instancePtr = nullptr;

UsfReferenceTracker& UsfReferenceTracker::Get()
{
    if (m_instancePtr == nullptr)
    {
        m_instancePtr = NewObject<UsfReferenceTracker>(GetTransientPackage(), "sfReferenceTracker",
            RF_Transient);
        m_instancePtr->AddToRoot();// Prevent the instance from being garbage collected
        FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddUObject(m_instancePtr,
            &UsfReferenceTracker::CollectGarbage);
    }
    return *m_instancePtr;
}

void UsfReferenceTracker::CleanUp()
{
    if (m_instancePtr != nullptr)
    {
        m_instancePtr->RemoveFromRoot();// Allow the instance to be garbage collected
        m_instancePtr = nullptr;
    }
}

void UsfReferenceTracker::Track(UObject* uobjPtr)
{
    m_references.Add(uobjPtr);
}

void UsfReferenceTracker::RemoveObjectsInLevel(ULevel* levelPtr)
{
    for (auto iter = m_references.CreateIterator(); iter; ++iter)
    {
        if (*iter == levelPtr || (*iter)->GetTypedOuter<ULevel>() == levelPtr)
        {
            iter.RemoveCurrent();
        }
    }
}

void UsfReferenceTracker::CollectGarbage()
{
    if (SceneFusion::Service->Session() == nullptr)
    {
        m_references.Empty();
        return;
    }
    for (auto iter = m_references.CreateIterator(); iter; ++iter)
    {
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(*iter);
        if (objPtr == nullptr || 
            (objPtr->Children().size() == 0 && SceneFusion::Service->Session()->GetReferences(objPtr).size() == 0))
        {
            iter.RemoveCurrent();
            if (objPtr != nullptr)
            {
                sfObjectMap::Remove(objPtr);
                if (objPtr->IsSyncing())
                {
                    SceneFusion::Service->Session()->Delete(objPtr);
                }
            }
        }
    }
}

#undef LOG_CHANNEL

