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
 * @note  Header-only value types, deliberately free of hardware and services (apart from
 *        the generated SoundId enum), so both the state machine and the host tests can use
 *        them without pulling ChibiOS in.
 */

#ifndef SOUND_TYPES_HPP
#define SOUND_TYPES_HPP

#include <SoundServiceBase.hpp>
#include <cstdint>

namespace xbot::driver::sound {

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
 * @brief What the high level is doing, collapsed to the states that change the audio.
 *
 * The high level already sends this with every cycle (`/high_level_status` →
 * HighLevelService: `HighLevelStatus` IDLE/AUTONOMOUS/RECORDING plus the behaviour name).
 * The *names* are what matters here: the mode alone cannot tell undocking from docking.
 */
enum class HlState : uint8_t {
  UNKNOWN,    ///< No high level state (yet, or an unknown behaviour)
  IDLE,       ///< Nothing runs: standing in the station or on the lawn
  UNDOCKING,  ///< Leaving the station (autonomous driving)
  MOWING,     ///< Mowing
  PAUSED,     ///< Mowing was paused
  DOCKING,    ///< Driving back to the station
  RECORDING,  ///< Area recording (the app drives it)
};

/** @brief Short names for logs (and test failures). */
inline const char* hl_state_name(HlState s) {
  switch (s) {
    case HlState::UNKNOWN: return "UNKNOWN";
    case HlState::IDLE: return "IDLE";
    case HlState::UNDOCKING: return "UNDOCKING";
    case HlState::MOWING: return "MOWING";
    case HlState::PAUSED: return "PAUSED";
    case HlState::DOCKING: return "DOCKING";
    case HlState::RECORDING: return "RECORDING";
  }
  return "?";
}

/**
 * @brief True while the high level transfers the robot to or from the dock.
 *
 * Those are the states where the drive warning runs (UNDOCKING, DOCKING). MOWING is
 * deliberately not one of them: mowing lasts for hours (and has its own background sound),
 * while the warning is meant for the transfer that starts or ends a job. Add MOWING here if
 * the robot should beep all the time it works.
 *
 * The cadence is the definition's `repeat_ms` (the player repeats the sound by itself),
 * so the machine only starts the pattern when such a transfer begins and stops it when the
 * transfer ends.
 */
inline bool HlStateIsDockTransfer(HlState state) {
  return state == HlState::UNDOCKING || state == HlState::DOCKING;
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
 * @brief One snapshot of the robot state the sound logic reacts to.
 *
 * The caller (SoundService) fills this from the services that own the information, so
 * the machine itself has no dependency on them.
 *
 * @note  There is deliberately no battery percentage here: the high level owns the
 *        battery policy and decides when a battery is "low" (and may play
 *        SoundId::LOW_BATTERY explicitly). The low level only reports what it can see.
 */
struct StateInputs {
  bool high_level_alive = false;                  ///< The ROS heartbeat is here (never true before the first one)
  EmergencyKind emergency = EmergencyKind::NONE;  ///< Which audible condition is active
  /**
   * @brief The emergency is *latched*, i.e. it only clears when the latch is released.
   *
   * Every audible condition reaches the low level together with EmergencyReason::LATCH
   * once it lasted its configured delay (input_service/mower_service), so a flickering
   * sensor never shows up here at all. A definition may switch latching off
   * (`latch: false`), then the condition simply disappears again — and so does the alert.
   *
   * The consequence for the machine: the alert is *entered* by the condition and only
   * *left* once neither the condition nor the latch is there any more.
   */
  bool emergency_latched = false;
  HlState hl_state = HlState::UNKNOWN;           ///< What the high level reports (its behaviour name)
  bool blade_running = false;                    ///< The mower motor turns (mowing)
  ChargerState charger = ChargerState::UNKNOWN;  ///< Charger phase (the high level reports no charging)
};

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
