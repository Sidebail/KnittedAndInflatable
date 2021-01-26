#include "sfMeshStandInTranslator.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/Consts.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/sfPropertyUtil.h"
#include "../../Public/sfLoader.h"
#include "../../Public/sfUnrealUtils.h"

#include <Engine/StaticMesh.h>
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 25
#include <Runtime/RawMesh/Public/RawMesh.h>
#else
#include <Developer/RawMesh/Public/RawMesh.h>
#endif

sfMeshStandInTranslator::sfMeshStandInTranslator()
{
    m_onGetAssetPropertyHandle = sfPropertyManager::Get().OnGetAssetProperty().AddRaw(this,
        &sfMeshStandInTranslator::SendBounds);
    m_materialPtr = LoadObject<UMaterialInterface>(nullptr, TEXT("/SceneFusion/StandIns/QuestionMaterial"));
}

sfMeshStandInTranslator::~sfMeshStandInTranslator()
{
    sfPropertyManager::Get().OnGetAssetProperty().Remove(m_onGetAssetPropertyHandle);
}

void sfMeshStandInTranslator::CleanUp()
{
    m_meshBoundsPtr = nullptr;
}

void sfMeshStandInTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    m_meshBoundsPtr = objPtr->Property()->AsDict();
}

void sfMeshStandInTranslator::Generate(const FString& path, UObject* uobjPtr)
{
    UStaticMesh* meshPtr = Cast<UStaticMesh>(uobjPtr);
    if (meshPtr == nullptr)
    {
        return;
    }
    FBox bounds = GetBounds(SanitizePath(path));
    FRawMesh mesh;
    // Generate vertices
    mesh.VertexPositions.Add(FVector{ bounds.Min.X, bounds.Min.Y, bounds.Min.Z });
    mesh.VertexPositions.Add(FVector{ bounds.Min.X, bounds.Min.Y, bounds.Max.Z });
    mesh.VertexPositions.Add(FVector{ bounds.Min.X, bounds.Max.Y, bounds.Min.Z });
    mesh.VertexPositions.Add(FVector{ bounds.Min.X, bounds.Max.Y, bounds.Max.Z });
    mesh.VertexPositions.Add(FVector{ bounds.Max.X, bounds.Min.Y, bounds.Min.Z });
    mesh.VertexPositions.Add(FVector{ bounds.Max.X, bounds.Min.Y, bounds.Max.Z });
    mesh.VertexPositions.Add(FVector{ bounds.Max.X, bounds.Max.Y, bounds.Min.Z });
    mesh.VertexPositions.Add(FVector{ bounds.Max.X, bounds.Max.Y, bounds.Max.Z });
    // Generate triangles
    mesh.WedgeIndices.Add(2); mesh.WedgeIndices.Add(1); mesh.WedgeIndices.Add(0);
    mesh.WedgeIndices.Add(1); mesh.WedgeIndices.Add(2); mesh.WedgeIndices.Add(3);
    mesh.WedgeIndices.Add(4); mesh.WedgeIndices.Add(5); mesh.WedgeIndices.Add(6);
    mesh.WedgeIndices.Add(7); mesh.WedgeIndices.Add(6); mesh.WedgeIndices.Add(5);
    mesh.WedgeIndices.Add(0); mesh.WedgeIndices.Add(1); mesh.WedgeIndices.Add(4);
    mesh.WedgeIndices.Add(5); mesh.WedgeIndices.Add(4); mesh.WedgeIndices.Add(1);
    mesh.WedgeIndices.Add(6); mesh.WedgeIndices.Add(3); mesh.WedgeIndices.Add(2);
    mesh.WedgeIndices.Add(3); mesh.WedgeIndices.Add(6); mesh.WedgeIndices.Add(7);
    mesh.WedgeIndices.Add(4); mesh.WedgeIndices.Add(2); mesh.WedgeIndices.Add(0);
    mesh.WedgeIndices.Add(2); mesh.WedgeIndices.Add(4); mesh.WedgeIndices.Add(6);
    mesh.WedgeIndices.Add(1); mesh.WedgeIndices.Add(3); mesh.WedgeIndices.Add(5);
    mesh.WedgeIndices.Add(7); mesh.WedgeIndices.Add(5); mesh.WedgeIndices.Add(3);
    for (int i = 0; i < mesh.WedgeIndices.Num(); i++)
    {
        mesh.WedgeColors.Add(FColor::White);
        mesh.WedgeTangentX.Add(FVector::ZeroVector);
        mesh.WedgeTangentY.Add(FVector::ZeroVector);
        mesh.WedgeTangentZ.Add(FVector::ZeroVector);
    }
    // Generate UVs
    int numFaces = mesh.WedgeIndices.Num() / 3;
    for (int i = 0; i < numFaces; i++)
    {
        mesh.FaceMaterialIndices.Add(0);
        mesh.FaceSmoothingMasks.Add(0xFFFFFFFF); // Phong

        FVector2D uv1, uv2, uv3;
        const FVector& v1 = mesh.VertexPositions[mesh.WedgeIndices[i * 3]];
        const FVector& v2 = mesh.VertexPositions[mesh.WedgeIndices[i * 3 + 1]];
        const FVector& v3 = mesh.VertexPositions[mesh.WedgeIndices[i * 3 + 2]];
        EAxis::Type x;
        EAxis::Type y;
        bool invertX;
        if (v1.X == v2.X && v1.X == v3.X)
        {
            x = EAxis::Type::Y;
            y = EAxis::Type::Z;
            invertX = v1.X > 0;
        }
        else if (v1.Y == v2.Y && v1.Y == v3.Y)
        {
            x = EAxis::Type::X;
            y = EAxis::Type::Z;
            invertX = v1.Y < 0;
        }
        else
        {
            x = EAxis::Type::X;
            y = EAxis::Type::Y;
            invertX = v1.Z > 0;
        }
        float minX = FMath::Min3(v1.GetComponentForAxis(x), v2.GetComponentForAxis(x), v3.GetComponentForAxis(x));
        float maxX = FMath::Max3(v1.GetComponentForAxis(x), v2.GetComponentForAxis(x), v3.GetComponentForAxis(x));
        float minY = FMath::Min3(v1.GetComponentForAxis(y), v2.GetComponentForAxis(y), v3.GetComponentForAxis(y));
        float maxY = FMath::Max3(v1.GetComponentForAxis(y), v2.GetComponentForAxis(y), v3.GetComponentForAxis(y));
        float originX = (maxX + minX) / 2;
        float originY = (maxY + minY) / 2;
        float size = FMath::Min(maxX - minX, maxY - minY);
        uv1.X = (v1.GetComponentForAxis(x) - originX) / size + 0.5f;
        uv1.Y = 1 - ((v1.GetComponentForAxis(y) - originY) / size + 0.5f);
        uv2.X = (v2.GetComponentForAxis(x) - originX) / size + 0.5f;
        uv2.Y = 1 - ((v2.GetComponentForAxis(y) - originY) / size + 0.5f);
        uv3.X = (v3.GetComponentForAxis(x) - originX) / size + 0.5f;
        uv3.Y = 1 - ((v3.GetComponentForAxis(y) - originY) / size + 0.5f);
        if (invertX)
        {
            uv1.X = 1 - uv1.X;
            uv2.X = 1 - uv2.X;
            uv3.X = 1 - uv3.X;
        }

        for (int uv = 0; uv < MAX_MESH_TEXTURE_COORDS; uv++)
        {
            mesh.WedgeTexCoords[uv].Add(uv1);
            mesh.WedgeTexCoords[uv].Add(uv2);
            mesh.WedgeTexCoords[uv].Add(uv3);
        }
    }
    sfUnrealUtils::GetSourceModels(meshPtr).Emplace();
    FStaticMeshSourceModel& model = sfUnrealUtils::GetSourceModels(meshPtr)[0];
    model.RawMeshBulkData->SaveRawMesh(mesh);
    model.BuildSettings.bRecomputeNormals = true;
    model.BuildSettings.bRecomputeTangents = true;
    model.BuildSettings.bUseMikkTSpace = false;
    model.BuildSettings.bGenerateLightmapUVs = true;
    model.BuildSettings.bBuildAdjacencyBuffer = false;
    model.BuildSettings.bBuildReversedIndexBuffer = false;
    model.BuildSettings.bUseFullPrecisionUVs = false;
    model.BuildSettings.bUseHighPrecisionTangentBasis = false;

    meshPtr->StaticMaterials.Add(m_materialPtr);
    sfUnrealUtils::GetSectionInfoMap(meshPtr).Set(0, 0, FMeshSectionInfo(0));

    meshPtr->CreateBodySetup();
    meshPtr->SetLightingGuid();
    meshPtr->PostEditChange();
}

void sfMeshStandInTranslator::SendBounds(UObject* uobjPtr)
{
    UStaticMesh* meshPtr = Cast<UStaticMesh>(uobjPtr);
    if (meshPtr == nullptr)
    {
        return;
    }
    if (m_meshBoundsPtr == nullptr)
    {
        if (!SceneFusion::IsSessionCreator || SceneFusion::Service->Session() == nullptr)
        {
            return;
        }
        m_meshBoundsPtr = sfDictionaryProperty::Create();
        sfObject::SPtr objPtr = sfObject::Create(sfType::MeshBounds, m_meshBoundsPtr);
        SceneFusion::Service->Session()->Create(objPtr);
    }
   sfName path = SanitizePath(uobjPtr->GetPathName());
   if (!m_meshBoundsPtr->HasKey(path))
   {
       m_meshBoundsPtr->Set(path, sfPropertyUtil::FromBox(meshPtr->GetBoundingBox()));
   }
}

FBox sfMeshStandInTranslator::GetBounds(const sfName& path)
{
    sfProperty::SPtr propPtr;
    if (m_meshBoundsPtr != nullptr && m_meshBoundsPtr->TryGet(path, propPtr))
    {
        return sfPropertyUtil::ToBox(propPtr);
    }
    return FBox{ FVector{ -50.0f, -50.0f, -50.0f }, FVector{ 50.0f, 50.0f, 50.0f } };
}

sfName sfMeshStandInTranslator::SanitizePath(FString path)
{
    // Replace illegal characters with double slashes
    path.ReplaceInline(TEXT("."), TEXT("//"));
    path.ReplaceInline(TEXT("["), TEXT("//"));
    path.ReplaceInline(TEXT("]"), TEXT("//"));
    return TCHAR_TO_UTF8(*path);
}