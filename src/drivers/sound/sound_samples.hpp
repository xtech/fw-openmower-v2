/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_samples.hpp
 * @brief Audio sample generation utilities for STM32H723
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-19
 */
#ifndef SOUND_SAMPLES_HPP
#define SOUND_SAMPLES_HPP

#include <cstdint>

namespace xbot::driver::sound::samples {

/**
 * @brief 64-entry sine lookup table (one full period)
 *
 * Values range from -32767 to +32767 (16-bit signed)
 * One full sine wave period with 64 samples.
 */
static constexpr int16_t sine_table_64[64] = {
    0,      3212,   6393,   9512,   12540,  15446,  18205,  20787,  23170,  25330,  27246,  28899,  30273,
    31357,  32138,  32610,  32767,  32610,  32138,  31357,  30273,  28899,  27246,  25330,  23170,  20787,
    18205,  15446,  12540,  9512,   6393,   3212,   0,      -3212,  -6393,  -9512,  -12540, -15446, -18205,
    -20787, -23170, -25330, -27246, -28899, -30273, -31357, -32138, -32610, -32767, -32610, -32138, -31357,
    -30273, -28899, -27246, -25330, -23170, -20787, -18205, -15446, -12540, -9512,  -6393,  -3212};

/**
 * @brief Generate sine wave sample using fixed-point phase accumulator
 *
 * Uses a 64-entry sine lookup table with 16.16 fixed-point phase accumulator
 * for smooth frequency generation at any sample rate.
 *
 * @param phase 16.16 fixed-point phase (upper 6 bits after fractional part = table index)
 * @return Sine wave sample value (-32767 to +32767)
 */
inline int16_t generate_sine_sample_fp(uint32_t phase) {
  /* Extract table index from upper 6 bits of phase (after the 16-bit fractional part) */
  /* Phase is 16.16 fixed point, we want bits 16-21 for the 64-entry table index */
  uint32_t phase_index = (phase >> 16) & 0x3F;
  return sine_table_64[phase_index];
}

/**
 * @brief Calculate phase increment for a given frequency and sample rate
 *
 * Computes the 16.16 fixed-point phase increment needed to generate
 * a sine wave of specified frequency at given sample rate.
 *
 * @param frequency Desired frequency in Hz
 * @param sample_rate Sample rate in Hz
 * @return 16.16 fixed-point phase increment
 */
inline uint32_t calculate_phase_increment(uint32_t frequency, uint32_t sample_rate) {
  /* phase_increment = (frequency * 64 * 65536) / sample_rate */
  /* 64 = sine table size, 65536 = 16-bit fractional part */
  return ((uint64_t)frequency * 64 * 65536) / sample_rate;
}

/**
 * @brief Generate a square wave sample
 *
 * Simple square wave generator for testing purposes.
 *
 * @param phase Current phase (0 to period-1)
 * @param period Total period in samples
 * @param amplitude Amplitude (0-32767)
 * @return Square wave sample
 */
inline int16_t generate_square_sample(uint32_t phase, uint32_t period, int16_t amplitude) {
  /* Square wave: high for first half of period, low for second half */
  return (phase < (period / 2)) ? amplitude : -amplitude;
}

/**
 * @brief Apply volume scaling to a sample
 *
 * Simple volume scaling with clamping to 16-bit range.
 *
 * @param sample Input sample (-32768 to 32767)
 * @param volume Volume percentage (0-100)
 * @return Volume-scaled sample
 */
inline int16_t apply_volume(int16_t sample, uint8_t volume) {
  /* Simple volume scaling: sample * volume / 100 */
  int32_t scaled = (int32_t)sample * volume / 100;

  /* Clamp to 16-bit range */
  if (scaled > 32767) scaled = 32767;
  if (scaled < -32768) scaled = -32768;

  return (int16_t)scaled;
}

}  // namespace xbot::driver::sound::samples

#endif
