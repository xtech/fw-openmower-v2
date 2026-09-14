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
  uint32_t heartbeat_age = 0;
  bool high_level_seen = false;
  {
    Lock lk{&mtx_};
    high_level_seen = (last_high_level_emergency_message_ != 0U);
    heartbeat_age = now - last_high_level_emergency_message_;
    if (TimeoutReached(heartbeat_age, kHighLevelTimeoutUs, block_time)) {
      reasons |= EmergencyReason::TIMEOUT_HIGH_LEVEL;
    }
  }
  constexpr uint16_t potential_reasons = EmergencyReason::TIMEOUT_HIGH_LEVEL | EmergencyReason::TIMEOUT_INPUTS;
  const uint16_t changed = UpdateEmergency(reasons, potential_reasons);
  if (high_level_seen) {
    AnnounceHighLevelLink(changed, heartbeat_age, block_time);
  }
  return block_time;
}

/**
 * @note  Only a timeout that follows a working link is announced: at boot the high level
 *        simply is not there yet (reasons_ starts with the timeout bit set), and the very
 *        first heartbeat turns that into a one time "connected".
 */
void EmergencyService::AnnounceHighLevelLink(uint16_t changed_reasons, uint32_t heartbeat_age_us,
                                             uint32_t& block_time) {
  const bool alive = (heartbeat_age_us < kHighLevelTimeoutUs);
  const bool link_changed = (changed_reasons & EmergencyReason::TIMEOUT_HIGH_LEVEL) != 0U;

  if (alive) {
    /* Heartbeat is here: say hello on the first connect, or when we said goodbye before.
       A hiccup that never got announced stays silent. */
    if (link_changed && (link_announced_ != LinkAnnounce::CONNECTED)) {
      link_announced_ = LinkAnnounce::CONNECTED;
      xbot::driver::sound::play_sound_id(SoundId::ROS_CONNECTED);
    }
    return;
  }

  if (link_announced_ == LinkAnnounce::LOST) return; /* already said it */

  const uint32_t announce_at = kHighLevelTimeoutUs + kLinkLostDelayUs;
  if (heartbeat_age_us >= announce_at) {
    link_announced_ = LinkAnnounce::LOST;
    /* A queued sound is enough — the alert EMERGENCY keeps its preempt privilege. */
    xbot::driver::sound::play_sound_id(SoundId::ROS_DISCONNECTED);
    return;
  }
  /* Not due yet: keep the service loop awake until it is (see TimeoutReached()). */
  block_time = etl::min(block_time, announce_at - heartbeat_age_us);
}

uint16_t EmergencyService::UpdateEmergency(uint16_t add, uint16_t clear) {
  bool was_latched;
  bool now_latched;
  uint16_t changed;
  {
    Lock lk{&mtx_};
    const uint16_t old_reason = reasons_;
    reasons_ &= ~clear;
    reasons_ |= add;
    if (reasons_ == old_reason) {
      return 0U;
    }
    changed = static_cast<uint16_t>(old_reason ^ reasons_);
    was_latched = (old_reason & EmergencyReason::LATCH) != 0;
    now_latched = (reasons_ & EmergencyReason::LATCH) != 0;
  }
  chEvtBroadcastFlags(&mower_events, MowerEvents::EMERGENCY_CHANGED);
  SendStatus();

  // Only latched emergencies matter for the audio feedback
  if (now_latched && !was_latched) {
    // The EMERGENCY definition carries preempt=true, so whatever is playing and drops the rest of the queue.
    xbot::driver::sound::play_sound_id(SoundId::EMERGENCY);
  } else if (!now_latched && was_latched) {
    xbot::driver::sound::stop();
    xbot::driver::sound::play_sound_id(SoundId::SUCCESS);
  }
  return changed;
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
