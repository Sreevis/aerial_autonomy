#pragma once
#include "aerial_autonomy/controllers/base_controller.h"
#include "aerial_autonomy/controllers/velocity_based_position_controller.h"
#include "aerial_autonomy/types/velocity_yaw.h"
#include "velocity_based_relative_pose_controller_config.pb.h"
#include <aerial_autonomy/VelocityBasedPositionControllerDynamicConfig.h>

#include <tuple>

#include <tf/tf.h>

/**
 * @brief A pose controller that keeps a pose relative to some feedback
 * pose using a velocity controller.
 * Note 1: Only the yaw of the desired pose is tracked
 * since a quadrotor is underactuated and cannot arbitrarily control roll/pitch
 * while hovering
 */
class VelocityBasedRelativePoseController
    : public Controller<std::tuple<tf::Transform, tf::Transform>, PositionYaw,
                        VelocityYawRate> {
public:
  /**
  * @brief Constructor
  */
  VelocityBasedRelativePoseController(
      VelocityBasedRelativePoseControllerConfig config,
      std::chrono::duration<double> dt = std::chrono::milliseconds(20))
      : config_(config),
        position_controller_(config.velocity_based_position_controller_config(),
                             dt) {}
  /**
   * @brief Destructor
   */
  virtual ~VelocityBasedRelativePoseController() {}

  /**
   * @brief Reset the internal position controller integrator
   */
  void resetIntegrator() { position_controller_.resetIntegrator(); }

  /**
   * @brief Get the default values of gains
   *
   * @return dynamic reconfigure with default values
   */
  aerial_autonomy::VelocityBasedPositionControllerDynamicConfig
  getDefaultConfig() const;

  /**
   * @brief Update internal controller config
   *
   * @param dynamic_config reconfigure configure values
   */
  void updateConfig(
      const aerial_autonomy::VelocityBasedPositionControllerDynamicConfig
          &dynamic_config);

protected:
  /**
   * @brief Run the control loop.  Uses a velocity controller to keep a desired
   * pose relative to a tracked pose.
   * @param sensor_data Pose of controlled point and tracked
   * pose.  NOTE: Both poses need to be given in the frame in which the
   * velocity command is executed
   *
   * @param goal Goal relative position and yaw in tracked pose frame
   * @param control Velocity command
   * @return True if controller is successful in running
   */
  virtual bool
  runImplementation(std::tuple<tf::Transform, tf::Transform> sensor_data,
                    PositionYaw goal, VelocityYawRate &control);
  /**
  * @brief Check if controller converged
  *
  * @param sensor_data Current control pose and tracked pose
  * @param goal Goal relative position and yaw in tracked pose frame
  * NOTE: Both poses need to be given in the frame in which the
  * velocity command is executed
  *
  * @return controller status that contains an enum and debug information.
  */
  virtual ControllerStatus isConvergedImplementation(
      std::tuple<tf::Transform, tf::Transform> sensor_data, PositionYaw goal);

private:
  /**
  * @brief Config specifies position tolerance
  */
  VelocityBasedRelativePoseControllerConfig config_;
  /**
   * @brief Internal velocity based position controller to get to the relative
   * pose
   */
  VelocityBasedPositionController position_controller_;
};
