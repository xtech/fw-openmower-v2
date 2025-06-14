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

void InputDriver::AddInput(Input* input) {
  if (inputs_head_ == nullptr) {
    inputs_head_ = input;
  } else {
    Input* current = inputs_head_;
    while (current->next_for_driver_ != nullptr) {
      current = current->next_for_driver_;
    }
    current->next_for_driver_ = input;
  }
}

void InputDriver::ClearInputs() {
  inputs_head_ = nullptr;
}

}  // namespace xbot::driver::input
