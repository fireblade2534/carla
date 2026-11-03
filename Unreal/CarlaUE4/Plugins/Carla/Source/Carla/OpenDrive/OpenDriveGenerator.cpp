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
    const bool bIsYellow = (Index < MarkInfo.size()) && (MarkInfo[Index].find("yellow") != std::string::npos);
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

void AOpenDriveGenerator::GenerateCrosswalkMesh()
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

  // Zebra bars built from the crosswalk outline polygons instead of one
  // texture-mapped cover slab. The only crosswalk-ish texture in the
  // content is the speed-bump set (red field + stripe band), which rendered
  // the slabs as pink; real bar geometry with the same clean white
  // marking material the lane markings use needs no texture at all and
  // scales to any crosswalk size. The road surface below stays visible
  // between the bars, exactly like the painted original.
  auto& CarlaMap = UCarlaStatics::GetGameMode(GetWorld())->GetMap();
  const std::vector<carla::geom::Location> Zones = CarlaMap->GetAllCrosswalkZones();
  if (Zones.empty())
  {
    return;
  }

  constexpr float BarWidth = 0.4f;   // m, along the crossing direction
  constexpr float BarGap = 0.4f;     // m, between bars
  constexpr float EdgeMargin = 0.1f; // m, kept clear along the bar ends

  FProceduralCustomMesh MeshData;

  auto AddZebraPolygon = [&MeshData](const std::vector<carla::geom::Location> &Poly, float ZOffsetCm)
  {
    if (Poly.size() < 3)
    {
      return;
    }
    // Local frame: U along the longest edge (the crossing direction), V
    // across it (the crosswalk's depth along the road).
    size_t Longest = 0;
    float LongestLen2 = -1.0f;
    for (size_t k = 0; k < Poly.size(); ++k)
    {
      const auto &P = Poly[k];
      const auto &Q = Poly[(k + 1) % Poly.size()];
      const float DX = Q.x - P.x, DY = Q.y - P.y;
      const float Len2 = DX * DX + DY * DY;
      if (Len2 > LongestLen2)
      {
        LongestLen2 = Len2;
        Longest = k;
      }
    }
    if (LongestLen2 < 1e-4f)
    {
      return;
    }
    const float InvLen = 1.0f / FMath::Sqrt(LongestLen2);
    const FVector2D U((Poly[(Longest + 1) % Poly.size()].x - Poly[Longest].x) * InvLen,
                      (Poly[(Longest + 1) % Poly.size()].y - Poly[Longest].y) * InvLen);
    const FVector2D V(-U.Y, U.X);

    struct FLocalPt { float A, B, Z; };
    TArray<FLocalPt> Ring;
    float AMin = FLT_MAX, AMax = -FLT_MAX, BMin = FLT_MAX, BMax = -FLT_MAX;
    for (const auto &P : Poly)
    {
      const float A = P.x * U.X + P.y * U.Y;
      const float B = P.x * V.X + P.y * V.Y;
      Ring.Add({A, B, P.z});
      AMin = FMath::Min(AMin, A); AMax = FMath::Max(AMax, A);
      BMin = FMath::Min(BMin, B); BMax = FMath::Max(BMax, B);
    }

    // Surface height at a local point: barycentric over the outline's
    // triangle fan (the corners already carry the road elevation at their
    // own s -- see Map::GetAllCrosswalkZones); nearest corner as fallback.
    auto SurfaceZ = [&Ring](float A, float B) -> float
    {
      for (int32 K = 1; K + 1 < Ring.Num(); ++K)
      {
        const FLocalPt &P0 = Ring[0], &P1 = Ring[K], &P2 = Ring[K + 1];
        const float Denom = (P1.B - P2.B) * (P0.A - P2.A) + (P2.A - P1.A) * (P0.B - P2.B);
        if (FMath::Abs(Denom) < 1e-9f)
        {
          continue;
        }
        const float W0 = ((P1.B - P2.B) * (A - P2.A) + (P2.A - P1.A) * (B - P2.B)) / Denom;
        const float W1 = ((P2.B - P0.B) * (A - P2.A) + (P0.A - P2.A) * (B - P2.B)) / Denom;
        const float W2 = 1.0f - W0 - W1;
        if (W0 >= -1e-3f && W1 >= -1e-3f && W2 >= -1e-3f)
        {
          return W0 * P0.Z + W1 * P1.Z + W2 * P2.Z;
        }
      }
      float Best = FLT_MAX, Z = Ring[0].Z;
      for (const FLocalPt &P : Ring)
      {
        const float D = FMath::Square(P.A - A) + FMath::Square(P.B - B);
        if (D < Best) { Best = D; Z = P.Z; }
      }
      return Z;
    };

    const float Extent = AMax - AMin;
    const int32 NumBars = FMath::Max(1, FMath::FloorToInt((Extent - BarWidth) / (BarWidth + BarGap)) + 1);
    const float Start = AMin + 0.5f * (Extent - (NumBars * (BarWidth + BarGap) - BarGap));
    for (int32 Bar = 0; Bar < NumBars; ++Bar)
    {
      const float A0 = Start + Bar * (BarWidth + BarGap);
      const float A1 = A0 + BarWidth;
      TArray<FVector2D> Quad;
      Quad.Add(FVector2D(A0, BMin + EdgeMargin));
      Quad.Add(FVector2D(A1, BMin + EdgeMargin));
      Quad.Add(FVector2D(A1, BMax - EdgeMargin));
      Quad.Add(FVector2D(A0, BMax - EdgeMargin));

      const int32 First = MeshData.Vertices.Num();
      for (const FVector2D &L : Quad)
      {
        const float X = L.X * U.X + L.Y * V.X;
        const float Y = L.X * U.Y + L.Y * V.Y;
        const float Z = SurfaceZ(L.X, L.Y);
        MeshData.Vertices.Add(FVector(1e2f * X, 1e2f * Y, 1e2f * Z + ZOffsetCm));
        MeshData.Normals.Add(FVector::UpVector);
        MeshData.UV0.Add(FVector2D((L.X - A0) / BarWidth, (L.Y - BMin) / FMath::Max(BMax - BMin, 0.1f)));
      }
      for (int32 K = 1; K + 1 < Quad.Num(); ++K)
      {
        // Same winding convention as the converted LibCarla meshes
        // (flip the last two indices for Unreal's left-handed frame).
        MeshData.Triangles.Add(First);
        MeshData.Triangles.Add(First + K + 1);
        MeshData.Triangles.Add(First + K);
      }
    }
  };

  // Split the flat vertex list into outline polygons: each polygon ends
  // when its first vertex repeats (same convention as GetAllCrosswalkMesh).
  std::vector<carla::geom::Location> Current;
  size_t StartIdx = 0;
  for (size_t i = 0; i < Zones.size(); ++i)
  {
    if (i != StartIdx && Zones[StartIdx] == Zones[i])
    {
      AddZebraPolygon(Current, DecalZOffset);
      Current.clear();
      if (i >= Zones.size() - 1)
      {
        break;
      }
      StartIdx = i + 1;
      continue;
    }
    Current.push_back(Zones[i]);
  }

  if (MeshData.Vertices.Num() == 0)
  {
    return;
  }

  AProceduralMeshActor* TempActor = GetWorld()->SpawnActor<AProceduralMeshActor>();
  UProceduralMeshComponent *TempPMC = TempActor->MeshComponent;
  TempPMC->bUseAsyncCooking = true;
  TempPMC->bUseComplexAsSimpleCollision = true;
  // Markings are paint: the road mesh below carries the physics. Thin bar
  // meshes with collision would only add pointless micro-ledges.
  TempPMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  TempPMC->SetCastShadow(false);

  TempPMC->CreateMeshSection_LinearColor(
      0,
      MeshData.Vertices,
      MeshData.Triangles,
      MeshData.Normals,
      MeshData.UV0,
      TArray<FLinearColor>(), // VertexColor
      TArray<FProcMeshTangent>(), // Tangents
      false); // no collision

  if (UMaterialInterface* ResolvedCrosswalkMaterial = LaneMarkingWhiteMaterial.LoadSynchronous())
  {
    TempPMC->SetMaterial(0, ResolvedCrosswalkMaterial);
  }

  // CARLA has no dedicated CityObjectLabel for crosswalks; the ue4 tooling
  // (CarlaTools OpenDriveToMap) tags road markings as RoadLines, mirrored here.
  TempPMC->ComponentTags.AddUnique(FName(*ATagger::GetTagAsString(carla::rpc::CityObjectLabel::RoadLines)));

  if (!Parameters.enable_mesh_visibility)
  {
    TempActor->SetActorHiddenInGame(true);
  }

  ActorMeshList.Add(TempActor);
}

void AOpenDriveGenerator::GenerateAll()
{
  GenerateRoadMesh();
  GenerateLaneMarkings();
  GenerateCrosswalkMesh();
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
