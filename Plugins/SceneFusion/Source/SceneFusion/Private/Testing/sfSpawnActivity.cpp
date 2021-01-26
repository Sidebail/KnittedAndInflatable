#include "sfSpawnActivity.h"

#include <Log.h>
#include <Editor.h>
#include <Engine/StaticMeshActor.h>
#include <Runtime/Core/Public/HAL/FileManagerGeneric.h>
#include <Runtime/Core/Public/Misc/Paths.h>
#include <Runtime/Core/Public/Containers/Ticker.h>
#include <Runtime/CoreUObject/Public/UObject/UObjectGlobals.h>
#include <Runtime/Engine/Classes/Animation/SkeletalMeshActor.h>
#include <Runtime/Engine/Classes/Components/SkeletalMeshComponent.h>
#include <Runtime/Engine/Classes/Particles/Emitter.h>
#include <Runtime/Engine/Classes/Particles/ParticleSystemComponent.h>

#define LOG_CHANNEL "sfSpawnActivity"

sfSpawnActivity::sfSpawnActivity(const FString& name, float weight)
    : sfBaseActivity{ name, weight }
{}

void sfSpawnActivity::HandleArgs(const TArray<FString>& args, int index)
{
    for (; index < args.Num(); index++)
    {
        const FString& path = args[index];
        if (path.Equals("-r", ESearchCase::IgnoreCase) || path.Equals("-reset", ESearchCase::IgnoreCase))
        {
            m_assetPaths.Empty();
            continue;
        }
        FString gamePath = "/Game/" + path;
        FString fullPath = FPaths::ProjectContentDir() + path;
        if (!FPaths::DirectoryExists(fullPath))
        {
            KS::Log::Warning(std::string(TCHAR_TO_UTF8(*fullPath)) + " not found.", LOG_CHANNEL);
            return;
        }
        TArray<FString> assetNames;
        FFileManagerGeneric::Get().FindFiles(assetNames, *fullPath, *FString(".uasset"));
        if (assetNames.Num() == 0)
        {
            KS::Log::Warning("Found no assets at " + std::string(TCHAR_TO_UTF8(*fullPath)), LOG_CHANNEL);
            return;
        }
        for (const FString& asset : assetNames)
        {
            FString assetPath = gamePath + "/" + asset;
            assetPath = assetPath.LeftChop(7);// Remove ".uasset" from the end
            m_assetPaths.Add(assetPath);
        }
    }
}

void sfSpawnActivity::Start()
{
    if (m_assetPaths.Num() == 0)
    {
        KS::Log::Warning("Spawn activity is not configured with an asset path.", LOG_CHANNEL);
        return;
    }

    FString& path = m_assetPaths[rand() % m_assetPaths.Num()];
    GIsSlowTask = true;
    GIsEditorLoadingPackage = true;
    UObject* assetPtr = LoadObject<UObject>(nullptr, *path);
    GIsEditorLoadingPackage = false;
    GIsSlowTask = false;

    UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
    FVector location = FMath::VRand() * 5000;
    FRotator rotation{ FMath::FRandRange(-90, 90), FMath::FRandRange(-90, 90), FMath::FRandRange(-90, 90) };

    UStaticMesh* meshPtr = Cast<UStaticMesh>(assetPtr);
    if (meshPtr != nullptr)
    {
        AStaticMeshActor* actorPtr = worldPtr->SpawnActor<AStaticMeshActor>(location, rotation);
        actorPtr->GetStaticMeshComponent()->SetStaticMesh(meshPtr);
        return;
    }

    USkeletalMesh* skeletalMeshPtr = Cast<USkeletalMesh>(assetPtr);
    if (skeletalMeshPtr != nullptr)
    {
        ASkeletalMeshActor* actorPtr = worldPtr->SpawnActor<ASkeletalMeshActor>(location, rotation);
        actorPtr->GetSkeletalMeshComponent()->SetSkeletalMesh(skeletalMeshPtr);
        return;
    }

    UParticleSystem* particleSystemPtr = Cast<UParticleSystem>(assetPtr);
    if (particleSystemPtr != nullptr)
    {
        AEmitter* actorPtr = worldPtr->SpawnActor<AEmitter>(location, rotation);
        actorPtr->GetParticleSystemComponent()->SetTemplate(particleSystemPtr);
        return;
    }
}

#undef LOG_CHANNEL