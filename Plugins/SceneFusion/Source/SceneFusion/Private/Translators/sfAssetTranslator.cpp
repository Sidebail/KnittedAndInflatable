#include "sfAssetTranslator.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/Consts.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/sfPropertyUtil.h"
#include "../../Public/sfUnrealUtils.h"
#include "../../Public/sfLoader.h"
#include "../../Public/sfChecksum.h"

#define DEFAULT_FLAGS (RF_Public | RF_Standalone | RF_Transactional | RF_WasLoaded | RF_LoadCompleted)
#define LOG_CHANNEL "sfAssetTranslator"

void sfAssetTranslator::Initialize()
{
    m_onDeleteHandle = UAssetManager::Get().GetAssetRegistry().OnAssetRemoved().AddRaw(this,
        &sfAssetTranslator::OnDeleteAsset);
    m_onCreateHandle = sfLoader::Get().OnCreateMissingAsset.AddRaw(this, &sfAssetTranslator::OnCreateMissingAsset);
}

void sfAssetTranslator::CleanUp()
{
    UAssetManager::Get().GetAssetRegistry().OnAssetRemoved().Remove(m_onDeleteHandle);
    sfLoader::Get().OnCreateMissingAsset.Remove(m_onCreateHandle);
    m_conflictingAssets.Empty();
    m_deletedAssets.Empty();
}

bool sfAssetTranslator::Create(UObject* uobjPtr, sfObject::SPtr& outObjPtr)
{
    // Do not sync if the uobject is not an asset or not one of the classes we sync.
    if (!sfLoader::Get().IsCreatableAssetType(uobjPtr->GetClass()) || uobjPtr->GetTypedOuter<ULevel>() != nullptr)
    {
        return false;
    }
    if (m_conflictingAssets.Contains(uobjPtr))
    {
        // Do not sync assets with conflicts
        return true;
    }
    sfDictionaryProperty::SPtr propertiesPtr = sfDictionaryProperty::Create();
    outObjPtr = sfObject::Create(sfType::Asset, propertiesPtr);
    sfObjectMap::Add(outObjPtr, uobjPtr);

    propertiesPtr->Set(sfProp::Name, sfPropertyUtil::FromString(uobjPtr->GetPathName()));
    propertiesPtr->Set(sfProp::Class, sfPropertyUtil::FromString(sfUnrealUtils::ClassToFString(uobjPtr->GetClass())));
    EObjectFlags flags = uobjPtr->GetFlags();
    if (flags != DEFAULT_FLAGS)
    {
        propertiesPtr->Set(sfProp::Flags, sfValueProperty::Create((uint32_t)flags));
    }

    sfPropertyManager::Get().CreateProperties(uobjPtr, propertiesPtr);
    // Compute a checksum from the initial property values. Used to detect and prevent assets with conlicts from
    // syncing.
    uint64_t checksum = Checksum(propertiesPtr);
    propertiesPtr->Set(sfProp::Checksum, sfValueProperty::Create((int64_t)checksum));

    SceneFusion::Service->Session()->Create(outObjPtr);
    return true;
}

void sfAssetTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    FString className = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Class));
    FString path = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
    UObject* assetPtr = sfLoader::Get().Load(path, className);
    if (assetPtr == nullptr)
    {
        return;
    }
    
    sfObject::SPtr currentPtr = sfObjectMap::GetSFObject(assetPtr);
    if (currentPtr != nullptr)
    {
        // The asset was created twice, which can happen if two users try to create the asset at the same time. Keep
        // the version that was created first and delete the second one.
        KS::Log::Warning(TCHAR_TO_UTF8(*("Asset '" + path + "' was uploaded by multiple users. The second " +
            " version will be ignored.")), LOG_CHANNEL);
        if (currentPtr->IsCreated())
        {
            SceneFusion::Service->Session()->Delete(objPtr);
            return;
        }
        SceneFusion::Service->Session()->Delete(currentPtr);
        sfObjectMap::Remove(currentPtr);
    }

    // If the asset was created on load, this user did not have the asset so we don't have to check for conflicts.
    if (!sfLoader::Get().WasCreatedOnLoad(assetPtr))
    {
        sfDictionaryProperty::SPtr dictPtr = sfDictionaryProperty::Create();
        sfPropertyManager::Get().CreateProperties(assetPtr, dictPtr);
        uint64_t checksum = Checksum(dictPtr);
        // If our checksum does not match either the initial checksum the asset was uploaded with, or the checksum
        // computed from the the current asset server values, the asset is conflicting and we won't sync it.
        if ((int64_t)checksum != (int64_t)propertiesPtr->Get(sfProp::Checksum)->AsValue()->GetValue() &&
            checksum != Checksum(propertiesPtr))
        {
            KS::Log::Warning(TCHAR_TO_UTF8(*("Your asset '" + path + "' conflicts with the server version and will " +
                "not sync.")), LOG_CHANNEL);
            m_conflictingAssets.Add(assetPtr);
            return;
        }
    }

    sfObjectMap::Add(objPtr, assetPtr);

    EObjectFlags flags = DEFAULT_FLAGS;
    sfProperty::SPtr propPtr;
    if (propertiesPtr->TryGet(sfProp::Flags, propPtr))
    {
        flags = (EObjectFlags)(uint32_t)propPtr->AsValue()->GetValue();
    }
    assetPtr->ClearFlags(RF_AllFlags);
    assetPtr->SetFlags(flags);

    sfPropertyManager::Get().ApplyProperties(assetPtr, propertiesPtr);
}

bool sfAssetTranslator::OnUndoRedo(sfObject::SPtr objPtr, UObject* uobjPtr)
{
    if (objPtr == nullptr || uobjPtr->IsPendingKill() || uobjPtr->GetTypedOuter<ULevel>() != nullptr)
    {
        return false;
    }
    if (!objPtr->IsSyncing())
    {
        return true;
    }
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    if (objPtr->IsLocked())
    {
        sfPropertyManager::Get().ApplyProperties(uobjPtr, propertiesPtr);
    }
    else
    {
        sfPropertyManager::Get().SendPropertyChanges(uobjPtr, propertiesPtr);
    }
    return true;
}

void sfAssetTranslator::PostPropertyChange(UObject* uobjPtr, UnrealProperty* upropPtr)
{
    sfBaseUObjectTranslator::PostPropertyChange(uobjPtr, upropPtr);
    uobjPtr->MarkPackageDirty();
}

void sfAssetTranslator::OnDeleteAsset(const FAssetData& assetData)
{
    // When a synced asset is deleted locally, we add its path and sfObject to the deleted assets map so if we need to
    // recreate it again (which happens if it gets referenced again) we can get the sfObject from the path.
    UObject* assetPtr = assetData.GetAsset();
    sfObject::SPtr objPtr = sfObjectMap::GetSFObject(assetPtr);
    if (objPtr != nullptr)
    {
        m_deletedAssets.Add(assetPtr->GetPathName(), objPtr);
    }
}

void sfAssetTranslator::OnCreateMissingAsset(UObject* assetPtr)
{
    // If this asset was already synced and then deleted locally, we need to sync it again.
    sfObject::SPtr objPtr;
    if (m_deletedAssets.RemoveAndCopyValue(assetPtr->GetPathName(), objPtr))
    {
        OnCreate(objPtr, 0);
    }
}

uint64_t sfAssetTranslator::Checksum(sfDictionaryProperty::SPtr dictPtr)
{
    return sfChecksum::Fletcher64(dictPtr, [this](const sfName& name)
    {
        // Don't include our custom properties beginning with # in the checksum
        return name->at(0) != '#';
    });
}

#undef LOG_CHANNEL
#undef DEFAULT_FLAGS