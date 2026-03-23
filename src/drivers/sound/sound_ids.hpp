/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_ids.hpp
 * @brief Sound identifiers and fallback tone table for the sound player.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-23
 *
 * @note  The WAV file for a given SoundId lives at /sounds/<id>.wav on LittleFS.
 *        If absent, the player synthesises the corresponding entry from kFallbackTones[].
 *        The fallback table lives entirely in .rodata — zero RAM, ~100 bytes flash.
 */

#ifndef SOUND_IDS_HPP
#define SOUND_IDS_HPP

#include <cstdint>

namespace xbot::driver::sound {

/** @brief Logical sound identifiers. */
enum class SoundId : uint8_t {
  BOOT,            ///< Boot sequence complete
  SUCCESS,         ///< Operation succeeded
  WARNING,         ///< Non-critical warning
  ERROR,           ///< Recoverable error
  EMERGENCY,       ///< Emergency stop triggered
  LOW_BATTERY,     ///< Battery low
  CHARGING_START,  ///< Charging begun
  CHARGING_DONE,   ///< Charging complete
  COUNT            ///< Sentinel — number of defined sound IDs
};

/** @brief Fallback tone parameters used when the WAV file is absent from flash. */
struct SoundFallback {
  uint32_t freq;         ///< Frequency in Hz
  uint32_t duration_ms;  ///< Duration in milliseconds
  uint8_t volume;        ///< Volume (0–100)
};

/**
 * @brief Fallback tone table indexed by SoundId.
 *
 * Entries must appear in the same order as SoundId enumerators.
 * Lives entirely in .rodata (constexpr) — no RAM cost.
 */
inline constexpr SoundFallback kFallbackTones[] = {
    /* BOOT           */ {440, 300, 70},
    /* SUCCESS        */ {600, 200, 75},
    /* WARNING        */ {800, 150, 80},
    /* ERROR          */ {1000, 200, 85},
    /* EMERGENCY      */ {1200, 100, 90},
    /* LOW_BATTERY    */ {500, 400, 75},
    /* CHARGING_START */ {440, 150, 65},
    /* CHARGING_DONE  */ {600, 300, 70},
};

static_assert(sizeof(kFallbackTones) / sizeof(kFallbackTones[0]) == static_cast<uint8_t>(SoundId::COUNT),
              "kFallbackTones must have one entry per SoundId");

}  // namespace xbot::driver::sound

#endif  // SOUND_IDS_HPP
