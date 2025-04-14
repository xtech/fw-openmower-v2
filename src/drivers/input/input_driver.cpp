#include "input_driver.hpp"

#include "globals.hpp"

namespace xbot::driver::input {

bool Input::Update(bool new_active) {
  bool expected = !new_active;
  if (active.compare_exchange_strong(expected, new_active)) {
    chEvtBroadcastFlags(&mower_events, MowerEvents::INPUTS_CHANGED);
    return true;
  } else {
    return false;
  }
}

}  // namespace xbot::driver::input
