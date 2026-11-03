// Copyright (c) 2019 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "carla/NonCopyable.h"
#include "carla/road/RoadTypes.h"

#include <string>

namespace carla {
namespace road {

  enum class ObjectOrientation {
    Positive,
    Negative,
    Both
  };

  class Object : private MovableNonCopyable {
  public:

    Object(
        ObjId id,
        RoadId road_id,
        std::string type,
        std::string subtype,
        std::string name,
        double s,
        double t,
        double zOffset,
        std::string orientation,
        double height,
        double length,
        double width,
        double hdg,
        double pitch,
        double roll)
      : _id(id),
        _road_id(road_id),
        _type(type),
        _subtype(subtype),
        _name(name),
        _s(s),
        _t(t),
        _zOffset(zOffset),
        _orientation(orientation),
        _height(height),
        _length(length),
        _width(width),
        _hdg(hdg),
        _pitch(pitch),
        _roll(roll) {}

    ObjId GetObjectId() const {
      return _id;
    }

    RoadId GetRoadId() const {
      return _road_id;
    }

    const std::string &GetType() const {
      return _type;
    }

    const std::string &GetSubtype() const {
      return _subtype;
    }

    const std::string &GetName() const {
      return _name;
    }

    double GetS() const {
      return _s;
    }

    double GetT() const {
      return _t;
    }

    double GetZOffset() const {
      return _zOffset;
    }

    ObjectOrientation GetOrientation() const {
      if (_orientation == "+") {
        return ObjectOrientation::Positive;
      } else if (_orientation == "-") {
        return ObjectOrientation::Negative;
      } else {
        return ObjectOrientation::Both;
      }
    }

    double GetHeight() const {
      return _height;
    }

    double GetLength() const {
      return _length;
    }

    double GetWidth() const {
      return _width;
    }

    double GetHdg() const {
      return _hdg;
    }

    double GetPitch() const {
      return _pitch;
    }

    double GetRoll() const {
      return _roll;
    }

  private:
    ObjId _id = 0u;
    RoadId _road_id = 0;
    std::string _type;
    std::string _subtype;
    std::string _name;
    double _s = 0.0;
    double _t = 0.0;
    double _zOffset = 0.0;
    std::string _orientation;
    double _height = 0.0;
    double _length = 0.0;
    double _width = 0.0;
    double _hdg = 0.0;
    double _pitch = 0.0;
    double _roll = 0.0;
  };

} // road
} // carla