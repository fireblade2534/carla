#include "CarlaSemanticRadarPublisher.h"

#include "carla/sensor/data/SemanticRadarData.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace carla {
namespace ros2 {

  struct SemanticRadarPoint {
    float x, y, z;
    float size_x, size_y, size_z;
    float qx, qy, qz, qw;
    float vx, vy, vz;
    uint32_t object_idx;
    uint32_t object_tag;
  };

  const size_t CarlaSemanticRadarPublisher::GetPointSize() {
    return sizeof(SemanticRadarPoint);
  }

  std::vector<sensor_msgs::msg::PointField> CarlaSemanticRadarPublisher::GetFields() {
    sensor_msgs::msg::PointField descriptor1;
    descriptor1.name("x");
    descriptor1.offset(0);
    descriptor1.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor1.count(1);

    sensor_msgs::msg::PointField descriptor2;
    descriptor2.name("y");
    descriptor2.offset(4);
    descriptor2.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor2.count(1);

    sensor_msgs::msg::PointField descriptor3;
    descriptor3.name("z");
    descriptor3.offset(8);
    descriptor3.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor3.count(1);

    sensor_msgs::msg::PointField descriptor4;
    descriptor4.name("size_x");
    descriptor4.offset(12);
    descriptor4.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor4.count(1);

    sensor_msgs::msg::PointField descriptor5;
    descriptor5.name("size_y");
    descriptor5.offset(16);
    descriptor5.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor5.count(1);

    sensor_msgs::msg::PointField descriptor6;
    descriptor6.name("size_z");
    descriptor6.offset(20);
    descriptor6.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor6.count(1);

    sensor_msgs::msg::PointField descriptor7;
    descriptor7.name("qx");
    descriptor7.offset(24);
    descriptor7.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor7.count(1);

    sensor_msgs::msg::PointField descriptor8;
    descriptor8.name("qy");
    descriptor8.offset(28);
    descriptor8.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor8.count(1);

    sensor_msgs::msg::PointField descriptor9;
    descriptor9.name("qz");
    descriptor9.offset(32);
    descriptor9.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor9.count(1);

    sensor_msgs::msg::PointField descriptor10;
    descriptor10.name("qw");
    descriptor10.offset(36);
    descriptor10.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor10.count(1);

    sensor_msgs::msg::PointField descriptor11;
    descriptor11.name("vx");
    descriptor11.offset(40);
    descriptor11.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor11.count(1);

    sensor_msgs::msg::PointField descriptor12;
    descriptor12.name("vy");
    descriptor12.offset(44);
    descriptor12.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor12.count(1);

    sensor_msgs::msg::PointField descriptor13;
    descriptor13.name("vz");
    descriptor13.offset(48);
    descriptor13.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor13.count(1);

    sensor_msgs::msg::PointField descriptor14;
    descriptor14.name("object_idx");
    descriptor14.offset(52);
    descriptor14.datatype(sensor_msgs::msg::PointField__UINT32);
    descriptor14.count(1);

    sensor_msgs::msg::PointField descriptor15;
    descriptor15.name("object_tag");
    descriptor15.offset(56);
    descriptor15.datatype(sensor_msgs::msg::PointField__UINT32);
    descriptor15.count(1);

    return {descriptor1, descriptor2, descriptor3, descriptor4, descriptor5, descriptor6, descriptor7, descriptor8, descriptor9, descriptor10, descriptor11, descriptor12, descriptor13, descriptor14, descriptor15};
  }

  std::vector<uint8_t> CarlaSemanticRadarPublisher::ComputePointCloud(uint32_t height, uint32_t width, uint8_t *data) {

    const auto *detections = reinterpret_cast<const sensor::data::SemanticRadarDetection*>(data);

    const size_t count = static_cast<size_t>(height) * width;
    std::vector<uint8_t> output(count * sizeof(SemanticRadarPoint));

    constexpr double DEG_TO_HALF_RAD = 3.14159265358979323846 / 360.0;

    for (size_t i = 0; i < count; ++i) {
      const auto &detection = detections[i];
      const auto &box = detection.bbox;
      SemanticRadarPoint point{};

      point.x = box.location.x;
      point.y = -box.location.y;
      point.z = box.location.z;

      point.size_x = 2.0f * box.extent.x;
      point.size_y = 2.0f * box.extent.y;
      point.size_z = 2.0f * box.extent.z;

      const double roll  =  box.rotation.roll  * DEG_TO_HALF_RAD;
      const double pitch = -box.rotation.pitch * DEG_TO_HALF_RAD;
      const double yaw   = -box.rotation.yaw   * DEG_TO_HALF_RAD;

      const double cr = std::cos(roll);
      const double sr = std::sin(roll);
      const double cp = std::cos(pitch);
      const double sp = std::sin(pitch);
      const double cy = std::cos(yaw);
      const double sy = std::sin(yaw);

      point.qx = static_cast<float>(sr * cp * cy - cr * sp * sy);
      point.qy = static_cast<float>(cr * sp * cy + sr * cp * sy);
      point.qz = static_cast<float>(cr * cp * sy - sr * sp * cy);
      point.qw = static_cast<float>(cr * cp * cy + sr * sp * sy);

      point.vx = detection.velocity.x;
      point.vy = -detection.velocity.y;
      point.vz = detection.velocity.z;

      point.object_idx = detection.object_idx;
      point.object_tag = detection.object_tag;

      std::memcpy(
          output.data() + i * sizeof(SemanticRadarPoint),
          &point,
          sizeof(point));
    }

    return output;
  }
}  // namespace ros2
}  // namespace carla