/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_state_machine.cpp
 * @brief The rules: which state change plays which sound.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-11-05
 *
 * @note  Everything that decides *what* is announced lives in this file; the plumbing
 *        (which service reports what, and how a sound is played) lives in SoundService.
 *        Read AlertSound()/ActivitySound() to change the behaviour.
 */

#include "sound_state_machine.hpp"

namespace xbot::driver::sound {

namespace {

/** @brief Signed difference of two ms stamps, so the 49-day wraparound compares right. */
inline bool time_reached(uint32_t now_ms, uint32_t due_ms) {
  return static_cast<int32_t>(now_ms - due_ms) >= 0;
}

}  // namespace

//=============================================================================
// Plumbing
//=============================================================================

StateMachine::StateMachine(Hooks hooks) : hooks_(hooks) {
  Reset();
}

void StateMachine::Reset() {
  started_ = false;
  activity_ = Activity::IDLE;
  // A booting robot has no high level yet; the first heartbeat is then a transition
  // into NONE, i.e. ROS_CONNECTED. See Update().
  alert_ = Alert::LINK_LOST;
  blade_ = Debounced{};
  charger_ = Debounced{};
  link_lost_timer_ = false;
  link_lost_since_ms_ = 0U;
  charge_done_seen_ = false;
  low_battery_announced_ = false;
  ambient_armed_ = false;
  ambient_due_ms_ = 0U;
}

bool StateMachine::Debounced::Update(bool raw, uint32_t now_ms, uint32_t stable_ms) {
  if (raw != raw_) {
    raw_ = raw;
    changed_ms_ = now_ms;
  }
  if (value_ != raw_ && (now_ms - changed_ms_) >= stable_ms) {
    value_ = raw_;
  }
  return value_;
}

StateAction StateMachine::Update(const StateInputs& inputs, uint32_t now_ms) {
  // --- The alert: emergency first, then a link loss that lasted long enough -------
  if (inputs.high_level_alive) {
    link_lost_timer_ = false;
  } else if (!link_lost_timer_) {
    link_lost_timer_ = true;
    link_lost_since_ms_ = now_ms;
  }

  Alert next_alert = Alert::NONE;
  if (inputs.emergency) {
    next_alert = Alert::EMERGENCY;
  } else if (!inputs.high_level_alive &&
             (alert_ == Alert::LINK_LOST || time_reached(now_ms, link_lost_since_ms_ + kLinkLostDelayMs))) {
    /* Only a loss that *persists* is a state change; while we are already in LINK_LOST
       it stays (the high level simply is not back yet). */
    next_alert = Alert::LINK_LOST;
  }

  // --- The activity: debounced, so a blade ramp-up or a charger handshake is no event
  const bool charging = charger_.Update(inputs.charging, now_ms, kInputDebounceMs);
  const bool blade = blade_.Update(inputs.blade_running, now_ms, kInputDebounceMs);
  const Activity next_activity = charging ? Activity::CHARGING : (blade ? Activity::MOWING : Activity::IDLE);

  if (inputs.charge_done) {
    /* DONE is not an active charge phase, so it can arrive in the same tick that ends
       CHARGING: remember it here, the activity transition below consumes it. */
    charge_done_seen_ = true;
  }

  // --- Boot: the power-up sweep, but an active emergency outranks it ---------------
  if (!started_) {
    started_ = true;
    activity_ = next_activity;
    alert_ = next_alert;
    AmbientArm(now_ms);
    return next_alert == Alert::EMERGENCY ? StateAction{SoundId::EMERGENCY} : StateAction{SoundId::BOOT_COMPLETE};
  }

  // --- An alert change is announced, and it swallows a pending activity change -----
  if (next_alert != alert_) {
    const Alert previous = alert_;
    alert_ = next_alert;
    activity_ = next_activity;
    AmbientArm(now_ms);
    return AlertAction(previous, next_alert);
  }

  // --- Otherwise the activity decides ---------------------------------------------
  if (next_activity != activity_) {
    const Activity previous = activity_;
    activity_ = next_activity;
    AmbientArm(now_ms);
    if (next_activity == Activity::CHARGING) {
      charge_done_seen_ = false;  // a fresh charge: DONE can only be reported from now on
    }
    const SoundId sound = ActivitySound(previous, next_activity, charge_done_seen_);
    if (previous == Activity::CHARGING) {
      charge_done_seen_ = false;
    }
    if (sound != kNoSound) {
      return StateAction{sound};
    }
  }

  // --- LOW_BATTERY: one announcement per dip, only while the blade runs ------------
  if (inputs.battery_percent != 255U) {
    if (inputs.battery_percent >= static_cast<uint8_t>(kLowBatteryPercent + kLowBatteryHysteresis)) {
      low_battery_announced_ = false;
    } else if (alert_ == Alert::NONE && activity_ == Activity::MOWING && !low_battery_announced_ &&
               inputs.battery_percent <= kLowBatteryPercent) {
      low_battery_announced_ = true;
      return StateAction{SoundId::LOW_BATTERY};
    }
  }

  // --- Ambience: the background sound of the current activity ----------------------
  return AmbientStep(now_ms);
}

//=============================================================================
// The rules
//=============================================================================

/**
 * @brief Which sound an alert transition plays.
 *
 * | from        | to          | sound                                    |
 * |-------------|-------------|------------------------------------------|
 * | anything    | EMERGENCY   | EMERGENCY (preempts, see its definition) |
 * | EMERGENCY   | LINK_LOST   | ROS_DISCONNECTED, after stopping the tone |
 * | EMERGENCY   | NONE        | EMERGENCY_CLEARED                         |
 * | LINK_LOST   | NONE        | ROS_CONNECTED                             |
 * | NONE        | LINK_LOST   | ROS_DISCONNECTED                          |
 *
 * @note  A repeating alert (the emergency tone holds ~8 s) has to be stopped before the
 *        "all clear" plays, hence stop_first.
 */
StateAction StateMachine::AlertAction(Alert from, Alert to) {
  if (to == Alert::EMERGENCY) {
    return StateAction{SoundId::EMERGENCY};
  }
  const bool was_emergency = (from == Alert::EMERGENCY);
  if (to == Alert::LINK_LOST) {
    return StateAction{SoundId::ROS_DISCONNECTED, was_emergency};
  }
  return StateAction{was_emergency ? SoundId::EMERGENCY_CLEARED : SoundId::ROS_CONNECTED, was_emergency};
}

/**
 * @brief Which sound an activity transition plays.
 *
 * | from              | to                | sound                        |
 * |-------------------|-------------------|------------------------------|
 * | IDLE              | CHARGING          | CHARGING_START               |
 * | MOWING            | CHARGING          | CHARGING_START               |
 * | CHARGING          | IDLE / MOWING     | CHARGING_DONE (only if DONE) |
 * | IDLE              | MOWING            | — (the high level owns it)   |
 * | MOWING            | IDLE              | — (pause and "done" differ, only the HL knows which) |
 *
 * @note  MOWING_START/PAUSED/RESUMED/COMPLETE are *semantic* events: the low level only
 *        sees the blade turn, which is neither a start nor a completion.  The high level
 *        plays those itself (SoundService RPC), so a state change here stays silent.
 */
SoundId StateMachine::ActivitySound(Activity from, Activity to, bool charge_done) {
  if (to == Activity::CHARGING) {
    return SoundId::CHARGING_START;
  }
  if (from == Activity::CHARGING && charge_done) {
    return SoundId::CHARGING_DONE;
  }
  return kNoSound;
}

/** @brief Which sound may play by itself while the activity lasts. */
SoundId StateMachine::AmbientSound(Activity activity) {
  switch (activity) {
    case Activity::MOWING: return SoundId::MOWING_AMBIENCE;  // every/chance come from the definition
    case Activity::IDLE:
    case Activity::CHARGING: break;
  }
  return kNoSound;
}

void StateMachine::AmbientArm(uint32_t now_ms) {
  ambient_armed_ = false;
  ambient_due_ms_ = 0U;

  const SoundId id = AmbientSound(activity_);
  if (id == kNoSound || hooks_.policy == nullptr) {
    return;
  }
  const AmbientPolicy policy = hooks_.policy(id);
  if (policy.every_s == 0U || policy.chance_pct == 0U) {
    return; /* the definition does not want an ambience */
  }
  ambient_armed_ = true;
  ambient_due_ms_ = now_ms + static_cast<uint32_t>(policy.every_s) * 1'000U;
}

StateAction StateMachine::AmbientStep(uint32_t now_ms) {
  /* No ambience under an alert: an emergency or a lost high level is not the moment for
     a cow. Also nothing while the player is busy with an alert - the alert change above
     has just re-armed the interval. */
  if (alert_ != Alert::NONE) {
    return StateAction{};
  }
  if (!ambient_armed_ || !time_reached(now_ms, ambient_due_ms_)) {
    return StateAction{};
  }

  const SoundId id = AmbientSound(activity_);
  if (id == kNoSound || hooks_.policy == nullptr) {
    ambient_armed_ = false;
    return StateAction{};
  }
  const AmbientPolicy policy = hooks_.policy(id);
  if (policy.every_s == 0U || policy.chance_pct == 0U) {
    ambient_armed_ = false;
    return StateAction{};
  }

  /* The interval restarts whether or not the dice hit: `every` is the evaluation rate,
     so a miss must not shorten the next wait. */
  ambient_due_ms_ = now_ms + static_cast<uint32_t>(policy.every_s) * 1'000U;

  if (hooks_.dice == nullptr || !hooks_.dice(policy.chance_pct)) {
    return StateAction{};
  }
  return StateAction{id};
}

}  // namespace xbot::driver::sound
