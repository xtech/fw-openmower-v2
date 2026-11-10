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
      /* A tone has no envelope of its own (its union slot has no room); reset it so a
         tone played after a sequence does not inherit the sequence's decay. */
      synth.set_envelope(0U, 0U);
      synth.start_tone(def.tone.freq, def.tone.duration_ms, def.waveform);
      type = SoundType::TONE;
      volume = def.volume;
      active = true;
      return true;

    case SoundType::SEQUENCE:
      synth.set_unison(def.unison, def.detune_hz);
      synth.set_envelope(def.sequence.attack_ms, def.sequence.decay_ms);
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
  const size_t frames = count / 2U;  // one frame = [L, R=0]
  // Combine the per-definition volume with the master volume into a single 0–100 scale
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
  /* Decode in small blocks instead of one sample per call: Mp3Decoder::read() serves a
     whole frame at a time anyway, so this turns ~16k calls/s into ~250. The block lives
     on the player thread's stack (64 x 2 B), not on the audio buffer. */
  static constexpr size_t kBlock = 64U;
  int16_t pcm[kBlock];
  size_t done = 0U;

  while (done < frames && active) {
    const size_t want = ((frames - done) < kBlock) ? (frames - done) : kBlock;
    const size_t got = mp3.read(pcm, want);
    const size_t n = (got < want) ? got : want;
    for (size_t i = 0U; i < n; ++i) {
      buf[2U * (done + i)] = scale_volume(pcm[i], vol);
      buf[2U * (done + i) + 1U] = 0;
    }
    done += n;
    if (got < want) {
      /* EOF or read error — stop; the rest of the half-buffer becomes silence. */
      mp3.close();
      active = false;
    }
  }

  for (size_t i = done; i < frames; ++i) {
    buf[2U * i] = 0;
    buf[2U * i + 1U] = 0;
  }
}

}  // namespace xbot::driver::sound
