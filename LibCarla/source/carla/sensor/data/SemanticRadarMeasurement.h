#pragma once

#include "carla/Debug.h"
#include "carla/rpc/Location.h"
#include "carla/rpc/BoundingBox.h"
#include "carla/sensor/data/Array.h"
#include "carla/sensor/s11n/SemanticRadarSerializer.h"
#include "carla/sensor/data/SemanticRadarData.h"

namespace carla {
namespace sensor {
namespace data {

  class SemanticRadarMeasurement : public Array<data::SemanticRadarDetection>  {
    static_assert(sizeof(data::SemanticRadarDetection) == 15u * sizeof(float), "SemanticRadarDetection size missmatch");
    using Super = Array<data::SemanticRadarDetection>;

  protected:
    using Serializer = s11n::SemanticRadarSerializer;

    friend Serializer;

    explicit SemanticRadarMeasurement(RawData &&data)
      : Super(0u, std::move(data)) {}

  public:
    Super::size_type GetDetectionAmount() const {
      return Super::size();
    }
  };
} // namespace data
} // namespace sensor
} // namespace carla
