// Copyright (c) 2019 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "Carla/Sensor/Sensor.h"

#include "Carla/Actor/ActorDefinition.h"
#include "Carla/Sensor/RadarDescription.h"

#include <compiler/disable-ue4-macros.h>
#include <carla/sensor/data/RadarData.h>
#include <compiler/enable-ue4-macros.h>

#include "Radar.generated.h"

/// A ray-cast based Radar sensor.
UCLASS()
class CARLA_API ARadar : public ASensor
{
  GENERATED_BODY()

  using FRadarData = carla::sensor::data::RadarData;

public:

  static FActorDefinition GetSensorDefinition();

  ARadar(const FObjectInitializer &ObjectInitializer);

  void Set(const FActorDescription &Description) override;
  void Set(const FRadarDescription &RadarDescription) override;

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

  FRadarData RadarData;

  FCollisionQueryParams TraceParams;

  FVector CurrentVelocity;

  /// Used to compute the velocity of the radar
  FVector PrevLocation;

  struct RayData {
    float Radius;
    float Angle;
    bool Hitted;
    float RelativeVelocity;
    FVector2D AzimuthAndElevation;
    float Distance;
  };

  std::vector<RayData> Rays;
};
