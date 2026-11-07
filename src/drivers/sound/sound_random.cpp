/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sound_random.hpp"

/* hal.h must come first: hal_trng.h uses ChibiOS types (msg_t, size_t, RNG_TypeDef). */
#include <hal.h>
#include <hal_trng.h>
#include <ulog.h>

namespace xbot::driver::sound {

namespace {

/** xorshift32 must never be 0; this is also the fallback when the TRNG fails. */
uint32_t s_random_state = 0x2545F491U;

/** Serialises the state: take selection (player thread) and dice (service thread) share it. */
MUTEX_DECL(s_random_mutex);

uint32_t random_next_locked() {
  uint32_t x = s_random_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s_random_state = x;
  return x;
}

}  // namespace

void random_seed() {
  uint32_t word = 0U;
  if (trngStart(&TRNGD1, nullptr) == HAL_RET_SUCCESS) {
    /* ChibiOS' TRNG LLD reports a timeout as true and success as false. */
    const bool timed_out = trngGenerate(&TRNGD1, sizeof(word), reinterpret_cast<uint8_t*>(&word));
    trngStop(&TRNGD1); /* one word at boot is all the take selection needs */
    if (!timed_out && word != 0U) {
      chMtxLock(&s_random_mutex);
      s_random_state = word;
      chMtxUnlock(&s_random_mutex);
      return;
    }
  }

  /* No word from the TRNG: keep the default state. The selection still varies per
     request, it just repeats the same sequence after the next boot. */
  ULOG_WARNING("Sound: no TRNG word at boot, the take selection sequence repeats itself");
}

bool random_chance(uint8_t chance_percent) {
  if (chance_percent == 0U) return false;
  if (chance_percent >= 100U) return true;

  chMtxLock(&s_random_mutex);
  const uint32_t roll = random_next_locked() % 100U;
  chMtxUnlock(&s_random_mutex);
  return roll < chance_percent;
}

uint32_t random_below(uint32_t bound) {
  if (bound == 0U) return 0U;
  chMtxLock(&s_random_mutex);
  const uint32_t value = random_next_locked() % bound;
  chMtxUnlock(&s_random_mutex);
  return value;
}

}  // namespace xbot::driver::sound
