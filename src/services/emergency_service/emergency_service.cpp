//
// Created by clemens on 26.07.24.
//

#include "emergency_service.hpp"

#include <drivers/sound/sound_player.hpp>
#include <xbot-service/Lock.hpp>

#include "services.hpp"

using xbot::service::Lock;

void EmergencyService::OnStop() {
  // We won't be getting further updates from high level, so set that flag immediately.
  UpdateEmergency(EmergencyReason::TIMEOUT_HIGH_LEVEL);
}

uint32_t EmergencyService::OnLoop(uint32_t now_micros, uint32_t) {
  return etl::min(CheckInputs(now_micros), etl::min(CheckTimeouts(now_micros), CheckRequiredServices()));
}

uint32_t EmergencyService::CheckInputs(uint32_t now) {
  constexpr uint16_t potential_reasons = EmergencyReason::STOP | EmergencyReason::LIFT |
                                         EmergencyReason::LIFT_MULTIPLE | EmergencyReason::COLLISION |
                                         EmergencyReason::COLLISION_MULTIPLE;
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
  UpdateEmergency(new_value[0], new_value[1] & ~EmergencyReason::SERVICE_NOT_READY);
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
  bool was_latched;
  bool now_latched;
  {
    Lock lk{&mtx_};
    const uint16_t old_reason = reasons_;
    reasons_ &= ~clear;
    reasons_ |= add;
    if (reasons_ == old_reason) {
      return;
    }
    was_latched = (old_reason & EmergencyReason::LATCH) != 0;
    now_latched = (reasons_ & EmergencyReason::LATCH) != 0;
  }
  chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
  SendStatus();

  // Only latched emergencies matter for the audio feedback
  if (now_latched && !was_latched) {
    xbot::driver::sound::play_sound_id(SoundId::EMERGENCY, /*high_priority=*/true);
  } else if (!now_latched && was_latched) {
    xbot::driver::sound::stop();
    xbot::driver::sound::play_sound_id(SoundId::SUCCESS);
  }
}

uint16_t EmergencyService::GetEmergencyReasons() {
  Lock lk{&mtx_};
  return reasons_;
}

void EmergencyService::RequireService(ServiceExt* svc) {
  Lock lk{&mtx_};
  required_services_.push_back(svc);
  reasons_ |= EmergencyReason::SERVICE_NOT_READY;
}

uint32_t EmergencyService::CheckRequiredServices() {
  if (required_services_.empty()) {
    // Nothing to do, no re-query
    return UINT32_MAX;
  }
  bool all_ready = true;
  for (auto* svc : required_services_) {
    if (!svc->IsHealthy()) {
      all_ready = false;
      break;
    }
  }
  // Retry in 100ms
  constexpr uint32_t retry_interval = 100'000;
  UpdateEmergency(all_ready ? 0 : EmergencyReason::SERVICE_NOT_READY, EmergencyReason::SERVICE_NOT_READY);
  return all_ready ? UINT32_MAX : retry_interval;
}

void EmergencyService::SendStatus() {
  xbot::service::Lock lk{&mtx_};
  StartTransaction();
  SendEmergencyReason(reasons_);
  CommitTransaction();
}
