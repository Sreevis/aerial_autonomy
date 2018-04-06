#pragma once
/**
 * @brief Base state for all states in logic state machine
 *
 * This class provides a wrapper for any action function and robot
 * system
 * Entry and exit function can also be overwritten by subclassing this base
 * state.
 */

// Boost Includes
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/state_machine_def.hpp>

// Include Base state:
#include <aerial_autonomy/actions_guards/base_functors.h>

// Static asserts
#include <type_traits>

#include <glog/logging.h>

namespace msmf = boost::msm::front;

template <class RobotSystemT, class LogicStateMachineT, class ActionFctr>
class BaseState : public msmf::state<> {
public:
  /**
   * @brief The internal_transition_table to call run function in every state
   */
  struct internal_transition_table
      : boost::mpl::vector<
            msmf::Internal<InternalTransitionEvent, ActionFctr, msmf::none>> {};

  /**
   * @brief Log function to show that the state machine entered a specific state
   *
   * @tparam Event The event triggering the entry
   * @tparam FSM The statemachine associated with the state
   */
  template <class Event, class FSM> void on_entry(Event const &, FSM &) {
    VLOG(2) << "Entering: " << typeid(*this).name();
  }
  /**
   * @brief Log function to show that the state machine exited a specific state
   *
   * @tparam Event The event triggering the exit
   * @tparam FSM The statemachine associated with the state
   */
  template <class Event, class FSM>
  void on_exit(Event const &, FSM &logic_state_machine) {
    VLOG(2) << "Exiting: " << typeid(*this).name();
  }

  /**
   * @brief Destructor
   */
  virtual ~BaseState() {}
};
