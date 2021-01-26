#include "sfLandscapeBrushTranslator.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/sfPropertyUtil.h"
#include "../sfLandscapeEdModeHack.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/Consts.h"
#include "../../Public/SceneFusion.h"
#include <LandscapeInfo.h>
#include <LandscapeToolInterface.h>
#include <LandscapeEdit.h>
#include <sfDictionaryProperty.h>
#include <Editor.h>
#include <EditorModeManager.h>
#include <Components/DecalComponent.h>

UMaterialInterface* sfLandscapeBrushTranslator::BrushMaterial = nullptr;

sfLandscapeBrushTranslator::sfLandscapeBrushTranslator() :
    m_localObjPtr{ nullptr }
{
    GIsSlowTask = true;
    m_materialPtr = LoadObject<UMaterialInterface>(nullptr, TEXT("/SceneFusion/DecalMaterial"));
    GIsSlowTask = false;
}

void sfLandscapeBrushTranslator::Initialize()
{
    m_sessionPtr = SceneFusion::Service->Session();
    m_landscapeTranslatorPtr = SceneFusion::Get().GetTranslator<sfLandscapeTranslator>(sfType::Landscape);
    m_tickHandle = SceneFusion::Get().OnTick.AddRaw(this, &sfLandscapeBrushTranslator::Tick);
    m_userLeaveEventPtr = m_sessionPtr->RegisterOnUserLeaveHandler([this](sfUser::SPtr userPtr)
    {
        OnUserLeave(userPtr);
    });
    m_colorChangeEventPtr = m_sessionPtr->RegisterOnUserColorChangeHandler([this](sfUser::SPtr userPtr)
    {
        OnUserColorChange(userPtr);
    });
}

void sfLandscapeBrushTranslator::CleanUp()
{
    SceneFusion::Get().OnTick.Remove(m_tickHandle);
    m_sessionPtr->UnregisterOnUserLeaveHandler(m_userLeaveEventPtr);
    m_sessionPtr->UnregisterOnUserColorChangeHandler(m_colorChangeEventPtr);
    
    for (auto pair : m_objToDecalMap)
    {
        if (!pair.Value->IsPendingKill())
        {
            GEditor->GetEditorWorldContext().World()->EditorDestroyActor(pair.Value, false);
        }
    }
    m_objToDecalMap.Empty();
    for (auto pair : m_userMaterialMap)
    {
        pair.Value->ClearFlags(EObjectFlags::RF_Standalone);// Allow unreal to destroy the material instance
    }
    m_userMaterialMap.Empty();
}

void sfLandscapeBrushTranslator::Tick(float deltaTime)
{
    FIntRect bounds;
    sfObject::SPtr parentPtr = sfObjectMap::GetSFObject(m_landscapeTranslatorPtr->GetActiveLandscape());
    if (parentPtr != nullptr && !parentPtr->IsFullyLocked() && GetBrushBounds(bounds))
    {
        sfValueProperty::SPtr propPtr = sfPropertyUtil::FromIntRect(bounds);
        if (m_localObjPtr == nullptr)
        {
            sfDictionaryProperty::SPtr propertiesPtr = sfDictionaryProperty::Create();
            propertiesPtr->Set(sfProp::Id, sfValueProperty::Create(m_sessionPtr->LocalUserId()));
            propertiesPtr->Set(sfProp::Parent, sfReferenceProperty::Create(parentPtr->Id()));
            propertiesPtr->Set(sfProp::Location, propPtr);
            m_localObjPtr = sfObject::Create(sfType::LandscapeBrush, propertiesPtr, sfObject::Transient);
            m_localObjPtr->RequestLock();
            m_sessionPtr->Create(m_localObjPtr);
        }
        else
        {
            sfDictionaryProperty::SPtr propertiesPtr = m_localObjPtr->Property()->AsDict();
            sfPropertyManager::Get().Copy(propertiesPtr->Get(sfProp::Parent), sfReferenceProperty::Create(parentPtr->Id()));
            sfPropertyManager::Get().Copy(propertiesPtr->Get(sfProp::Location), propPtr);
        }
    }
    else if (m_localObjPtr != nullptr)
    {
        m_sessionPtr->Delete(m_localObjPtr);
        m_localObjPtr = nullptr;
    }
}

bool sfLandscapeBrushTranslator::GetBrushBounds(FIntRect& bounds)
{
    FEdMode* modePtr = GLevelEditorModeTools().GetActiveMode("EM_Landscape");
    if (modePtr == nullptr)
    {
        return false;
    }
    // Cast to our hack class to expose the current brush member.
    sfLandscapeEdModeHack* hackPtr = static_cast<sfLandscapeEdModeHack*>(modePtr);
    if (hackPtr->CurrentBrush == nullptr)
    {
        return false;
    }
    FString brushName = hackPtr->CurrentBrush->GetBrushName();
    FVector2D lastMousePosition;
    // Cast to one of our hack classes to expose the last mouse position member.
    if (brushName.StartsWith("Circle") || brushName == "Pattern" || brushName == "Alpha")
    {
        CircleBrushHack* brushHackPtr = static_cast<CircleBrushHack*>(hackPtr->CurrentBrush);
        lastMousePosition = brushHackPtr->LastMousePosition;
    }
    else if (brushName == "Component")
    {
        ComponentBrushHack* brushHackPtr = static_cast<ComponentBrushHack*>(hackPtr->CurrentBrush);
        lastMousePosition = brushHackPtr->LastMousePosition;
    }
    else
    {
        return false;
    }
    TArray<FLandscapeToolInteractorPosition> positions;
    positions.Emplace(FLandscapeToolInteractorPosition{ lastMousePosition, false });
    FLandscapeBrushData data = hackPtr->CurrentBrush->ApplyBrush(positions);
    bounds = data.GetBounds();
    // Make bounds inclusive
    bounds.Max.X -= 1;
    bounds.Max.Y -= 1;
    return true;
}

float sfLandscapeBrushTranslator::GetHeight(ALandscapeProxy* landscapePtr, float x, float y)
{
    FLandscapeEditDataInterface dataInterface{ landscapePtr->GetLandscapeInfo() };
    // Get the 4 points around the desired coordinate and interpolate
    uint16_t* dataPtr = new uint16_t[4];
    // Initialize to half max height. If the brush center is not on the landscape (can happen when adding components)
    // this value will be used.
    for (int i = 0; i < 4; i++)
    {
        dataPtr[i] = UINT16_MAX / 2;
    }
    int xi = FMath::FloorToInt(x);
    int yi = FMath::FloorToInt(y);
    dataInterface.GetHeightDataFast(xi, yi, xi + 1, yi + 1, dataPtr, 0);
    // Interpolate
    float a = x - xi;
    float y1 = dataPtr[0] * (1.0f - a) + dataPtr[1] * a;
    float y2 = dataPtr[2] * (1.0f - a) + dataPtr[3] * a;
    delete[] dataPtr;
    float height = y1 * (1.0f - a) + y2 * a; // height between 0 and max uint16
    height = 2.0f * height / UINT16_MAX - 1.0f; // height between -1 and 1
    return height * 256.0f; // landscape height values are multiplied by 256
}

void sfLandscapeBrushTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    sfObject::SPtr parentPtr = m_sessionPtr->GetObject(
        propertiesPtr->Get(sfProp::Parent)->AsReference()->GetObjectId());
    ALandscapeProxy* landscapePtr = sfObjectMap::Get<ALandscapeProxy>(parentPtr);
    if (landscapePtr == nullptr)
    {
        return;
    }
    UMaterialInstanceDynamic* materialPtr = GetUserMaterial(propertiesPtr->Get(sfProp::Id)->AsValue()->GetValue());
    if (materialPtr == nullptr)
    {
        return;
    }
    FActorSpawnParameters spawnInfo;
    spawnInfo.ObjectFlags = EObjectFlags::RF_Transient;
    spawnInfo.OverrideLevel = landscapePtr->GetLevel();
    UWorld * worldPtr = GEditor->GetEditorWorldContext().World();
    AsfDecalActor* decalPtr = worldPtr->SpawnActor<AsfDecalActor>(spawnInfo);
    decalPtr->SetDecalMaterial(materialPtr);
    decalPtr->AttachToActor(landscapePtr, FAttachmentTransformRules::KeepRelativeTransform);
    SetLocationAndSize(decalPtr, landscapePtr, propertiesPtr->Get(sfProp::Location));
    m_objToDecalMap.Add(objPtr->Id(), decalPtr);
}

void sfLandscapeBrushTranslator::OnDelete(sfObject::SPtr objPtr)
{
    AsfDecalActor* decalPtr;
    if (m_objToDecalMap.RemoveAndCopyValue(objPtr->Id(), decalPtr))
    {
        GEditor->GetEditorWorldContext().World()->EditorDestroyActor(decalPtr, false);
        SceneFusion::RedrawActiveViewport();
    }
}

void sfLandscapeBrushTranslator::OnPropertyChange(sfProperty::SPtr propertyPtr)
{
    AsfDecalActor* decalPtr = m_objToDecalMap.FindRef(propertyPtr->GetContainerObject()->Id());
    if (decalPtr == nullptr)
    {
        OnCreate(propertyPtr->GetContainerObject(), 0);
        return;
    }
    if (propertyPtr->Key() == sfProp::Location)
    {
        SetLocationAndSize(decalPtr, Cast<ALandscapeProxy>(decalPtr->GetAttachParentActor()), propertyPtr);
    }
    else if (propertyPtr->Key() == sfProp::Parent)
    {
        sfObject::SPtr parentPtr = m_sessionPtr->GetObject(propertyPtr->AsReference()->GetObjectId());
        ALandscapeProxy* landscapePtr = sfObjectMap::Get<ALandscapeProxy>(parentPtr);
        if (landscapePtr == nullptr)
        {
            return;
        }
        if (landscapePtr->GetLevel() != decalPtr->GetLevel())
        {
            // The decal and landscape are in different levels. We can't move actors between levels, so we destroy the
            // decal and create a new one in the other level.
            OnDelete(propertyPtr->GetContainerObject());
            OnCreate(propertyPtr->GetContainerObject(), 0);
        }
        else
        {
            decalPtr->AttachToActor(landscapePtr, FAttachmentTransformRules::KeepRelativeTransform);
        }
    }
}

void sfLandscapeBrushTranslator::SetLocationAndSize(AsfDecalActor* decalPtr, ALandscapeProxy* landscapePtr, sfProperty::SPtr propertyPtr)
{
    if (landscapePtr == nullptr)
    {
        return;
    }
    FIntRect rect = sfPropertyUtil::ToIntRect(propertyPtr);
    FVector position;
    position.X = (rect.Max.X + rect.Min.X) / 2.0f;
    position.Y = (rect.Max.Y + rect.Min.Y) / 2.0f;
    position.Z = GetHeight(landscapePtr, position.X, position.Y);
    decalPtr->SetActorRelativeLocation(position);
    decalPtr->SetActorRelativeRotation(FRotator{ -90.0f, 0.0f, 0.0f });
    // Decal size is before rotation, so X corresponds to vertical axis.
    decalPtr->GetDecal()->DecalSize = FVector{ 20.0f, (rect.Max.Y - rect.Min.Y) / 2.0f,
        (rect.Max.X - rect.Min.X) / 2.0f };
    SceneFusion::RedrawActiveViewport();
}

UMaterialInstanceDynamic* sfLandscapeBrushTranslator::GetUserMaterial(uint32_t userId)
{
    UMaterialInstanceDynamic* materialPtr = m_userMaterialMap.FindRef(userId);
    if (materialPtr == nullptr)
    {
        materialPtr = UMaterialInstanceDynamic::Create(m_materialPtr, nullptr);
        materialPtr->SetFlags(EObjectFlags::RF_Standalone);//prevent material from being destroyed
        m_userMaterialMap.Add(userId, materialPtr);

        sfUser::SPtr userPtr = m_sessionPtr->GetUser(userId);
        if (userPtr != nullptr)
        {
            OnUserColorChange(userPtr);
        }
    }
    return materialPtr;
}

void sfLandscapeBrushTranslator::OnUserColorChange(sfUser::SPtr userPtr)
{
    UMaterialInstanceDynamic* materialPtr = m_userMaterialMap.FindRef(userPtr->Id());
    if (materialPtr != nullptr)
    {
        ksColor color = userPtr->Color();
        FLinearColor ucolor(color.R(), color.G(), color.B(), 0.5f);
        materialPtr->SetVectorParameterValue("Color", ucolor);
        SceneFusion::RedrawActiveViewport();
    }
}

void sfLandscapeBrushTranslator::OnUserLeave(sfUser::SPtr userPtr)
{
    UMaterialInstanceDynamic* materialPtr;
    if (m_userMaterialMap.RemoveAndCopyValue(userPtr->Id(), materialPtr))
    {
        materialPtr->ClearFlags(EObjectFlags::RF_Standalone);// Allow unreal to destroy the material instance
    }
}