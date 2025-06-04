#include "input_driver.hpp"

#include "globals.hpp"
#include "services.hpp"

namespace xbot::driver::input {

bool Input::Update(bool new_active) {
  bool expected = !new_active;
  if (active.compare_exchange_strong(expected, new_active)) {
    if (new_active) {
      active_since = system::getTimeMicros();
    }
    input_service.OnInputChanged(*this);
    chEvtBroadcastFlags(&mower_events, MowerEvents::INPUTS_CHANGED);
    return true;
  }
  return false;
}

}  // namespace xbot::driver::input
