#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Carla/Traffic/TrafficLightState.h"

#include "GltfTrafficLightHead.generated.h"

class UStaticMeshComponent;
class UTrafficLightComponent;

USTRUCT()
struct FLamp
{
  GENERATED_BODY()

  UPROPERTY()
  TWeakObjectPtr<UStaticMeshComponent> Mesh;

  UPROPERTY()
  FTrafficLightState Color = FTrafficLightState::Red;

  UPROPERTY()
  bool bOnVariant = false;
};

UCLASS()
class CARLA_API AGltfTrafficLightHead : public AActor
{
  GENERATED_BODY()

  public:
    AGltfTrafficLightHead();
    
    void AddLamp(const FLamp& Lamp);

    UFUNCTION()
    void OnSignalChanged();

    UFUNCTION()
    void SetSourceSignal(UTrafficLightComponent* Signal);

  protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  private:
    UPROPERTY()
    UTrafficLightComponent* SourceSignal = nullptr;

    UPROPERTY()
    TArray<FLamp> Lamps;
};