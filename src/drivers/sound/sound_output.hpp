/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_output.hpp
 * @brief The two player operations the sound logic needs.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-11-07
 *
 * @note  The state machine has no link to the player (or ChibiOS); it only knows this
 *        interface. The service implements it with `stop()` / `play_sound_id()`, a host
 *        test with a recorder. That keeps the rules testable without pulling the
 *        hardware in.
 */

#ifndef SOUND_OUTPUT_HPP
#define SOUND_OUTPUT_HPP

#include <SoundServiceBase.hpp>

namespace xbot::driver::sound {

class SoundOutput {
 public:
  virtual ~SoundOutput() = default;

  /** @brief Stop the current playback and clear the player queue. */
  virtual void StopPlayback() = 0;

  /** @brief Push one sound request into the player queue (call order = play order). */
  virtual void PlaySound(SoundId id) = 0;
};

}  // namespace xbot::driver::sound

#endif  // SOUND_OUTPUT_HPP
