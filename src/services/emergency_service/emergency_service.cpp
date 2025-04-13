//
// Created by clemens on 26.07.24.
//

#include "emergency_service.hpp"

MowerStatus EmergencyService::TriggerEmergency(const char* reason) {
  emergency_reason = reason;
  constexpr auto cb = [](MowerStatus& mower_status) { mower_status.emergency_latch = true; };
  MowerStatus new_status = UpdateMowerStatus(cb);
  chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
  return new_status;
}

MowerStatus EmergencyService::ClearEmergency() {
  emergency_reason = "None";
  constexpr auto cb = [](MowerStatus& mower_status) { mower_status.emergency_latch = false; };
  MowerStatus new_status = UpdateMowerStatus(cb);
  chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
  return new_status;
}

bool EmergencyService::OnStart() {
  TriggerEmergency("Boot");
  return true;
}

void EmergencyService::OnStop() {
  TriggerEmergency("Stopped");
}

void EmergencyService::tick() {
  // Check timeout, but only overwrite if no emergency is currently active
  // reasoning is that we want to keep the original reason and not overwrite
  // with "timeout"
  MowerStatus mower_status = GetMowerStatus();
  if (!mower_status.emergency_latch && chVTTimeElapsedSinceX(last_clear_emergency_message_) > TIME_S2I(1)) {
    mower_status = TriggerEmergency("Timeout");
  }

  StartTransaction();
  SendEmergencyActive(mower_status.emergency_active);
  SendEmergencyLatch(mower_status.emergency_latch);
  SendEmergencyReason(emergency_reason.c_str(), emergency_reason.length());
  CommitTransaction();
}

void EmergencyService::OnSetEmergencyChanged(const uint8_t& new_value) {
  if (new_value) {
    TriggerEmergency("High Level Emergency");
  } else {
    // Want to reset emergency, but only do it, if no emergency exists right now.
    MowerStatus mower_status = GetMowerStatus();
    if (!mower_status.emergency_active) {
      ClearEmergency();
      last_clear_emergency_message_ = chVTGetSystemTime();
    }
  }
}
