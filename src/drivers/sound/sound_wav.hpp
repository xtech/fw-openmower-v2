/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_wav.hpp
 * @brief Minimal RIFF/PCM WAV header parser for the sound player.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-23
 *
 * @note  Accepts only canonical 44-byte RIFF headers: PCM (format=1),
 *        mono, 16-bit, 16 kHz.  Rejects compressed formats, non-canonical
 *        chunk layouts, and any other sample rate or bit depth.
 *        Header-only — safe to include from multiple translation units.
 */

#ifndef SOUND_WAV_HPP
#define SOUND_WAV_HPP

#include <cstdint>
#include <cstring>

#include "filesystem/file.hpp"

namespace xbot::driver::sound {

/** @brief Result of a successful WAV header parse. */
struct WavInfo {
  uint32_t data_offset;  ///< Byte offset of the first PCM sample in the file (always 44 for canonical WAVs)
  uint32_t num_samples;  ///< Number of mono int16_t samples in the data chunk
};

/**
 * @brief Parse a canonical 44-byte RIFF/PCM WAV header.
 *
 * The file must be positioned at byte 0 on entry.
 * On success the file position is at @p info.data_offset (first PCM byte).
 *
 * @param[in]  file  Open File positioned at byte 0.
 * @param[out] info  Filled on success with data offset and sample count.
 * @return true if the file is a valid 16 kHz / 16-bit / mono PCM WAV;
 *         false otherwise (file position is undefined on failure).
 */
inline bool wav_parse_header(File& file, WavInfo& info) {
  uint8_t hdr[44];
  if (file.read(hdr, sizeof(hdr)) != static_cast<int>(sizeof(hdr))) {
    return false;
  }

  /* "RIFF" */
  if (hdr[0] != 'R' || hdr[1] != 'I' || hdr[2] != 'F' || hdr[3] != 'F') return false;
  /* "WAVE" */
  if (hdr[8] != 'W' || hdr[9] != 'A' || hdr[10] != 'V' || hdr[11] != 'E') return false;
  /* "fmt " */
  if (hdr[12] != 'f' || hdr[13] != 'm' || hdr[14] != 't' || hdr[15] != ' ') return false;

  /* fmt chunk size must be exactly 16 (PCM, no extension) */
  uint32_t fmt_size;
  memcpy(&fmt_size, hdr + 16, sizeof(fmt_size));
  if (fmt_size != 16U) return false;

  /* Audio format = 1 (linear PCM) */
  uint16_t audio_format;
  memcpy(&audio_format, hdr + 20, sizeof(audio_format));
  if (audio_format != 1U) return false;

  /* Channels = 1 (mono) */
  uint16_t channels;
  memcpy(&channels, hdr + 22, sizeof(channels));
  if (channels != 1U) return false;

  /* Sample rate = 16000 Hz */
  uint32_t sample_rate;
  memcpy(&sample_rate, hdr + 24, sizeof(sample_rate));
  if (sample_rate != 16000U) return false;

  /* Bits per sample = 16 */
  uint16_t bits_per_sample;
  memcpy(&bits_per_sample, hdr + 34, sizeof(bits_per_sample));
  if (bits_per_sample != 16U) return false;

  /* "data" sub-chunk marker */
  if (hdr[36] != 'd' || hdr[37] != 'a' || hdr[38] != 't' || hdr[39] != 'a') return false;

  /* Data chunk byte size */
  uint32_t data_size;
  memcpy(&data_size, hdr + 40, sizeof(data_size));

  info.data_offset = 44U;
  info.num_samples = data_size / 2U; /* 2 bytes per int16_t sample */
  return true;
}

}  // namespace xbot::driver::sound

#endif  // SOUND_WAV_HPP
