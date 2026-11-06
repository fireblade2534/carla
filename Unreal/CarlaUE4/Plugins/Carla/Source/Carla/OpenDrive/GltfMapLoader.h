#pragma once

#include "CoreMinimal.h"
#include "glTFRuntimeAssetActor.h"
#include "Carla/Traffic/GltfTrafficLightHead.h"

#include "GltfMapLoader.generated.h"

UCLASS()
class CARLA_API AGltfMapLoader
  : public AglTFRuntimeAssetActor
{
   GENERATED_BODY()

public:
  AGltfMapLoader();

  void BindTrafficLightSources();

protected:
  virtual void BeginPlay() override;

  virtual void ReceiveOnStaticMeshComponentCreated_Implementation(
    UStaticMeshComponent* Component,
    const FglTFRuntimeNode& Node
  ) override;

  void CollectSignalMesh(UStaticMeshComponent* Component, const FglTFRuntimeNode& Node);

  TMap<FString, FString> GuidToSignalId;

  TMap<FString, AGltfTrafficLightHead*> TrafficLightHeads;
};