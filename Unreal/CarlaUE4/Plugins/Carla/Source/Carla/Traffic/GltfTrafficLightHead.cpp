#include "Carla.h"
#include "GltfTrafficLightHead.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Carla/Traffic/TrafficLightComponent.h"
#include <cstdio>

static const char* GltfHeadStateName(ETrafficLightState State)
{
  switch (State)
  {
    case ETrafficLightState::Red: return "Red";
    case ETrafficLightState::Yellow: return "Yellow";
    case ETrafficLightState::Green: return "Green";
    case ETrafficLightState::Off: return "Off";
    default: return "Unknown";
  }
}

AGltfTrafficLightHead::AGltfTrafficLightHead()
{
  PrimaryActorTick.bCanEverTick = false;

  RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
}

void AGltfTrafficLightHead::AddLamp(const FLamp& Lamp)
{
  if (!Lamp.Mesh.IsValid())
  {
    return;
  }

  Lamps.Add(Lamp);

  OnSignalChanged();
}

void AGltfTrafficLightHead::SetSourceSignal(UTrafficLightComponent* Signal)
{
  if (IsValid(SourceSignal))
  {
    SourceSignal->LightChangeDispatcher.RemoveDynamic(
        this,
        &AGltfTrafficLightHead::OnSignalChanged);
  }

  SourceSignal = IsValid(Signal) ? Signal : nullptr;

  if (SourceSignal)
  {
    SourceSignal->LightChangeDispatcher.AddUniqueDynamic(
        this,
        &AGltfTrafficLightHead::OnSignalChanged);
  }

  OnSignalChanged();
}

void AGltfTrafficLightHead::OnSignalChanged()
{
  const bool bValidSource = IsValid(SourceSignal);
  const ETrafficLightState State = bValidSource
      ? SourceSignal->GetLightState()
      : ETrafficLightState::Off;
  const FString SignalId = bValidSource ? SourceSignal->GetSignId() : TEXT("<none>");

  for (const FLamp& Lamp : Lamps)
  {
    UStaticMeshComponent* Mesh = Lamp.Mesh.Get();

    if (!IsValid(Mesh))
    {
      continue;
    }

    const bool bColorActive = State == Lamp.Color;
    const bool bShow = Lamp.bOnVariant ? bColorActive : !bColorActive;
    const bool bWasVisible = Mesh->IsVisible();

    // Update attached CARLA tagged components.
    Mesh->SetVisibility(bShow, true);
  }
}

void AGltfTrafficLightHead::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  if (IsValid(SourceSignal))
  {
    SourceSignal->LightChangeDispatcher.RemoveDynamic(
        this,
        &AGltfTrafficLightHead::OnSignalChanged);
  }

  SourceSignal = nullptr;
  Lamps.Empty();

  Super::EndPlay(EndPlayReason);
}