/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_source.hpp
 * @brief Active playback source: dispatches between synthesis and WAV streaming.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-23
 *
 * @note  A SoundSource owns the state of whatever is currently playing — either
 *        a Synth (TONE/SEQUENCE) or a WAV File.  It is a plain value type (no
 *        virtual dispatch, no heap) so adding a format means extending
 *        SoundType and start()/fill(), nothing more.
 */

#ifndef SOUND_SOURCE_HPP
#define SOUND_SOURCE_HPP

#include <cstdint>

#include "filesystem/file.hpp"
#include "sound_definition.hpp"
#include "sound_synth.hpp"

namespace xbot::driver::sound {

struct SoundSource {
  SoundType type = SoundType::TONE;
  bool active = false;
  Synth synth;
  File wav_file;
  uint32_t samples_left = 0U;  ///< WAV: samples remaining in the file
  uint8_t volume = 80U;        ///< Per-definition volume (0–100)

  bool is_active() const {
    return active;
  }

  /** @brief Release resources (close WAV file) and mark the source inactive. */
  void stop() {
    if (active && type == SoundType::FILE) {
      wav_file.close();
    }
    active = false;
  }

  /** @brief Start playback from a @p SoundDefinition. @return false on error (source left inactive). */
  bool start(const SoundDefinition& def);

  /**
   * @brief Fill @p count int16_t slots (L+R stereo pairs) from this source.
   *
   * Right channel is always written as 0 (MAX98357A is left-channel only).
   * When the source is exhausted mid-half the remainder is filled with silence.
   *
   * @param master_volume Master volume (0–100) combined with the per-definition volume.
   */
  void fill(int16_t* buf, size_t count, uint8_t master_volume);

 private:
  void fill_wav(int16_t* buf, size_t frames, uint8_t vol);
};

}  // namespace xbot::driver::sound

#endif  // SOUND_SOURCE_HPP
