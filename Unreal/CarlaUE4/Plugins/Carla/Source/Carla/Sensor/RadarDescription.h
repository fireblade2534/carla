#pragma once

#include "RadarDescription.generated.h"

USTRUCT()
struct CARLA_API FRadarDescription
{
  GENERATED_BODY()

  UPROPERTY(EditAnywhere)
  float HorizontalFOV = 30.0f;

  UPROPERTY(EditAnywhere)
  float VerticalFOV = 30.0f;

  /// Measure distance in centimeters.
  UPROPERTY(EditAnywhere)
  float Range = 100.0f;

  UPROPERTY(EditAnywhere)
  uint32 PointsPerSecond = 1500u;

  UPROPERTY(EditAnywhere)
  uint32 TrackThreshold = 10u;

  UPROPERTY(EditAnywhere)
  uint32 RandomSeed = 0u;
};