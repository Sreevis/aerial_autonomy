#include <aerial_autonomy/common/math.h>
#include <aerial_autonomy/controllers/velocity_based_position_controller.h>

VelocityYaw
VelocityBasedPositionController::runImplementation(PositionYaw sensor_data,
                                                   PositionYaw goal) {
  PositionYaw position_diff = goal - sensor_data;
  // \todo Matt account for wrapping
  auto yaw_cmd = math::angleWrap(
      sensor_data.yaw + math::clamp(config_.yaw_gain() * position_diff.yaw,
                                    -config_.max_yaw_rate(),
                                    config_.max_yaw_rate()));
  double position_norm = position_diff.position().norm();
  double velocity =
      std::min(config_.max_velocity(), config_.position_gain() * position_norm);
  return VelocityYaw(velocity * position_diff.x / position_norm,
                     velocity * position_diff.y / position_norm,
                     velocity * position_diff.z / position_norm, yaw_cmd);
}
