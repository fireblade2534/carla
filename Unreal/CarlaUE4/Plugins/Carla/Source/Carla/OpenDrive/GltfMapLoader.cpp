#include "GltfMapLoader.h"

#include "Carla/Game/Tagger.h"
#include "Carla/Game/TaggedComponent.h"
#include "Carla/Game/CarlaGameModeBase.h"
#include "Carla/Traffic/TrafficLightManager.h"
#include "Carla/Traffic/TrafficLightComponent.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Carla/Traffic/TrafficLightState.h"
#include "Carla/Traffic/TrafficLightBase.h"
#include "glTFRuntimeFunctionLibrary.h"
#include "Misc/Paths.h"
#include <cstdio>

#include <compiler/disable-ue4-macros.h>
#include <carla/road/Map.h>
#include <carla/road/Signal.h>
#include <compiler/enable-ue4-macros.h>

using Label = carla::rpc::CityObjectLabel;

// Convert RoadRunner's categories to CARLA categories.
static Label ToCarlaLabel(const FString& Type)
{
  // RoadRunner default categories
  if (Type == TEXT("Road"))       return Label::Roads;
  if (Type == TEXT("Marking"))    return Label::RoadLines;
  if (Type == TEXT("Sidewalk"))   return Label::Sidewalks;
  if (Type == TEXT("Curb"))       return Label::Sidewalks;
  if (Type == TEXT("Gutter"))     return Label::Roads;
  if (Type == TEXT("Ground"))     return Label::Ground;
  if (Type == TEXT("Building"))   return Label::Buildings;
  if (Type == TEXT("Vehicle"))    return Label::Car;
  if (Type == TEXT("Bike"))       return Label::Bicycle;
  if (Type == TEXT("Pedestrian")) return Label::Pedestrians;
  if (Type == TEXT("Sign"))       return Label::TrafficSigns;
  if (Type == TEXT("Signal"))     return Label::TrafficLight;
  if (Type == TEXT("Foliage"))    return Label::Vegetation;
  if (Type == TEXT("Prop"))       return Label::Static;

  // Additional terrain and vegetation categories
  if (Type == TEXT("Terrain"))    return Label::Terrain;
  if (Type == TEXT("Grass"))      return Label::Terrain;
  if (Type == TEXT("Tree"))       return Label::Vegetation;
  if (Type == TEXT("Bush"))       return Label::Vegetation;
  if (Type == TEXT("Vegetation")) return Label::Vegetation;

  // Additional road marking categories
  if (Type == TEXT("RoadLine"))       return Label::RoadLines;
  if (Type == TEXT("RoadLines"))      return Label::RoadLines;
  if (Type == TEXT("Crosswalk"))      return Label::RoadLines;
  if (Type == TEXT("LaneMarking"))    return Label::RoadLines;
  if (Type == TEXT("DashedMarking"))  return Label::RoadLines;
  if (Type == TEXT("SolidMarking"))   return Label::RoadLines;
  if (Type == TEXT("DoubleMarking"))  return Label::RoadLines;

  // Additional vehicles
  if (Type == TEXT("Car"))        return Label::Car;
  if (Type == TEXT("Truck"))      return Label::Truck;
  if (Type == TEXT("Bus"))        return Label::Bus;
  if (Type == TEXT("Train"))      return Label::Train;
  if (Type == TEXT("Motorcycle")) return Label::Motorcycle;
  if (Type == TEXT("Bicycle"))    return Label::Bicycle;
  if (Type == TEXT("Rider"))      return Label::Rider;

  // Additional roadside objects
  if (Type == TEXT("TrafficLight")) return Label::TrafficLight;
  if (Type == TEXT("TrafficSign"))  return Label::TrafficSigns;
  if (Type == TEXT("Pole"))         return Label::Poles;
  if (Type == TEXT("Wall"))         return Label::Walls;
  if (Type == TEXT("Fence"))        return Label::Fences;
  if (Type == TEXT("GuardRail"))    return Label::GuardRail;
  if (Type == TEXT("Guardrail"))    return Label::GuardRail;
  if (Type == TEXT("Bridge"))       return Label::Bridge;
  if (Type == TEXT("RailTrack"))    return Label::RailTrack;

  // Additional categories
  if (Type == TEXT("SideWalk")) return Label::Sidewalks;
  if (Type == TEXT("Water"))    return Label::Water;
  if (Type == TEXT("Static"))   return Label::Static;
  if (Type == TEXT("Dynamic"))  return Label::Dynamic;
  if (Type == TEXT("Other"))    return Label::Other;

  return Label::None;
}

static bool TryGetLamp(
    UStaticMeshComponent* Component,
    const FString& NodeName,
    FLamp& Lamp)
{
  Lamp.Mesh = Component;

  if (NodeName.StartsWith(TEXT("light_red_off"))) {
    Lamp.Color = ETrafficLightState::Red;
    Lamp.bOnVariant = false;
  } else if (NodeName.StartsWith(TEXT("light_red_on"))) {
    Lamp.Color = ETrafficLightState::Red;
    Lamp.bOnVariant = true;
  } else if (NodeName.StartsWith(TEXT("light_yellow_off"))) {
    Lamp.Color = ETrafficLightState::Yellow;
    Lamp.bOnVariant = false;
  } else if (NodeName.StartsWith(TEXT("light_yellow_on"))) {
    Lamp.Color = ETrafficLightState::Yellow;
    Lamp.bOnVariant = true;
  } else if (NodeName.StartsWith(TEXT("light_green_off"))) {
    Lamp.Color = ETrafficLightState::Green;
    Lamp.bOnVariant = false;
  } else if (NodeName.StartsWith(TEXT("light_green_on"))) {
    Lamp.Color = ETrafficLightState::Green;
    Lamp.bOnVariant = true;
  } else {
    return false;
  }

  return true;
}

static FString CanonicalGuid(const FString& Text)
{
  FGuid Guid;

  return FGuid::Parse(Text, Guid) ? Guid.ToString(EGuidFormats::DigitsWithHyphens).ToLower() : FString();
}

static void TagMesh(UStaticMeshComponent* Component, Label Semantic, uint32 ActorID)
{
  Component->SetSimulatePhysics(false);

  Component->SetCollisionProfileName(
      TEXT("BlockAll")
  );

  Component->SetCollisionEnabled(
      ECollisionEnabled::QueryAndPhysics
  );

  ATagger::SetStencilValue(*Component, Semantic, true);

  UTaggedComponent* Tag = NewObject<UTaggedComponent>(Component);

  Tag->SetupAttachment(Component);
  Tag->RegisterComponent();
  Tag->SetColor(ATagger::GetLabelColor(ActorID, Semantic));
  Tag->MarkRenderStateDirty();
}

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

void AGltfMapLoader::ReceiveOnStaticMeshComponentCreated_Implementation(
  UStaticMeshComponent* Component,
  const FglTFRuntimeNode& Node)
{
  if (!Asset || !Component || !Component->GetStaticMesh())
  {
    return;
  }

  TArray<FglTFRuntimePathItem> Path;

  // Walk through the json
  TArray<FglTFRuntimePathItem> PrimativePathObject;

  FglTFRuntimePathItem MeshItem;
  MeshItem.Path = TEXT("meshes");
  MeshItem.Index = Node.MeshIndex;
  PrimativePathObject.Add(MeshItem);

  FglTFRuntimePathItem PrimitiveItem;
  PrimitiveItem.Path = TEXT("primitives");
  PrimitiveItem.Index = INDEX_NONE;
  PrimativePathObject.Add(PrimitiveItem);

  bool bFound = false;
  const int32 Count = Asset->GetArraySizeFromPath(PrimativePathObject, bFound);

  if (!bFound || Count <= 0)
  {
    return;
  }

  TArray<Label> Labels;

  for (int32 i = 0; i < Count; ++i)
  {
    TArray<FglTFRuntimePathItem> SemanticPathObject;

    FglTFRuntimePathItem MeshItem;
    MeshItem.Path = TEXT("meshes");
    MeshItem.Index = Node.MeshIndex;
    SemanticPathObject.Add(MeshItem);

    FglTFRuntimePathItem PrimitiveItem;
    PrimitiveItem.Path = TEXT("primitives");
    PrimitiveItem.Index = i;
    SemanticPathObject.Add(PrimitiveItem);

    FglTFRuntimePathItem ExtensionsItem;
    ExtensionsItem.Path = TEXT("extensions");
    ExtensionsItem.Index = INDEX_NONE;
    SemanticPathObject.Add(ExtensionsItem);

    FglTFRuntimePathItem ContainerItem;
    ContainerItem.Path = TEXT("SemanticContainer");
    ContainerItem.Index = INDEX_NONE;
    SemanticPathObject.Add(ContainerItem);

    FglTFRuntimePathItem TypeItem;
    TypeItem.Path = TEXT("Type");
    TypeItem.Index = INDEX_NONE;
    SemanticPathObject.Add(TypeItem);

    bFound = false;
    const FString Type = Asset->GetStringFromPath(SemanticPathObject, bFound);

    const Label SemanticLabel = ToCarlaLabel(Type);


    if (!bFound)
    {
      UE_LOG(LogTemp, Error, TEXT("Missing semantic type for primitive %d"), i);
      Labels.Add(Label::None);
      continue;
    }

    Labels.Add(SemanticLabel);
  }

  bool bUniform = true;
  for (int32 i = 0; i < Labels.Num(); ++i)
  {
    if (Labels[i] != Labels[0]) {
      bUniform = false;
      break;
    }
  }

  if (bUniform)
  {
    UE_LOG(LogTemp, Warning, TEXT("Applying uniform tag to mesh %d"), Node.MeshIndex);
    TagMesh(Component, Labels[0], GetUniqueID());
    CollectSignalMesh(Component, Node);
    return;
  }

  FglTFRuntimeStaticMeshConfig Config = StaticMeshConfig;

  Config.Outer = Component;

  const TArray<UStaticMesh*> Parts = Asset->LoadStaticMeshesFromPrimitives(Node.MeshIndex, Config);

  if (Parts.Num() != Labels.Num())
  {
    UE_LOG(LogTemp, Error, TEXT("Primitive count mismatch on mesh %d"), Node.MeshIndex);
    return;
  }

  for (int32 i = 0; i < Parts.Num(); ++i)
  {
    UStaticMeshComponent* PartComponent = NewObject<UStaticMeshComponent>(this);

    AddInstanceComponent(PartComponent);

    PartComponent->SetupAttachment(Component);
    PartComponent->SetRelativeTransform(FTransform::Identity);

    PartComponent->SetStaticMesh(Parts[i]);
    PartComponent->RegisterComponent();

    TagMesh(PartComponent, Labels[i], GetUniqueID());
    CollectSignalMesh(PartComponent, Node);
  }

  Component->SetCollisionEnabled(
    ECollisionEnabled::NoCollision
  );
  Component->SetStaticMesh(nullptr);

  UE_LOG(LogTemp, Warning, TEXT("Split mesh %d into %d tagged primitives"), Node.MeshIndex, Parts.Num());
}

void AGltfMapLoader::CollectSignalMesh(UStaticMeshComponent* Component, const FglTFRuntimeNode& Node)
{
  FglTFRuntimeNode Ancestor = Node;
  TSet<int32> Visited;
  while (!Visited.Contains(Ancestor.Index))
  {
    Visited.Add(Ancestor.Index);
    int32 EndBrace = INDEX_NONE;
    if (Ancestor.Name.StartsWith(TEXT("{")) &&
        Ancestor.Name.FindChar(TEXT('}'), EndBrace))
    {
      const FString Guid = CanonicalGuid(Ancestor.Name.Left(EndBrace + 1));
      if (const FString* Id = GuidToSignalId.Find(Guid))
      {
        if (AGltfTrafficLightHead* const* HeadPtr = TrafficLightHeads.Find(*Id))
        {
          FLamp Lamp;
          if (TryGetLamp(Component, Node.Name, Lamp)) {
            (*HeadPtr)->AddLamp(Lamp);
          }
        }
        
        return;
      }
    }
    if (Ancestor.ParentIndex == INDEX_NONE) break;
    FglTFRuntimeNode Parent;
    if (!Asset->GetNode(Ancestor.ParentIndex, Parent)) break;
    Ancestor = Parent;
  }
}

void AGltfMapLoader::BeginPlay()
{
    // Location of RPC file
    const FString AbsoluteGLTFPath = FPaths::ConvertRelativePathToFull(
      FPaths::ProjectContentDir() + "Carla/Maps/OpenDrive/GltfMap.gltf");

    if (!FPaths::FileExists(AbsoluteGLTFPath))
    {
      Super::BeginPlay();
      return;
    }

    FglTFRuntimeConfig Config;

    Asset = UglTFRuntimeFunctionLibrary::glTFLoadAssetFromFilename(
      AbsoluteGLTFPath,
      false,
      Config
    );

    UWorld* World = GetWorld();

    ACarlaGameModeBase* GameMode = Cast<ACarlaGameModeBase>(World->GetAuthGameMode());

    if (GameMode && GameMode->GetMap())
    {

      for (const auto& Pair : GameMode->GetMap()->GetSignals())
      {
        const auto& Signal = *Pair.second;
        const FString Guid = CanonicalGuid(UTF8_TO_TCHAR(Signal.GetRoadRunnerSignalId().c_str()));


        if (Signal.GetDynamic() && !Guid.IsEmpty()) {
          const FString SignalId = UTF8_TO_TCHAR(Signal.GetSignalId().c_str());
          GuidToSignalId.Add(Guid, SignalId);

          FActorSpawnParameters Params;
          Params.Owner = this;
          Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

          AGltfTrafficLightHead* Head = World->SpawnActor<AGltfTrafficLightHead>(
            AGltfTrafficLightHead::StaticClass(),
            FVector(0.0f, 0.0f, 0.0f),
            FRotator::ZeroRotator,
            Params);

          if (Head)
          {
            TrafficLightHeads.Add(SignalId, Head);
          }
        }
      }
    }

    Super::BeginPlay();

    if (!Asset)
    {
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



      ++MeshCount;
    }

    UE_LOG(
      LogTemp,
      Display,
      TEXT("Gltf map loaded with %d static mesh components"),
      MeshCount
    );

}

void AGltfMapLoader::BindTrafficLightSources()
{
  ACarlaGameModeBase* GameMode = Cast<ACarlaGameModeBase>(GetWorld()->GetAuthGameMode());

  if (!GameMode)
  {
    return;
  }

  ATrafficLightManager* Manager = GameMode->GetTrafficLightManager();

  for (const auto& Pair : TrafficLightHeads)
  {
    AGltfTrafficLightHead* Head = Pair.Value;

    if (!IsValid(Head))
    {
      continue;
    }

    USignComponent* RegisteredSign = Manager->GetTrafficSign(Pair.Key);

    ATrafficLightBase* TrafficLight = IsValid(RegisteredSign)
        ? Cast<ATrafficLightBase>(RegisteredSign->GetOwner())
        : nullptr;

    UTrafficLightComponent* Signal = IsValid(TrafficLight)
        ? TrafficLight->GetTrafficLightComponent()
        : nullptr;

    const FString RegisteredClass = IsValid(RegisteredSign)
        ? RegisteredSign->GetClass()->GetName()
        : TEXT("<none>");

    if (IsValid(Signal))
    {
      Head->SetSourceSignal(Signal);
    }
  }
}