/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_definition.hpp
 * @brief Serialisable sound definitions and the ROM default table.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-23
 *
 * @note  A SoundDefinition describes what a given robot event sounds like:
 *        a simple tone, a note sequence, or a 16 kHz mono MP3 file.  The
 *        struct is flat and pointer-free so the very same
 *        type can be used both as a constexpr ROM default and as a raw blob
 *        stored in flash (LittleFS) — flash overrides take precedence over the
 *        ROM defaults.
 *
 *        ROM defaults are tone/sequence only; MP3 files live in flash overrides
 *        (uploaded by the high-level system or a future Sound-CLI).
 *
 *        TODO(HL): MP3 sounds must be 16 kHz mono. The high-level upload
 *        path must verify (and downsample if needed) before uploading — the
 *        firmware decodes at 16 kHz only and does NOT resample.
 */

#ifndef SOUND_DEFINITION_HPP
#define SOUND_DEFINITION_HPP

#include <SoundServiceBase.hpp>
#include <cstddef>
#include <cstdint>

#include "sound_synth.hpp"

namespace xbot::driver::sound {

/**
 * @brief Maximum length of the MP3 path stored in a FILE definition.
 *
 * The firmware prefixes "/sounds/", so this must also hold that prefix,
 * the file name and the terminating NUL:
 */
constexpr size_t kMaxPath = 64U;

/**
 * @brief A complete, self-contained sound definition.
 */
struct SoundDefinition {
  SoundType type;
  uint8_t volume;                      ///< Per-definition volume (0–100); master volume applies on top
  Waveform waveform = Waveform::SINE;  ///< Oscillator waveform (tone/sequence only)
  uint8_t unison = 1U;                 ///< Number of detuned voices (1 = single, odd: 3/5/7)
  uint16_t detune_hz = 0U;             ///< Frequency spread between unison voices in Hz
  bool preempt = false;                ///< Preemptive sounds drop the queue and play immediately (e.g. EMERGENCY)
  uint8_t reserved_ = 0U;              ///< Explicit padding: keeps the layout (and the persisted size) stable
  union alignas(4) {
    struct {
      uint16_t freq;
      uint16_t duration_ms;
    } tone;
    struct {
      Note notes[kMaxNotes];
      uint8_t count;
    } sequence;
    char path[kMaxPath];  ///< MP3 file path (16 kHz mono)
  };
};

/*---------------------------------------------------------------------------
 * ROM defaults — tone/sequence only, indexed by SoundId.
 *---------------------------------------------------------------------------*/

inline constexpr SoundDefinition kDefaultSoundDefs[] = {
    // BOOT_PING
    {SoundType::TONE, 40, .tone = {250, 60}},
    // BOOT_COMPLETE
    {SoundType::SEQUENCE, 85, .waveform = Waveform::TRIANGLE, .unison = 3, .detune_hz = 6,
     .sequence = {{{262, 90, 0, 0}, {330, 90, 0, 0}, {392, 90, 0, 0}, {523, 300, 0, 0}}, 4}},
    // SUCCESS
    {SoundType::SEQUENCE, 75, .waveform = Waveform::TRIANGLE,
     .sequence = {{{523, 120, 0, 0}, {659, 120, 0, 0}, {784, 300, 0, 0}}, 3}},
    // WARNING
    {SoundType::SEQUENCE, 80, .waveform = Waveform::SAW,
     .sequence = {{{880, 150, 0, 0}, {0, 80, 0, 0}, {880, 150, 0, 0}, {0, 80, 0, 0}}, 4}},
    // ERROR
    {SoundType::SEQUENCE, 85, .waveform = Waveform::SAW,
     .sequence = {{{300, 200, 0, 0}, {240, 200, 0, 0}, {180, 300, 0, 0}}, 3}},
    // EMERGENCY
    {SoundType::SEQUENCE, 90, .preempt = true, .sequence = {{{950, 8000, 20, 220}}, 1}},
    // LOW_BATTERY
    {SoundType::SEQUENCE, 75,
     .sequence = {{{659, 300, 0, 0}, {587, 300, 0, 0}, {523, 300, 0, 0}, {440, 400, 0, 0}}, 4}},
    // CHARGING_START
    {SoundType::SEQUENCE, 65, .waveform = Waveform::SINE,
     .sequence = {{{523, 100, 0, 0}, {659, 100, 0, 0}, {784, 150, 0, 0}}, 3}},
    // CHARGING_DONE  */
    {SoundType::SEQUENCE, 70, .waveform = Waveform::TRIANGLE, .unison = 3, .detune_hz = 6,
     .sequence = {{{523, 120, 0, 0}, {659, 120, 0, 0}, {784, 120, 0, 0}, {1046, 300, 0, 0}}, 4}},
    // GPS_RTK_FIX    */
    {SoundType::SEQUENCE, 80, .waveform = Waveform::SINE, .unison = 3, .detune_hz = 4,
     .sequence = {{{880, 120, 0, 0}, {1174, 180, 0, 0}}, 2}},
    // GPS_RTK_LOST   */
    {SoundType::SEQUENCE, 80, .waveform = Waveform::SAW, .sequence = {{{660, 120, 0, 0}, {440, 200, 0, 0}}, 2}},
};

static_assert(sizeof(kDefaultSoundDefs) / sizeof(kDefaultSoundDefs[0]) == SoundId_count,
              "kDefaultSoundDefs must have one entry per SoundId");

/* Two reasons to pin the size:
   1. it is part of the persisted format — /cfg/sound_defs.bin stores raw struct bytes
      (kSoundDefsVersion 1), so a layout/size change would make existing files
      unreadable and silently produce garbage definitions;
   2. the sound objects live in AXI SRAM (ram0), where the linker report already shows
      100 % — growth would eat into the heap that is reserved inside that region.
   preempt and reserved_ reuse padding, so the 76 bytes are unchanged since v1. */
static_assert(sizeof(SoundDefinition) == 76U, "SoundDefinition size changed — it is part of the flash format");

}  // namespace xbot::driver::sound

#endif  // SOUND_DEFINITION_HPP
