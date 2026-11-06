#include "Carla.h"
#include "GltfTrafficLightHead.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Carla/Traffic/TrafficLightComponent.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
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
static void TintLampMesh(
    UStaticMeshComponent* Mesh,
    const FLinearColor& Tint)
{
  const FName Parameters[] = {TEXT("baseColorFactor"), TEXT("emissiveFactor")};

  for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
  {
    UMaterialInstanceDynamic* Original = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot));

    if (!IsValid(Original))
    {
      continue;
    }

    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Original->Parent, Mesh);

    if (!Material)
    {
      continue;
    }

    Material->CopyParameterOverrides(Original);

    for (const FName& Parameter : Parameters)
    {
      FLinearColor Value;

      if (Material->GetVectorParameterValue(Parameter, Value))
      {
        Material->SetVectorParameterValue(Parameter, Value * Tint);
      }
    }

    Mesh->SetMaterial(Slot, Material);
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

  FLinearColor Tint = FLinearColor::White;

  if (Lamp.bOnVariant)
  {
    switch (Lamp.Color)
    {
      case ETrafficLightState::Red:
        // Make the on version of the red bulb look more red
        Tint = FLinearColor(1.0f, 0.5f, 0.5f, 1.0f);
        break;

      case ETrafficLightState::Yellow:
        Tint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
        break;

      case ETrafficLightState::Green:
        Tint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
        break;

      default:
        break;
    }
  } else {
    switch (Lamp.Color)
    {
      case ETrafficLightState::Red:
        Tint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
        break;

      case ETrafficLightState::Yellow:
        // Make the off version of the yellow bulb look significantly less orange
        Tint = FLinearColor(0.25f, 0.5f, 0.02f, 1.0f);
        break;

      case ETrafficLightState::Green:
        Tint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
        break;

      default:
        break;
    }
  }

  TintLampMesh(Lamp.Mesh.Get(), Tint);

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