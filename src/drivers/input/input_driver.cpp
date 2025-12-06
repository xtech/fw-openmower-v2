#include "input_driver.hpp"

#include "globals.hpp"
#include "services.hpp"

namespace xbot::driver::input {

bool Input::Update(bool new_active, uint32_t predate) {
  if (invert) {
    new_active = !new_active;
  }
  bool expected = !new_active;
  if (active.compare_exchange_strong(expected, new_active)) {
    const uint32_t now = xbot::service::system::getTimeMicros() - predate;
    if (new_active) {
      active_since = now;
      input_service.OnInputChanged(*this, true, 0);
    } else {
      const uint32_t duration = ActiveDuration(now);
      if (emergency_reason != 0 && duration >= emergency_delay_ms * 1'000) {
        emergency_pending = true;
      }
      input_service.OnInputChanged(*this, false, duration);
    }
    chEvtBroadcastFlags(&mower_events, MowerEvents::INPUTS_CHANGED);
    return true;
  }
  return false;
}

bool Input::GetAndClearPendingEmergency() {
  bool only_if_pending = true;
  return emergency_pending.compare_exchange_strong(only_if_pending, false);
}

void Input::InjectPress(bool long_press) {
  InjectPress(input_service.GetPressDelay(long_press));
}

void Input::InjectPress(uint32_t duration) {
  Update(true, duration);
  Update(false);
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
