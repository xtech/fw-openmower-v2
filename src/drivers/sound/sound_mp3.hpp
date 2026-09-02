/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_mp3.hpp
 * @brief Streaming MP3 decoder (minimp3) for 16 kHz mono sound files.
 *
 * @note  Wraps minimp3's low-level decode API and feeds it from a LittleFS File.
 *        The firmware does NOT resample: files must already be 16 kHz mono
 *        (verified by the high-level upload path — see sound_definition.hpp).
 *
 *        minimp3 needs a ~13 KB scratch buffer on the stack during
 *        mp3dec_decode_frame(), so this decoder must be driven by a thread with
 *        a large enough working area (the sound player thread uses 16 KB).
 */

#ifndef SOUND_MP3_HPP
#define SOUND_MP3_HPP

#include <cstddef>
#include <cstdint>

#include "filesystem/file.hpp"
#include "minimp3.h"

namespace xbot::driver::sound {

/** Read-ahead buffer size for the MP3 bitstream. */
constexpr size_t kMp3InputSize = 8192U;
/** Keep at least this many bytes buffered so a frame header is never split. */
constexpr size_t kMp3MinBuffered = 4096U;

/**
 * @brief Streaming 16 kHz mono MP3 decoder.
 *
 * Plain value type (no heap). Decodes frames on demand and returns int16 mono
 * samples. The `mp3dec_t` state (~6.5 KB), read-ahead buffer and one-frame PCM
 * buffer (~4.6 KB) are all members, so instances are large — allocate
 * statically (the SoundSource owning this lives as a static global).
 */
struct Mp3Decoder {
  mp3dec_t mp3d{};
  File file;
  uint8_t input[kMp3InputSize]{};
  mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME]{};

  size_t input_size = 0;  ///< valid bytes in input[]
  size_t input_pos = 0;   ///< consumed bytes in input[]
  size_t pcm_count = 0;   ///< decoded samples in pcm[]
  size_t pcm_pos = 0;     ///< consumed samples in pcm[]
  bool eof = false;
  bool validated = false;  ///< first frame's rate/channels checked

  /** @brief Open @p path and reset decoder state. @return false on open error. */
  bool open(const char* path);

  /** @brief Close the file and reset streaming state. */
  void close();

  /**
   * @brief Decode up to @p count mono int16 samples into @p out.
   * @return number of samples written (0 = EOF or error).
   */
  size_t read(int16_t* out, size_t count);

 private:
  /** @brief Compact + refill the read-ahead buffer from the file. */
  bool refill();
};

}  // namespace xbot::driver::sound

#endif  // SOUND_MP3_HPP
