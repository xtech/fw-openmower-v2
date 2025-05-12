//
// Created by clemens on 26.07.24.
//

#include "emergency_service.hpp"

#include <xbot-service/Lock.hpp>

#include "services.hpp"

using xbot::driver::input::Input;
using xbot::service::Lock;

void EmergencyService::TriggerEmergency(const char* reason) {
  {
    xbot::service::Lock lk{&mtx_};
    // keep the reason for the initial emergency
    if (emergency_latch) {
      return;
    }
    emergency_reason = reason;
    emergency_latch = true;
  }
  chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
}

void EmergencyService::ClearEmergency() {
  {
    xbot::service::Lock lk{&mtx_};
    emergency_reason = "None";
    emergency_latch = false;
  }
  chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
}

bool EmergencyService::OnStart() {
  TriggerEmergency("Boot");
  return true;
}

void EmergencyService::OnStop() {
  TriggerEmergency("Stopped");
}

void EmergencyService::tick() {
  xbot::service::Lock lk{&mtx_};
  // Check timeout, but only overwrite if no emergency is currently active
  // reasoning is that we want to keep the original reason and not overwrite
  // with "timeout"
  if (!emergency_latch && chVTTimeElapsedSinceX(last_clear_emergency_message_) > TIME_S2I(1)) {
    TriggerEmergency("Timeout");
  }

  StartTransaction();
  SendEmergencyActive(emergency_latch);
  SendEmergencyLatch(emergency_latch);
  SendEmergencyReason(emergency_reason.c_str(), emergency_reason.length());
  CommitTransaction();
}

static bool CheckDelayed(bool condition, systimestamp_t& triggered_since, systimestamp_t delay, sysinterval_t now) {
  if (condition) {
    if (triggered_since == 0) {
      triggered_since = now;
      return false;
    } else {
      return chTimeStampDiffX(triggered_since, now) > delay;
    }
  } else {
    triggered_since = 0;
    return false;
  }
}

void EmergencyService::OnInputsChangedEvent() {
  static const sysinterval_t STOP_DELAY = TIME_MS2I(20);
  static systimestamp_t stop_started = 0;

  size_t num_stop = 0, num_lift_tilt = 0;
  {
    // Although the input states are atomic, the vector of inputs is not.
    Lock lk(&input_service.mutex_);
    for (auto& input : input_service.GetAllInputs()) {
      if (input->IsActive()) {
        switch (input->emergency_mode) {
          case Input::EmergencyMode::EMERGENCY: num_stop++; break;
          case Input::EmergencyMode::TRAPPED: num_lift_tilt++; break;
          default: break;
        }
      }
    }
  }

  const systimestamp_t now = chVTGetTimeStamp();

  if (CheckDelayed(num_stop > 0, stop_started, STOP_DELAY, now)) {
    TriggerEmergency("Stop");
  }
}

void EmergencyService::OnSetEmergencyChanged(const uint8_t& new_value) {
  if (new_value) {
    TriggerEmergency("High Level Emergency");
  } else {
    ClearEmergency();
    last_clear_emergency_message_ = chVTGetSystemTime();
  }
}
bool EmergencyService::GetEmergency() {
  xbot::service::Lock lk{&mtx_};
  return emergency_latch;
}
