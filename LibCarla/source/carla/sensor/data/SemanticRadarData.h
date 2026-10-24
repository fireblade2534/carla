#pragma once

#include <cstdint>
#include <vector>
#include <cstdio>

#include "carla/rpc/Location.h"
#include "carla/rpc/BoundingBox.h"

namespace carla {

namespace ros2 {
  class ROS2;
}

namespace sensor {

namespace s11n {
  class SemanticRadarSerializer;
}

namespace data {

  #pragma pack(push, 1)
  class SemanticRadarDetection {
    public:
      geom::BoundingBox bbox{};
      geom::Vector3D velocity{};
      uint32_t object_idx;
      uint32_t object_tag;

      SemanticRadarDetection() = default;

      SemanticRadarDetection(geom::BoundingBox bb, geom::Vector3D v, uint32_t idx, uint32_t tag) :
            bbox(bb), velocity(v), object_idx{idx}, object_tag{tag} { }


  };
  #pragma pack(pop)

  class SemanticRadarData {
      static_assert(sizeof(float) == sizeof(uint32_t), "Invalid float size");
      static_assert(sizeof(SemanticRadarDetection) == 60u, "Invalid SemanticRadarDetection size");

  public:
    explicit SemanticRadarData() = default;

    constexpr static auto detection_size = sizeof(SemanticRadarDetection);

    SemanticRadarData &operator=(SemanticRadarData &&) = default;

    /// Set a new resolution for the RadarData.
    /// Allocates / Deallocates space in memory if needed.
    ///
    /// @warning This is expensive, not to be called each tick!
    void SetResolution(uint32_t resolution) {
      // Remove the capacity of the vector to zero
      _detections.clear();
      _detections.shrink_to_fit();
      // Set the new vector's capacity
      _detections.reserve(resolution);
    }

    /// Returns the number of current detections.
    size_t GetDetectionCount() const {
      return _detections.size();
    }

    /// Deletes the current detections.
    /// It doesn't change the resolution nor the allocated memory.
    void Reset() {
      _detections.clear();
    }

    /// Adds a new detection.
    void WriteDetection(SemanticRadarDetection detection) {
      _detections.push_back(detection);
    }

    const std::vector<SemanticRadarDetection> &GetDetections() const {
      return _detections;
    }

  private:
    std::vector<SemanticRadarDetection> _detections;

    friend class s11n::SemanticRadarSerializer;
    friend class carla::ros2::ROS2;
  };
} // namespace s11n
} // namespace sensor
} // namespace carla