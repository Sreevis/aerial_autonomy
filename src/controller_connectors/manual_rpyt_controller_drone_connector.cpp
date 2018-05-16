#include "aerial_autonomy/controller_connectors/manual_rpyt_controller_drone_connector.h"

bool ManualRPYTControllerDroneConnector::extractSensorData(
    Joystick &sensor_data) {
  parsernode::common::quaddata quad_data;
  drone_hardware_.getquaddata(quad_data);
  sensor_data = Joystick(quad_data.servo_in[0], quad_data.servo_in[1],
                         quad_data.servo_in[2], quad_data.servo_in[3]);
  return true;
}

void ManualRPYTControllerDroneConnector::sendControllerCommands(
    RollPitchYawRateThrust controls) {
  geometry_msgs::Quaternion rpyt_command;
  rpyt_command.x = controls.r;
  rpyt_command.y = controls.p;
  rpyt_command.z = controls.y;
  rpyt_command.w = controls.t;
  drone_hardware_.cmdrpyawratethrust(rpyt_command);
}
