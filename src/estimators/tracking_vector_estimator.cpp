#include <aerial_autonomy/estimators/tracking_vector_estimator.h>
#include <aerial_autonomy/log/log.h>
#include <glog/logging.h>

TrackingVectorEstimator::TrackingVectorEstimator(
    TrackingVectorEstimatorConfig config,
    std::chrono::duration<double> propagation_step)
    : config_(config), filter_(3, 3, 3, CV_64F), zero_tolerance_(1e-6),
      initial_state_initialized_(false) {
  // Assuming x = [Marker direction] and u = [velocity]
  // Transition matrix
  filter_.transitionMatrix = cv::Mat_<double>::eye(3, 3);
  filter_.controlMatrix =
      -propagation_step.count() * cv::Mat_<double>::eye(3, 3);
  // Measurement matrix
  filter_.measurementMatrix = cv::Mat_<double>::eye(3, 3);
  // Check stdeviation vectors
  checkStdVector(config_.marker_process_stdev());
  checkStdVector(config_.marker_meas_stdev());
  checkStdVector(config_.marker_initial_stdev());
  checkStdVector(config_.marker_dilation_stdev());
  // Noise matrices
  setCovarianceMatrix(filter_.processNoiseCov, config_.marker_process_stdev());
  setCovarianceMatrix(filter_.measurementNoiseCov, config_.marker_meas_stdev());
  // Save variables
  marker_dilation_stdev_ = tf::Vector3(config_.marker_dilation_stdev().x(),
                                       config_.marker_dilation_stdev().y(),
                                       config_.marker_dilation_stdev().z());
  marker_meas_stdev_ = tf::Vector3(config_.marker_meas_stdev().x(),
                                   config_.marker_meas_stdev().y(),
                                   config_.marker_meas_stdev().z());
  // Set initial state
  initializeState(tf::Vector3(0, 0, 0));
  DATA_HEADER("tracking_vector_estimator") << "Measured_Marker_x"
                                           << "Measured_Marker_y"
                                           << "Measured_Marker_z"
                                           << "Marker_x"
                                           << "Marker_y"
                                           << "Marker_z"
                                           << "Meas_noise_x"
                                           << "Meas_noise_y"
                                           << "Meas_noise_z"
                                           << "Noise_x"
                                           << "Noise_y"
                                           << "Noise_z" << DataStream::endl;
}

void TrackingVectorEstimator::initializeState(tf::Vector3 marker_direction) {
  VLOG(1) << "Initializing KF with Marker dirxn: " << marker_direction.x()
          << marker_direction.y() << marker_direction.z();
  filter_.statePre = (cv::Mat_<double>(3, 1) << marker_direction.x(),
                      marker_direction.y(), marker_direction.z());
  setCovarianceMatrix(filter_.errorCovPost, config_.marker_initial_stdev());
  filter_.statePre.copyTo(filter_.statePost);
  initial_state_initialized_ = true;
}

void TrackingVectorEstimator::predict(tf::Vector3 velocity) {
  cv::Mat_<double> control =
      (cv::Mat_<double>(3, 1) << velocity.x(), velocity.y(), velocity.z());
  filter_.predict(control);
}

void TrackingVectorEstimator::setMeasurementCovariance(
    cv::Mat &covariance_mat,
    std::chrono::time_point<std::chrono::high_resolution_clock>
        marker_time_stamp) {
  double dt = std::chrono::duration<double>(
                  std::chrono::high_resolution_clock::now() - marker_time_stamp)
                  .count();
  if (dt < 0) {
    LOG(WARNING) << "dt negative: " << dt;
    dt = 0;
  } else if (dt > 1.0) {
    LOG(WARNING) << "dt too high: " << dt;
    dt = 1.0;
  }
  tf::Vector3 current_meas_stdev =
      marker_meas_stdev_ + dt * marker_dilation_stdev_;
  setCovarianceMatrix(covariance_mat, current_meas_stdev);
}

void TrackingVectorEstimator::correct(
    tf::Vector3 marker_direction,
    std::chrono::time_point<std::chrono::high_resolution_clock>
        marker_time_stamp) {
  if (!initial_state_initialized_) {
    initializeState(marker_direction);
    return;
  }
  cv::Mat_<double> measurement =
      (cv::Mat_<double>(3, 1) << marker_direction.x(), marker_direction.y(),
       marker_direction.z());
  setMeasurementCovariance(filter_.measurementNoiseCov, marker_time_stamp);
  filter_.correct(measurement);
  // Log data
  tf::Vector3 marker_noise = getMarkerNoise();
  auto state = filter_.statePost;
  auto &data_stream = Log::instance()["tracking_vector_estimator"];
  data_stream << DataStream::startl;
  for (int i = 0; i < 3; ++i) {
    data_stream << measurement.at<double>(i);
  }
  for (int i = 0; i < 3; ++i) {
    data_stream << state.at<double>(i);
  }
  for (int i = 0; i < 3; ++i) {
    data_stream << sqrt(filter_.measurementNoiseCov.at<double>(i, i));
  }
  for (int i = 0; i < 3; ++i) {
    data_stream << marker_noise[i];
  }
  data_stream << DataStream::endl;
}
