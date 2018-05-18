#pragma once
#include "aerial_autonomy/controller_connectors/base_controller_connector.h"
#include "aerial_autonomy/controllers/constant_heading_depth_controller.h"
#include "aerial_autonomy/trackers/base_tracker.h"
#include "aerial_autonomy/types/position_yaw.h"
#include "aerial_autonomy/types/velocity_yaw.h"
#include "uav_vision_system_config.pb.h"

#include <parsernode/parser.h>

#include <tf/tf.h>

/**
 * @brief A visual servoing controller that uses a tracker output as feedback
 */
class VisualServoingControllerDroneConnector
    : public ControllerConnector<PositionYaw, Position, VelocityYawRate> {
public:
  /**
   * @brief Constructor
   */
  VisualServoingControllerDroneConnector(
      BaseTracker &tracker, parsernode::Parser &drone_hardware,
      ConstantHeadingDepthController &controller,
      tf::Transform camera_transform)
      : ControllerConnector(controller, ControllerGroup::UAV),
        drone_hardware_(drone_hardware), tracker_(tracker),
        camera_transform_(camera_transform) {}
  /**
   * @brief Destructor
   */
  virtual ~VisualServoingControllerDroneConnector() {}

  /**
   * @brief Get the tracking vector of the tracker in the global
   * frame
   * @param tracking_vector Returned tracking vector
   * @return True if successful and false otherwise
   */
  bool getTrackingVectorGlobalFrame(Position &tracking_vector);

protected:
  /**
   * @brief Extracts pose data from ROI
   *
   * @param sensor_data Position and yaw of object tracked by ROI
   *
   * @return true if able to extract ROI position and yaw
   */
  virtual bool extractSensorData(PositionYaw &sensor_data);

  /**
   * @brief  Send velocity commands to hardware
   *
   * @param controls velocity command to send to UAV
   */
  virtual void sendControllerCommands(VelocityYawRate controls);

private:
  /**
   * @brief Get the rotation of the uav body frame
   * @return The rotation transform
   */
  tf::Matrix3x3 getBodyFrameRotation();

  /**
  * @brief Quad hardware to send commands
  */
  parsernode::Parser &drone_hardware_;
  /**
  * @brief Tracks whatever we are servoing to
  */
  BaseTracker &tracker_;
  /**
  * @brief camera transform with respect to body
  */
  tf::Transform camera_transform_;
};
