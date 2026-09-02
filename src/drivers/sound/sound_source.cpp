/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sound_source.hpp"

#include <ulog.h>

#include <cstring>

namespace xbot::driver::sound {

bool SoundSource::start(const SoundDefinition& def) {
  stop(); /* ensure inactive and file closed */

  switch (def.type) {
    case SoundType::TONE:
      synth.set_unison(def.unison, def.detune_hz);
      synth.start_tone(def.tone.freq, def.tone.duration_ms, def.waveform);
      type = SoundType::TONE;
      volume = def.volume;
      active = true;
      return true;

    case SoundType::SEQUENCE:
      synth.set_unison(def.unison, def.detune_hz);
      synth.start_sequence(def.sequence.notes, def.sequence.count, def.waveform);
      type = SoundType::SEQUENCE;
      volume = def.volume;
      active = true;
      return true;

    case SoundType::MP3:
      if (!mp3.open(def.path)) {
        ULOG_WARNING("Sound: cannot open MP3 '%s'", def.path);
        return false;
      }
      type = SoundType::MP3;
      volume = def.volume;
      active = true;
      return true;

    default: return false;
  }
}

void SoundSource::fill(int16_t* buf, size_t count, uint8_t master_volume) {
  const size_t frames = count / 2U; /* one frame = [L, R=0] */
  /* Combine the per-definition volume with the master volume into a single
     0–100 scale so only one division is needed per sample. */
  const uint8_t vol = static_cast<uint8_t>((static_cast<uint32_t>(volume) * master_volume) / 100U);

  if (!active) {
    memset(buf, 0, count * sizeof(int16_t));
    return;
  }

  switch (type) {
    case SoundType::TONE:
    case SoundType::SEQUENCE:
      if (!synth.fill(buf, frames, vol)) {
        active = false; /* synth exhausted */
      }
      break;
    case SoundType::MP3: fill_mp3(buf, frames, vol); break;
    default: break;
  }
}

void SoundSource::fill_mp3(int16_t* buf, size_t frames, uint8_t vol) {
  for (size_t i = 0U; i < frames; ++i) {
    int16_t s = 0;
    if (active) {
      int16_t raw = 0;
      if (mp3.read(&raw, 1U) == 1U) {
        s = scale_volume(raw, vol);
      } else {
        /* EOF or read error — silence and stop */
        mp3.close();
        active = false;
      }
    }
    buf[2U * i] = s;
    buf[2U * i + 1] = 0;
  }
}

}  // namespace xbot::driver::sound
