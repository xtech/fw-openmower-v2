/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_synth.hpp
 * @brief Sine-wave tone/sequence synthesis (oscillator + LFO + note sequencer).
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-23
 *
 * @note  Renders TONE and SEQUENCE SoundDefinitions into int16 samples using a
 *        fixed-point phase-accumulator oscillator and an optional LFO.  A 64-entry
 *        sine table keeps it in flash — no math library, no float.
 */

#ifndef SOUND_SYNTH_HPP
#define SOUND_SYNTH_HPP

#include <cstddef>
#include <cstdint>

#include "sound_definition.hpp"

namespace xbot::driver::sound {

/** @brief Audio sample rate shared by synthesis, WAV decoding and I2S. */
constexpr uint32_t SAMPLE_RATE = 16000U;

/**
 * @brief Scale @p sample by a 0–100 volume, clamped to int16 range.
 *
 * Shared between the synth and the WAV file source (sound_source.cpp).
 */
inline int16_t scale_volume(int16_t sample, uint8_t volume) {
  const int32_t v = static_cast<int32_t>(sample) * volume / 100;
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);
}

/**
 * @brief Algorithmic tone/sequence synthesis.
 *
 * Plain value type (no virtual dispatch, no heap).  Exactly one instance lives
 * inside SoundSource and is only driven by the player thread.  A tone is simply
 * a one-note sequence without LFO.
 */
struct Synth {
  uint32_t phase = 0U;
  uint32_t phase_inc = 0U;
  uint32_t samples_left = 0U;  ///< Samples remaining in the current note
  Note notes[kMaxNotes]{};
  uint8_t count = 0U;
  uint8_t idx = 0U;
  /* LFO (all zero when inactive) */
  uint32_t lfo_phase = 0U;
  uint32_t lfo_inc = 0U;
  int32_t lfo_depth_inc = 0;  ///< Modulation depth in phase_inc units (pre-calculated)

  /** @brief Configure a fixed-frequency tone. */
  void start_tone(uint32_t freq, uint32_t duration_ms);
  /** @brief Configure a note sequence (first note is loaded on first fill). */
  void start_sequence(const Note* notes, uint8_t count);

  /**
   * @brief Fill @p frames stereo [L,R] frames (R always 0).
   * @return false once the tone/sequence is exhausted (rest of the buffer is silence).
   */
  bool fill(int16_t* buf, size_t frames, uint8_t volume);
};

}  // namespace xbot::driver::sound

#endif  // SOUND_SYNTH_HPP
