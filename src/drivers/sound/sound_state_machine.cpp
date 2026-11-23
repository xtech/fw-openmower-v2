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

void StateMachine::Update(Signals now, uint32_t now_ms) {
  /* One sample per tick: `prev` is what the last call saw, `edge` the bits that changed.
     Building it here (and not inside every rule) is what keeps the rules free of the
     "which one is the edge" question. */
  const Sample t = MakeSample(now, rt_.now);

  /* The link loss is timed before it is announced: a hiccup must not play a sound. */
  if (t.now & HL_ALIVE) {
    rt_.link_lost_timer = false;
  } else if (!rt_.link_lost_timer) {
    rt_.link_lost_timer = true;
    rt_.link_lost_since_ms = now_ms;
  }

  /* The ambience gate is a bit test now: the interval restarts whenever it changes. */
  const bool ambience_changed = (t.now & kAmbienceGate) != (t.prev & kAmbienceGate);

  rt_.now = t.now; /* tracking: every rule below sees prev/edge, never its own history */

  if (ambience_changed) {
    AmbientArm(now_ms);
  }

  /* The order is the priority: the first rule that plays a sound owns the tick. */
  if (AnnounceAlert(t, now_ms)) return;
  if (AnnounceCharging(t)) return;
  if (AnnounceHighLevel(t)) return;
  if (AnnounceGps(t)) return;
  AmbientStep(now_ms);
}

bool StateMachine::AnnounceAlert(const Sample& t, uint32_t now_ms) {
  /* An audible condition starts the alert; leaving it needs the latch released as well, so
     a sensor that lets go while the robot is still latched keeps the alert. */
  Alert next = Alert::NONE;
  const bool dangerous = EmergencyKindOf(t.now) != EmergencyKind::NONE;
  if (dangerous || ((rt_.alert == Alert::EMERGENCY) && (t.now & EMERGENCY_LATCHED))) {
    next = Alert::EMERGENCY;
  } else if (!(t.now & HL_ALIVE) &&
             (rt_.alert == Alert::LINK_LOST || time_reached(now_ms, rt_.link_lost_since_ms + kLinkLostDelayMs))) {
    next = Alert::LINK_LOST;
  }

  if (!rt_.started) {
    rt_.started = true;
    rt_.alert = next;
    AmbientArm(now_ms);
    if (next == Alert::EMERGENCY) {
      AlertAction(Alert::NONE, Alert::EMERGENCY, EmergencyKindOf(t.now));
    } else {
      output_.PlaySound(SoundId::BOOT_COMPLETE);
    }
    return true;
  }

  if (next == rt_.alert) return false;

  const Alert previous = rt_.alert;
  rt_.alert = next;
  /* The alert swallowed the charge edges: a DONE from the previous charge must not end the
     next one (the wired-in version cleared it on reconnect for the same reason). */
  rt_.memo &= ~MEMO_CHARGE_DONE_SEEN;
  AmbientArm(now_ms);
  AlertAction(previous, next, EmergencyKindOf(t.now));
  return true;
}

bool StateMachine::AnnounceCharging(const Sample& t) {
  /* DONE is not an active phase, so it can arrive in the same tick that ends the charge:
     remember it here, the edge below consumes it. */
  if (t.now & CHARGER_DONE) {
    rt_.memo |= MEMO_CHARGE_DONE_SEEN;
  }

  if (!(t.edge & CHARGING)) return false;

  if (t.now & CHARGING) {
    rt_.memo &= ~MEMO_CHARGE_DONE_SEEN; /* a fresh charge */
    /* Docking contact and DOCKING -> IDLE arrive in the same sample on a real dock: the
       transfer ends where the charge begins, so its sound is stopped before the new one is
       queued (a stop after the play would swallow CHARGING_START). An active alarm owns the
       speaker, though: the stop must not end it the same way the high level path would. */
    const Signals was = HlMode(t.prev);
    if (rt_.alert != Alert::EMERGENCY && (was == HL_MODE_DOCKING || was == HL_MODE_UNDOCKING)) {
      output_.StopPlayback();
    }
    output_.PlaySound(SoundId::CHARGING_START);
    return true;
  }

  const bool done = (rt_.memo & MEMO_CHARGE_DONE_SEEN) != 0U;
  rt_.memo &= ~MEMO_CHARGE_DONE_SEEN;
  if (!done) return false;
  output_.PlaySound(SoundId::CHARGING_DONE);
  return true;
}

bool StateMachine::AnnounceHighLevel(const Sample& t) {
  const Signals from = HlMode(t.prev);
  const Signals to = HlMode(t.now);
  if (from == to) return false;

  /* An active emergency owns the speaker: a behaviour change must not stop the alarm (a lift
     while docking would end it with the dock exit stop below). The mode is in rt_.now either
     way, and there is nothing to replay once the alert is gone. */
  if (rt_.alert == Alert::EMERGENCY) return false;

  /* Without a link there is nobody who announced this behaviour: it is tracked (the bits
     are) but not played. */
  if (!(t.now & HL_ALIVE)) return false;

  if (to == HL_MODE_UNDOCKING) {
    /* The drive starts with a single autonomous notice. Its definition may repeat, so
       leaving UNDOCKING stops it below. */
    output_.StopPlayback();
    output_.PlaySound(SoundId::AUTONOMOUS_NOTICE);
    return true;
  }

  if (to == HL_MODE_DOCKING || from == HL_MODE_UNDOCKING || from == HL_MODE_DOCKING) {
    /* Leaving a dock transfer ends its sound; entering DOCKING cuts the mowing theme. */
    output_.StopPlayback();
  }

  const SoundId sound = HlSound(from, to);
  if (sound == kNoSound) return false;
  output_.PlaySound(sound);
  return true;
}

bool StateMachine::AnnounceGps(const Sample& t) {
  const bool gps_off = (t.now & HL_ALIVE) != 0U && HlMode(t.now) == HL_MODE_IDLE;
  const bool had_fix = (t.prev & RTK_FIX) != 0U;
  const bool fix = (t.now & RTK_FIX) != 0U;

  if (gps_off) return false;

  if (fix == had_fix) return false;

  /* An active alert owns the speaker: the fix is tracked either way (rt_.now), only the
     announcement waits. A lost that happens during the emergency is consumed here as well,
     so it is never replayed after the all clear; LINK_LOST keeps the GPS audible. */
  const bool audible = rt_.alert != Alert::EMERGENCY;

  if (fix) {
    if (!audible) return false;
    output_.PlaySound(SoundId::GPS_RTK_FIX);
    return true;
  }

  if (!audible) return false;
  output_.PlaySound(SoundId::GPS_RTK_LOST);
  return true;
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
SoundId StateMachine::HlSound(Signals from, Signals to) {
  if (from == to) return kNoSound;  // Update() only calls this for a change

  switch (from) {
    case HL_MODE_MOWING:
      switch (to) {
        case HL_MODE_PAUSED: return SoundId::MOWING_PAUSED;
        case HL_MODE_DOCKING: return SoundId::DOCKING_START;
        case HL_MODE_RECORDING: return SoundId::AREA_RECORDING_MODE;
        default: return kNoSound;
      }

    case HL_MODE_PAUSED:
      switch (to) {
        case HL_MODE_MOWING: return SoundId::MOWING_RESUMED;
        case HL_MODE_DOCKING: return SoundId::DOCKING_START;
        case HL_MODE_RECORDING: return SoundId::AREA_RECORDING_MODE;
        default: return kNoSound;
      }

    case HL_MODE_UNDOCKING:
      switch (to) {
        case HL_MODE_MOWING: return SoundId::MOWING_START;
        case HL_MODE_DOCKING: return SoundId::DOCKING_START;
        case HL_MODE_RECORDING: return SoundId::AREA_RECORDING_MODE;
        default: return kNoSound;
      }

    case HL_MODE_DOCKING:
      switch (to) {
        case HL_MODE_MOWING: return SoundId::MOWING_START;
        case HL_MODE_RECORDING: return SoundId::AREA_RECORDING_MODE;
        default: return kNoSound;
      }

    case HL_MODE_IDLE:
    case HL_MODE_RECORDING:
    case HL_MODE_UNKNOWN:
    /* AUTONOMOUS is the fallback for a behaviour name this firmware does not know: the
       behaviour itself stays silent, but the next known mode keeps its entry sound. */
    case HL_MODE_AUTONOMOUS:
      switch (to) {
        case HL_MODE_MOWING: return SoundId::MOWING_START;
        case HL_MODE_DOCKING: return SoundId::DOCKING_START;
        case HL_MODE_RECORDING: return SoundId::AREA_RECORDING_MODE;
        default: return kNoSound;
      }

    default: return kNoSound;
  }
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
  /* The ambience belongs to the work, not to the plan: it needs the blade, an active (not
     paused) mowing behaviour and a high level that runs it. The gate is a bit test, so a
     change of any of its bits re-arms the interval in Update(). */
  const bool active =
      HlMode(rt_.now) == HL_MODE_MOWING && (rt_.now & (HL_ALIVE | BLADE_RUNNING)) == (HL_ALIVE | BLADE_RUNNING);
  return active ? SoundId::MOWING_AMBIENCE : kNoSound;
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
  /* No ambience under an alert: the gate already excludes an emergency and a lost link,
     this is the belt to its braces (and keeps the "alert re-arms" rule in one place). */
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
