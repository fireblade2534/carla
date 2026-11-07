#pragma once

#include "CoreMinimal.h"

#include "TrafficLightState.generated.h"

USTRUCT(BlueprintType)
struct CARLA_API FTrafficLightState
{
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Light")
  bool bRed = false;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Light")
  bool bYellow = false;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Light")
  bool bGreen = false;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Light")
  bool bTurn = false;

  constexpr FTrafficLightState() = default;

  constexpr FTrafficLightState(
      bool InRed,
      bool InYellow,
      bool InGreen,
      bool InTurn)
    : bRed(InRed),
      bYellow(InYellow),
      bGreen(InGreen),
      bTurn(InTurn)
  {}
  
  static const FTrafficLightState Off;
  static const FTrafficLightState Red;
  static const FTrafficLightState Yellow;
  static const FTrafficLightState Green;
  static const FTrafficLightState OnlyTurn;
  static const FTrafficLightState GreenWithTurn;

  bool IsOff() const
  {
    return !bRed && !bYellow && !bGreen && !bTurn;
  }

  bool operator==(const FTrafficLightState& Other) const
  {
    return bRed == Other.bRed
        && bYellow == Other.bYellow
        && bGreen == Other.bGreen
        && bTurn == Other.bTurn;
  }

  bool operator!=(const FTrafficLightState& Other) const
  {
    return !(*this == Other);
  }

  constexpr uint8 ToMask() const
  {
    return static_cast<uint8>(
        (bRed    ? 0x01 : 0)
      | (bYellow ? 0x02 : 0)
      | (bGreen  ? 0x04 : 0)
      | (bTurn   ? 0x08 : 0));
  }

  static FTrafficLightState FromMask(uint8 Mask)
  {
    return FTrafficLightState(
        (Mask & 0x01) != 0,
        (Mask & 0x02) != 0,
        (Mask & 0x04) != 0,
        (Mask & 0x08) != 0);
  }

  constexpr operator uint8() const
  {
    return ToMask();
  }
};

inline constexpr FTrafficLightState FTrafficLightState::Off{
    false, false, false, false
};

inline constexpr FTrafficLightState FTrafficLightState::Red{
    true, false, false, false
};

inline constexpr FTrafficLightState FTrafficLightState::Yellow{
    false, true, false, false
};

inline constexpr FTrafficLightState FTrafficLightState::Green{
    false, false, true, false
};

inline constexpr FTrafficLightState FTrafficLightState::OnlyTurn{
    false, false, false, true
};

inline constexpr FTrafficLightState FTrafficLightState::GreenWithTurn{
    false, false, true, true
};