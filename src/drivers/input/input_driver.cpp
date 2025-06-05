#include "input_driver.hpp"

#include "globals.hpp"
#include "services.hpp"

namespace xbot::driver::input {

bool Input::Update(bool new_active) {
  if (invert) {
    new_active = !new_active;
  }
  bool expected = !new_active;
  if (active.compare_exchange_strong(expected, new_active)) {
    const uint32_t now = xbot::service::system::getTimeMicros();
    if (new_active) {
      active_since = now;
    }
    input_service.OnInputChanged(*this, new_active, ActiveDuration(now));
    chEvtBroadcastFlags(&mower_events, MowerEvents::INPUTS_CHANGED);
    return true;
  }
  return false;
}

}  // namespace xbot::driver::input
