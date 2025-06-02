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
  CheckInputs();
  CheckTimeouts();
}

void EmergencyService::CheckInputs() {
  uint16_t reasons = 0;
  {
    // Although the input states are atomic, the vector of inputs is not.
    Lock lk(&input_service.mutex_);
    for (auto& input : input_service.GetAllInputs()) {
      // TODO: What if the input was triggered so briefly that we couldn't observe it?
      if (input->emergency_reason == 0 || !input->IsActive()) continue;
      const uint32_t duration = input->ActiveDuration();
      if (duration < input->emergency_delay * 1000) continue;
      reasons |= input->emergency_reason;
    }
  }

  constexpr uint16_t potential_reasons =
      EmergencyReason::STOP | EmergencyReason::LIFT | EmergencyReason::TILT | EmergencyReason::COLLISION;
  UpdateEmergency(reasons, potential_reasons);
}

void EmergencyService::OnHighLevelEmergencyChanged(const uint16_t* new_value, uint32_t length) {
  (void)length;
  {
    Lock lk(&mtx_);
    last_high_level_emergency_message_ = chVTGetSystemTime();
  }
  UpdateEmergency(new_value[0], new_value[1]);
}

void EmergencyService::CheckTimeouts() {
  uint16_t reasons = 0;
  {
    Lock lk{&mtx_};
    if (chVTTimeElapsedSinceX(last_high_level_emergency_message_) > TIME_S2I(1)) {
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
