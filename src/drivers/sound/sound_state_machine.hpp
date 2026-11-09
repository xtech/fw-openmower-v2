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
 *        Only a *change* makes a sound, and an alert is announced first: a link loss
 *        matters more than the blade starting. While an alert lasts, every other state
 *        change is tracked silently, so nothing stale is announced when the alert clears.
 */

#ifndef SOUND_STATE_MACHINE_HPP
#define SOUND_STATE_MACHINE_HPP

#include <cstdint>

#include "sound_output.hpp"
#include "sound_types.hpp"

namespace xbot::driver::sound {

/**
 * @brief Event-driven sound logic: state in, stop/play out.
 *
 * Call Reset() once (boot), then Update() a few times per second from the service
 * loop. Update() performs the decision through the SoundOutput interface: stop the
 * current playback (if needed), then push the sound(s) into the player queue in order.
 *
 * @note  All times are uint32 milliseconds — the same type as the clock they are
 *        compared with, so no conversion can silently truncate an interval.
 */
class StateMachine {
 public:
  /**
   * @brief How the machine reaches the outside world.
   *
   * The input hooks are function pointers (deterministic in tests); the output is the
   * small SoundOutput interface, implemented by the service with the real player.
   */
  struct Hooks {
    /** every/chance of a sound as configured; nullptr or every_ms 0 = no ambience. */
    AmbientPolicy (*policy)(SoundId id) = nullptr;
    /** true with the given probability (0..100); nullptr = never. */
    bool (*dice)(uint8_t chance_pct) = nullptr;
  };

  /**
   * @brief How long a link loss has to last before it is announced.
   *
   * The emergency status bit flips after one second; a *sound* must not follow that
   * fast, or a hiccup would play "disconnected" and "connected" half a second later.
   */
  static constexpr uint32_t kLinkLostDelayMs = 2000;

  /**
   * @brief Construct with the player output and the input hooks.
   *
   * The output must outlive the machine: the service passes itself, a host test passes
   * its recorder.
   */
  StateMachine(SoundOutput& output, Hooks hooks);

  /**
   * @brief Forget every state; the next Update() announces the boot.
   *
   * The machine starts in Reset() state (the constructor calls it), so this is only
   * needed if a caller wants to replay the boot announcement.
   */
  void Reset();

  /**
   * @brief Evaluate one snapshot and perform the resulting output.
   *
   * @param inputs  What the services report (see StateInputs).
   * @param now_ms  A monotonic millisecond clock. It has to be 32 bit: the value is
   *                compared up to a minute apart (the ambience `every`) and wraps only
   *                after 49 days, while a uint16 clock would wrap every 65 seconds.
   */
  void Update(const StateInputs& inputs, uint32_t now_ms);

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
    return rt_.charging;
  }

  /** @brief True while the blade turns (the ambience follows it). */
  bool blade_running() const {
    return rt_.blade_running;
  }

  /** @brief The current alert (NONE, LINK_LOST or EMERGENCY). */
  Alert alert() const {
    return rt_.alert;
  }

  /** @brief The last high-level state reported to the machine. */
  HlState hl_state() const {
    return rt_.hl_state;
  }

 private:
  /** @brief The output of an alert transition (rules: sound_state_machine.cpp). */
  void AlertAction(Alert from, Alert to, EmergencyKind kind);

  /** @brief The sound of a high level state transition (rules: sound_state_machine.cpp). */
  static SoundId HlSound(HlState from, HlState to);

  /** @brief Which sound may play by itself while the blade turns (the ambience). */
  SoundId AmbientSound() const;

  /** @brief Fetch a usable every/chance policy for @p id; false when it has none. */
  bool AmbientPolicyFor(SoundId id, AmbientPolicy& out) const;

  /** @brief Restart the ambient interval (on boot and on every state change). */
  void AmbientArm(uint32_t now_ms);

  /** @brief One ambience decision: due and lucky -> play. */
  void AmbientStep(uint32_t now_ms);

  SoundOutput& output_;
  Hooks hooks_{};

  /**
   * @brief Everything the machine remembers between two Update() calls.
   *
   * Grouped so Reset() is one assignment: a new field cannot be forgotten there.
   * The 32-bit timers come first and the small values afterwards, so the struct has
   * no internal padding (16 B instead of 20 B) without bit-field tricks.
   */
  struct Runtime {
    uint32_t link_lost_since_ms = 0U;  ///< A link loss started here
    uint32_t ambient_due_ms = 0U;      ///< The ambience interval is due here

    Alert alert = Alert::LINK_LOST;       ///< The robot boots without a high level
    HlState hl_state = HlState::UNKNOWN;  ///< Last reported high level state

    bool started = false;           ///< The boot announcement was done
    bool charging = false;          ///< Last charger state (the CHARGING_START/DONE edges)
    bool blade_running = false;     ///< Last blade state (the ambience follows it)
    bool link_lost_timer = false;   ///< A link loss is being timed
    bool charge_done_seen = false;  ///< The charger reached DONE during this charge
    bool ambient_armed = false;     ///< An interval is running
  };

  Runtime rt_{};
};

}  // namespace xbot::driver::sound

#endif  // SOUND_STATE_MACHINE_HPP
