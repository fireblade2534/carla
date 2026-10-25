#pragma once

#include "Carla/Sensor/Sensor.h"

#include "Carla/Actor/ActorDefinition.h"

#include <compiler/disable-ue4-macros.h>
#include <carla/sensor/data/SemanticRadarData.h>
#include <compiler/enable-ue4-macros.h>

#include "SemanticRadar.generated.h"

/// A ray-cast based Semantic Radar sensor.
UCLASS()
class CARLA_API ASemanticRadar : public ASensor
{
  GENERATED_BODY()

  using FRadarData = carla::sensor::data::SemanticRadarData;

public:

  static FActorDefinition GetSensorDefinition();

  ARadar(const FObjectInitializer &ObjectInitializer);

  void Set(const FActorDescription &Description) override;

  UFUNCTION(BlueprintCallable, Category = "Semantic Radar")
  void SetHorizontalFOV(float NewHorizontalFOV);

  UFUNCTION(BlueprintCallable, Category = "Semantic Radar")
  void SetVerticalFOV(float NewVerticalFOV);

  UFUNCTION(BlueprintCallable, Category = "Semantic Radar")
  void SetRange(float NewRange);

  UFUNCTION(BlueprintCallable, Category = "Semantic Radar")
  void SetPointsPerSecond(int NewPointsPerSecond);

protected:

  void BeginPlay() override;

  // virtual void PrePhysTick(float DeltaTime) override;
  virtual void PostPhysTick(UWorld *World, ELevelTick TickType, float DeltaTime) override;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Semantic Detection")
  float Range;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Semantic Detection")
  float HorizontalFOV;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Semantic Detection")
  float VerticalFOV;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Semantic Detection")
  int PointsPerSecond;

private:

  void CalculateCurrentVelocity(const float DeltaTime);

  void SendLineTraces(float DeltaTime);

  float CalculateRelativeVelocity(const FHitResult& OutHit, const FVector& RadarLocation);

  FRadarData RadarData;

  FCollisionQueryParams TraceParams;

  FVector CurrentVelocity;

  /// Used to compute the velocity of the radar
  FVector PrevLocation;

  struct SemanticRayData {
    float Radius;
    float Angle;
    bool Hitted;
    float RelativeVelocity;
    FVector2D AzimuthAndElevation;
    float Distance;
  };

  std::vector<SemanticRayData> Rays;
};