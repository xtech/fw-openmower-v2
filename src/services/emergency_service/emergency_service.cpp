//
// Created by clemens on 26.07.24.
//

#include "emergency_service.hpp"

#include <ulog.h>

#include "../../globals.hpp"

#include <xbot-service/portable/system.hpp>

bool EmergencyService::OnStart() {
  emergency_reason = "Boot";
  // set the emergency and notify services
  const auto cb = [](MowerStatus& mower_status) { mower_status.emergency_latch = true; };
  UpdateMowerStatus(cb);
  chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
  return true;
}

void EmergencyService::OnStop() {
  emergency_reason = "Stopped";
  // set the emergency and notify services
  const auto cb = [](MowerStatus& mower_status) { mower_status.emergency_latch = true; };
  UpdateMowerStatus(cb);
  chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
}
void EmergencyService::OnCreate() {
  callback_delegate_ = etl::delegate<void()>::create([](){ULOG_INFO("timer test");});
  const auto timer_id = callback_timer_.register_timer(callback_delegate_, 1'000'000, true);
  callback_timer_.enable(true);
  callback_timer_.start(timer_id);
}

void EmergencyService::tick() {
  // Get the current emergency state
  MowerStatus mower_status = GetMowerStatus();

  // Check timeout, but only overwrite if no emergency is currently active
  // reasoning is that we want to keep the original reason and not overwrite
  // with "timeout"
  if (!mower_status.emergency_latch && chVTTimeElapsedSinceX(last_clear_emergency_message_) > TIME_S2I(1)) {
    emergency_reason = "Timeout";
    // set the emergency and notify services
    const auto cb = [](MowerStatus& mower_status) { mower_status.emergency_latch = true; };
    mower_status = UpdateMowerStatus(cb);
    chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
  }

  StartTransaction();
  SendEmergencyActive(mower_status.emergency_active);
  SendEmergencyLatch(mower_status.emergency_latch);
  SendEmergencyReason(emergency_reason.c_str(), emergency_reason.length());
  CommitTransaction();


  // Begin timer test
  static uint32_t last_time_micros = 0;
  uint32_t now_micros = xbot::service::system::getTimeMicros();
  callback_timer_.tick(now_micros - last_time_micros);
  last_time_micros = now_micros;
  // End timer test
}

void EmergencyService::OnSetEmergencyChanged(const uint8_t& new_value) {
  if (new_value) {
    emergency_reason = "High Level Emergency";
    // set the emergency and notify services
    const auto cb = [](MowerStatus& mower_status) { mower_status.emergency_latch = true; };
    UpdateMowerStatus(cb);
    chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
  } else {
    // Get the current emergency state
    MowerStatus mower_status = GetMowerStatus();

    // want to reset emergency, but only do it, if no emergency exists right now
    if (!mower_status.emergency_active) {
      // clear the emergency and notify services
      const auto cb =[](MowerStatus& mower_status) { mower_status.emergency_latch = false; };
      UpdateMowerStatus(cb);
      chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
      emergency_reason = "None";
      last_clear_emergency_message_ = chVTGetSystemTime();
    }
  }
}
