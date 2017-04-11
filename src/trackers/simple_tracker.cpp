#include "aerial_autonomy/trackers/simple_tracker.h"
#include <glog/logging.h>

SimpleTracker::SimpleTracker(parsernode::Parser &drone_hardware,
                             UAVVisionSystemConfig config)
    : drone_hardware_(drone_hardware) {
  auto camera_transform = config.camera_transform();
  if (camera_transform.size() != 6) {
    LOG(WARNING) << "Camera transform configuration does not have 6 parameters";
    tracking_valid_ = false;
  } else {
    tracking_valid_ = true;
    camera_transform_.setOrigin(tf::Vector3(
        camera_transform[0], camera_transform[1], camera_transform[2]));
    camera_transform_.setRotation(tf::createQuaternionFromRPY(
        camera_transform[3], camera_transform[4], camera_transform[5]));
  }
}

bool SimpleTracker::getTrackingVector(Position &p) {
  if (!trackingIsValid()) {
    return false;
  }
  parsernode::common::quaddata uav_data;
  drone_hardware_.getquaddata(uav_data);
  tf::Transform quad_tf_global(
      tf::createQuaternionFromRPY(uav_data.rpydata.x, uav_data.rpydata.y,
                                  uav_data.rpydata.z),
      tf::Vector3(uav_data.localpos.x, uav_data.localpos.y,
                  uav_data.localpos.z));
  tf::Vector3 target_position_global(target_position_.x, target_position_.y,
                                     target_position_.z);
  tf::Vector3 target_position_camera = camera_transform_.inverse() *
                                       quad_tf_global.inverse() *
                                       target_position_global;
  p.x = target_position_camera.getX();
  p.y = target_position_camera.getY();
  p.z = target_position_camera.getZ();
  return true;
}

void SimpleTracker::setTargetPositionGlobalFrame(Position p) {
  target_position_ = p;
}

bool SimpleTracker::trackingIsValid() { return tracking_valid_; }

void SimpleTracker::setTrackingIsValid(bool is_valid) {
  tracking_valid_ = is_valid;
}

tf::Transform SimpleTracker::cameraTransform() { return camera_transform_; }
