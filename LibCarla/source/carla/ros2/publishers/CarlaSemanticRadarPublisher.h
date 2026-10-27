#pragma once

#include <memory>
#include <vector>

#include "CarlaPointCloudPublisher.h"

namespace carla {
namespace ros2 {

  class CarlaSemanticRadarPublisher : public CarlaPointCloudPublisher {
    public:
      CarlaSemanticRadarPublisher(std::string base_topic_name, std::string frame_id) :
        CarlaPointCloudPublisher(base_topic_name, frame_id) {}

    private:
      const size_t GetPointSize() override;
      std::vector<sensor_msgs::msg::PointField> GetFields() override;

      std::vector<uint8_t> ComputePointCloud(uint32_t height, uint32_t width, uint8_t *data) override;
  };
}
}
