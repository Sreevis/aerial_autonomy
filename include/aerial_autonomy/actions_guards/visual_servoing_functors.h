#pragma once
#include <aerial_autonomy/actions_guards/base_functors.h>
#include <aerial_autonomy/actions_guards/uav_status_functor.h>
#include <aerial_autonomy/common/math.h>
#include <aerial_autonomy/logic_states/base_state.h>
#include <aerial_autonomy/robot_systems/uav_vision_system.h>
#include <aerial_autonomy/types/completed_event.h>
#include <glog/logging.h>
#include <parsernode/common.h>

/**
* @brief Empty for now
*
* @tparam LogicStateMachineT Logic state machine used to process events
*/
template <class LogicStateMachineT>
struct VisualServoingTransitionActionFunctor_
    : EventAgnosticActionFunctor<UAVVisionSystem, LogicStateMachineT> {
  void run(UAVVisionSystem &robot_system,
           LogicStateMachineT &logic_state_machine) {}
};

/**
* @brief
*
* @tparam LogicStateMachineT Logic state machine used to process events
*/
template <class LogicStateMachineT>
struct VisualServoingAbortActionFunctor_
    : EventAgnosticActionFunctor<UAVVisionSystem, LogicStateMachineT> {
  void run(UAVVisionSystem &robot_system, LogicStateMachineT &) {
    LOG(WARNING) << "Aborting visual servoing controller";
    robot_system.abortController(HardwareType::UAV);
  }
};

/**
* @brief Action to reach a pre designated point
*
* @tparam LogicStateMachineT Logic state machine used to process events
*/
template <class LogicStateMachineT>
struct GoHomeTransitionActionFunctor_
    : EventAgnosticActionFunctor<UAVVisionSystem, LogicStateMachineT> {
  void run(UAVVisionSystem &robot_system, LogicStateMachineT &) {
    PositionYaw home_location = robot_system.getHomeLocation();
    VLOG(1) << "Going home";
    robot_system.setGoal<PositionControllerDroneConnector, PositionYaw>(
        home_location);
  }
};

/**
* @brief Guard for home transition
*
* @tparam LogicStateMachineT Logic state machine used to process events
*/
template <class LogicStateMachineT>
struct GoHomeTransitionGuardFunctor_
    : EventAgnosticGuardFunctor<UAVVisionSystem, LogicStateMachineT> {
  bool guard(UAVVisionSystem &robot_system, LogicStateMachineT &) {
    return robot_system.isHomeLocationSpecified();
  }
};

/**
* @brief Logic to check while reaching a visual servoing goal
*
* @tparam LogicStateMachineT Logic state machine used to process events
*/
template <class LogicStateMachineT>
struct VisualServoingInternalActionFunctor_
    : UAVStatusActionFunctor<UAVVisionSystem, LogicStateMachineT> {
  /**
  * @brief check if we reached VS goal and trigger completed event
  *
  * @param robot_system robot system to get sensor data
  * @param logic_state_machine logic state machine to trigger events
  */
  void statusIndependentRun(UAVVisionSystem &robot_system,
                            LogicStateMachineT &logic_state_machine) {
    ControllerStatus status =
        robot_system.getStatus<VisualServoingControllerDroneConnector>();
    // Define tolerance and check if reached goal
    if (status == ControllerStatus::Completed) {
      VLOG(1) << "Reached goal";
      logic_state_machine.process_event(Completed());
    } else if (status == ControllerStatus::Critical) {
      LOG(WARNING) << "Lost tracking while servoing.";
      logic_state_machine.process_event(be::Abort());
    }
  }
};

/**
* @brief Check tracking is valid before starting visual servoing
* @tparam LogicStateMachineT Logic state machine used to process events
*/
template <class LogicStateMachineT>
struct VisualServoingTransitionGuardFunctor_
    : EventAgnosticGuardFunctor<UAVVisionSystem, LogicStateMachineT> {
  bool guard(UAVVisionSystem &robot_system, LogicStateMachineT &) {
    if (!robot_system.initializeTracker()) {
      LOG(WARNING) << "Could not initialize tracking.";
      return false;
    }

    Position tracking_vector;
    if (!robot_system.getTrackingVector(tracking_vector)) {
      LOG(WARNING) << "Lost tracking while servoing.";
      return false;
    }
    VLOG(1) << "Setting tracking vector";
    double desired_distance = robot_system.getConfiguration()
                                  .uav_vision_system_config()
                                  .desired_visual_servoing_distance();
    double tracking_vector_norm = tracking_vector.norm();
    if (tracking_vector_norm < 1e-6) {
      LOG(WARNING) << "Tracking vector too small cannot initialize direction";
      return false;
    } else {
      // \todo Matt: could possibly move this block to the action functor
      VLOG(1) << "Selecting home location";
      robot_system.setHomeLocation();
      robot_system.setGoal<VisualServoingControllerDroneConnector, Position>(
          tracking_vector * desired_distance / tracking_vector_norm);
    }
    return true;
  }
};

/**
* @brief State that uses position control functor to reach a desired goal.
*
* @tparam LogicStateMachineT Logic state machine used to process events
*/
template <class LogicStateMachineT>
using VisualServoing_ =
    BaseState<UAVVisionSystem, LogicStateMachineT,
              VisualServoingInternalActionFunctor_<LogicStateMachineT>>;
