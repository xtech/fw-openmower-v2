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
 * @brief Logical sound identifiers (one per robot event).
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-23
 */

#ifndef SOUND_IDS_HPP
#define SOUND_IDS_HPP

#include <cstdint>

namespace xbot::driver::sound {

/** @brief Logical sound identifiers (one per robot event). */
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

}  // namespace xbot::driver::sound

#endif  // SOUND_IDS_HPP
