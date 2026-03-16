/*
 * i2s6_lld.h - I2S6 Low-Level Driver for STM32H723
 *
 * This is a minimal polling-based I2S driver for SPI6 (I2S6) on STM32H723.
 * Designed to work with MAX98357 PCM/I2S Class D amplifier.
 *
 * Copyright (c) 2025 OpenMower Project
 */

#ifndef I2S6_LLD_H
#define I2S6_LLD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * @brief Initialize I2S6 peripheral with polling mode
 *
 * @param config Pointer to configuration structure
 * @return true if initialization successful, false otherwise
 */
bool i2s6_lld_init(const i2s6_config_t* config);

/**
 * @brief Send a single 16-bit audio sample via polling
 *
 * @param left_sample Left channel sample (16-bit signed)
 * @param right_sample Right channel sample (16-bit signed)
 */
void i2s6_lld_send_sample(int16_t left_sample, int16_t right_sample);

/**
 * @brief Send a single 32-bit audio sample via polling
 *
 * @param left_sample Left channel sample (32-bit signed)
 * @param right_sample Right channel sample (32-bit signed)
 */
void i2s6_lld_send_sample_32(int32_t left_sample, int32_t right_sample);

/**
 * @brief Optimized version for high-performance audio playback
 * Minimal overhead - no error checking, just TXP flag check
 *
 * @param left_sample Left channel sample (16-bit signed)
 * @param right_sample Right channel sample (16-bit signed)
 */
void i2s6_lld_send_sample_fast(int16_t left_sample, int16_t right_sample);

/**
 * @brief Check if I2S6 transmitter is ready for new data
 *
 * @return true if ready, false if busy
 */
bool i2s6_lld_tx_ready(void);

/**
 * @brief Enable I2S6 peripheral
 */
void i2s6_lld_enable(void);

/**
 * @brief Disable I2S6 peripheral
 */
void i2s6_lld_disable(void);

/**
 * @brief Get current I2S6 status
 *
 * @return Status register value
 */
uint32_t i2s6_lld_get_status(void);

#ifdef __cplusplus
}
#endif

#endif /* I2S6_LLD_H */
