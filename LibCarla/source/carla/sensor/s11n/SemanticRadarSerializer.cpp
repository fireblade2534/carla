
#include "carla/sensor/s11n/SemanticRadarSerializer.h"

#include "carla/sensor/data/SemanticRadarMeasurement.h"

namespace carla {
namespace sensor {
namespace s11n {

  SharedPtr<SensorData> SemanticRadarSerializer::Deserialize(RawData &&data) {
    return SharedPtr<data::SemanticRadarMeasurement>(
        new data::SemanticRadarMeasurement{std::move(data)});
  }

} // namespace s11n
} // namespace sensor
} // namespace carla
