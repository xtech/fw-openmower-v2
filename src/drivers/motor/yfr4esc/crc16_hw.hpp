// CRC16-CCITT-FALSE helper for YFR4 ESC (STM32H7 HW only)
// - Polynomial: 0x1021, Init: 0xFFFF, RefIn/RefOut: false, XorOut: 0x0000
// - Feeds bytes MSB-first semantics via CRC peripheral with REV_IN/REV_OUT disabled.
// - Thread-safety: guarded by a lightweight mutex (blocking). Not ISR-safe.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ch.h"
#include "hal.h"

namespace yfr4esc_crc {
// HW CRC compute; assumes STM32H7 CRC peripheral
static inline uint16_t crc16_ccitt_false_hw(const uint8_t* data, size_t len) {
  // Enable and configure CRC peripheral once
  static bool configured = false;
  if (!configured) {
    rccEnableCRC(false);
    // Configure for 16-bit CCITT-FALSE
    CRC->POL = 0x1021U;   // Polynomial
    CRC->INIT = 0xFFFFU;  // Init value
                          // Clear and set control: REV_IN=00, REV_OUT=0, POLYSIZE=01 (16-bit)
#ifdef CRC_CR_POLYSIZE
    CRC->CR &= ~(CRC_CR_REV_IN | CRC_CR_REV_OUT | CRC_CR_POLYSIZE);
    CRC->CR |= (0U << CRC_CR_REV_IN_Pos) | (0U << CRC_CR_REV_OUT_Pos) | (1U << CRC_CR_POLYSIZE_Pos);
#else
    // Fallback if POLYSIZE not defined (unlikely on H7)
    CRC->CR &= ~(CRC_CR_REV_IN | CRC_CR_REV_OUT);
#endif
    configured = true;
  }
  // Reset to INIT for each calculation
  CRC->CR |= CRC_CR_RESET;  // Load INIT

  // Feed bytes
  volatile uint8_t* dr8 = reinterpret_cast<volatile uint8_t*>(&CRC->DR);
  for (size_t i = 0; i < len; ++i) {
    *dr8 = data[i];
  }
  uint16_t result = (uint16_t)(CRC->DR & 0xFFFFu);
  return result;
}

// Public API with mutex protection
static inline uint16_t crc16_ccitt_false(const uint8_t* data, size_t len) {
  static mutex_t mtx;
  static bool mtx_initialized = false;
  if (!mtx_initialized) {
    chMtxObjectInit(&mtx);
    mtx_initialized = true;
  }
  chMtxLock(&mtx);
  uint16_t out = crc16_ccitt_false_hw(data, len);
  chMtxUnlock(&mtx);
  return out;
}

}  // namespace yfr4esc_crc
