// Copyright (c) 2020 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "Carla.h"
#include "Carla/Game/CarlaStatics.h"
#include "OpenDriveGenerator.h"
#include "Traffic/TrafficLightManager.h"
#include "Util/ProceduralCustomMesh.h"
#include <cstdio>

#include <compiler/disable-ue4-macros.h>
#include <carla/opendrive/OpenDriveParser.h>
#include <carla/rpc/String.h>
#include <compiler/enable-ue4-macros.h>

#include "Game/Tagger.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Classes/Interfaces/Interface_CollisionDataProvider.h"
#include "PhysicsCore/Public/BodySetupEnums.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
  struct FOpenDriveMeshMapping
  {
    FString MeshPath;
    FTransform LocalCorrection = FTransform::Identity;
  };


  bool ResolveObjectMapping(
      const carla::road::Object& Object,
      FOpenDriveMeshMapping& OutMapping)
  {
    const FString ObjectName =
        UTF8_TO_TCHAR(Object.GetName().c_str());

    const FString Section =
        FString(TEXT("OpenDrive.Object.Name.")) + ObjectName;

    FString MeshPath;

    if (!GConfig || !GConfig->GetString(*Section, TEXT("Mesh"), MeshPath, GGameIni) || MeshPath.IsEmpty())
    {
      return false;
    }

    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;

    GConfig->GetVector(*Section, TEXT("Location"), Location, GGameIni);

    GConfig->GetRotator(*Section, TEXT("Rotation"), Rotation, GGameIni);

    GConfig->GetVector(*Section, TEXT("Scale"), Scale, GGameIni);

    OutMapping.MeshPath = MoveTemp(MeshPath);
    OutMapping.LocalCorrection = FTransform(Rotation, Location, Scale);

    return true;
  }
}

AProceduralMeshActor::AProceduralMeshActor()
{
  PrimaryActorTick.bCanEverTick = false;
  MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RootComponent"));
  RootComponent = MeshComponent;
}

AOpenDriveGenerator::AOpenDriveGenerator(const FObjectInitializer &ObjectInitializer)
  : Super(ObjectInitializer)
{
  PrimaryActorTick.bCanEverTick = false;
  RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
  SetRootComponent(RootComponent);
  RootComponent->Mobility = EComponentMobility::Static;
}

bool AOpenDriveGenerator::LoadOpenDrive(const FString &OpenDrive)
{
  using OpenDriveLoader = carla::opendrive::OpenDriveParser;

  if (OpenDrive.IsEmpty())
  {
    UE_LOG(LogCarla, Error, TEXT("The OpenDrive is empty"));
    return false;
  }

  OpenDriveData = OpenDrive;
  return true;
}

const FString &AOpenDriveGenerator::GetOpenDrive() const
{
  return OpenDriveData;
}

bool AOpenDriveGenerator::IsOpenDriveValid() const
{
  return UCarlaStatics::GetGameMode(GetWorld())->GetMap().has_value();
}

void AOpenDriveGenerator::GenerateRoadMesh()
{
  if (!IsOpenDriveValid())
  {
    UE_LOG(LogCarla, Error, TEXT("The OpenDrive has not been loaded"));
    return;
  }

  carla::rpc::OpendriveGenerationParameters Parameters;
  UCarlaGameInstance * GameInstance = UCarlaStatics::GetGameInstance(GetWorld());
  if(GameInstance)
  {
    Parameters = GameInstance->GetOpendriveGenerationParameters();
  }
  else
  {
    carla::log_warning("Missing game instance");
  }

  auto& CarlaMap = UCarlaStatics::GetGameMode(GetWorld())->GetMap();
  const auto Meshes = CarlaMap->GenerateChunkedMesh(Parameters);
  for (const auto &Mesh : Meshes) {
    if (!Mesh->GetVertices().size())
    {
      continue;
    }
    AProceduralMeshActor* TempActor = GetWorld()->SpawnActor<AProceduralMeshActor>();
    UProceduralMeshComponent *TempPMC = TempActor->MeshComponent;
    TempPMC->bUseAsyncCooking = true;
    TempPMC->bUseComplexAsSimpleCollision = true;
    TempPMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    const FProceduralCustomMesh MeshData = *Mesh;
    TempPMC->CreateMeshSection_LinearColor(
        0,
        MeshData.Vertices,
        MeshData.Triangles,
        MeshData.Normals,
        TArray<FVector2D>(), // UV0
        TArray<FLinearColor>(), // VertexColor
        TArray<FProcMeshTangent>(), // Tangents
        true); // Create collision

    ActorMeshList.Add(TempActor);
  }

  if(!Parameters.enable_mesh_visibility)
  {
    for(AActor * actor : ActorMeshList)
    {
      actor->SetActorHiddenInGame(true);
    }
  }

  // // Build collision data
  // FTriMeshCollisionData CollisitonData;
  // CollisitonData.bDeformableMesh = false;
  // CollisitonData.bDisableActiveEdgePrecompute = false;
  // CollisitonData.bFastCook = false;
  // CollisitonData.bFlipNormals = false;
  // CollisitonData.Indices = TriIndices;
  // CollisitonData.Vertices = Vertices;

  // RoadMesh->ContainsPhysicsTriMeshData(true);
  // bool Success = RoadMesh->GetPhysicsTriMeshData(&CollisitonData, true);
  // if (!Success)
  // {
  //   UE_LOG(LogCarla, Error, TEXT("The road collision mesh could not be generated!"));
  // }
}

void AOpenDriveGenerator::GenerateObjects()
{
  if (!IsOpenDriveValid())
  {
    UE_LOG(
        LogCarla,
        Error,
        TEXT("Cannot generate objects: OpenDRIVE map is missing"));

    return;
  }

  // Replace props from a previous call.
  for (AActor* Actor : ObjectActors)
  {
    if (IsValid(Actor))
    {
      Actor->Destroy();
    }
  }

  ObjectActors.Empty();

  carla::rpc::OpendriveGenerationParameters Parameters;

  if (UCarlaGameInstance* GameInstance =
          UCarlaStatics::GetGameInstance(GetWorld()))
  {
    Parameters = GameInstance->GetOpendriveGenerationParameters();
  }

  const auto& CarlaMap = UCarlaStatics::GetGameMode(GetWorld())->GetMap();

  for (const auto& ObjectPair : CarlaMap->GetObjects())
  {
    const carla::road::Object& Object = *ObjectPair.second;

    const FString ObjectName = UTF8_TO_TCHAR(Object.GetName().c_str());

    if (Object.GetType() == "crosswalk" ||
        ObjectName.Contains(
            TEXT("crosswalk"),
            ESearchCase::IgnoreCase))
    {
      continue;
    }

    FOpenDriveMeshMapping Mapping;

    if (!ResolveObjectMapping(Object, Mapping))
    {
      UE_LOG(
          LogCarla,
          Warning,
          TEXT("No asset mapping for XODR object %u: %s"),
          Object.GetObjectId(),
          *ObjectName);

      continue;
    }

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Mapping.MeshPath);

    if (!Mesh)
    {
      UE_LOG(
          LogCarla,
          Error,
          TEXT(
              "Could not load mesh for XODR object %u: %s"),
          Object.GetObjectId(),
          *Mapping.MeshPath);

      continue;
    }

    

    const FTransform ObjectTransform(CarlaMap->ComputeObjectTransform(Object));

    const FTransform MeshTransform = Mapping.LocalCorrection * ObjectTransform;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AStaticMeshActor* Actor =
        GetWorld()->SpawnActor<AStaticMeshActor>(
            MeshTransform.GetLocation(),
            MeshTransform.Rotator(),
            SpawnParams);

    if (!Actor)
    {
      UE_LOG(
          LogCarla,
          Error,
          TEXT("Failed to spawn XODR object %u"),
          Object.GetObjectId());

      continue;
    }

    UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent();

    MeshComponent->SetMobility(EComponentMobility::Movable);

    MeshComponent->SetStaticMesh(Mesh);

    Actor->SetActorTransform(MeshTransform);

    MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));

    MeshComponent->SetMobility(EComponentMobility::Static);

    Actor->SetActorHiddenInGame(!Parameters.enable_mesh_visibility);

    Actor->Tags.Add(
        FName(*FString::Printf(
            TEXT("OpenDriveObject_%u"),
            Object.GetObjectId())));

    ObjectActors.Add(Actor);
    const FVector Position = Actor->GetActorLocation();

    UE_LOG(
        LogCarla,
        Log,
        TEXT("Spawned XODR object %u: %s"),
        Object.GetObjectId(),
        *ObjectName);
  }

  UE_LOG(
      LogCarla,
      Log,
      TEXT("Generated %d OpenDRIVE prop actors"),
      ObjectActors.Num());
}

void AOpenDriveGenerator::GeneratePoles()
{
  if (!IsOpenDriveValid())
  {
    UE_LOG(LogCarla, Error, TEXT("The OpenDrive has not been loaded"));
    return;
  }
  /// TODO: To implement
}

void AOpenDriveGenerator::GenerateSpawnPoints()
{
  if (!IsOpenDriveValid())
  {
    UE_LOG(LogCarla, Error, TEXT("The OpenDrive has not been loaded"));
    return;
  }
  auto& CarlaMap = UCarlaStatics::GetGameMode(GetWorld())->GetMap();
  const auto Waypoints = CarlaMap->GenerateWaypointsOnRoadEntries();
  for (const auto &Wp : Waypoints)
  {
    const FTransform Trans = CarlaMap->ComputeTransform(Wp);
    AVehicleSpawnPoint *Spawner = GetWorld()->SpawnActor<AVehicleSpawnPoint>();
    Spawner->SetActorRotation(Trans.GetRotation());
    Spawner->SetActorLocation(Trans.GetTranslation() + FVector(0.f, 0.f, SpawnersHeight));
    VehicleSpawners.Add(Spawner);
  }
}


void AOpenDriveGenerator::GenerateLaneMarkings()
{
  if (!IsOpenDriveValid())
  {
    UE_LOG(LogCarla, Error, TEXT("The OpenDrive has not been loaded"));
    return;
  }

  carla::rpc::OpendriveGenerationParameters Parameters;
  UCarlaGameInstance * GameInstance = UCarlaStatics::GetGameInstance(GetWorld());
  if (GameInstance)
  {
    Parameters = GameInstance->GetOpendriveGenerationParameters();
  }
  else
  {
    carla::log_warning("Missing game instance");
  }

  auto& CarlaMap = UCarlaStatics::GetGameMode(GetWorld())->GetMap();

  // Map::GenerateLineMarkings filters roads by a min/max bounding box; it
  // backs the tiled/streamed generation path (GenerateOrderedChunkedMeshInLocations).
  // generate_opendrive_world builds the whole map in one shot, so pass an
  // effectively unbounded box instead. Map::FilterRoadsByPosition expects
  // minpos.y > maxpos.y (Unreal's left-handed Y vs. OpenDRIVE), mirrored here.
  constexpr float BoundsExtentMeters = 1e6f;
  const carla::geom::Vector3D MinPos(-BoundsExtentMeters, BoundsExtentMeters, -BoundsExtentMeters);
  const carla::geom::Vector3D MaxPos(BoundsExtentMeters, -BoundsExtentMeters, BoundsExtentMeters);

  std::fprintf(stderr, "Generating road markings\n");

  std::vector<std::string> MarkInfo;
  const auto Markings = CarlaMap->GenerateLineMarkings(Parameters, MinPos, MaxPos, MarkInfo);

  UE_LOG(
    LogTemp, Warning,
    TEXT("GenerateLineMarkings returned %d meshes"),
    static_cast<int32>(Markings.size()));

  UMaterialInterface* ResolvedWhite = LaneMarkingWhiteMaterial.LoadSynchronous();
  UMaterialInterface* ResolvedYellow = LaneMarkingYellowMaterial.LoadSynchronous();

  if (!ResolvedWhite || !ResolvedYellow)
  {
    UE_LOG(
        LogCarla,
        Error,
        TEXT("Lane-marking materials failed to load: white=%s, yellow=%s"),
        ResolvedWhite ? TEXT("loaded") : TEXT("MISSING"),
        ResolvedYellow ? TEXT("loaded") : TEXT("MISSING"));

    return;
  }

  size_t Index = 0;
  for (const auto &Mark : Markings)
  {
    // Map::GenerateLineMarkings/MeshFactory::GenerateLaneMarkForRoad push one
    // color tag per lane pass but only push a mesh when it produced valid
    // geometry, so MarkInfo and Markings can fall out of sync (matches
    // CarlaTools OpenDriveToMap::GenerateLaneMarks, which assumes the same
    // 1:1 index). Guard the lookup so a mismatch can't read out of bounds.
    const bool bIsYellow = (Index < MarkInfo.size()) &&
        (MarkInfo[Index].find("yellow") != std::string::npos);
    ++Index;

    if (!Mark || !Mark->GetVertices().size())
    {
      continue;
    }

    std::fprintf(stderr, "Generating road marking mesh %zu\n", Index);
    AProceduralMeshActor* TempActor = GetWorld()->SpawnActor<AProceduralMeshActor>();

    if (!TempActor)
    {
      continue;
    }

    UProceduralMeshComponent *TempPMC = TempActor->MeshComponent;
    TempPMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FProceduralCustomMesh MeshData = *Mark;
    for (FVector &Vertex : MeshData.Vertices)
    {
      // Small z-offset above the road surface to avoid z-fighting.
      Vertex.Z += DecalZOffset;
    }

    TempPMC->CreateMeshSection_LinearColor(
        0,
        MeshData.Vertices,
        MeshData.Triangles,
        MeshData.Normals,
        MeshData.UV0,
        TArray<FLinearColor>(), // VertexColor
        TArray<FProcMeshTangent>(), // Tangents
        false); // Create collision -- flush with the road, no separate collision needed

    UMaterialInterface* SectionMaterial = bIsYellow ? ResolvedYellow : ResolvedWhite;
    if (SectionMaterial)
    {
      TempPMC->SetMaterial(0, SectionMaterial);
    }

    ATagger::SetStencilValue(
      *TempPMC,
      carla::rpc::CityObjectLabel::RoadLines,
      true);

    TempPMC->ComponentTags.Add(FName(*ATagger::GetTagAsString(carla::rpc::CityObjectLabel::RoadLines)));

    if (!Parameters.enable_mesh_visibility)
    {
      TempActor->SetActorHiddenInGame(true);
    }

    ActorMeshList.Add(TempActor);
  }
}

void AOpenDriveGenerator::GenerateAll()
{
  GenerateRoadMesh();
  GenerateLaneMarkings();
  GenerateSpawnPoints();
  GeneratePoles();

  GenerateObjects();
}

void AOpenDriveGenerator::BeginPlay()
{
  Super::BeginPlay();

  const FString XodrContent = UOpenDrive::GetXODR(GetWorld());
  LoadOpenDrive(XodrContent);

  GenerateAll();

  auto World = GetWorld();
  check(World != nullptr);

  // Autogenerate signals
  AActor* TrafficLightManagerActor =  UGameplayStatics::GetActorOfClass(World, ATrafficLightManager::StaticClass());
  if(TrafficLightManagerActor == nullptr) {
    World->SpawnActor<ATrafficLightManager>();
  }
}
