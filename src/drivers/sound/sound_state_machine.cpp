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
    rt_.gps_rtk = inputs.gps_rtk;
    /* Do not replay the fix announcement at boot, but a fix that already existed still
       arms the later lost sound. */
    rt_.gps_fix_seen = (inputs.gps_rtk == GpsRtkMode::RTK_FIX);
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
    if (next_alert == Alert::NONE) {
      /* Absorb the GPS state that is current when an alert clears: a fix that happened
         while the alert was active is not announced retroactively, but it still arms a
         later lost sound. */
      rt_.gps_rtk = inputs.gps_rtk;
      rt_.gps_fix_seen = (inputs.gps_rtk == GpsRtkMode::RTK_FIX);
    } else {
      rt_.gps_rtk = GpsRtkMode::RTK_NONE;
      rt_.gps_fix_seen = false;
    }
    AmbientArm(now_ms);
    AlertAction(previous, next_alert, inputs.emergency);
    return;
  }

  // --- While an alert lasts, changes are tracked but never announced ---------------
  if (rt_.alert != Alert::NONE) {
    rt_.charging = charging;
    rt_.hl_state = inputs.hl_state;
    rt_.charge_done_seen = false;
    rt_.gps_rtk = GpsRtkMode::RTK_NONE;
    rt_.gps_fix_seen = false;
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

    if (rt_.hl_state == HlState::UNDOCKING) {
      /* The drive starts with a single autonomous notice. Its definition may repeat (the
         ROM fallback uses repeat_ms), so leaving UNDOCKING stops it below. */
      output_.StopPlayback();
      output_.PlaySound(SoundId::AUTONOMOUS_NOTICE);
      return;
    }

    if (rt_.hl_state == HlState::DOCKING || previous == HlState::UNDOCKING || previous == HlState::DOCKING) {
      /* Leaving a dock transfer ends its sound - the ROM notice repeats until a new sound
         arrives, and a docking theme may still be running. Entering DOCKING also cuts the
         mowing sound before its own announcement. */
      output_.StopPlayback();
    }

    const SoundId sound = HlSound(previous, rt_.hl_state);
    if (sound != kNoSound) {
      output_.PlaySound(sound);
      return;
    }
  }

  // --- GPS: RTK fix/lost edges, independent of the high level's state changes ------
  UpdateGpsSounds(inputs);

  // --- Ambience: the background sound while the blade turns ------------------------
  AmbientStep(now_ms);
}

void StateMachine::UpdateGpsSounds(const StateInputs& inputs) {
  /* IDLE is the high level's "GPS is off" state (it calls setGPS(false) there). Reset the
     fix latch so a fix from the previous session cannot make a later lost sound. While
     idle, GPS transitions are deliberately silent. */
  if (inputs.hl_state == HlState::IDLE || rt_.alert != Alert::NONE) {
    rt_.gps_rtk = GpsRtkMode::RTK_NONE;
    rt_.gps_fix_seen = false;
    return;
  }

  if (inputs.gps_rtk == rt_.gps_rtk) {
    return;
  }

  if (inputs.gps_rtk == GpsRtkMode::RTK_FIX) {
    output_.PlaySound(SoundId::GPS_RTK_FIX);
    rt_.gps_fix_seen = true;
  } else if (rt_.gps_rtk == GpsRtkMode::RTK_FIX && rt_.gps_fix_seen) {
    /* FLOAT and NONE both mean "the fix is gone"; the sound is the same. */
    output_.PlaySound(SoundId::GPS_RTK_LOST);
    rt_.gps_fix_seen = false;
  }
  rt_.gps_rtk = inputs.gps_rtk;
}

//=============================================================================
// The rules
//=============================================================================

/**
 * @brief Which sound an alert transition plays.
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
 */
SoundId StateMachine::HlSound(HlState from, HlState to) {
  if (from == to) return kNoSound;  // Update() only calls this for a change

  switch (from) {
    case HlState::MOWING:
      switch (to) {
        case HlState::PAUSED: return SoundId::MOWING_PAUSED;
        case HlState::DOCKING: return SoundId::DOCKING_START;
        case HlState::RECORDING: return SoundId::AREA_RECORDING_MODE;
        default: return kNoSound;
      }

    case HlState::PAUSED:
      switch (to) {
        case HlState::MOWING: return SoundId::MOWING_RESUMED;
        case HlState::DOCKING: return SoundId::DOCKING_START;
        case HlState::RECORDING: return SoundId::AREA_RECORDING_MODE;
        default: return kNoSound;
      }

    case HlState::UNDOCKING:
      switch (to) {
        case HlState::MOWING: return SoundId::MOWING_START;
        case HlState::DOCKING: return SoundId::DOCKING_START;
        case HlState::RECORDING: return SoundId::AREA_RECORDING_MODE;
        default: return kNoSound;
      }

    case HlState::DOCKING:
      switch (to) {
        case HlState::MOWING: return SoundId::MOWING_START;
        case HlState::RECORDING: return SoundId::AREA_RECORDING_MODE;
        default: return kNoSound;
      }

    case HlState::IDLE:
    case HlState::RECORDING:
    case HlState::UNKNOWN:
      switch (to) {
        case HlState::MOWING: return SoundId::MOWING_START;
        case HlState::DOCKING: return SoundId::DOCKING_START;
        case HlState::RECORDING: return SoundId::AREA_RECORDING_MODE;
        default: return kNoSound;
      }
  }
  return kNoSound;
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
