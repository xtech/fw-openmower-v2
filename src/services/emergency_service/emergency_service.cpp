//
// Created by clemens on 26.07.24.
//

#include "emergency_service.hpp"

#include <xbot-service/Lock.hpp>

#include "services.hpp"

using xbot::service::Lock;

void EmergencyService::OnStop() {
  // We won't be getting further updates from high level, so set that flag immediately.
  UpdateEmergency(EmergencyReason::TIMEOUT_HIGH_LEVEL);
}

void EmergencyService::Check() {
  // TODO: This shouldn't need to run with a fixed schedule, we can calculate the next due time.
  const uint32_t now = xbot::service::system::getTimeMicros();
  CheckInputs(now);
  CheckTimeouts(now);
}

void EmergencyService::CheckInputs(uint32_t now) {
  constexpr uint16_t potential_reasons =
      EmergencyReason::STOP | EmergencyReason::LIFT | EmergencyReason::TILT | EmergencyReason::COLLISION;
  UpdateEmergency(input_service.GetEmergencyReasons(now), potential_reasons);
}

void EmergencyService::OnHighLevelEmergencyChanged(const uint16_t* new_value, uint32_t length) {
  (void)length;
  {
    Lock lk(&mtx_);
    last_high_level_emergency_message_ = xbot::service::system::getTimeMicros();
  }
  UpdateEmergency(new_value[0], new_value[1]);
}

void EmergencyService::CheckTimeouts(uint32_t now) {
  uint16_t reasons = 0;
  {
    Lock lk{&mtx_};
    if (now - last_high_level_emergency_message_ > 1'000'000) {
      reasons |= EmergencyReason::TIMEOUT_HIGH_LEVEL;
    }
  }

  constexpr uint16_t potential_reasons = EmergencyReason::TIMEOUT_HIGH_LEVEL | EmergencyReason::TIMEOUT_INPUTS;
  UpdateEmergency(reasons, potential_reasons);
}

void EmergencyService::UpdateEmergency(uint16_t add, uint16_t clear) {
  {
    Lock lk{&mtx_};
    uint16_t old_reason = reasons_;
    reasons_ &= ~clear;
    reasons_ |= add;
    if (reasons_ == old_reason) {
      return;
    }
  }
  chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
  SendStatus();
}

bool EmergencyService::GetEmergency() {
  Lock lk{&mtx_};
  return reasons_ != 0;
}

void EmergencyService::SendStatus() {
  xbot::service::Lock lk{&mtx_};
  StartTransaction();
  SendEmergencyReason(reasons_);
  CommitTransaction();
}
