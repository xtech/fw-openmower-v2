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

StateMachine::StateMachine(SoundOutput& output, Hooks hooks) : output_(output), hooks_(hooks) {
  Reset();
}

void StateMachine::Reset() {
  // Runtime's default member initializers hold the whole boot state (including
  // alert = LINK_LOST): one assignment cannot miss a field added later.
  rt_ = Runtime{};
}

void StateMachine::Update(const StateInputs& inputs, uint32_t now_ms) {
  // --- The alert: emergency first, then a link loss that lasted long enough -------
  if (inputs.high_level_alive) {
    rt_.link_lost_timer = false;
  } else if (!rt_.link_lost_timer) {
    rt_.link_lost_timer = true;
    rt_.link_lost_since_ms = now_ms;
  }

  Alert next_alert = Alert::NONE;
  /* An audible condition *starts* the alert; leaving it needs the latch to be released as
     well, so a sensor that lets go while the robot is still latched keeps the alert (and
     the "all clear" waits for the reset). Reasons whose definition disables latching
     (`latch: false`) clear as soon as the condition is gone. */
  const bool dangerous = (inputs.emergency != EmergencyKind::NONE);
  if (dangerous || (rt_.alert == Alert::EMERGENCY && inputs.emergency_latched)) {
    next_alert = Alert::EMERGENCY;
  } else if (!inputs.high_level_alive &&
             (rt_.alert == Alert::LINK_LOST || time_reached(now_ms, rt_.link_lost_since_ms + kLinkLostDelayMs))) {
    /* Only a loss that *persists* is a state change; while we are already in LINK_LOST
       it stays (the high level simply is not back yet). */
    next_alert = Alert::LINK_LOST;
  }

  // --- The sensors: the sources own their filtering; follow their changes directly ---
  const bool charging = (inputs.charger == ChargerState::CHARGING);
  const bool blade_before = rt_.blade_running;
  rt_.blade_running = inputs.blade_running;
  if (rt_.blade_running != blade_before) {
    /* The ambience belongs to the blade (see AmbientSound()): its interval restarts when
       the motor starts or stops. */
    AmbientArm(now_ms);
  }

  if (inputs.charger == ChargerState::DONE) {
    /* DONE is not an active charge phase, so it can arrive in the same tick that ends the
       charge: remember it here, the edge below consumes it. */
    rt_.charge_done_seen = true;
  }

  // --- Boot: the power-up sweep, but an active emergency outranks it ---------------
  if (!rt_.started) {
    rt_.started = true;
    rt_.charging = charging;
    rt_.alert = next_alert;
    /* The first look at the high level is not a transition: it must not replay a state
       that started before the firmware did (e.g. a reconnect while mowing). */
    rt_.hl_state = inputs.hl_state;
    AmbientArm(now_ms);
    if (next_alert == Alert::EMERGENCY) {
      AlertAction(Alert::NONE, Alert::EMERGENCY, inputs.emergency);
    } else {
      output_.PlaySound(SoundId::BOOT_COMPLETE);
    }
    return;
  }

  // --- An alert change is announced, and it swallows every other transition --------
  if (next_alert != rt_.alert) {
    const Alert previous = rt_.alert;
    rt_.alert = next_alert;
    /* The alert is the message. Everything else is taken over silently, so the machine
       does not replay a state that changed while the alert was active. */
    rt_.charging = charging;
    rt_.hl_state = inputs.hl_state;
    rt_.charge_done_seen = false;
    AmbientArm(now_ms);
    AlertAction(previous, next_alert, inputs.emergency);
    if (next_alert == Alert::NONE && HlStateIsDockTransfer(rt_.hl_state)) {
      /* The alert (and its stop) ended the drive warning: start it again after the
         all-clear. */
      output_.PlaySound(SoundId::AUTONOMOUS_NOTICE);
    }
    return;
  }

  // --- While an alert lasts, changes are tracked but never announced ---------------
  if (rt_.alert != Alert::NONE) {
    rt_.charging = charging;
    rt_.hl_state = inputs.hl_state;
    rt_.charge_done_seen = false;
    return;
  }

  // --- Charging: the charger is the only source (the high level reports no charging)
  if (charging != rt_.charging) {
    rt_.charging = charging;
    AmbientArm(now_ms);
    if (charging) {
      rt_.charge_done_seen = false;  // a fresh charge: DONE can only be reported from now on
      output_.PlaySound(SoundId::CHARGING_START);
      return;
    }
    const bool done = rt_.charge_done_seen;
    rt_.charge_done_seen = false;
    if (done) {
      output_.PlaySound(SoundId::CHARGING_DONE);
      return;
    }
  }

  // --- The high level's own state: its announcements ------------------------------
  if (inputs.hl_state != rt_.hl_state) {
    const HlState previous = rt_.hl_state;
    rt_.hl_state = inputs.hl_state;

    if (HlStateIsDockTransfer(rt_.hl_state) && !HlStateIsDockTransfer(previous)) {
      /* A drive starts the drive warning. Its cadence is the definition's repeat_ms, so
         the machine only has to start it. A docking announcement is queued behind it;
         the player plays the requests in the order they arrive. */
      const SoundId announce = HlSound(previous, rt_.hl_state);
      if (announce != kNoSound) {
        output_.StopPlayback();
        output_.PlaySound(SoundId::AUTONOMOUS_NOTICE);
        output_.PlaySound(announce);
      } else {
        output_.StopPlayback();
        output_.PlaySound(SoundId::AUTONOMOUS_NOTICE);
      }
      return;
    }
    if (!HlStateIsDockTransfer(rt_.hl_state) && HlStateIsDockTransfer(previous)) {
      /* The drive is over: stop the pattern (repeat_ms would run on by itself). */
      output_.StopPlayback();
      return;
    }

    const SoundId sound = HlSound(previous, rt_.hl_state);
    if (sound != kNoSound) {
      output_.PlaySound(sound);
      return;
    }
  }

  // --- Ambience: the background sound while the blade turns ------------------------
  AmbientStep(now_ms);
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
 * The "what happened" announcement plays first and the alarm follows it (second sound):
 * the alarm is queued behind the voice, so the user hears what happened before the
 * tone. Every audible emergency stops whatever runs first — an alert
 * must not wait behind an announcement. While an emergency is latched no further
 * announcement is made - the alert simply stays.
 *
 * @note  A repeating alert (the emergency tone holds several seconds) has to be stopped
 *        before the "all clear" plays, hence the stop.
 */
void StateMachine::AlertAction(Alert from, Alert to, EmergencyKind kind) {
  if (to == Alert::EMERGENCY) {
    switch (kind) {
      case EmergencyKind::STOP:
        output_.StopPlayback();
        output_.PlaySound(SoundId::STOP_BUTTON);
        return;
      case EmergencyKind::LIFT:
        output_.StopPlayback();
        output_.PlaySound(SoundId::LIFT_TRIGGERED);
        output_.PlaySound(SoundId::EMERGENCY);
        return;
      case EmergencyKind::COLLISION:
        output_.StopPlayback();
        output_.PlaySound(SoundId::COLLISION_TRIGGERED);
        output_.PlaySound(SoundId::EMERGENCY);
        return;
      case EmergencyKind::RPM_LIMIT:
        output_.StopPlayback();
        output_.PlaySound(SoundId::MOWER_RPM_LIMIT);
        output_.PlaySound(SoundId::EMERGENCY);
        return;
      case EmergencyKind::RPM_TIMEOUT:
        output_.StopPlayback();
        output_.PlaySound(SoundId::MOWER_RPM_TIMEOUT);
        output_.PlaySound(SoundId::EMERGENCY);
        return;
      case EmergencyKind::OTHER:
      case EmergencyKind::NONE: break;
    }
    output_.StopPlayback();
    output_.PlaySound(SoundId::EMERGENCY);
    return;
  }

  const bool was_emergency = (from == Alert::EMERGENCY);
  if (was_emergency) {
    output_.StopPlayback();
  }
  if (to == Alert::LINK_LOST) {
    output_.PlaySound(SoundId::ROS_DISCONNECTED);
    return;
  }
  output_.PlaySound(was_emergency ? SoundId::EMERGENCY_CLEARED : SoundId::ROS_CONNECTED);
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
  return kNoSound; /* undocking and docking are silent - the drive warning starts/stops */
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
  return rt_.blade_running ? SoundId::MOWING_AMBIENCE : kNoSound;
}

bool StateMachine::AmbientPolicyFor(SoundId id, AmbientPolicy& out) const {
  if (id == kNoSound || hooks_.policy == nullptr) {
    return false;
  }
  out = hooks_.policy(id);
  return out.every_s != 0U && out.chance_pct != 0U;
}

void StateMachine::AmbientArm(uint32_t now_ms) {
  rt_.ambient_armed = false;
  rt_.ambient_due_ms = 0U;

  const SoundId id = AmbientSound();
  AmbientPolicy policy{};
  if (!AmbientPolicyFor(id, policy)) {
    return; /* the blade is off, or the definition does not want an ambience */
  }
  rt_.ambient_armed = true;
  rt_.ambient_due_ms = now_ms + static_cast<uint32_t>(policy.every_s) * 1'000U;
}

void StateMachine::AmbientStep(uint32_t now_ms) {
  /* No ambience under an alert: an emergency or a lost high level is not the moment for
     a cow. The alert change above has just re-armed the interval. */
  if (rt_.alert != Alert::NONE) {
    return;
  }
  if (!rt_.ambient_armed || !time_reached(now_ms, rt_.ambient_due_ms)) {
    return;
  }

  const SoundId id = AmbientSound();
  AmbientPolicy policy{};
  if (!AmbientPolicyFor(id, policy)) {
    rt_.ambient_armed = false;
    return;
  }

  /* The interval restarts whether or not the dice hit: `every` is the evaluation rate,
     so a miss must not shorten the next wait. */
  rt_.ambient_due_ms = now_ms + static_cast<uint32_t>(policy.every_s) * 1'000U;

  if (hooks_.dice == nullptr || !hooks_.dice(policy.chance_pct)) {
    return;
  }
  output_.PlaySound(id);
}

}  // namespace xbot::driver::sound
