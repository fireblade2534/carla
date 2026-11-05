#include "GltfMapLoader.h"

#include "Carla/Game/Tagger.h"
#include "Carla/Game/TaggedComponent.h"

#include "Components/StaticMeshComponent.h"
#include "glTFRuntimeFunctionLibrary.h"
#include "Misc/Paths.h"
#include <cstdio>

using Label = carla::rpc::CityObjectLabel;

// Convert RoadRunner's categories to CARLA categories.
static Label ToCarlaLabel(const FString& Type)
{
  if (Type == TEXT("Road"))     return Label::Roads;
  if (Type == TEXT("Marking"))  return Label::RoadLines;
  if (Type == TEXT("Sidewalk")) return Label::Sidewalks;
  if (Type == TEXT("Curb"))     return Label::Sidewalks;
  if (Type == TEXT("Gutter"))   return Label::Roads;
  if (Type == TEXT("Signal"))   return Label::TrafficLight;
  if (Type == TEXT("Prop"))     return Label::Static;

  return Label::None;
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
  }

  Component->SetCollisionEnabled(
    ECollisionEnabled::NoCollision
  );
  Component->SetStaticMesh(nullptr);

  UE_LOG(LogTemp, Warning, TEXT("Split mesh %d into %d tagged primitives"), Node.MeshIndex, Parts.Num());
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

      Super::BeginPlay();
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



      ++MeshCount;
    }

    UE_LOG(
      LogTemp,
      Display,
      TEXT("Gltf map loaded with %d static mesh components"),
      MeshCount
    );
}