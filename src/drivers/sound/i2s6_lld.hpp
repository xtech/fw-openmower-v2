/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file i2s6_lld.hpp
 * @brief I2S6 is a minimal Low-Level Driver for STM32H723, designed to work with MAX98357 PCM/I2S Class D amplifier
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-19
 */

#ifndef I2S6_LLD_H
#define I2S6_LLD_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief I2S6 configuration structure
 */
typedef struct {
  uint32_t sample_rate;  ///< Sample rate in Hz (e.g., 44100)
  uint8_t data_bits;     ///< Data bits per channel (16 or 32)
  uint8_t channel_bits;  ///< Bits per channel (16 or 32)
  bool master_mode;      ///< true = master, false = slave
  bool i2s_standard;     ///< true = I2S Philips standard, false = MSB justified
  bool i2s_mode;         ///< true = I2S mode, false = PCM mode
} i2s6_config_t;

namespace xbot::driver::sound::i2s6 {

/**
 * @brief Initialize I2S6 peripheral with polling mode
 *
 * @param config Pointer to configuration structure
 * @return true if initialization successful, false otherwise
 */
bool init(const i2s6_config_t* config);

/**
 * @brief Send a single 16-bit audio sample via polling
 *
 * @param left_sample Left channel sample (16-bit signed)
 * @param right_sample Right channel sample (16-bit signed)
 */
void send_sample(int16_t left_sample, int16_t right_sample);

/**
 * @brief Send a single 32-bit audio sample via polling
 *
 * @param left_sample Left channel sample (32-bit signed)
 * @param right_sample Right channel sample (32-bit signed)
 */
void send_sample_32(int32_t left_sample, int32_t right_sample);

/**
 * @brief Optimized version for high-performance audio playback
 * Minimal overhead - no error checking, just TXP flag check
 *
 * @param left_sample Left channel sample (16-bit signed)
 * @param right_sample Right channel sample (16-bit signed)
 */
void send_sample_fast(int16_t left_sample, int16_t right_sample);

/**
 * @brief Check if I2S6 transmitter is ready for new data
 *
 * @return true if ready, false if busy
 */
bool tx_ready(void);

/**
 * @brief Enable I2S6 peripheral
 */
void enable(void);

/**
 * @brief Disable I2S6 peripheral
 */
void disable(void);

/**
 * @brief Get current I2S6 status
 *
 * @return Status register value
 */
uint32_t get_status(void);

/**
 * @brief Test function: Generate 440Hz sine wave using fixed-point phase
 */
void test_tone_440hz(void);

/**
 * @brief Test function: Generate square wave
 */
void test_square_wave(void);

/**
 * @brief Simple test function to verify I2S6 functionality
 * This function should be called in a loop to generate continuous audio
 */
void test_continuous(void);

/**
 * @brief Simple test function with square wave
 */
void test_square_wave_continuous(void);

#ifdef __cplusplus
}  // namespace xbot::driver::sound::i2s6
#endif

#endif
