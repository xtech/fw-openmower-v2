/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_types.hpp
 * @brief The vocabulary of the sound logic: alerts, high-level states, inputs and actions.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-11-07
 *
 * @note  Header-only value types. The state machine itself stays free of service wiring;
 *        the vocabulary reuses the generated SoundId enum and the GPS driver's RTK enum,
 *        so there is only one GPS RTK enum in the firmware.
 */

#ifndef SOUND_TYPES_HPP
#define SOUND_TYPES_HPP

#include <drivers/gps/gps_driver.h>

#include <SoundServiceBase.hpp>
#include <cstdint>

namespace xbot::driver::sound {

using xbot::driver::gps::GpsRtkMode;

/** @brief What is wrong; overrides everything else while it lasts (EMERGENCY wins). */
enum class Alert : uint8_t {
  NONE,       ///< Everything is fine
  LINK_LOST,  ///< The high level (ROS) is gone
  EMERGENCY,  ///< An audible emergency condition is active
};

inline const char* alert_name(Alert a) {
  switch (a) {
    case Alert::NONE: return "NONE";
    case Alert::LINK_LOST: return "LINK_LOST";
    case Alert::EMERGENCY: return "EMERGENCY";
  }
  return "?";
}

/**
 * @brief Which condition triggered the emergency.
 *
 * Decides *what* is announced: a pressed stop button is no surprise (the user just did it),
 * so it gets a plain confirmation instead of the alert tone, while a lift or a collision is
 * reported as "what happened" and *then* raises the alarm.
 *
 * The values are ordered by precedence — when several conditions are latched at once, the
 * first one in this list names the episode.
 */
enum class EmergencyKind : uint8_t {
  NONE,         ///< No condition
  STOP,         ///< The user pressed stop — confirm, do not alarm
  LIFT,         ///< Lifted / handle released
  COLLISION,    ///< Bumped into something
  RPM_LIMIT,    ///< Blade RPM above the safe limit (the low level cut the power)
  RPM_TIMEOUT,  ///< Blade did not spin up (the high level gave up after its timeout)
  OTHER,        ///< Anything else audible — alarm only
};

/** @brief Short names for logs (and test failures). */
inline const char* emergency_kind_name(EmergencyKind k) {
  switch (k) {
    case EmergencyKind::NONE: return "NONE";
    case EmergencyKind::STOP: return "STOP";
    case EmergencyKind::LIFT: return "LIFT";
    case EmergencyKind::COLLISION: return "COLLISION";
    case EmergencyKind::RPM_LIMIT: return "RPM_LIMIT";
    case EmergencyKind::RPM_TIMEOUT: return "RPM_TIMEOUT";
    case EmergencyKind::OTHER: return "OTHER";
  }
  return "?";
}

/**
 * @brief What the charger reports right now.
 *
 * One value instead of separate `charging`/`charge_done` booleans: the two states are
 * mutually exclusive, and the enum makes that impossible to get wrong.
 */
enum class ChargerState : uint8_t {
  UNKNOWN,   ///< No power service, or no charger data yet
  IDLE,      ///< Not charging, not done
  CHARGING,  ///< Any active charge phase (trickle, pre-charge, CC, CV, top-off)
  DONE,      ///< Charge complete
};

/** @brief Short names for logs (and test failures). */
inline const char* charger_state_name(ChargerState s) {
  switch (s) {
    case ChargerState::UNKNOWN: return "UNKNOWN";
    case ChargerState::IDLE: return "IDLE";
    case ChargerState::CHARGING: return "CHARGING";
    case ChargerState::DONE: return "DONE";
  }
  return "?";
}

/**
 * @brief What is true in this tick, one bit per fact.
 *
 * The machine is a *sampler*: Update() runs every 100 ms and nobody tells it what changed.
 */
using Signals = uint16_t;
enum : Signals {
  /* Bits 0..2: the *mode* as a field - exactly one value, not a flag set. The name avoids a
     collision with the high level's own vocabulary: HighLevelStatus calls 0..3 its states
     (NULL/IDLE/AUTONOMOUS/RECORDING), while 4..7 are SUBSTATES of AUTONOMOUS - i.e.
     behaviours, not states. In the sound logic both are simply "what is it doing". */
  HL_MODE_SHIFT = 0,
  HL_MODE_MASK = 0x7U << HL_MODE_SHIFT,
  HL_MODE_UNKNOWN = 0U,     ///< = HIGH_LEVEL_STATE_NULL
  HL_MODE_IDLE = 1U,        ///< = HIGH_LEVEL_STATE_IDLE
  HL_MODE_AUTONOMOUS = 2U,  ///< = HIGH_LEVEL_STATE_AUTONOMOUS, behaviour name unknown
  HL_MODE_RECORDING = 3U,   ///< = HIGH_LEVEL_STATE_RECORDING
  HL_MODE_UNDOCKING = 4U,   ///< SUBSTATE: leaving the station
  HL_MODE_MOWING = 5U,      ///< SUBSTATE: mowing
  HL_MODE_PAUSED = 6U,      ///< SUBSTATE: mowing was paused
  HL_MODE_DOCKING = 7U,     ///< SUBSTATE: driving back to the station

  HL_ALIVE = 1u << 8,  ///< The high level heartbeat is here => ROS is running

  /* Bits 3..5: which audible condition is active, as a field (0 = none). The field holds the
     EmergencyKind value itself, so the cast is lossless and there is no second table here.
     Whether an emergency is active is therefore not a bit - it is "the field is not NONE". */
  EMERGENCY_KIND_SHIFT = 3,
  EMERGENCY_KIND_MASK = 0x7U << EMERGENCY_KIND_SHIFT,

  EMERGENCY_LATCHED = 1u << 9,  ///< It stays until the latch is released
  CHARGING = 1u << 10,          ///< The charger is in an active phase
  CHARGER_DONE = 1u << 11,      ///< The charger reports the charge complete
  BLADE_RUNNING = 1u << 12,     ///< The mower motor turns
  RTK_FIX = 1u << 13            ///< The local GPS has an RTK fix (falling edge = lost)
};

/** @brief The mode field of the signals (one of HL_MODE_*). */
inline Signals HlMode(Signals s) {
  return s & HL_MODE_MASK;
}

/** @brief The audible condition of the signals (NONE when there is none). */
inline EmergencyKind EmergencyKindOf(Signals s) {
  return static_cast<EmergencyKind>((s & EMERGENCY_KIND_MASK) >> EMERGENCY_KIND_SHIFT);
}

/** @brief The bits of an audible condition, for building a sample. */
inline Signals EmergencyKindBits(EmergencyKind kind) {
  return static_cast<Signals>((static_cast<Signals>(kind) << EMERGENCY_KIND_SHIFT) & EMERGENCY_KIND_MASK);
}

/** @brief Short names for logs (and test failures). */
inline const char* hl_name(Signals s) {
  switch (HlMode(s)) {
    case HL_MODE_IDLE: return "IDLE";
    case HL_MODE_AUTONOMOUS: return "AUTONOMOUS";
    case HL_MODE_RECORDING: return "RECORDING";
    case HL_MODE_UNDOCKING: return "UNDOCKING";
    case HL_MODE_MOWING: return "MOWING";
    case HL_MODE_PAUSED: return "PAUSED";
    case HL_MODE_DOCKING: return "DOCKING";
    default: return "UNKNOWN";
  }
}

/**
 * @brief One sample of the machine: what is true now, what was true, and the xor'ed edge.
 */
struct Sample {
  Signals now = 0U;   ///< Signals of this tick
  Signals prev = 0U;  ///< Signals of the last tick
  Signals edge = 0U;  ///< now ^ prev: the bits that changed
};

inline Sample MakeSample(Signals now, Signals prev) {
  return Sample{now, prev, static_cast<Signals>(now ^ prev)};
}

/**
 * @brief every/chance of one sound, as configured in the definition.
 *
 * The values come from sounds_*.yaml (`every: 60s`, `chance: 0.25`) and travel in the
 * definitions blob *in milliseconds*; here the rate is kept in **seconds**, because that
 * is the granularity of a background sound and it halves the table (`uint16_t` + padding
 * = 4 B per sound instead of 8 B). 65535 s = 18 h is the longest an ambience may wait —
 * a longer `every` is clamped, which is still "essentially never".
 */
struct AmbientPolicy {
  uint16_t every_s = 0U;    ///< 0: never by itself
  uint8_t chance_pct = 0U;  ///< 0..100: probability per interval
};

/**
 * @brief "No sound": one past the last real SoundId.
 *
 * The codegen derives SoundId_count from the highest value in the definitions blob
 * (max + 1), so this stays correct when the protocol grows. The static_assert only
 * guards the cast into the uint8_t-based SoundId: with count 255 the highest real id
 * is 254 and 255 is still representable; count 256 would wrap to 0.
 */
constexpr SoundId kNoSound = static_cast<SoundId>(SoundId_count);
static_assert(SoundId_count <= 255U, "SoundId (uint8_t) must leave room for the no-sound sentinel");

}  // namespace xbot::driver::sound

#endif  // SOUND_TYPES_HPP
