#pragma once

#include "Carla/Sensor/Sensor.h"

#include "Carla/Actor/ActorDefinition.h"
#include "Carla/Sensor/RadarDescription.h"

#include <compiler/disable-ue4-macros.h>
#include <carla/sensor/data/SemanticRadarData.h>
#include <compiler/enable-ue4-macros.h>

#include "SemanticRadar.generated.h"

/// A ray-cast based Semantic Radar sensor.
UCLASS()
class CARLA_API ASemanticRadar : public ASensor
{
  GENERATED_BODY()

  using FSemanticRadarData = carla::sensor::data::SemanticRadarData;

public:

  static FActorDefinition GetSensorDefinition();

  ASemanticRadar(const FObjectInitializer &ObjectInitializer);

  void Set(const FActorDescription &Description) override;
  virtual void Set(const FRadarDescription &RadarDescription);

protected:

  void BeginPlay() override;

  // virtual void PrePhysTick(float DeltaTime) override;
  virtual void PostPhysTick(UWorld *World, ELevelTick TickType, float DeltaTime) override;

  UPROPERTY(EditAnywhere)
  FRadarDescription Description;

private:

  void CalculateCurrentVelocity(const float DeltaTime);

  void SendLineTraces(float DeltaTime);

  float CalculateRelativeVelocity(const FHitResult& OutHit, const FVector& RadarLocation);

  FSemanticRadarData RadarData;

  FCollisionQueryParams TraceParams;

  FVector CurrentVelocity;

  /// Used to compute the velocity of the radar
  FVector PrevLocation;

  struct SemanticRayData {
    float Radius;
    float Angle;
    bool Hitted;
    TWeakObjectPtr<AActor> HitActor;
  };

  std::vector<SemanticRayData> Rays;
};