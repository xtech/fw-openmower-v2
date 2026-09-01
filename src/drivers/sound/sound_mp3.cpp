/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_mp3.cpp
 * @brief Streaming MP3 decoder implementation (minimp3, no resampling).
 */

#include "sound_mp3.hpp"

#include <ulog.h>

#include <cstring>

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

namespace xbot::driver::sound {

bool Mp3Decoder::open(const char* path) {
  close();
  if (file.open(path, LFS_O_RDONLY) != LFS_ERR_OK) {
    ULOG_WARNING("Sound: cannot open MP3 '%s'", path);
    return false;
  }
  mp3dec_init(&mp3d);
  return true;
}

void Mp3Decoder::close() {
  file.close();
  input_size = 0;
  input_pos = 0;
  pcm_count = 0;
  pcm_pos = 0;
  eof = false;
  validated = false;
}

bool Mp3Decoder::refill() {
  // Compact un-consumed bytes to the front of the buffer.
  const size_t remaining = input_size - input_pos;
  if (remaining > 0 && input_pos > 0) {
    memmove(input, input + input_pos, remaining);
  }
  input_size = remaining;
  input_pos = 0;

  if (eof) return input_size > 0;

  const int r = file.read(input + input_size, kMp3InputSize - input_size);
  if (r < 0) {
    eof = true;
    return input_size > 0;
  }
  input_size += static_cast<size_t>(r);
  if (r == 0) eof = true;
  return input_size > 0;
}

size_t Mp3Decoder::read(int16_t* out, size_t count) {
  size_t produced = 0;

  while (produced < count) {
    // Serve already-decoded samples first.
    if (pcm_pos < pcm_count) {
      size_t n = count - produced;
      if (n > pcm_count - pcm_pos) n = pcm_count - pcm_pos;
      memcpy(out + produced, pcm + pcm_pos, n * sizeof(int16_t));
      pcm_pos += n;
      produced += n;
      continue;
    }

    // Decode the next frame.
    for (;;) {
      // Keep at least one full frame buffered so a frame header can never be
      // split across the read-ahead boundary (a split header would otherwise
      // be discarded by mp3d_find_frame and lose a whole frame).
      if (input_size - input_pos < kMp3MinBuffered) {
        refill();
      }

      if (input_pos >= input_size) {
        return produced;  // EOF / read error
      }

      mp3dec_frame_info_t info{};
      const int samples = mp3dec_decode_frame(&mp3d, input + input_pos, input_size - input_pos, pcm, &info);
      input_pos += static_cast<size_t>(info.frame_bytes);

      if (samples > 0) {
        if (!validated) {
          validated = true;
          if (info.hz != 16000 || info.channels != 1) {
            ULOG_WARNING("Sound: MP3 is %d Hz / %d ch (expected 16 kHz mono)", info.hz, info.channels);
            return produced;
          }
        }
        pcm_count = static_cast<size_t>(samples) * static_cast<size_t>(info.channels);
        pcm_pos = 0;
        break;
      }

      // samples == 0: need more data (resync or EOF). Refill and retry.
      refill();
    }
  }

  return produced;
}

}  // namespace xbot::driver::sound
