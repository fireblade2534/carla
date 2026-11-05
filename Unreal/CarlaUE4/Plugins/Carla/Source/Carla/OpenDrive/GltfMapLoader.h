#pragma once

#include "CoreMinimal.h"
#include "glTFRuntimeAssetActor.h"

#include "GltfMapLoader.generated.h"

UCLASS()
class CARLA_API AGltfMapLoader
    : public AglTFRuntimeAssetActor
{
    GENERATED_BODY()

public:
    AGltfMapLoader();

protected:
    virtual void BeginPlay() override;

    virtual void ReceiveOnStaticMeshComponentCreated_Implementation(
        UStaticMeshComponent* Component,
        const FglTFRuntimeNode& Node
    ) override;
};