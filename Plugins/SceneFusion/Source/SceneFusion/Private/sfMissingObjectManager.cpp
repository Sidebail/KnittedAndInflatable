#include "../Public/sfMissingObjectManager.h"
#include "../Public/sfUnrealUtils.h"
#include "../Public/SceneFusion.h"
#include <Developer/HotReload/Public/IHotReload.h>

void sfMissingObjectManager::Initialize()
{
    m_onHotReloadHandle = IHotReloadModule::Get().OnHotReload().AddRaw(this, &sfMissingObjectManager::OnHotReload);
    m_onNewAssetHandle = UAssetManager::Get().GetAssetRegistry().OnAssetAdded().AddRaw(this,
        &sfMissingObjectManager::OnNewAsset);
}

void sfMissingObjectManager::CleanUp()
{
    IHotReloadModule::Get().OnHotReload().Remove(m_onHotReloadHandle);
    UAssetManager::Get().GetAssetRegistry().OnAssetAdded().Remove(m_onNewAssetHandle);
    m_standInMap.Empty();
}

void sfMissingObjectManager::AddStandIn(IsfMissingObject* standInPtr)
{
    TArray<IsfMissingObject*>& standIns = m_standInMap.FindOrAdd(standInPtr->MissingClass());
    standIns.Add(standInPtr);
    if (standIns.Num() == 1)
    {
        OnMissingAsset.Broadcast(standInPtr->MissingClass());
    }
}

void sfMissingObjectManager::RemoveStandIn(IsfMissingObject* standInPtr)
{
    TArray<IsfMissingObject*>* standInsPtr = m_standInMap.Find(standInPtr->MissingClass());
    if (standInsPtr != nullptr)
    {
        standInsPtr->Remove(standInPtr);
    }
}

void sfMissingObjectManager::AddMissingObject(const FString& path, sfObject::SPtr objPtr)
{
    m_missingObjects.Add(path, objPtr);
}

void sfMissingObjectManager::OnHotReload(bool automatic)
{
    // Iterate using a copy of the keys so modifications are safe while iterating
    TArray<FString> keys;
    m_standInMap.GetKeys(keys);
    for (FString& key : keys)
    {
        if (sfUnrealUtils::LoadClass(key, true) != nullptr)
        {
            TArray<IsfMissingObject*> standIns;
            if (m_standInMap.RemoveAndCopyValue(key, standIns))
            {
                for (IsfMissingObject* standInPtr : standIns)
                {
                    standInPtr->Reload();
                }
            }
        }
    }
}

void sfMissingObjectManager::OnNewAsset(const FAssetData& assetData)
{
    FString path = assetData.ObjectPath.ToString();
    int index = path.Find(".");
    if (index >= 0)
    {
        path = path.Left(index);
    }

    // Redispatch the create event for the object for this asset
    sfObject::SPtr objPtr;
    if (m_missingObjects.RemoveAndCopyValue(path, objPtr))
    {
        SceneFusion::ObjectEventDispatcher->OnCreate(objPtr, 0);
    }

    TArray<IsfMissingObject*> standIns;
    if (!m_standInMap.RemoveAndCopyValue(path, standIns))
    {
        return;
    }
    for (IsfMissingObject* standInPtr : standIns)
    {
        standInPtr->Reload();
    }
    OnFoundMissingAsset.Broadcast(path);
}