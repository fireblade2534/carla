#include "GltfMapLoader.h"

#include "Components/StaticMeshComponent.h"
#include "glTFRuntimeFunctionLibrary.h"
#include "Misc/Paths.h"

AGltfMapLoader::AGltfMapLoader()
{
    // Our environment is static.
    PrimaryActorTick.bCanEverTick = false;

    bAllowCameras = false;
    bAllowLights = false;

    // Disable animations
    bAllowNodeAnimations = false;
    bAllowSkeletalAnimations = false;
    bAllowPoseAnimations = false;

    StaticMeshConfig.bBuildComplexCollision = true;

    StaticMeshConfig.CollisionComplexity =
        ECollisionTraceFlag::CTF_UseComplexAsSimple;

    StaticMeshConfig.bAllowCPUAccess = true;
}

void AGltfMapLoader::BeginPlay()
{
    // Location of RPC file
    const FString AbsoluteGLTFPath = FPaths::ConvertRelativePathToFull(
      FPaths::ProjectContentDir() + "Carla/Maps/OpenDrive/GltfMap.gltf");

    if (!FPaths::FileExists(AbsoluteGLTFPath))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("GLTF file does not exist: %s"),
            *AbsoluteGLTFPath
        );

        return;
    }

    FglTFRuntimeConfig Config;

    Asset = UglTFRuntimeFunctionLibrary::glTFLoadAssetFromFilename(
        AbsoluteGLTFPath,
        false,
        Config
    );

    Super::BeginPlay();

    if (!Asset)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Unable to load GLTF asset")
        );
        return;
    }

    TArray<UStaticMeshComponent*> Components;
    GetComponents<UStaticMeshComponent>(Components);

    int32 MeshCount = 0;

    for (UStaticMeshComponent* Component : Components)
    {
        if (!Component || !Component->GetStaticMesh())
        {
            continue;
        }

        Component->SetSimulatePhysics(false);

        Component->SetCollisionProfileName(
            TEXT("BlockAll")
        );

        Component->SetCollisionEnabled(
            ECollisionEnabled::QueryAndPhysics
        );

        ++MeshCount;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Gltf map loaded with %d static mesh components"),
        MeshCount
    );
}