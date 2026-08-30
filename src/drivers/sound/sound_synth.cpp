/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sound_synth.hpp"

namespace xbot::driver::sound {

/** @brief 64-entry sine lookup table — one full period, values -32767…+32767. */
static constexpr int16_t kSineTable[64] = {
    0,      3212,   6393,   9512,   12540,  15446,  18205,  20787,  23170,  25330,  27246,  28899,  30273,
    31357,  32138,  32610,  32767,  32610,  32138,  31357,  30273,  28899,  27246,  25330,  23170,  20787,
    18205,  15446,  12540,  9512,   6393,   3212,   0,      -3212,  -6393,  -9512,  -12540, -15446, -18205,
    -20787, -23170, -25330, -27246, -28899, -30273, -31357, -32138, -32610, -32767, -32610, -32138, -31357,
    -30273, -28899, -27246, -25330, -23170, -20787, -18205, -15446, -12540, -9512,  -6393,  -3212,
};

/** @brief 16.16 fixed-point phase → sine sample. */
static inline int16_t sine_sample(uint32_t phase) {
  return kSineTable[(phase >> 16U) & 0x3FU];
}

/** @brief 16.16 fixed-point phase increment for @p freq at SAMPLE_RATE. */
static inline uint32_t calc_phase_increment(uint32_t freq) {
  return static_cast<uint32_t>((static_cast<uint64_t>(freq) * 64U * 65536U) / SAMPLE_RATE);
}

void Synth::start_tone(uint32_t freq, uint32_t duration_ms) {
  /* A tone is a single-note sequence without LFO.  Frequencies above 65535 Hz
     (beyond audible) and durations beyond ~65 s are unsupported. */
  notes[0] = {static_cast<uint16_t>(freq), static_cast<uint16_t>(duration_ms), 0, 0};
  count = 1U;
  idx = 0U;
  phase = 0U;
  phase_inc = 0U;
  samples_left = 0U;
  lfo_phase = 0U;
  lfo_inc = 0U;
  lfo_depth_inc = 0;
}

void Synth::start_sequence(const Note* src, uint8_t n) {
  count = (n <= kMaxNotes) ? n : kMaxNotes;
  for (uint8_t i = 0U; i < count; ++i) {
    notes[i] = src[i];
  }
  idx = 0U;
  phase = 0U;
  phase_inc = 0U;
  samples_left = 0U;
  lfo_phase = 0U;
  lfo_inc = 0U;
  lfo_depth_inc = 0;
}

bool Synth::fill(int16_t* buf, size_t frames, uint8_t volume) {
  bool exhausted = false;
  for (size_t i = 0U; i < frames; ++i) {
    /* Advance to the next note whenever the current one has been exhausted. */
    while (samples_left == 0U && idx < count) {
      const Note& n = notes[idx++];
      samples_left = (SAMPLE_RATE * n.duration_ms) / 1000U;
      phase = 0U;
      phase_inc = (n.freq > 0U) ? calc_phase_increment(n.freq) : 0U;
      if (n.lfo_hz_x10 > 0U && n.freq > 0U) {
        lfo_phase = 0U;
        /* LFO phase increment: same formula as calc_phase_increment but divided by 10
           to convert lfo_hz_x10 (rate × 10) back to Hz. */
        lfo_inc = static_cast<uint32_t>(static_cast<uint64_t>(n.lfo_hz_x10) * 64U * 65536U / (SAMPLE_RATE * 10U));
        /* Depth in phase_inc units — pre-calculated to avoid per-sample division. */
        lfo_depth_inc = static_cast<int32_t>(calc_phase_increment(n.freq + n.lfo_depth) - phase_inc);
      } else {
        lfo_inc = 0U;
        lfo_depth_inc = 0;
      }
    }

    int16_t s = 0;
    if (samples_left > 0U) {
      if (phase_inc > 0U) {
        uint32_t eff_inc = phase_inc;
        if (lfo_inc > 0U) {
          eff_inc += static_cast<uint32_t>((static_cast<int64_t>(lfo_depth_inc) * sine_sample(lfo_phase)) >> 15);
          lfo_phase += lfo_inc;
        }
        s = scale_volume(sine_sample(phase), volume);
        phase += eff_inc;
      }
      if (--samples_left == 0U && idx >= count) {
        exhausted = true; /* sequence finished */
      }
    }
    buf[2U * i] = s;
    buf[2U * i + 1] = 0;
  }
  return !exhausted;
}

}  // namespace xbot::driver::sound
