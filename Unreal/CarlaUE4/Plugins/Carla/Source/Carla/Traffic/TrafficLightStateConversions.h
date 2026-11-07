#pragma once

#include "TrafficLightState.h"

#include <compiler/disable-ue4-macros.h>
#include <carla/rpc/TrafficLightState.h>
#include <compiler/enable-ue4-macros.h>

inline carla::rpc::TrafficLightState ToRpcTrafficLightState(
    const FTrafficLightState& State)
{
  using TLS = carla::rpc::TrafficLightState;

  if (State.bRed)
  {
    return TLS::Red;
  }

  if (State.bYellow)
  {
    return TLS::Yellow;
  }

  if (State.bGreen)
  {
    return TLS::Green;
  }

  // The old API cannot describe a turn-only permission.
  if (State.bTurn)
  {
    return TLS::Red;
  }

  return TLS::Off;
}

inline bool TryFromRpcTrafficLightState(
    carla::rpc::TrafficLightState State,
    FTrafficLightState& OutState)
{
  using TLS = carla::rpc::TrafficLightState;

  switch (State)
  {
    case TLS::Red:
      OutState = FTrafficLightState::Red;
      return true;

    case TLS::Yellow:
      OutState = FTrafficLightState::Yellow;
      return true;

    case TLS::Green:
      OutState = FTrafficLightState::Green;
      return true;

    case TLS::Off:
      OutState = FTrafficLightState::Off;
      return true;

    default:
      return false;
  }
}

inline uint8 EncodeTrafficLightState(const FTrafficLightState& State)
{
  // Values 0x10–0x1F identify the new four-bit encoding.
  return static_cast<uint8>(0x10 | State.ToMask());
}

inline bool TryDecodeTrafficLightState(
    uint8 Value,
    FTrafficLightState& OutState)
{
  if ((Value & 0xF0) == 0x10)
  {
    OutState = FTrafficLightState::FromMask(static_cast<uint8>(Value & 0x0F));
    return true;
  }

  // Original recordings used enum values 0–3.
  switch (Value)
  {
    case 0:
      OutState = FTrafficLightState::Red;
      return true;

    case 1:
      OutState = FTrafficLightState::Yellow;
      return true;

    case 2:
      OutState = FTrafficLightState::Green;
      return true;

    case 3:
      OutState = FTrafficLightState::Off;
      return true;

    default:
      return false;
  }
}