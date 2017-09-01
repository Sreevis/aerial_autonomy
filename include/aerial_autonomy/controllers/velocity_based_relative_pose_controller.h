#pragma once
#include "aerial_autonomy/controllers/base_controller.h"
#include "aerial_autonomy/controllers/velocity_based_position_controller.h"
#include "aerial_autonomy/types/velocity_yaw.h"
#include "velocity_based_relative_pose_controller_config.pb.h"

#include <tuple>

#include <tf/tf.h>

/**
 * @brief A pose controller that keeps a pose relative to some feedback
 * pose using a velocity controller
 */
class VelocityBasedRelativePoseController
    : public Controller<std::tuple<tf::Transform, tf::Transform>, tf::Transform,
                        VelocityYawRate> {
public:
  /**
  * @brief Constructor
  */
  VelocityBasedRelativePoseController(
      VelocityBasedRelativePoseControllerConfig config)
      : config_(config),
        position_controller_(
            config.velocity_based_position_controller_config()) {}
  /**
   * @brief Destructor
   */
  virtual ~VelocityBasedRelativePoseController() {}

protected:
  /**
   * @brief Run the control loop.  Uses a velocity controller to keep a desired
   * pose relative to a tracked pose.
   * @param sensor_data Pose of controlled point and tracked
   * pose
   * @param goal Goal relative pose in tracked pose frame
   * @param control Velocity command
   * @return True if controller is successful in running
   */
  virtual bool
  runImplementation(std::tuple<tf::Transform, tf::Transform> sensor_data,
                    tf::Transform goal, VelocityYawRate &control);
  /**
  * @brief Check if controller converged
  *
  * @param sensor_data Current control pose and tracked pose
  * @param goal Goal relative pose in tracked pose frame
  *
  * @return controller status that contains an enum and debug information.
  */
  virtual ControllerStatus isConvergedImplementation(
      std::tuple<tf::Transform, tf::Transform> sensor_data, tf::Transform goal);

private:
  /**
  * @brief Config specifies position tolerance
  */
  VelocityBasedRelativePoseControllerConfig config_;
  VelocityBasedPositionController position_controller_;
};
