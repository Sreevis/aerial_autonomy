#pragma once
#include "aerial_autonomy/trackers/tracking_strategy.h"

#include <tf/tf.h>

#include <tuple>
#include <unordered_map>

/**
 * @brief Interface for classes that provide a vector
 * to a tracked target
 */
class BaseTracker {
public:
  BaseTracker(std::unique_ptr<TrackingStrategy> tracking_strategy)
      : tracking_strategy_(std::move(tracking_strategy)) {}
  /**
  * @brief Initialze the tracker.  Can simply return true if the subclass
  * requires no additional initialization.
  * @return True if initialization succeeds, false otherwise
  */
  virtual bool initialize();
  /**
  * @brief Set the tracker's tracking strategy
  * @param The desired tracking strategy
  */
  void setTrackingStrategy(std::unique_ptr<TrackingStrategy> tracking_strategy);
  /**
   * @brief Get the tracking vector
   * @param pos Returned tracking vector
   * @return True if successful, false otherwise
   */
  virtual bool getTrackingVector(tf::Transform &pos);
  /**
   * @brief Get the tracking vector
   * @param pos Returned tracking vector
   * @return True if successful, false otherwise
   */
  virtual bool getTrackingVector(std::tuple<uint32_t, tf::Transform> &pos);
  /**
  * @brief Get the tracking pose corresponding to specified id
  * @param tracked_id id for which tracking vector is required
  * @param pose the output tracking pose
  * @return true if the id is found otherwise false
  */
  // \todo Matt Make this a specific tracking strategy
  // Also implement VisionSystem with a way to set tracking strategies
  // for trackers on the fly (like setGoal in RobotSystem)
  virtual bool getTrackingVector(uint32_t tracked_id, tf::Transform &pose);
  /**
   * @brief Get the tracking vectors
   * @param pos Returned map of tracking vectors
   * @return True if successful, false otherwise
   */
  virtual bool
  getTrackingVectors(std::unordered_map<uint32_t, tf::Transform> &pos) = 0;
  /**
  * @brief Check whether tracking is valid
  * @return True if the tracking is valid, false otherwise
  */
  virtual bool trackingIsValid() = 0;

private:
  /**
  * @brief Strategy used to choose which object to track among multiple objects
  */
  std::unique_ptr<TrackingStrategy> tracking_strategy_;
};
