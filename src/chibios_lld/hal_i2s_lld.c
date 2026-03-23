/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file    hal_i2s_lld.c
 * @brief   STM32H7 I2S LLD for SPI6 (D3 domain, BDMA) — SPIv3 variant
 *
 * @note    Implements the ChibiOS I2S HAL LLD API (@p HAL_USE_I2S).
 *          SPI6 on STM32H7 resides in the D3 power domain and requires:
 *            - Clock source set via RCC->D3CCIPR (PCLK4, not the default)
 *            - BDMA (not DMA1/DMA2) for DMA transfers
 *            - Audio buffer in SRAM4 (D3 memory, 0x38000000)
 *
 *          Prescaler is calculated at run-time from config->sample_rate
 *          and the fixed PCLK4 frequency (137.5 MHz for this board).
 *
 * @addtogroup I2S
 * @{
 */

#include "hal.h"

#if (HAL_USE_I2S == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/** @brief I2S6 driver identifier (maps to SPI6 hardware). */
#if STM32_I2S_USE_SPI6 || defined(__DOXYGEN__)
I2SDriver I2SD6;
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Calculate I2S prescaler values for desired sample rate.
 * @details Searches I2SDIV (1..255) and ODD (0..1) for the combination
 *          that minimises the error to @p sample_rate using the formula:
 *            F_WS = F_pclk4 / [32 × (CHLEN+1) × ((2×I2SDIV)+ODD)]
 *
 * @param[in] sample_rate   desired WS frequency in Hz
 * @param[in] chlen         0 = 16-bit channel, 1 = 32-bit channel
 * @return    Combined prescaler bits for OR-ing into I2SCFGR
 *            (I2SDIV in bits [23:16], ODD in bit 24)
 */
static uint32_t i2s_lld_calc_prescaler(uint32_t sample_rate, uint32_t chlen) {
  const uint32_t denom = 32U * (chlen + 1U);
  uint32_t best_i2sdiv = 1U, best_odd = 0U, best_err = UINT32_MAX;

  for (uint32_t i2sdiv = 1U; i2sdiv <= 255U; i2sdiv++) {
    for (uint32_t odd = 0U; odd <= 1U; odd++) {
      uint32_t divisor = (2U * i2sdiv) + odd;
      uint32_t actual = STM32_SPI6CLK / (denom * divisor);
      uint32_t err = (actual > sample_rate) ? (actual - sample_rate) : (sample_rate - actual);
      if (err < best_err) {
        best_err = err;
        best_i2sdiv = i2sdiv;
        best_odd = odd;
      }
      if (err == 0U) break;
    }
    if (best_err == 0U) break;
  }

  return (best_i2sdiv << SPI_I2SCFGR_I2SDIV_Pos) | (best_odd << SPI_I2SCFGR_ODD_Pos);
}

/**
 * @brief   BDMA TX end-of-transfer interrupt handler.
 * @note    Called by the BDMA ISR for both half-transfer and full-transfer
 *          events.  Drives the portable @p _i2s_isr_half_code and
 *          @p _i2s_isr_full_code macros so the application callback is
 *          invoked to refill the double-buffer.
 */
static void i2s_lld_serve_bdma_tx_interrupt(I2SDriver* i2sp, uint32_t flags) {
  /* DMA error → halt (configurable via STM32_I2S_DMA_ERROR_HOOK). */
  if ((flags & STM32_BDMA_ISR_TEIF) != 0U) {
    STM32_I2S_DMA_ERROR_HOOK(i2sp);
  }

  /* Full-transfer (TCIF): second half played, refill first half. */
  if ((flags & STM32_BDMA_ISR_TCIF) != 0U) {
    _i2s_isr_full_code(i2sp);
  }
  /* Half-transfer (HTIF): first half played, refill second half. */
  else if ((flags & STM32_BDMA_ISR_HTIF) != 0U) {
    _i2s_isr_half_code(i2sp);
  }
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level I2S driver initialisation.
 * @note    Called once by @p i2sInit() at HAL start.
 * @notapi
 */
void i2s_lld_init(void) {
#if STM32_I2S_USE_SPI6
  i2sObjectInit(&I2SD6);
  I2SD6.spi = SPI6;
  I2SD6.bdmatx = NULL;
  /* Base BDMA mode: M→P, half-word (16-bit), circular, half+full interrupts. */
  I2SD6.txdmamode = STM32_BDMA_CR_PL(STM32_I2S_SPI6_BDMA_PRIORITY) | STM32_BDMA_CR_DIR_M2P | STM32_BDMA_CR_PSIZE_HWORD |
                    STM32_BDMA_CR_MSIZE_HWORD | STM32_BDMA_CR_CIRC | STM32_BDMA_CR_HTIE | STM32_BDMA_CR_TCIE |
                    STM32_BDMA_CR_TEIE;
#endif
}

/**
 * @brief   Configures and activates the I2S peripheral.
 * @details Allocates the BDMA stream, configures the D3 clock source,
 *          calculates and applies the I2S prescaler, and sets up SPI6
 *          registers for I2S master TX operation.  Does NOT start
 *          clock generation — that happens in @p i2s_lld_start_exchange().
 *
 * @param[in] i2sp  pointer to the @p I2SDriver object
 * @notapi
 */
void i2s_lld_start(I2SDriver* i2sp) {
  if (i2sp->state != I2S_STOP) {
    return;
  }

#if STM32_I2S_USE_SPI6
  if (&I2SD6 == i2sp) {
    /* Allocate BDMA TX stream (SPI6 uses BDMA, not DMA1/DMA2). */
    i2sp->bdmatx = bdmaStreamAllocI(STM32_I2S_SPI6_TX_BDMA_STREAM, STM32_I2S_SPI6_IRQ_PRIORITY,
                                    (stm32_dmaisr_t)i2s_lld_serve_bdma_tx_interrupt, (void*)i2sp);
    osalDbgAssert(i2sp->bdmatx != NULL, "unable to allocate BDMA stream");
    bdmaSetRequestSource(i2sp->bdmatx, STM32_DMAMUX2_SPI6_TX);

    /* Point BDMA peripheral address at SPI6 TXDR (fixed for lifetime of driver). */
    bdmaStreamSetPeripheral(i2sp->bdmatx, &i2sp->spi->TXDR);

    /* Select PCLK4 as SPI6 kernel clock (D3CCIPR SPI6SEL = 000b → PCLK4).
     * ChibiOS rccEnableSPI6() does not set this mux — must be done here. */
    RCC->D3CCIPR = (RCC->D3CCIPR & ~RCC_D3CCIPR_SPI6SEL_Msk) | (0x0U << RCC_D3CCIPR_SPI6SEL_Pos);

    /* Enable SPI6 peripheral clock. */
    rccEnableSPI6(true);
  }
#endif

  /* -----------------------------------------------------------------------
   * (Re)configure SPI6 in I2S master TX mode.
   * ----------------------------------------------------------------------- */

  /* Calculate prescaler from desired sample rate and channel width. */
  uint32_t chlen = (i2sp->config->i2scfgr & SPI_I2SCFGR_CHLEN) ? 1U : 0U;
  uint32_t presc = i2s_lld_calc_prescaler(i2sp->config->sample_rate, chlen);

  /* Build I2SCFGR value:
   *   user bits  : I2SSTD, DATLEN, CHLEN  (from config->i2scfgr)
   *   driver bits: I2SMOD = 1 (I2S mode, not SPI)
   *                I2SCFG = 0b10 (Master Transmitter)
   *                MCKOE  = 0 (no master clock output)
   *                CKPOL  = 0 (default for Philips standard)
   *   prescaler  : I2SDIV + ODD  (calculated above)
   */
  uint32_t i2scfgr = i2sp->config->i2scfgr & (SPI_I2SCFGR_I2SSTD_Msk | SPI_I2SCFGR_DATLEN_Msk | SPI_I2SCFGR_CHLEN);
  i2scfgr |= SPI_I2SCFGR_I2SMOD;   /* I2S mode */
  i2scfgr |= SPI_I2SCFGR_I2SCFG_1; /* Master transmitter (I2SCFG = 0b10) */
  i2scfgr |= presc;                /* I2SDIV + ODD */

  /* Reset peripheral registers before configuration. */
  i2sp->spi->CR1 = 0U;
  i2sp->spi->CR2 = 0U;
  i2sp->spi->IFCR = 0xFFFFFFFFU;
  i2sp->spi->I2SCFGR = i2scfgr;

  /* CFG1: enable TX DMA request, FIFO threshold = 1 frame (FTHLV=0). */
  i2sp->spi->CFG1 = SPI_CFG1_TXDMAEN;

  /* CFG2: AFCNTR = 1 → peripheral controls the I2S GPIO pins. */
  i2sp->spi->CFG2 = SPI_CFG2_AFCNTR;
}

/**
 * @brief   Deactivates the I2S peripheral.
 * @details Frees the BDMA stream and disables the SPI6 clock.
 *
 * @param[in] i2sp  pointer to the @p I2SDriver object
 * @notapi
 */
void i2s_lld_stop(I2SDriver* i2sp) {
  if (i2sp->state == I2S_READY) {
    /* Reset peripheral. */
    i2sp->spi->CR1 = 0U;
    i2sp->spi->CR2 = 0U;
    i2sp->spi->CFG1 = 0U;
    i2sp->spi->CFG2 = 0U;
    i2sp->spi->IFCR = 0xFFFFFFFFU;
    i2sp->spi->I2SCFGR = 0U;

    /* Release BDMA stream. */
    bdmaStreamFreeI(i2sp->bdmatx);
    i2sp->bdmatx = NULL;

#if STM32_I2S_USE_SPI6
    if (&I2SD6 == i2sp) {
      rccDisableSPI6();
    }
#endif
  }
}

/**
 * @brief   Starts the I2S DMA exchange.
 * @details Sets up the BDMA circular transfer, enables the SPI peripheral,
 *          and starts I2S clock generation (BCLK + WS via CSTART).
 * @note    The TX buffer must reside in SRAM4 (D3 domain), because BDMA
 *          can only access D3-domain memory on STM32H7.
 * @note    @p config->size is the TOTAL number of 16-bit samples in the
 *          double-buffer (i.e., 2 × half-buffer size).  BDMA HTIF fires at
 *          size/2 (fill second half) and TCIF fires at size (fill first half).
 *
 * @param[in] i2sp  pointer to the @p I2SDriver object
 * @notapi
 */
void i2s_lld_start_exchange(I2SDriver* i2sp) {
  /* Configure BDMA: memory address, transaction size, and mode. */
  bdmaStreamSetMemory(i2sp->bdmatx, i2sp->config->tx_buffer);
  bdmaStreamSetTransactionSize(i2sp->bdmatx, i2sp->config->size);
  /* Add MINC (memory-increment) to the base txdmamode. */
  bdmaStreamSetMode(i2sp->bdmatx, i2sp->txdmamode | STM32_BDMA_CR_MINC);
  bdmaStreamEnable(i2sp->bdmatx);

  /* Enable SPI peripheral. */
  i2sp->spi->CR1 |= SPI_CR1_SPE;

  /* Start I2S clock generation (BCLK + WS).  BDMA fills the FIFO
   * automatically once TXP fires.  CSTART self-clears in master mode. */
  i2sp->spi->CR1 |= SPI_CR1_CSTART;
}

/**
 * @brief   Stops the ongoing I2S exchange.
 * @details Disables the BDMA stream, waits for the current transmission
 *          to complete (TXC), then disables the SPI peripheral.
 *
 * @param[in] i2sp  pointer to the @p I2SDriver object
 * @notapi
 */
void i2s_lld_stop_exchange(I2SDriver* i2sp) {
  /* Disable BDMA — no more data will be fed to the FIFO. */
  bdmaStreamDisable(i2sp->bdmatx);

  /* Wait for Transmission Complete (TXC): shift register drained.
   * From STM32H7 RM: to stop I2S, wait for TXC=1, then clear SPE. */
  while ((i2sp->spi->SR & SPI_SR_TXC) == 0U) {
    /* Busy wait — typically a few BCLK periods. */
  }

  /* Disable I2S clock generation and the SPI peripheral. */
  i2sp->spi->CR1 &= ~SPI_CR1_CSTART;
  i2sp->spi->CR1 &= ~SPI_CR1_SPE;
  i2sp->spi->IFCR = 0xFFFFFFFFU;
}

#endif /* HAL_USE_I2S == TRUE */

/** @} */
