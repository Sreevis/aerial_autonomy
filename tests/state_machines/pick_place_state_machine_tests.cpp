#include <aerial_autonomy/state_machines/pick_place_state_machine.h>
#include <aerial_autonomy/tests/test_utils.h>
#include <aerial_autonomy/trackers/simple_tracker.h>
#include <arm_parsers/arm_simulator.h>
#include <gtest/gtest.h>
// Timer stuff
#include <chrono>
#include <thread>
// Quad Simulator
#include <quad_simulator_parser/quad_simulator.h>

/**
* @brief Namespace for UAV Simulator Hardware
*/
using namespace quad_simulator;

/**
* @brief Visual servoing events such as TrackROI, GoHome
*/
namespace vse = visual_servoing_events;

/**
* @brief Namespace for pick place events
*/
namespace pe = pick_place_events;

/**
* @brief Namespace for basic events such as takeoff, land.
*/
namespace be = uav_basic_events;

class PickPlaceStateMachineTests : public ::testing::Test {
public:
  PickPlaceStateMachineTests() : goal_tolerance_position_(0.1) {
    auto uav_vision_system_config = config_.mutable_uav_vision_system_config();
    for (int i = 0; i < 6; ++i) {
      uav_vision_system_config->add_camera_transform(0.0);
      uav_vision_system_config->add_tracking_offset_transform(0.0);
    }
    uav_vision_system_config->set_desired_visual_servoing_distance(1.0);
    auto uav_arm_system_config =
        uav_vision_system_config->mutable_uav_arm_system_config();
    for (int i = 0; i < 6; ++i) {
      uav_arm_system_config->add_arm_transform(0);
    }
    for (int i = 0; i < 12; ++i) {
      uav_arm_system_config->add_arm_goal_transform(0);
    }
    auto depth_config =
        uav_vision_system_config
            ->mutable_constant_heading_depth_controller_config();
    depth_config->set_radial_gain(0.5);
    auto position_tolerance = depth_config->mutable_position_controller_config()
                                  ->mutable_goal_position_tolerance();
    position_tolerance->set_x(goal_tolerance_position_);
    position_tolerance->set_y(goal_tolerance_position_);
    position_tolerance->set_z(goal_tolerance_position_);

    // Position controller params
    auto pos_controller_config =
        config_.mutable_velocity_based_position_controller_config();
    pos_controller_config->set_position_gain(10.);
    pos_controller_config->set_yaw_gain(10.);
    pos_controller_config->set_max_velocity(20.);
    pos_controller_config->set_max_yaw_rate(5.);
    auto goal_position_tolerance =
        pos_controller_config->mutable_position_controller_config()
            ->mutable_goal_position_tolerance();
    pos_controller_config->mutable_position_controller_config()
        ->set_goal_yaw_tolerance(0.1);
    goal_position_tolerance->set_x(goal_tolerance_position_);
    goal_position_tolerance->set_y(goal_tolerance_position_);
    goal_position_tolerance->set_z(goal_tolerance_position_);

    // Relative visual servoing controller params
    auto vel_based_vs_controller_config =
        uav_vision_system_config
            ->mutable_velocity_based_relative_pose_controller_config()
            ->mutable_velocity_based_position_controller_config();
    vel_based_vs_controller_config->set_position_gain(10.);
    vel_based_vs_controller_config->set_max_velocity(10.);
    auto relative_pose_vs_position_tolerance =
        vel_based_vs_controller_config->mutable_position_controller_config()
            ->mutable_goal_position_tolerance();
    vel_based_vs_controller_config->mutable_position_controller_config()
        ->set_goal_yaw_tolerance(0.1);
    relative_pose_vs_position_tolerance->set_x(goal_tolerance_position_);
    relative_pose_vs_position_tolerance->set_y(goal_tolerance_position_);
    relative_pose_vs_position_tolerance->set_z(goal_tolerance_position_);

    // Relative marker goal for pick
    auto vision_state_machine_config =
        state_machine_config_.mutable_visual_servoing_state_machine_config();
    auto pose_goal = vision_state_machine_config->add_relative_pose_goals();
    auto pose_goal_position = pose_goal->mutable_position();
    pose_goal_position->set_x(1);
    pose_goal_position->set_y(1);
    pose_goal_position->set_z(2);
    pose_goal->set_yaw(0);
    // Relative marker goal for place
    vision_state_machine_config->add_relative_pose_goals();

    // Post-pick waypoints
    for (int i = 0; i < 2; i++) {
      auto pose_goal = uav_vision_system_config->mutable_uav_arm_system_config()
                           ->add_way_points();
      auto pose_goal_position = pose_goal->mutable_position();
      pose_goal_position->set_x(-i);
      pose_goal_position->set_y(0);
      pose_goal_position->set_z(1);
      pose_goal->set_yaw(0);
    }

    // Post-place waypoints
    for (int i = 0; i < 2; i++) {
      auto pose_goal = uav_vision_system_config->mutable_uav_arm_system_config()
                           ->add_way_points();
      auto pose_goal_position = pose_goal->mutable_position();
      pose_goal_position->set_x(i);
      pose_goal_position->set_y(0);
      pose_goal_position->set_z(1);
      pose_goal->set_yaw(M_PI);
    }

    // Arm controller params
    auto arm_position_tolerance =
        uav_arm_system_config->mutable_position_controller_config()
            ->mutable_goal_position_tolerance();
    arm_position_tolerance->set_x(goal_tolerance_position_);
    arm_position_tolerance->set_y(goal_tolerance_position_);
    arm_position_tolerance->set_z(goal_tolerance_position_);

    tf::Transform camera_transform = math::getTransformFromVector(
        uav_vision_system_config->camera_transform());
    tracker_.reset(new SimpleTracker(drone_hardware_, camera_transform));
    uav_arm_system_.reset(
        new UAVArmSystem(*tracker_, drone_hardware_, arm_, config_));
    logic_state_machine_.reset(new PickPlaceStateMachine(
        boost::ref(*uav_arm_system_), boost::cref(state_machine_config_)));
    logic_state_machine_->start();
    // Move to landed state
    logic_state_machine_->process_event(InternalTransitionEvent());

    // Targets used in pick place
    std::unordered_map<uint32_t, tf::Transform> targets;
    // Target object
    targets[0] =
        tf::Transform(tf::Quaternion(0, 0, 0, 1), tf::Vector3(2, 0, 0.5));
    // Place goal
    targets[15] =
        tf::Transform(tf::Quaternion(0, 0, 0, 1), tf::Vector3(-3, 0, 0.5));
    tracker_->setTargetPosesGlobalFrame(targets);
  }

  static void SetUpTestCase() {
    // Configure logging
    LogConfig log_config;
    log_config.set_directory("/tmp/data");
    Log::instance().configure(log_config);
    DataStreamConfig data_config;
    data_config.set_stream_id(
        "relative_pose_visual_servoing_controller_drone_connector");
    Log::instance().addDataStream(data_config);
    data_config.set_stream_id("velocity_based_position_controller");
    Log::instance().addDataStream(data_config);
    data_config.set_stream_id("velocity_based_relative_pose_controller");
    Log::instance().addDataStream(data_config);
  }

  ~PickPlaceStateMachineTests() {
    logic_state_machine_->stop();
    uav_arm_system_.reset();
    logic_state_machine_.reset();
  }

protected:
  QuadSimulator drone_hardware_;
  ArmSimulator arm_;
  UAVSystemConfig config_;
  BaseStateMachineConfig state_machine_config_;
  std::unique_ptr<SimpleTracker> tracker_;
  std::unique_ptr<UAVArmSystem> uav_arm_system_;
  std::unique_ptr<PickPlaceStateMachine> logic_state_machine_;
  double goal_tolerance_position_;

  template <class EventT> void testManualControlAbort() {
    // First takeoff
    GoToHoverFromLanded();
    logic_state_machine_->process_event(EventT());
    logic_state_machine_->process_event(InternalTransitionEvent());
    ASSERT_STRNE(pstate(*logic_state_machine_), "Hovering");
    // Disable SDK
    drone_hardware_.flowControl(false);
    // Check we are in Hovering
    logic_state_machine_->process_event(InternalTransitionEvent());
    ASSERT_STREQ(pstate(*logic_state_machine_), "Hovering");
    // And finally in manual control state
    logic_state_machine_->process_event(InternalTransitionEvent());
    ASSERT_STREQ(pstate(*logic_state_machine_), "ManualControlArmState");
    // Enable SDK
    drone_hardware_.flowControl(true);
    // Check we are back in hovering
    logic_state_machine_->process_event(InternalTransitionEvent());
    ASSERT_STREQ(pstate(*logic_state_machine_), "Hovering");
  }

  template <class EventT> void testArmOffAbort() {
    // First takeoff
    GoToHoverFromLanded();
    logic_state_machine_->process_event(EventT());
    logic_state_machine_->process_event(InternalTransitionEvent());
    ASSERT_STRNE(pstate(*logic_state_machine_), "Hovering");
    // Power off arm
    arm_.sendCmd(ArmParser::Command::POWER_OFF);
    // Check we are in Hovering
    logic_state_machine_->process_event(InternalTransitionEvent());
    ASSERT_STREQ(pstate(*logic_state_machine_), "Hovering");
  }

  void GoToHoverFromLanded() {
    drone_hardware_.setBatteryPercent(100);
    logic_state_machine_->process_event(be::Takeoff());
    // Powers on arm and folds it; Starts takeoff
    logic_state_machine_->process_event(InternalTransitionEvent());
    // Completes takeoff and goes to hovering
    logic_state_machine_->process_event(InternalTransitionEvent());
  }
};

TEST_F(PickPlaceStateMachineTests, InitialState) {
  // Validate
  ASSERT_STREQ(pstate(*logic_state_machine_), "Landed");
  GoToHoverFromLanded();
}

/// \brief Test Arm folding during landing and takeoff
TEST_F(PickPlaceStateMachineTests, HoveringandLanding) {
  // Takeoff
  drone_hardware_.setBatteryPercent(100);
  logic_state_machine_->process_event(be::Takeoff());
  ASSERT_STREQ(pstate(*logic_state_machine_), "ArmPreTakeoffFolding");
  // Powers on arm and folds it; Starts takeoff
  logic_state_machine_->process_event(InternalTransitionEvent());
  ASSERT_STREQ(pstate(*logic_state_machine_), "Takingoff");
  // Completes takeoff and goes to hovering
  logic_state_machine_->process_event(InternalTransitionEvent());
  ASSERT_STREQ(pstate(*logic_state_machine_), "Hovering");
  logic_state_machine_->process_event(be::Land());
  ASSERT_STREQ(pstate(*logic_state_machine_), "ArmPreLandingFolding");
  logic_state_machine_->process_event(InternalTransitionEvent());
  ASSERT_STREQ(pstate(*logic_state_machine_), "Landing");
  logic_state_machine_->process_event(InternalTransitionEvent());
  ASSERT_STREQ(pstate(*logic_state_machine_), "Landed");
}
///
/// \brief Test Pick Place
// Try full pick-and-place
TEST_F(PickPlaceStateMachineTests, PickPlace) {
  GoToHoverFromLanded();
  // Start Pick
  logic_state_machine_->process_event(pe::Pick());
  // Check we are waiting for pick
  ASSERT_STREQ(pstate(*logic_state_machine_), "WaitingForPick");
  // Check in PickState
  logic_state_machine_->process_event(InternalTransitionEvent());
  ASSERT_STREQ(pstate(*logic_state_machine_), "PickState");
  // Check UAV and arm controllers are active
  ASSERT_EQ(
      uav_arm_system_
          ->getStatus<RelativePoseVisualServoingControllerDroneConnector>(),
      ControllerStatus::Active);
  ASSERT_EQ(uav_arm_system_->getStatus<BuiltInPoseControllerArmConnector>(),
            ControllerStatus::Active);
  // Keep running the controller until its completed or timeout
  auto getStatusRunControllers = [&]() {
    uav_arm_system_->runActiveController(HardwareType::UAV);
    uav_arm_system_->runActiveController(HardwareType::Arm);
    return uav_arm_system_->getActiveControllerStatus(HardwareType::UAV) ==
               ControllerStatus::Active ||
           uav_arm_system_->getActiveControllerStatus(HardwareType::Arm) ==
               ControllerStatus::Active;
  };
  ASSERT_FALSE(test_utils::waitUntilFalse()(getStatusRunControllers,
                                            std::chrono::seconds(5),
                                            std::chrono::milliseconds(0)));

  // Grip object for required duration
  arm_.setGripperStatus(true);
  logic_state_machine_->process_event(InternalTransitionEvent());
  this_thread::sleep_for(std::chrono::milliseconds(1100));
  logic_state_machine_->process_event(InternalTransitionEvent());
  // Check we are in Post-Pick
  ASSERT_STREQ(pstate(*logic_state_machine_), "ReachingPostPickWaypoint");
  ASSERT_EQ(logic_state_machine_->lastProcessedEventIndex(), typeid(Completed));
  // Run controllers through two waypoints
  ASSERT_FALSE(test_utils::waitUntilFalse()(getStatusRunControllers,
                                            std::chrono::seconds(5),
                                            std::chrono::milliseconds(0)));
  logic_state_machine_->process_event(InternalTransitionEvent());
  ASSERT_FALSE(test_utils::waitUntilFalse()(getStatusRunControllers,
                                            std::chrono::seconds(5),
                                            std::chrono::milliseconds(0)));
  logic_state_machine_->process_event(InternalTransitionEvent());
  // Check we are in Place
  ASSERT_STREQ(pstate(*logic_state_machine_), "PlaceState");
  ASSERT_EQ(logic_state_machine_->lastProcessedEventIndex(), typeid(Completed));
  // Place the object
  ASSERT_FALSE(test_utils::waitUntilFalse()(getStatusRunControllers,
                                            std::chrono::seconds(5),
                                            std::chrono::milliseconds(0)));
  logic_state_machine_->process_event(InternalTransitionEvent());
  // Check Place completed with ungrip
  ASSERT_STREQ(pstate(*logic_state_machine_), "ReachingPostPlaceWaypoint");
  ASSERT_EQ(logic_state_machine_->lastProcessedEventIndex(), typeid(Completed));
  ASSERT_FALSE(arm_.getGripperValue());
  // Run controllers through two waypoints
  ASSERT_FALSE(test_utils::waitUntilFalse()(getStatusRunControllers,
                                            std::chrono::seconds(5),
                                            std::chrono::milliseconds(0)));
  logic_state_machine_->process_event(InternalTransitionEvent());
  ASSERT_FALSE(test_utils::waitUntilFalse()(getStatusRunControllers,
                                            std::chrono::seconds(5),
                                            std::chrono::milliseconds(0)));
  logic_state_machine_->process_event(InternalTransitionEvent());
  // Check we are back to waiting for pick
  ASSERT_STREQ(pstate(*logic_state_machine_), "WaitingForPick");
  ASSERT_EQ(logic_state_machine_->lastProcessedEventIndex(), typeid(Completed));
}

TEST_F(PickPlaceStateMachineTests, PickTimeout) {
  GoToHoverFromLanded();
  // Start Pick
  logic_state_machine_->process_event(pe::Pick());
  // Check we are waiting for pick
  ASSERT_STREQ(pstate(*logic_state_machine_), "WaitingForPick");
  // Check in PickState
  logic_state_machine_->process_event(InternalTransitionEvent());
  ASSERT_STREQ(pstate(*logic_state_machine_), "PickState");
  // Check UAV and arm controllers are active
  ASSERT_EQ(
      uav_arm_system_
          ->getStatus<RelativePoseVisualServoingControllerDroneConnector>(),
      ControllerStatus::Active);
  ASSERT_EQ(uav_arm_system_->getStatus<BuiltInPoseControllerArmConnector>(),
            ControllerStatus::Active);
  // Keep running the controller until its completed or timeout
  auto getStatusRunControllers = [&]() {
    uav_arm_system_->runActiveController(HardwareType::UAV);
    uav_arm_system_->runActiveController(HardwareType::Arm);
    return uav_arm_system_->getActiveControllerStatus(HardwareType::UAV) ==
               ControllerStatus::Active ||
           uav_arm_system_->getActiveControllerStatus(HardwareType::Arm) ==
               ControllerStatus::Active;
  };
  ASSERT_FALSE(test_utils::waitUntilFalse()(getStatusRunControllers,
                                            std::chrono::seconds(5),
                                            std::chrono::milliseconds(0)));

  // Grip has failed
  arm_.setGripperStatus(false);

  auto grip = [&]() {
    logic_state_machine_->process_event(InternalTransitionEvent());
    return pstate(*logic_state_machine_) == std::string("PickState");
  };
  ASSERT_FALSE(test_utils::waitUntilFalse()(grip, std::chrono::seconds(5),
                                            std::chrono::milliseconds(20)));
  // Check we are resetting
  ASSERT_STREQ(pstate(*logic_state_machine_), "ResetVisualServoing");
  ASSERT_EQ(logic_state_machine_->lastProcessedEventIndex(), typeid(Reset));
}

TEST_F(PickPlaceStateMachineTests, PickWaitForGrip) {
  GoToHoverFromLanded();
  // Start Pick
  logic_state_machine_->process_event(pe::Pick());
  // Check we are waiting for pick
  ASSERT_STREQ(pstate(*logic_state_machine_), "WaitingForPick");
  // Check in PickState
  logic_state_machine_->process_event(InternalTransitionEvent());
  ASSERT_STREQ(pstate(*logic_state_machine_), "PickState");

  // Initially not gripping
  arm_.setGripperStatus(false);
  logic_state_machine_->process_event(InternalTransitionEvent());
  this_thread::sleep_for(std::chrono::milliseconds(900));
  // Now gripping
  arm_.setGripperStatus(true);

  auto grip = [&]() {
    logic_state_machine_->process_event(InternalTransitionEvent());
    return pstate(*logic_state_machine_) == std::string("PickState");
  };
  ASSERT_FALSE(test_utils::waitUntilFalse()(grip, std::chrono::seconds(5),
                                            std::chrono::milliseconds(0)));
  ASSERT_EQ(logic_state_machine_->lastProcessedEventIndex(), typeid(Completed));
  // Check we are in hovering state
  ASSERT_STREQ(pstate(*logic_state_machine_), "ReachingPostPickWaypoint");
}

TEST_F(PickPlaceStateMachineTests, PickGripTooLate) {
  GoToHoverFromLanded();
  // Start Pick
  logic_state_machine_->process_event(pe::Pick());
  // Check we are waiting for pick
  ASSERT_STREQ(pstate(*logic_state_machine_), "WaitingForPick");
  // Check in PickState
  logic_state_machine_->process_event(InternalTransitionEvent());
  ASSERT_STREQ(pstate(*logic_state_machine_), "PickState");

  // Initially not gripping
  arm_.setGripperStatus(false);
  logic_state_machine_->process_event(InternalTransitionEvent());
  this_thread::sleep_for(std::chrono::milliseconds(1100));
  // Now gripping, but not enough time to complete grip before timeout
  arm_.setGripperStatus(true);

  auto grip = [&]() {
    logic_state_machine_->process_event(InternalTransitionEvent());
    return pstate(*logic_state_machine_) == std::string("PickState");
  };
  ASSERT_FALSE(test_utils::waitUntilFalse()(grip, std::chrono::seconds(5),
                                            std::chrono::milliseconds(0)));
  ASSERT_STREQ(pstate(*logic_state_machine_), "ResetVisualServoing");
  ASSERT_EQ(logic_state_machine_->lastProcessedEventIndex(), typeid(Reset));
}

// Abort due to arm not enabled
TEST_F(PickPlaceStateMachineTests, PickArmAbort) {
  testArmOffAbort<pe::Pick>();
}

TEST_F(PickPlaceStateMachineTests, PlaceArmAbort) {
  testArmOffAbort<pe::Place>();
}

// Abort due to tracking becoming invalid
TEST_F(PickPlaceStateMachineTests, PickPlaceInvalidTrackingAbort) {
  // First takeoff
  GoToHoverFromLanded();
  logic_state_machine_->process_event(pe::Pick());
  logic_state_machine_->process_event(InternalTransitionEvent());
  // Check we are in pick state
  ASSERT_STREQ(pstate(*logic_state_machine_), "PickState");
  // Set tracker to invalid
  tracker_->setTrackingIsValid(false);
  // Run active controllers
  for (int i = 0; i < 2; ++i) {
    uav_arm_system_->runActiveController(HardwareType::UAV);
    uav_arm_system_->runActiveController(HardwareType::Arm);
  }
  // Check we are resetting
  logic_state_machine_->process_event(InternalTransitionEvent());
  ASSERT_STREQ(pstate(*logic_state_machine_), "ResetVisualServoing");
}
// Manual rc abort
TEST_F(PickPlaceStateMachineTests, PickManualControlAbort) {
  testManualControlAbort<pe::Pick>();
}
TEST_F(PickPlaceStateMachineTests, PlaceManualControlAbort) {
  testManualControlAbort<pe::Place>();
}
// Manual control internal actions
TEST_F(PickPlaceStateMachineTests, PickPlaceManualControlInternalActions) {
  // Disable SDK
  drone_hardware_.flowControl(false);
  // Move to manual control state
  logic_state_machine_->process_event(InternalTransitionEvent());
  // Check we are in manual control state
  ASSERT_STREQ(pstate(*logic_state_machine_), "ManualControlArmState");
  // Enable arm
  uav_arm_system_->power(true);
  // Process Poweroff
  logic_state_machine_->process_event(arm_events::PowerOff());
  // Check arm is powered off
  ASSERT_FALSE(uav_arm_system_->enabled());
  // Process Poweron
  logic_state_machine_->process_event(arm_events::PowerOn());
  // Check arm is powered on
  ASSERT_TRUE(uav_arm_system_->enabled());
  // Check command status is false
  ASSERT_FALSE(arm_.getCommandStatus());
  // Check we can process fold
  logic_state_machine_->process_event(arm_events::Fold());
  ASSERT_TRUE(arm_.getCommandStatus());
  // Check we can process right fold
  logic_state_machine_->process_event(arm_events::RightAngleFold());
  ASSERT_TRUE(arm_.getCommandStatus());
  // Check we are still in Manual Control State
  ASSERT_STREQ(pstate(*logic_state_machine_), "ManualControlArmState");
}
///

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
