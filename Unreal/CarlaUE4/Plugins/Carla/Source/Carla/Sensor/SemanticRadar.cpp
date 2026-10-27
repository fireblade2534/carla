
#include <PxScene.h>

#include "Carla.h"
#include "Carla/Sensor/SemanticRadar.h"
#include "Carla/Actor/ActorBlueprintFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"
#include "Carla/Actor/ActorRegistry.h"
#include "Carla/Game/CarlaEpisode.h"
#include "Carla/Vehicle/CarlaWheeledVehicle.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Carla/Util/BoundingBoxCalculator.h"

#include <compiler/disable-ue4-macros.h>
#include <map>
#include "carla/geom/Math.h"
#include "carla/ros2/ROS2.h"
#include <compiler/enable-ue4-macros.h>

FActorDefinition ASemanticRadar::GetSensorDefinition() {
  return UActorBlueprintFunctionLibrary::MakeRadarDefinition(TEXT("radar_semantic"));
}

ASemanticRadar::ASemanticRadar(const FObjectInitializer& ObjectInitializer)
  : Super(ObjectInitializer)
{
  PrimaryActorTick.bCanEverTick = true;

  RandomEngine = CreateDefaultSubobject<URandomEngine>(TEXT("RandomEngine"));

  TraceParams = FCollisionQueryParams(FName(TEXT("Laser_Trace")), true, this);
  TraceParams.bTraceComplex = true;
  TraceParams.bReturnPhysicalMaterial = false;

}

void ASemanticRadar::Set(const FActorDescription &ActorDescription)
{
  Super::Set(ActorDescription);

  FRadarDescription RadarDescription;

  UActorBlueprintFunctionLibrary::SetRadar(ActorDescription, RadarDescription);

  if (!ActorDescription.Variations.Contains("noise_seed")) {
    RadarDescription.RandomSeed =
        GetRandomEngine()->GenerateRandomSeed();
  }

  Set(RadarDescription);
}

void ASemanticRadar::Set(const FRadarDescription &RadarDescription)
{
  Description = RadarDescription;

  SetSeed(Description.RandomSeed);
  RadarData.SetResolution(Description.PointsPerSecond);
}

void ASemanticRadar::BeginPlay()
{
  Super::BeginPlay();

  PrevLocation = GetActorLocation();
}

void ASemanticRadar::PostPhysTick(UWorld *World, ELevelTick TickType, float DeltaTime)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(ASemanticRadar::PostPhysTick);
  CalculateCurrentVelocity(DeltaTime);

  RadarData.Reset();
  SendLineTraces(DeltaTime);

  auto DataStream = GetDataStream(*this);

  // ROS2
  #if defined(WITH_ROS2)
  auto ROS2 = carla::ros2::ROS2::GetInstance();
  if (ROS2->IsEnabled())
  {
    TRACE_CPUPROFILER_EVENT_SCOPE_STR("ROS2 Send");
    AActor* ParentActor = GetAttachParentActor();
    auto Transform = (ParentActor) ? GetActorTransform().GetRelativeTransform(ParentActor->GetActorTransform()) : GetActorTransform();
    ROS2->ProcessDataFromRadar(DataStream.GetSensorType(), Transform, RadarData, this);
  }
  #endif

  {
    TRACE_CPUPROFILER_EVENT_SCOPE_STR("Send Stream");
    DataStream.SerializeAndSend(*this, RadarData, DataStream.PopBufferFromPool());
  }
}

void ASemanticRadar::CalculateCurrentVelocity(const float DeltaTime)
{
  const FVector RadarLocation = GetActorLocation();
  CurrentVelocity = (RadarLocation - PrevLocation) / DeltaTime;
  PrevLocation = RadarLocation;
}

void ASemanticRadar::SendLineTraces(float DeltaTime)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(ASemanticRadar::SendLineTraces);
  constexpr float TO_METERS = 1e-2;
  const FTransform& ActorTransform = GetActorTransform();
  const FRotator& TransformRotator = ActorTransform.Rotator();
  const FVector& RadarLocation = GetActorLocation();
  const FVector& ForwardVector = GetActorForwardVector();
  const FVector TransformXAxis = ActorTransform.GetUnitAxis(EAxis::X);
  const FVector TransformYAxis = ActorTransform.GetUnitAxis(EAxis::Y);
  const FVector TransformZAxis = ActorTransform.GetUnitAxis(EAxis::Z);

  // Maximum radar radius in horizontal and vertical direction
  const float MaxRx = FMath::Tan(FMath::DegreesToRadians(Description.HorizontalFOV * 0.5f)) * Description.Range;
  const float MaxRy = FMath::Tan(FMath::DegreesToRadians(Description.VerticalFOV * 0.5f)) * Description.Range;
  const int NumPoints = (int)(Description.PointsPerSecond * DeltaTime);

  // Generate the parameters of the rays in a deterministic way
  Rays.clear();
  Rays.resize(NumPoints);
  for (int i = 0; i < Rays.size(); i++) {
    Rays[i].Radius = RandomEngine->GetUniformFloat();
    Rays[i].Angle = RandomEngine->GetUniformFloatInRange(0.0f, carla::geom::Math::Pi2<float>());
    Rays[i].Hitted = false;
  }

  FCriticalSection Mutex;
  GetWorld()->GetPhysicsScene()->GetPxScene()->lockRead();
  {
    TRACE_CPUPROFILER_EVENT_SCOPE(ParallelFor);
    ParallelFor(NumPoints, [&](int32 idx) {
      TRACE_CPUPROFILER_EVENT_SCOPE(ParallelForTask);
      FHitResult OutHit(ForceInit);
      const float Radius = Rays[idx].Radius;
      const float Angle  = Rays[idx].Angle;

      float Sin, Cos;
      FMath::SinCos(&Sin, &Cos, Angle);

      const FVector EndLocation = RadarLocation + TransformRotator.RotateVector({
        Description.Range,
        MaxRx * Radius * Cos,
        MaxRy * Radius * Sin
      });

      const bool Hitted = GetWorld()->ParallelLineTraceSingleByChannel(
        OutHit,
        RadarLocation,
        EndLocation,
        ECC_GameTraceChannel2,
        TraceParams,
        FCollisionResponseParams::DefaultResponseParam
      );

      // STORE MAP OF HIT ACTORS AND A COUNT

      const TWeakObjectPtr<AActor> HittedActor = OutHit.Actor;
      if (Hitted && HittedActor.Get()) {
        Rays[idx].Hitted = true;
        Rays[idx].HitActor = HittedActor;
      }
    });
  }
  GetWorld()->GetPhysicsScene()->GetPxScene()->unlockRead();

  TMap<TWeakObjectPtr<AActor>, std::uint32_t> ActorHitCounts;

  // Write the detections in the output structure
  for (auto& ray : Rays) {
    if (!ray.Hitted) {
      continue;
    }

    AActor* HitActor = ray.HitActor.Get();

    if (!HitActor) {
      continue;
    }

    const bool IsVehicle = HitActor->IsA<ACarlaWheeledVehicle>();
    const bool IsPedestrian = HitActor->IsA<ACharacter>();

    if (!IsVehicle && !IsPedestrian)
    {
      continue;
    }

    ActorHitCounts.FindOrAdd(ray.HitActor) += 1;
  }

  const auto &Registry = GetEpisode().GetActorRegistry();
  const FTransform SensorTransform = GetActorTransform();
  const FQuat SensorInverseRotation = SensorTransform.GetRotation().Inverse();

  for (const auto& Pair : ActorHitCounts) {
    if (Pair.Value < Description.TrackThreshold) {
      continue;
    }
    
    if (AActor* Actor = Pair.Key.Get()) {
      carla::sensor::data::SemanticRadarDetection Detection{};

      const auto *ActorView = Registry.FindCarlaActor(Actor);
      Detection.object_idx = ActorView ? ActorView->GetActorId() : 0u;

      const USkeletalMeshComponent *Mesh = nullptr;

      if (const auto *Vehicle = Cast<ACarlaWheeledVehicle>(Actor)) {
        Mesh = Vehicle->GetMesh();
      } else if (const auto *Pedestrian = Cast<ACharacter>(Actor)) {
        Mesh = Pedestrian->GetMesh();
      }

      Detection.object_tag = Mesh ? static_cast<uint32_t>(Mesh->CustomDepthStencilValue) : 0u;

      const FBoundingBox LocalBox = UBoundingBoxCalculator::GetActorBoundingBox(Actor);

      const FTransform ActorTransform = Actor->GetActorTransform();

      const FVector WorldCenter = ActorTransform.TransformPosition(LocalBox.Origin);

      const FVector Center = SensorTransform.InverseTransformPositionNoScale(WorldCenter) * TO_METERS;

      const FQuat WorldBoxRotation = ActorTransform.GetRotation() * LocalBox.Rotation.Quaternion();

      const FRotator Rotation = (SensorInverseRotation * WorldBoxRotation).Rotator();

      const FVector Extent = LocalBox.Extent * TO_METERS;

      Detection.bbox.location = {Center.X, Center.Y, Center.Z};
      Detection.bbox.rotation = {Rotation.Pitch, Rotation.Yaw, Rotation.Roll};
      Detection.bbox.extent = {Extent.X, Extent.Y, Extent.Z};
      Detection.bbox.actor_id = Detection.object_idx;

      const FVector Velocity = SensorTransform.InverseTransformVectorNoScale(Actor->GetVelocity() - CurrentVelocity) * TO_METERS;

      Detection.velocity = {Velocity.X, Velocity.Y, Velocity.Z};

      RadarData.WriteDetection(Detection);
    }
  }
}