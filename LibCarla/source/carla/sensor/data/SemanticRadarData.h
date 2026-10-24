#pragma once

#include <cstdint>
#include <vector>
#include <cstdio>

namespace carla {

namespace ros2 {
  class ROS2;
}

namespace sensor {

namespace s11n {
  class SemanticRadarSerializer;
}

namespace data {

  struct SemanticRadarVector3D {
    float x;
    float y;
    float z;
  };

  struct SemanticRadarQuaternion {
    float x;
    float y;
    float z;
    float w;
  };

  struct SemanticRadarPose3D {
    SemanticRadarVector3D position;
    SemanticRadarQuaternion orientation;
  };

  struct SemanticRadarBoundingBox3D {
    SemanticRadarPose3D center;
    SemanticRadarVector3D size;
  };

  struct SemanticRadarDetection {
    uint32_t object_idx;
    uint32_t object_tag;
    SemanticRadarBoundingBox3D bbox;
    SemanticRadarVector3D velocity;
  };

  class SemanticRadarData {

  public:
    explicit SemanticRadarData() = default;

    constexpr static auto detection_size = sizeof(SemanticRadarDetection);

    SemanticRadarData &operator=(SemanticRadarData &&) = default;
  };
} // namespace s11n
} // namespace sensor
} // namespace carla