// Copyright (c) 2020 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"

#include <compiler/disable-ue4-macros.h>
#include <boost/optional.hpp>
#include "carla/road/Map.h"
#include <compiler/enable-ue4-macros.h>

#include "Vehicle/VehicleSpawnPoint.h"

#include "OpenDriveGenerator.generated.h"

class UMaterialInterface;


UCLASS()
class CARLA_API AProceduralMeshActor : public AActor
{
  GENERATED_BODY()
public:
  AProceduralMeshActor();

  UPROPERTY(Category = "Procedural Mesh Actor", VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
  UProceduralMeshComponent* MeshComponent;
};

UCLASS()
class CARLA_API AOpenDriveGenerator : public AActor
{
  GENERATED_BODY()

public:

  AOpenDriveGenerator(const FObjectInitializer &ObjectInitializer);

  /// Set the OpenDRIVE information as string and generates the
  /// queryable map structure.
  bool LoadOpenDrive(const FString &OpenDrive);

  /// Get the OpenDRIVE information as string.
  const FString &GetOpenDrive() const;

  /// Checks if the OpenDrive has been loaded and it's valid.
  bool IsOpenDriveValid() const;

  /// Generates the road and sidewalk mesh based on the OpenDRIVE information.
  void GenerateRoadMesh();

  /// Generates pole meshes based on the OpenDRIVE information.
  void GeneratePoles();

  /// Generates spawn points along the road.
  void GenerateSpawnPoints();

  /// Generate lane markings
  void GenerateLaneMarkings();

  void GenerateAll();

protected:

  virtual void BeginPlay() override;

  /// Determine the height where the spawners will be placed, relative to each
  /// RoutePlanner
  UPROPERTY(Category = "Spawners", EditAnywhere)
  float SpawnersHeight = 300.f;

  UPROPERTY(Category = "Spawners", EditAnywhere)
  TArray<AVehicleSpawnPoint *> VehicleSpawners;

  UPROPERTY(EditAnywhere)
  FString OpenDriveData;

  UPROPERTY(EditAnywhere)
  TArray<AActor *> ActorMeshList;

  UPROPERTY(Category = "OpenDRIVE Markings", EditAnywhere)
  TSoftObjectPtr<UMaterialInterface> LaneMarkingWhiteMaterial =
      TSoftObjectPtr<UMaterialInterface>(
          FSoftObjectPath(TEXT(
              "/Game/Carla/Static/GenericMaterials/LaneMarking/M_MarkingLane_W.M_MarkingLane_W")));

  UPROPERTY(Category = "OpenDRIVE Markings", EditAnywhere)
  TSoftObjectPtr<UMaterialInterface> LaneMarkingYellowMaterial =
      TSoftObjectPtr<UMaterialInterface>(
          FSoftObjectPath(TEXT(
              "/Game/Carla/Static/GenericMaterials/LaneMarking/M_MarkingLane_Y.M_MarkingLane_Y")));

  UPROPERTY(Category = "OpenDRIVE Markings", EditAnywhere)
  float DecalZOffset = 0.5f;
};
