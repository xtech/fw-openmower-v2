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

void EmergencyService::OnInputsChangedEvent() {
  // Although the input states are atomic, the vector of inputs is not.
  Lock lk(&input_service.mutex_);

  Input* longest_triggered = nullptr;
  uint32_t longest_duration = 0;
  bool latch = false;
  for (auto& input : input_service.GetAllInputs()) {
    // TODO: What if the input was triggered so briefly that we couldn't observe it?
    if (!input->emergency_trigger || !input->IsActive()) continue;
    const uint32_t duration = input->ActiveDuration();
    if (duration < input->emergency_delay * 1000) continue;

    if (longest_triggered == nullptr || duration > longest_duration) {
      longest_triggered = input;
      longest_duration = duration;
    }

    latch |= input->emergency_latch;
  }

  if (longest_triggered != nullptr) {
    // TODO: Conditionally set latch.
    TriggerEmergency(longest_triggered->name.c_str());
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
