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

  if (!HasAnyFlags(RF_ClassDefaultObject))
  {
    std::printf("[TL-HEAD][CREATE] head=%s ptr=%p\n",
        TCHAR_TO_UTF8(*GetName()), static_cast<void*>(this));
    std::fflush(stdout);
  }
}

void AGltfTrafficLightHead::AddLamp(const FLamp& Lamp)
{
  if (!Lamp.Mesh.IsValid())
  {
    std::printf("[TL-HEAD][ADD-LAMP-FAILED] head=%s invalid mesh\n",
        TCHAR_TO_UTF8(*GetName()));
    std::fflush(stdout);
    return;
  }

  Lamps.Add(Lamp);

  std::printf("[TL-HEAD][ADD-LAMP] head=%s mesh=%s color=%s variant=%s total=%d\n",
      TCHAR_TO_UTF8(*GetName()),
      TCHAR_TO_UTF8(*Lamp.Mesh->GetName()),
      GltfHeadStateName(Lamp.Color),
      Lamp.bOnVariant ? "on" : "off",
      Lamps.Num());
  std::fflush(stdout);

  OnSignalChanged();
}

void AGltfTrafficLightHead::SetSourceSignal(UTrafficLightComponent* Signal)
{
  std::printf("[TL-HEAD][SET-SOURCE] head=%s old=%p incoming=%p valid=%d\n",
      TCHAR_TO_UTF8(*GetName()),
      static_cast<void*>(SourceSignal),
      static_cast<void*>(Signal),
      IsValid(Signal) ? 1 : 0);
  std::fflush(stdout);

  if (IsValid(SourceSignal))
  {
    SourceSignal->LightChangeDispatcher.RemoveDynamic(
        this,
        &AGltfTrafficLightHead::OnSignalChanged);

    std::printf("[TL-HEAD][UNBOUND] head=%s signal_id=%s\n",
        TCHAR_TO_UTF8(*GetName()),
        TCHAR_TO_UTF8(*SourceSignal->GetSignId()));
  }

  SourceSignal = IsValid(Signal) ? Signal : nullptr;

  if (SourceSignal)
  {
    SourceSignal->LightChangeDispatcher.AddUniqueDynamic(
        this,
        &AGltfTrafficLightHead::OnSignalChanged);

    std::printf("[TL-HEAD][BOUND] head=%s signal_id=%s owner=%s controller=%p state=%s lamps=%d\n",
        TCHAR_TO_UTF8(*GetName()),
        TCHAR_TO_UTF8(*SourceSignal->GetSignId()),
        TCHAR_TO_UTF8(*GetNameSafe(SourceSignal->GetOwner())),
        static_cast<void*>(SourceSignal->GetController()),
        GltfHeadStateName(SourceSignal->GetLightState()),
        Lamps.Num());
  }
  else
  {
    std::printf("[TL-HEAD][NO-SOURCE] head=%s cannot subscribe; displaying Off\n",
        TCHAR_TO_UTF8(*GetName()));
  }
  std::fflush(stdout);

  OnSignalChanged();
}

void AGltfTrafficLightHead::OnSignalChanged()
{
  const bool bValidSource = IsValid(SourceSignal);
  const ETrafficLightState State = bValidSource
      ? SourceSignal->GetLightState()
      : ETrafficLightState::Off;
  const FString SignalId = bValidSource ? SourceSignal->GetSignId() : TEXT("<none>");

  std::printf("[TL-HEAD][STATE-READ] head=%s signal_id=%s source_valid=%d state=%s lamps=%d\n",
      TCHAR_TO_UTF8(*GetName()),
      TCHAR_TO_UTF8(*SignalId),
      bValidSource ? 1 : 0,
      GltfHeadStateName(State),
      Lamps.Num());

  if (Lamps.Num() == 0)
  {
    std::printf("[TL-HEAD][NO-LAMPS] head=%s has no collected lamp meshes\n",
        TCHAR_TO_UTF8(*GetName()));
  }

  for (const FLamp& Lamp : Lamps)
  {
    UStaticMeshComponent* Mesh = Lamp.Mesh.Get();

    if (!IsValid(Mesh))
    {
      std::printf("[TL-HEAD][MESH-INVALID] head=%s color=%s variant=%s\n",
          TCHAR_TO_UTF8(*GetName()), GltfHeadStateName(Lamp.Color),
          Lamp.bOnVariant ? "on" : "off");
      continue;
    }

    const bool bColorActive = State == Lamp.Color;
    const bool bShow = Lamp.bOnVariant ? bColorActive : !bColorActive;
    const bool bWasVisible = Mesh->IsVisible();

    // Update attached CARLA tagged components.
    Mesh->SetVisibility(bShow, true);

    std::printf("[TL-HEAD][VISIBILITY] head=%s mesh=%s color=%s variant=%s active=%d was_visible=%d requested=%d visible_after=%d\n",
        TCHAR_TO_UTF8(*GetName()),
        TCHAR_TO_UTF8(*Mesh->GetName()),
        GltfHeadStateName(Lamp.Color),
        Lamp.bOnVariant ? "on" : "off",
        bColorActive ? 1 : 0,
        bWasVisible ? 1 : 0,
        bShow ? 1 : 0,
        Mesh->IsVisible() ? 1 : 0);
  }
  std::fflush(stdout);
}

void AGltfTrafficLightHead::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  std::printf("[TL-HEAD][END] head=%s reason=%d source=%p lamps=%d\n",
      TCHAR_TO_UTF8(*GetName()), static_cast<int>(EndPlayReason),
      static_cast<void*>(SourceSignal), Lamps.Num());
  std::fflush(stdout);

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