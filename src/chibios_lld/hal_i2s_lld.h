/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file    hal_i2s_lld.h
 * @brief   STM32H7 I2S LLD for SPI6 (D3 domain, BDMA) — SPIv3 variant
 *
 * @note    This driver implements the ChibiOS I2S HAL LLD API for SPI6 on
 *          STM32H7 devices.  SPI6 lives in the D3 power domain and requires
 *          BDMA (not DMA1/DMA2).  The audio TX buffer MUST reside in SRAM4
 *          (0x38000000) because BDMA can only access D3-domain memory.
 *
 *          Supports:  I2S Master TX, Philips standard, 16-bit stereo.
 *          BDMA is the only DMA type usable for SPI6 on STM32H7.
 *
 * @addtogroup I2S
 * @{
 */

#ifndef HAL_I2S_LLD_H
#define HAL_I2S_LLD_H

#if (HAL_USE_I2S == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @name    Static I2S modes (kept compatible with SPIv2 I2S naming)
 * @{
 */
#define STM32_I2S_MODE_SLAVE 0
#define STM32_I2S_MODE_MASTER 1
#define STM32_I2S_MODE_RX 2
#define STM32_I2S_MODE_TX 4
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */

/**
 * @brief   I2S6 (SPI6) driver enable switch.
 * @note    The default is @p FALSE.
 */
#if !defined(STM32_I2S_USE_SPI6) || defined(__DOXYGEN__)
#define STM32_I2S_USE_SPI6 FALSE
#endif

/**
 * @brief   I2S6 BDMA interrupt priority level.
 */
#if !defined(STM32_I2S_SPI6_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define STM32_I2S_SPI6_IRQ_PRIORITY 10
#endif

/**
 * @brief   I2S6 BDMA priority (0..3|lowest..highest).
 */
#if !defined(STM32_I2S_SPI6_BDMA_PRIORITY) || defined(__DOXYGEN__)
#define STM32_I2S_SPI6_BDMA_PRIORITY 1
#endif

/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !STM32_I2S_USE_SPI6
#error "I2S driver activated but STM32_I2S_USE_SPI6 is not set to TRUE"
#endif

#if !defined(STM32_I2S_SPI6_TX_BDMA_STREAM)
#error "STM32_I2S_SPI6_TX_BDMA_STREAM not defined — add it to mcuconf.h"
#endif

#if !defined(STM32_BDMA_REQUIRED)
#define STM32_BDMA_REQUIRED
#endif

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Low level fields of the I2S driver structure.
 * @note    Inlined into @p hal_i2s_driver via @p i2s_lld_driver_fields.
 */
#define i2s_lld_driver_fields                              \
  /** Pointer to the SPI6 registers block. */              \
  SPI_TypeDef* spi;                                        \
  /** Allocated BDMA TX stream. */                         \
  const stm32_bdma_stream_t* bdmatx;                       \
  /** TX BDMA mode bit mask (set once in i2s_lld_init). */ \
  uint32_t txdmamode

/**
 * @brief   Low level fields of the I2S configuration structure.
 * @note    Inlined into @p hal_i2s_config via @p i2s_lld_config_fields.
 *          @p i2scfgr should contain only user-configurable bits:
 *          I2SSTD, DATLEN, CHLEN.  I2SMOD and I2SCFG (master TX) are set
 *          by the driver.  The prescaler is derived from @p sample_rate.
 */
#define i2s_lld_config_fields                                        \
  /** Target sample rate in Hz (prescaler is computed from this). */ \
  uint32_t sample_rate;                                              \
  /** I2SCFGR user bits: I2SSTD[1:0], DATLEN[1:0], CHLEN. */         \
  uint32_t i2scfgr

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if STM32_I2S_USE_SPI6 && !defined(__DOXYGEN__)
extern I2SDriver I2SD6;
#endif

#ifdef __cplusplus
extern "C" {
#endif
void i2s_lld_init(void);
void i2s_lld_start(I2SDriver* i2sp);
void i2s_lld_stop(I2SDriver* i2sp);
void i2s_lld_start_exchange(I2SDriver* i2sp);
void i2s_lld_stop_exchange(I2SDriver* i2sp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_I2S == TRUE */

#endif /* HAL_I2S_LLD_H */

/** @} */
