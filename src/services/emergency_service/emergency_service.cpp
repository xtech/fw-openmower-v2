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

uint32_t EmergencyService::OnLoop(uint32_t now_micros, uint32_t) {
  return etl::min(CheckInputs(now_micros), CheckTimeouts(now_micros));
}

uint32_t EmergencyService::CheckInputs(uint32_t now) {
  constexpr uint16_t potential_reasons =
      EmergencyReason::STOP | EmergencyReason::LIFT | EmergencyReason::LIFT_MULTIPLE | EmergencyReason::COLLISION;
  auto [reasons, block_time] = input_service.GetEmergencyReasons(now);
  UpdateEmergency(reasons, potential_reasons);
  return block_time;
}

void EmergencyService::OnHighLevelEmergencyChanged(const uint16_t* new_value, uint32_t length) {
  (void)length;
  {
    Lock lk(&mtx_);
    last_high_level_emergency_message_ = xbot::service::system::getTimeMicros();
  }
  UpdateEmergency(new_value[0], new_value[1]);
}

uint32_t EmergencyService::CheckTimeouts(uint32_t now) {
  uint16_t reasons = 0;
  uint32_t block_time = UINT32_MAX;
  {
    Lock lk{&mtx_};
    if (TimeoutReached(now - last_high_level_emergency_message_, 1'000'000, block_time)) {
      reasons |= EmergencyReason::TIMEOUT_HIGH_LEVEL;
    }
  }
  constexpr uint16_t potential_reasons = EmergencyReason::TIMEOUT_HIGH_LEVEL | EmergencyReason::TIMEOUT_INPUTS;
  UpdateEmergency(reasons, potential_reasons);
  return block_time;
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
