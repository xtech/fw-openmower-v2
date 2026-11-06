/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_state_machine.hpp
 * @brief Decides what the robot announces — in one place, with one rule table.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-11-05
 *
 * @note  The services own *state* (emergency reasons, charger, blade), the sound player
 *        owns *audio* (queue, formats, volume).  This machine is the thin layer in
 *        between: it turns a state transition into a sound.
 *
 *        The inputs are plain booleans (see StateInputs), so the rules read like
 *        sentences, and the machine needs neither hardware nor a service to run — which
 *        is what makes it testable.
 *
 *        The state has three sources, and only one of them needs an axis of its own:
 *          HlState — what the high level says it does (idle, undocking, mowing, paused,
 *                    docking, recording)
 *          Alert   — what is wrong (link lost, emergency) and therefore wins.
 *          Sensors — the charger (the high level does not report charging) and the blade
 *                    (the ambience runs while it turns).
 *        Only a *change* makes a sound, and an alert change is announced first: a link
 *        loss matters more than the blade starting, and the machine catches up with the
 *        rest silently afterwards.
 */

#ifndef SOUND_STATE_MACHINE_HPP
#define SOUND_STATE_MACHINE_HPP

#include <SoundServiceBase.hpp>
#include <cstdint>

namespace xbot::driver::sound {

/** @brief What is wrong; overrides everything else while it lasts (EMERGENCY wins). */
enum class Alert : uint8_t {
  NONE,       ///< Everything is fine
  LINK_LOST,  ///< The high level (ROS) is gone
  EMERGENCY,  ///< An audible emergency condition is active
};

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

inline const char* alert_name(Alert a) {
  switch (a) {
    case Alert::NONE: return "NONE";
    case Alert::LINK_LOST: return "LINK_LOST";
    case Alert::EMERGENCY: return "EMERGENCY";
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

/**
 * @brief True while the high level *manoeuvres* the robot (the warning device runs).
 *
 * Deliberately not MOWING: mowing runs for hours (and has its own background sounds), while
 * the warning is meant for the docking manoeuvres - the reversing alarm the definition was
 * written for. Add MOWING here if the robot should beep all the time it works.
 *
 * The *cadence* is the definition's `repeat_ms` (the player repeats the sound by itself),
 * so the machine only has to start the pattern when a manoeuvre begins and stop it when it
 * ends.
 */
inline bool hl_manoeuvres(HlState state) {
  return state == HlState::UNDOCKING || state == HlState::DOCKING;
}

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
 * @brief One snapshot of the robot state the sound logic reacts to.
 *
 * The caller (SoundService) fills this from the services that own the information, so
 * the machine itself has no dependency on them.
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
  HlState hl_state = HlState::UNKNOWN;  ///< What the high level reports (its behaviour name)
  bool blade_running = false;           ///< The mower motor turns (mowing)
  bool charging = false;                ///< The charger charges right now
  bool charge_done = false;             ///< The charger reports DONE
  uint8_t battery_percent = 255U;       ///< 0..100; 255 = unknown (no LOW_BATTERY announcement)
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

/** @brief "No sound": one past the last id (SoundId_count is a size_t, not a SoundId). */
constexpr SoundId kNoSound = static_cast<SoundId>(SoundId_count);

/** @brief What the caller should do with the audio after one Update(). */
struct StateAction {
  SoundId sound = kNoSound;      ///< Play now (kNoSound: nothing)
  SoundId follow_up = kNoSound;  ///< Play as soon as `sound` finished (kNoSound: nothing)
  bool stop_first = false;       ///< Flush the running sound and the queue first

  bool silent() const {
    return sound == kNoSound;
  }
};

/**
 * @brief Event-driven sound logic: state in, sound out.
 *
 * Call Reset() once (boot), then Update() a few times per second from the service
 * loop.  Every call returns at most one sound, so a burst of input changes never
 * floods the player queue.
 *
 * @note  All times are uint32 milliseconds — the same type as the clock they are
 *        compared with, so no conversion can silently truncate an interval.
 */
class StateMachine {
 public:
  /**
   * @brief How the machine reaches the outside world.
   *
   * Function pointers instead of virtual methods: they are set once at construction
   * (the service passes static functions) and a test can pass deterministic ones.
   */
  struct Hooks {
    /** every/chance of a sound as configured; nullptr or every_ms 0 = no ambience. */
    AmbientPolicy (*policy)(SoundId id) = nullptr;
    /** true with the given probability (0..100); nullptr = never. */
    bool (*dice)(uint8_t chance_pct) = nullptr;
  };

  /** @brief How long the blade/charger input has to be stable before it counts. */
  static constexpr uint32_t kInputDebounceMs = 500;

  /**
   * @brief How long a link loss has to last before it is announced.
   *
   * The emergency status bit flips after one second; a *sound* must not follow that
   * fast, or a hiccup would play "disconnected" and "connected" half a second later.
   */
  static constexpr uint32_t kLinkLostDelayMs = 2000;

  /** @brief LOW_BATTERY is announced below this, and re-armed this much above it. */
  static constexpr uint8_t kLowBatteryPercent = 15;
  static constexpr uint8_t kLowBatteryHysteresis = 5;

  explicit StateMachine(Hooks hooks);

  /** @brief No hooks: no ambience, never a lucky roll (used by tests). */
  StateMachine() = default;

  /**
   * @brief Forget every state; the next Update() announces the boot.
   *
   * The machine starts in Reset() state (the constructor calls it), so this is only
   * needed if a caller wants to replay the boot announcement.
   */
  void Reset();

  /**
   * @brief Evaluate one snapshot.
   *
   * @param inputs  What the services report (see StateInputs).
   * @param now_ms  A monotonic millisecond clock. It has to be 32 bit: the value is
   *                compared up to a minute apart (the ambience `every`) and wraps only
   *                after 49 days, while a uint16 clock would wrap every 65 seconds.
   * @return The sound to play, or an empty action.
   */
  StateAction Update(const StateInputs& inputs, uint32_t now_ms);

  /**
   * @brief Re-evaluate the ambient rate, e.g. after a new definitions blob.
   *
   * The state stays as it is — only the next ambience interval restarts, so a changed
   * `every`/`chance` takes effect without waiting for the next state change.
   */
  void AmbientPolicyChanged(uint32_t now_ms) {
    AmbientArm(now_ms);
  }

  /** @brief True while the charger charges (the only state the high level lacks). */
  bool charging() const {
    return charging_;
  }

  /** @brief True while the blade turns (the ambience and LOW_BATTERY follow it). */
  bool blade_running() const {
    return blade_.value();
  }

  Alert alert() const {
    return alert_;
  }

  HlState hl_state() const {
    return hl_state_;
  }

 private:
  /** @brief A boolean input that only flips after it has been stable for a while. */
  class Debounced {
   public:
    /** @return the debounced value of this sample. */
    bool Update(bool raw, uint32_t now_ms, uint32_t stable_ms);

    bool value() const {
      return value_;
    }

   private:
    bool value_ = false;
    bool raw_ = false;
    uint32_t changed_ms_ = 0U;
  };

  /** @brief The sound of an alert transition (rules: sound_state_machine.cpp). */
  static StateAction AlertAction(Alert from, Alert to, EmergencyKind kind);

  /** @brief The sound of a high level state transition (rules: sound_state_machine.cpp). */
  static SoundId HlSound(HlState from, HlState to);

  /** @brief Which sound may play by itself while the blade turns (the ambience). */
  SoundId AmbientSound() const;

  /** @brief Restart the ambient interval (on boot and on every state change). */
  void AmbientArm(uint32_t now_ms);

  /** @brief One ambience decision: due and lucky -> play. */
  StateAction AmbientStep(uint32_t now_ms);

  Hooks hooks_{};

  bool started_ = false;
  bool charging_ = false;           ///< Debounced charger state (the CHARGING_START/DONE edges)
  Alert alert_ = Alert::LINK_LOST;  ///< The robot boots without a high level

  Debounced blade_;
  Debounced charger_;

  bool link_lost_timer_ = false;         ///< A link loss is being timed
  uint32_t link_lost_since_ms_ = 0U;     ///< … and started here
  HlState hl_state_ = HlState::UNKNOWN;  ///< Last reported high level state
  bool charge_done_seen_ = false;        ///< The charger reached DONE during this charge
  bool low_battery_announced_ = false;

  bool ambient_armed_ = false;    ///< An interval is running
  uint32_t ambient_due_ms_ = 0U;  ///< … and is due here
};

}  // namespace xbot::driver::sound

#endif  // SOUND_STATE_MACHINE_HPP
