/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sound_random.hpp
 * @brief The player's small PRNG for take selection and the ambience dice.
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-11-07
 *
 * @note  rand() from newlib is a linear congruential generator, and its seed is a boot
 *        tick that is nearly the same on every boot. A xorshift32 mixes the bits a
 *        "% count" uses and needs no library call. The state is seeded once at boot from
 *        the MCU's true random generator and advances on every request.
 */

#ifndef SOUND_RANDOM_HPP
#define SOUND_RANDOM_HPP

#include <cstdint>

namespace xbot::driver::sound {

/** @brief Seed the PRNG from the MCU TRNG (keeps the built-in state when it fails). */
void random_seed();

/** @brief True with a probability of @p chance_percent (0..100). Thread safe. */
bool random_chance(uint8_t chance_percent);

/**
 * @brief A number in [0, @p bound); 0 when @p bound is 0.
 *
 * Thread safe, so the take selection and the state machine's dice can share it.
 */
uint32_t random_below(uint32_t bound);

}  // namespace xbot::driver::sound

#endif  // SOUND_RANDOM_HPP
