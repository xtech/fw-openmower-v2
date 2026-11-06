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
 *        Read AlertAction()/HlSound() to change the behaviour.
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
  charging_ = false;
  // A booting robot has no high level yet; the first heartbeat is then a transition
  // into NONE, i.e. ROS_CONNECTED. See Update().
  alert_ = Alert::LINK_LOST;
  blade_ = Debounced{};
  charger_ = Debounced{};
  link_lost_timer_ = false;
  link_lost_since_ms_ = 0U;
  hl_state_ = HlState::UNKNOWN;
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
  /* An audible condition *starts* the alert; leaving it needs the latch to be released as
     well, so a sensor that lets go while the robot is still latched keeps the alert (and
     the "all clear" waits for the reset). Reasons whose definition disables latching
     (`latch: false`) clear as soon as the condition is gone. */
  const bool dangerous = (inputs.emergency != EmergencyKind::NONE);
  if (dangerous || (alert_ == Alert::EMERGENCY && inputs.emergency_latched)) {
    next_alert = Alert::EMERGENCY;
  } else if (!inputs.high_level_alive &&
             (alert_ == Alert::LINK_LOST || time_reached(now_ms, link_lost_since_ms_ + kLinkLostDelayMs))) {
    /* Only a loss that *persists* is a state change; while we are already in LINK_LOST
       it stays (the high level simply is not back yet). */
    next_alert = Alert::LINK_LOST;
  }

  // --- The sensors: debounced, so a blade ramp-up or a charger handshake is no event
  const bool charging = charger_.Update(inputs.charging, now_ms, kInputDebounceMs);
  const bool blade_before = blade_.value();
  blade_.Update(inputs.blade_running, now_ms, kInputDebounceMs);
  if (blade_.value() != blade_before) {
    /* The ambience belongs to the blade (see AmbientSound()): its interval restarts when
       the motor starts or stops. */
    AmbientArm(now_ms);
  }

  if (inputs.charge_done) {
    /* DONE is not an active charge phase, so it can arrive in the same tick that ends the
       charge: remember it here, the edge below consumes it. */
    charge_done_seen_ = true;
  }

  // --- Boot: the power-up sweep, but an active emergency outranks it ---------------
  if (!started_) {
    started_ = true;
    charging_ = charging;
    alert_ = next_alert;
    AmbientArm(now_ms);
    if (next_alert == Alert::EMERGENCY) {
      return AlertAction(Alert::NONE, Alert::EMERGENCY, inputs.emergency);
    }
    return StateAction{SoundId::BOOT_COMPLETE};
  }

  // --- An alert change is announced, and it swallows a pending state change --------
  if (next_alert != alert_) {
    const Alert previous = alert_;
    alert_ = next_alert;
    charging_ = charging;
    AmbientArm(now_ms);
    StateAction action = AlertAction(previous, next_alert, inputs.emergency);
    if (next_alert == Alert::NONE && hl_manoeuvres(hl_state_)) {
      /* The alert (and its stop_first) ended the warning pattern: start it again. */
      action.follow_up = SoundId::AUTONOMOUS_NOTICE;
    }
    return action;
  }

  // --- Charging: the charger is the only source (the high level reports no charging)
  if (charging != charging_) {
    charging_ = charging;
    AmbientArm(now_ms);
    if (charging) {
      charge_done_seen_ = false;  // a fresh charge: DONE can only be reported from now on
      return StateAction{SoundId::CHARGING_START};
    }
    const bool done = charge_done_seen_;
    charge_done_seen_ = false;
    if (done) {
      return StateAction{SoundId::CHARGING_DONE};
    }
  }

  // --- The high level's own state: its announcements ------------------------------
  if (inputs.hl_state != hl_state_) {
    const HlState previous = hl_state_;
    hl_state_ = inputs.hl_state;

    if (hl_manoeuvres(hl_state_) && !hl_manoeuvres(previous)) {
      /* A manoeuvre starts the warning device. Its cadence is the definition's
         repeat_ms, so this only has to start it - and as a follow-up when the same
         transition also has an announcement (the queue plays that one first). */
      const SoundId announce = HlSound(previous, hl_state_);
      if (announce != kNoSound) {
        return StateAction{announce, SoundId::AUTONOMOUS_NOTICE, true};
      }
      return StateAction{SoundId::AUTONOMOUS_NOTICE, kNoSound, true};
    }
    if (!hl_manoeuvres(hl_state_) && hl_manoeuvres(previous)) {
      /* The manoeuvre is over: stop the pattern (repeat_ms would run on by itself). */
      return StateAction{kNoSound, kNoSound, true};
    }

    const SoundId sound = HlSound(previous, hl_state_);
    if (sound != kNoSound) {
      return StateAction{sound};
    }
  }

  // --- LOW_BATTERY: one announcement per dip, only while the blade runs ------------
  if (inputs.battery_percent != 255U) {
    if (inputs.battery_percent >= static_cast<uint8_t>(kLowBatteryPercent + kLowBatteryHysteresis)) {
      low_battery_announced_ = false;
    } else if (alert_ == Alert::NONE && blade_.value() && !low_battery_announced_ &&
               inputs.battery_percent <= kLowBatteryPercent) {
      low_battery_announced_ = true;
      return StateAction{SoundId::LOW_BATTERY};
    }
  }

  // --- Ambience: the background sound while the blade turns ------------------------
  const StateAction ambient = AmbientStep(now_ms);
  if (!ambient.silent()) {
    return ambient;
  }

  return StateAction{};
}

//=============================================================================
// The rules
//=============================================================================

/**
 * @brief Which sound an alert transition plays.
 *
 * | from      | to          | sound                                            |
 * |-----------|-------------|--------------------------------------------------|
 * | anything  | EMERGENCY   | by kind, see below                                |
 * | EMERGENCY | LINK_LOST   | ROS_DISCONNECTED, after stopping the tone         |
 * | EMERGENCY | NONE        | EMERGENCY_CLEARED                                 |
 * | LINK_LOST | NONE        | ROS_CONNECTED                                     |
 * | NONE      | LINK_LOST   | ROS_DISCONNECTED                                  |
 *
 * Entering an emergency depends on *why* it happened — a pressed stop button is no
 * surprise, the user just did it, so it is confirmed in words instead of being alarmed:
 *
 * | kind        | sound                                  |
 * |-------------|----------------------------------------|
 * | STOP_BUTTON | STOP_BUTTON only (never the alarm)     |
 * | LIFT        | LIFT_TRIGGERED, then the alarm         |
 * | COLLISION   | COLLISION_TRIGGERED, then the alarm    |
 * | MOWER_RPM_LIMIT | MOWER_RPM_LIMIT, then the alarm  |
 * | MOWER_RPM_TIMEOUT    | MOWER_RPM_TIMEOUT, then the alarm     |
 * | OTHER       | the alarm only (e.g. a firmware fault) |
 *
 * The "what happened" announcement plays first and the alarm follows it (follow_up): the
 * alarm preempts by its definition, so it cannot be queued *before* the voice without
 * cutting it off. While an emergency is latched no further announcement is made - the
 * alert simply stays.
 *
 * @note  A repeating alert (the emergency tone holds several seconds) has to be stopped
 *        before the "all clear" plays, hence stop_first.
 */
StateAction StateMachine::AlertAction(Alert from, Alert to, EmergencyKind kind) {
  if (to == Alert::EMERGENCY) {
    switch (kind) {
      case EmergencyKind::STOP: return StateAction{SoundId::STOP_BUTTON, kNoSound, true};
      case EmergencyKind::LIFT: return StateAction{SoundId::LIFT_TRIGGERED, SoundId::EMERGENCY, true};
      case EmergencyKind::COLLISION: return StateAction{SoundId::COLLISION_TRIGGERED, SoundId::EMERGENCY, true};
      case EmergencyKind::RPM_LIMIT: return StateAction{SoundId::MOWER_RPM_LIMIT, SoundId::EMERGENCY, true};
      case EmergencyKind::RPM_TIMEOUT: return StateAction{SoundId::MOWER_RPM_TIMEOUT, SoundId::EMERGENCY, true};
      case EmergencyKind::OTHER:
      case EmergencyKind::NONE: break;
    }
    return StateAction{SoundId::EMERGENCY};
  }
  const bool was_emergency = (from == Alert::EMERGENCY);
  if (to == Alert::LINK_LOST) {
    return StateAction{SoundId::ROS_DISCONNECTED, kNoSound, was_emergency};
  }
  return StateAction{was_emergency ? SoundId::EMERGENCY_CLEARED : SoundId::ROS_CONNECTED, kNoSound, was_emergency};
}

/**
 * @brief Which sound a high level state change plays.
 *
 * | from                    | to          | sound                 |
 * |-------------------------|-------------|-----------------------|
 * | IDLE / UNDOCKING        | MOWING      | MOWING_START          |
 * | MOWING                  | PAUSED      | MOWING_PAUSED         |
 * | PAUSED                  | MOWING      | MOWING_RESUMED        |
 * | MOWING / PAUSED         | DOCKING     | MOWING_COMPLETE       |
 * | anything else           | DOCKING     | DOCKING_START         |
 * | anything else           | RECORDING   | AREA_RECORDING_MODE   |
 * | anything else           | …           | silent                |
 *
 * @note  Only the *high level* knows these: the blade says "the robot mows", but not
 *        whether it is undocking, paused, done or merely driving home. A docking that
 *        follows mowing is reported as "complete"; the high level may also dock for rain
 *        or a low battery, and the low level cannot tell - the announcement is the same
 *        either way.
 */
SoundId StateMachine::HlSound(HlState from, HlState to) {
  switch (to) {
    case HlState::MOWING: return (from == HlState::PAUSED) ? SoundId::MOWING_RESUMED : SoundId::MOWING_START;
    case HlState::PAUSED: return (from == HlState::MOWING) ? SoundId::MOWING_PAUSED : kNoSound;
    case HlState::DOCKING:
      return (from == HlState::MOWING || from == HlState::PAUSED) ? SoundId::MOWING_COMPLETE : SoundId::DOCKING_START;
    case HlState::RECORDING: return SoundId::AREA_RECORDING_MODE;
    case HlState::UNKNOWN:
    case HlState::IDLE:
    case HlState::UNDOCKING: break;
  }
  return kNoSound; /* undocking and stopping are silent - the warning device starts/stops */
}

/**
 * @brief Which sound may play by itself while the blade turns.
 *
 * The blade is the low level's own truth for "the robot is cutting": the high level's
 * MOWING starts before the motor spins up (and keeps running while it waits for GPS), and a
 * background sound should belong to the work, not to the plan. The ambience rate and the
 * probability come from the definition (`every`, `chance`).
 */
SoundId StateMachine::AmbientSound() const {
  return blade_.value() ? SoundId::MOWING_AMBIENCE : kNoSound;
}

void StateMachine::AmbientArm(uint32_t now_ms) {
  ambient_armed_ = false;
  ambient_due_ms_ = 0U;

  const SoundId id = AmbientSound();
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

  const SoundId id = AmbientSound();
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
