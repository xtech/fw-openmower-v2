//
// Created by clemens on 26.07.24.
//

#include "emergency_service.hpp"

#include "../../globals.hpp"

bool EmergencyService::OnStart() {
  emergency_reason = "Boot";
  // set the emergency and notify services
  chMtxLock(&mower_status_mutex);
  mower_status.emergency_latch = true;
  chMtxUnlock(&mower_status_mutex);
  chEvtBroadcastFlags(&mower_events, MOWER_EVT_EMERGENCY_CHANGED);
  return true;
}

void EmergencyService::OnStop() {
  emergency_reason = "Stopped";
  // set the emergency and notify services
  chMtxLock(&mower_status_mutex);
  mower_status.emergency_latch = true;
  chMtxUnlock(&mower_status_mutex);
  chEvtBroadcastFlags(&mower_events, MOWER_EVT_EMERGENCY_CHANGED);
}

void EmergencyService::tick() {
  // Get the current emergency state
  chMtxLock(&mower_status_mutex);
  MowerStatus status_copy = mower_status;
  chMtxUnlock(&mower_status_mutex);

  // Check timeout, but only overwrite if no emergency is currently active
  // reasoning is that we want to keep the original reason and not overwrite
  // with "timeout"
  if (!status_copy.emergency_latch && chVTTimeElapsedSinceX(last_clear_emergency_message_) > TIME_S2I(1)) {
    emergency_reason = "Timeout";
    // set the emergency and notify services
    chMtxLock(&mower_status_mutex);
    mower_status.emergency_latch = true;
    chMtxUnlock(&mower_status_mutex);
    chEvtBroadcastFlags(&mower_events, MOWER_EVT_EMERGENCY_CHANGED);
    // The last flags did not have emergency yet, so need to set it here as well
    status_copy.emergency_latch = true;
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
    chMtxLock(&mower_status_mutex);
    mower_status.emergency_latch = true;
    chMtxUnlock(&mower_status_mutex);
    chEvtBroadcastFlags(&mower_events, MOWER_EVT_EMERGENCY_CHANGED);
  } else {
    // Get the current emergency state
    chMtxLock(&mower_status_mutex);
    MowerStatus status_copy = mower_status;
    chMtxUnlock(&mower_status_mutex);

    // want to reset emergency, but only do it, if no emergency exists right now
    if (!status_copy.emergency_active) {
      // clear the emergency and notify services
      chMtxLock(&mower_status_mutex);
      mower_status.emergency_latch = false;
      chMtxUnlock(&mower_status_mutex);
      chEvtBroadcastFlags(&mower_events, MOWER_EVT_EMERGENCY_CHANGED);
      emergency_reason = "None";
      last_clear_emergency_message_ = chVTGetSystemTime();
    }
  }
}
