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
 * @brief Sine-wave tone/sequence synthesis (oscillator + LFO + envelope + note sequencer).
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-23
 *
 * @note  Renders TONE and SEQUENCE SoundDefinitions into int16 samples using a
 *        fixed-point phase-accumulator oscillator and an optional LFO.  A 64-entry
 *        sine table keeps it in flash — no math library, no float.
 */

#ifndef SOUND_SYNTH_HPP
#define SOUND_SYNTH_HPP

#include <SoundServiceBase.hpp>
#include <cstddef>
#include <cstdint>

namespace xbot::driver::sound {

/** @brief Audio sample rate shared by synthesis, WAV decoding and I2S. */
constexpr uint32_t SAMPLE_RATE = 16000U;

/** @brief Maximum notes per sequence (fixed for serialization). */
constexpr uint8_t kMaxNotes = 8U;

/** @brief A single note in a sequence.  POD (8 bytes) — serializable. */
struct Note {
  uint16_t freq;         ///< Fundamental frequency in Hz; 0 = silence/pause
  uint16_t duration_ms;  ///< Note duration in milliseconds
  uint16_t lfo_hz_x10;   ///< LFO rate × 10  (e.g. 20 = 2.0 Hz); 0 = disabled
  uint16_t lfo_depth;    ///< Frequency deviation in Hz (ignored when lfo_hz_x10 == 0)
};

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
  int32_t lfo_depth_inc = 0;           ///< Modulation depth in phase_inc units (pre-calculated)
  Waveform waveform = Waveform::SINE;  ///< Oscillator waveform
  uint8_t unison = 0U;                 ///< Number of detuned voices (1 = single, 3/5/7 = spread)
  uint32_t detune_inc = 0U;            ///< Phase-increment offset between adjacent voices
  uint32_t detune_phase = 0U;          ///< Running detune phase accumulator
  /* Amplitude envelope — per note, all zero while it is disabled */
  uint8_t env_attack_ms = 0U;     ///< Linear fade-in at the note start in ms (0 = instant onset)
  uint8_t env_decay_ms = 0U;      ///< Exponential fade down to about -60 dB in ms (0 = hold the note)
  uint32_t env_gain = 65536U;     ///< Q16 gain applied to the oscillator output (65536 = unity)
  uint32_t env_attack_inc = 0U;   ///< Q16 gain increment per sample during the attack
  uint32_t env_attack_left = 0U;  ///< Samples left in the attack ramp
  uint32_t env_decay_mult = 0U;   ///< Q16 per-sample multiplier of the exponential fade

  /** @brief Stack @p voices detuned oscillators (odd: 1/3/5/7), spread by @p detune_hz. */
  void set_unison(uint8_t voices, uint32_t detune_hz);

  /**
   * @brief Configure the per-note amplitude envelope.
   *
   * Without an envelope a note is a hard-gated rectangle, which is what the
   * alert/announcement sounds want.  A decay turns a note into a "ping" that
   * fades away instead of being cut off.
   *
   * @param attack_ms  Linear ramp from silence at the note start (0 = instant onset).
   * @param decay_ms   Exponential fade to about -60 dB, starting after the attack
   *                   (0 = hold the note at full level until its duration is over).
   *
   * @note  Both are limited to 255 ms (the definition stores them in a byte): a
   *        longer fade needs several notes or an MP3.
   */
  void set_envelope(uint8_t attack_ms, uint8_t decay_ms);

  /**
   * @brief Start the envelope for the note fill() is about to play.
   *
   * Called by fill() whenever it loads a note, so every note of a sequence gets
   * its own attack/decay ("plucked" instead of one envelope over the whole
   * sequence).
   */
  void arm_envelope();

  /** @brief Configure a fixed-frequency tone. */
  void start_tone(uint16_t freq, uint16_t duration_ms, Waveform wf);
  /** @brief Configure a note sequence (first note is loaded on first fill). */
  void start_sequence(const Note* notes, uint8_t count, Waveform wf);

  /**
   * @brief Fill @p frames stereo [L,R] frames (R always 0).
   * @return false once the tone/sequence is exhausted (rest of the buffer is silence).
   */
  bool fill(int16_t* buf, size_t frames, uint8_t volume);
};

}  // namespace xbot::driver::sound

#endif  // SOUND_SYNTH_HPP
