/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_config.hpp
 * @brief Parser for the high-level sound definitions blob.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-11-07
 *
 * @note  The blob is a heatshrink-compressed JSON stream. The parser applies every
 *        definition to the runtime override table, stages the take lists and fills the
 *        ambient-policy table. A rejected blob leaves the previous take lists in place
 *        and reports false; the service then keeps the previous definitions.
 */

#ifndef SOUND_CONFIG_HPP
#define SOUND_CONFIG_HPP

#include <cstddef>
#include <drivers/sound/sound_types.hpp>

namespace xbot::driver::sound {

class SoundConfig {
 public:
  /**
   * @brief Parse and apply one definitions blob.
   * @param data   Heatshrink-compressed JSON bytes.
   * @param length Number of bytes.
   * @return true when the blob was accepted and applied.
   */
  static bool Parse(const void* data, size_t length);

  /** @brief every/chance of one sound as configured (the machine's policy hook). */
  static AmbientPolicy PolicyFor(SoundId id);
};

}  // namespace xbot::driver::sound

#endif  // SOUND_CONFIG_HPP
