//
// Created by clemens on 26.07.24.
//

#include "emergency_service.hpp"

#include "../../globals.hpp"

bool EmergencyService::OnStart() {
  emergency_reason = "Boot";
  // set the emergency and notify services
  UpdateMowerStatus([](MowerStatus& mower_status) { mower_status.emergency_latch = true; });
  chEvtBroadcastFlags(&mower_events, MOWER_EVT_EMERGENCY_CHANGED);
  return true;
}

void EmergencyService::OnStop() {
  emergency_reason = "Stopped";
  // set the emergency and notify services
  UpdateMowerStatus([](MowerStatus& mower_status) { mower_status.emergency_latch = true; });
  chEvtBroadcastFlags(&mower_events, MOWER_EVT_EMERGENCY_CHANGED);
}

void EmergencyService::tick() {
  // Get the current emergency state
  MowerStatus status_copy = GetMowerStatus();

  // Check timeout, but only overwrite if no emergency is currently active
  // reasoning is that we want to keep the original reason and not overwrite
  // with "timeout"
  if (!status_copy.emergency_latch && chVTTimeElapsedSinceX(last_clear_emergency_message_) > TIME_S2I(1)) {
    emergency_reason = "Timeout";
    // set the emergency and notify services
    status_copy = UpdateMowerStatus([](MowerStatus& mower_status) { mower_status.emergency_latch = true; });
    chEvtBroadcastFlags(&mower_events, MOWER_EVT_EMERGENCY_CHANGED);
  }

  StartTransaction();
  SendEmergencyActive(status_copy.emergency_active);
  SendEmergencyLatch(status_copy.emergency_latch);
  SendEmergencyReason(emergency_reason.c_str(), emergency_reason.length());
  CommitTransaction();
}

void EmergencyService::OnSetEmergencyChanged(const uint8_t& new_value) {
  if (new_value) {
    emergency_reason = "High Level Emergency";
    // set the emergency and notify services
    UpdateMowerStatus([](MowerStatus& mower_status) { mower_status.emergency_latch = true; });
    chEvtBroadcastFlags(&mower_events, MOWER_EVT_EMERGENCY_CHANGED);
  } else {
    // Get the current emergency state
    MowerStatus status_copy = GetMowerStatus();

    // want to reset emergency, but only do it, if no emergency exists right now
    if (!status_copy.emergency_active) {
      // clear the emergency and notify services
      UpdateMowerStatus([](MowerStatus& mower_status) { mower_status.emergency_latch = false; });
      chEvtBroadcastFlags(&mower_events, MOWER_EVT_EMERGENCY_CHANGED);
      emergency_reason = "None";
      last_clear_emergency_message_ = chVTGetSystemTime();
    }
  }
}
