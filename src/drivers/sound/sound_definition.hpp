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
 *        a simple tone, a note sequence, or a file (WAV now, a compressed
 *        format later).  The struct is flat and pointer-free so the very same
 *        type can be used both as a constexpr ROM default and as a raw blob
 *        stored in flash (LittleFS) — flash overrides take precedence over the
 *        ROM defaults.
 *
 *        ROM defaults are tone/sequence only; WAV lives in flash overrides
 *        (uploaded by the high-level system or a future Sound-CLI).
 */

#ifndef SOUND_DEFINITION_HPP
#define SOUND_DEFINITION_HPP

#include <cstddef>
#include <cstdint>

#include "sound_ids.hpp"

namespace xbot::driver::sound {

/** @brief How a sound is produced. */
enum class SoundType : uint8_t { TONE, SEQUENCE, FILE };

/** @brief Maximum notes per sequence definition (fixed for serialisation). */
constexpr uint8_t kMaxNotes = 8U;
/** @brief Maximum path length of a FILE definition. */
constexpr size_t kMaxPath = 64U;

/** @brief A single note in a sequence.  POD (8 bytes) — serialisable. */
struct Note {
  uint16_t freq;         ///< Fundamental frequency in Hz; 0 = silence/pause
  uint16_t duration_ms;  ///< Note duration in milliseconds
  uint16_t lfo_hz_x10;   ///< LFO rate × 10  (e.g. 20 = 2.0 Hz); 0 = disabled
  uint16_t lfo_depth;    ///< Frequency deviation in Hz (ignored when lfo_hz_x10 == 0)
};

/**
 * @brief A complete, self-contained sound definition.
 *
 * A union keeps the struct at ~68 bytes — identical to the previous queue
 * entry — which matters because AXI SRAM (ram0) is fully occupied.  The ROM
 * table below uses C++20 designated initialisers (available as a GNU extension
 * in this project's gnu++17 build).
 */
struct SoundDefinition {
  SoundType type;
  uint8_t volume;  ///< Per-definition volume (0–100); master volume applies on top
  union {
    struct {
      uint32_t freq;
      uint32_t duration_ms;
    } tone;
    struct {
      Note notes[kMaxNotes];
      uint8_t count;
    } sequence;
    char path[kMaxPath];  ///< FILE (WAV now, compressed later)
  };
};

/*---------------------------------------------------------------------------
 * ROM defaults — tone/sequence only, indexed by SoundId.
 *---------------------------------------------------------------------------*/

inline constexpr SoundDefinition kDefaultSoundDefs[] = {
    /* BOOT           */ {SoundType::SEQUENCE, 70, .sequence = {{{440, 200, 0, 0}, {660, 350, 0, 0}}, 2}},
    /* SUCCESS        */
    {SoundType::SEQUENCE, 75, .sequence = {{{523, 90, 0, 0}, {659, 90, 0, 0}, {784, 250, 0, 0}}, 3}},
    /* WARNING        */
    {SoundType::SEQUENCE, 80, .sequence = {{{880, 150, 0, 0}, {0, 50, 0, 0}, {880, 150, 0, 0}}, 3}},
    /* ERROR          */
    {SoundType::SEQUENCE, 85, .sequence = {{{466, 140, 0, 0}, {349, 140, 0, 0}, {220, 320, 0, 0}}, 3}},
    /* EMERGENCY      */ {SoundType::SEQUENCE, 90, .sequence = {{{950, 8000, 20, 220}}, 1}},
    /* LOW_BATTERY    */
    {SoundType::SEQUENCE, 75,
     .sequence = {{{523, 250, 0, 0}, {0, 100, 0, 0}, {392, 250, 0, 0}, {0, 100, 0, 0}, {261, 450, 0, 0}}, 5}},
    /* CHARGING_START */
    {SoundType::SEQUENCE, 65, .sequence = {{{440, 80, 0, 0}, {554, 80, 0, 0}, {659, 80, 0, 0}}, 3}},
    /* CHARGING_DONE  */
    {SoundType::SEQUENCE, 70, .sequence = {{{392, 90, 0, 0}, {523, 90, 0, 0}, {659, 90, 0, 0}, {784, 300, 0, 0}}, 4}},
};

static_assert(sizeof(kDefaultSoundDefs) / sizeof(kDefaultSoundDefs[0]) == static_cast<size_t>(SoundId::COUNT),
              "kDefaultSoundDefs must have one entry per SoundId");

}  // namespace xbot::driver::sound

#endif  // SOUND_DEFINITION_HPP
