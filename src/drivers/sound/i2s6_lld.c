/*
 * i2s6_lld.c - I2S6 Low-Level Driver for STM32H723
 *
 * This is a minimal polling-based I2S driver for SPI6 (I2S6) on STM32H723.
 * Designed to work with MAX98357 PCM/I2S Class D amplifier.
 *
 * Copyright (c) 2025 OpenMower Project
 */

#include "i2s6_lld.h"

#include <stddef.h>
#include <stdlib.h>
#include <ulog.h>

#include "hal.h"
#include "stm32_gpio.h"

/* Use ChibiOS STM32H7 register definitions */
#include "stm32h7xx.h"

/* SPI6 is already defined in stm32h723xx.h as ((SPI_TypeDef *) SPI6_BASE) */
/* RCC macros are already defined in ChibiOS headers */

/* Note: All SPI/I2S register bit definitions are already provided by ChibiOS STM32H7xx headers */

/* System Clock Frequency (HCLK3 for D3 domain) */
#define HCLK3_FREQ 200000000UL /* 200 MHz for D3 domain */

/* Default configuration for MAX98357A in I2S standard mode (SD_MODE=0, GAIN_SLOT=0) */
static const i2s6_config_t default_config = {
    .sample_rate = 16000, /* 16 kHz target - according to architecture documentation */
    .data_bits = 16,      /* 16-bit data for I2S mode */
    .channel_bits = 16,   /* 16-bit per channel */
    .master_mode = true,  /* Master mode */
    .i2s_standard = true, /* I2S Philips standard */
    .i2s_mode = true,     /* I2S mode (not PCM) */
};

static bool i2s6_initialized = false;

/**
 * @brief Calculate I2S prescaler values (I2SDIV and ODD) for desired sample rate
 *
 * For I2S Philips standard without master clock output (MCKOE=0):
 * F_WS = F_i2s_clk / [32 × (CHLEN + 1) × {(2 × I2SDIV) + ODD}]
 * Where:
 * - F_WS = sample rate (word select frequency)
 * - F_i2s_clk = peripheral clock (PCLK4 = 100MHz for D3 domain)
 * - CHLEN = 0 for 16-bit channel length, 1 for 32-bit channel length
 * - I2SDIV = prescaler value (0-255)
 * - ODD = 0 or 1
 *
 * We need to find I2SDIV and ODD such that F_WS is as close as possible to desired sample rate.
 */
static uint32_t calculate_i2s_prescaler(uint32_t sample_rate, uint8_t channel_bits) {
  // PCLK4 frequency for D3 domain (from mcuconf.h)
  const uint32_t i2s_clk = 100000000UL;  // 100 MHz PCLK4

  // CHLEN value: 0 for 16-bit, 1 for 32-bit
  uint32_t chlen = (channel_bits == 32) ? 1 : 0;

  // Calculate denominator: 32 × (CHLEN + 1) for I2S mode
  uint32_t denominator = 32 * (chlen + 1);

  ULOG_INFO("I2S6: I2S prescaler calculation: target=%u Hz, i2s_clk=%u Hz, CHLEN=%u, denominator=%u", sample_rate,
            i2s_clk, chlen, denominator);

  // Find best I2SDIV and ODD values
  uint32_t best_i2sdiv = 0;
  uint32_t best_odd = 0;
  uint32_t best_error = 0xFFFFFFFF;
  uint32_t best_actual_rate = 0;

  // Search through possible I2SDIV values (0-255)
  for (uint32_t i2sdiv = 0; i2sdiv <= 255; i2sdiv++) {
    // Try ODD = 0 and ODD = 1
    for (uint32_t odd = 0; odd <= 1; odd++) {
      // Calculate divisor: (2 × I2SDIV) + ODD
      uint32_t divisor = (2 * i2sdiv) + odd;

      // Avoid division by zero
      if (divisor == 0) continue;

      // Calculate actual sample rate
      uint32_t actual_rate = i2s_clk / (denominator * divisor);

      // Calculate error
      uint32_t error = abs((int32_t)actual_rate - (int32_t)sample_rate);

      // Update best values if this is better
      if (error < best_error) {
        best_error = error;
        best_i2sdiv = i2sdiv;
        best_odd = odd;
        best_actual_rate = actual_rate;

        ULOG_INFO("I2S6: New best: I2SDIV=%u, ODD=%u, divisor=%u, actual=%u Hz, error=%u", best_i2sdiv, best_odd,
                  divisor, best_actual_rate, best_error);
      }

      // If we found an exact match, break early
      if (error == 0) {
        ULOG_INFO("I2S6: Found exact match: I2SDIV=%u, ODD=%u", i2sdiv, odd);
        return (i2sdiv << 16) | (odd << 24);
      }
    }
  }

  ULOG_INFO("I2S6: Best I2S prescaler: I2SDIV=%u, ODD=%u, actual=%u Hz, error=%u", best_i2sdiv, best_odd,
            best_actual_rate, best_error);

  // Return prescaler value with I2SDIV in bits 16-23 and ODD in bit 24
  return (best_i2sdiv << 16) | (best_odd << 24);
}

bool i2s6_lld_init(const i2s6_config_t* config) {
  if (config == NULL) {
    config = &default_config;
  }

  ULOG_INFO("I2S6: Initializing with sample_rate=%u, data_bits=%u, channel_bits=%u", config->sample_rate,
            config->data_bits, config->channel_bits);

  /* First configure clock source (PCLK4 as per mcuconf.h) */
  RCC->D3CCIPR &= ~RCC_D3CCIPR_SPI6SEL_Msk;
  RCC->D3CCIPR |= (0x0 << RCC_D3CCIPR_SPI6SEL_Pos); /* PCLK4 */
  ULOG_INFO("I2S6: Configured clock source (RCC_D3CCIPR=0x%08X)", RCC->D3CCIPR);

  /* Small delay after clock configuration */
  for (volatile uint32_t i = 0; i < 100; i++) {
  }

  /* Enable SPI6 clock */
  RCC->APB4ENR |= RCC_APB4ENR_SPI6EN;
  ULOG_INFO("I2S6: Enabled SPI6 clock (RCC_APB4ENR=0x%08X)", RCC->APB4ENR);

  /* Clear SPI6 reset bit (bring peripheral out of reset) */
  RCC->APB4RSTR &= ~RCC_APB4RSTR_SPI6RST;
  ULOG_INFO("I2S6: Cleared SPI6 reset (RCC_APB4RSTR=0x%08X)", RCC->APB4RSTR);

  /* Wait for clock to stabilize */
  for (volatile uint32_t i = 0; i < 1000; i++) {
  }

  /* Debug: Check if GPIO pins are configured correctly */
  ULOG_INFO("I2S6: Checking GPIO configuration...");
  ULOG_INFO("I2S6: GPIOA->MODER=0x%08X, GPIOA->AFRH=0x%08X", GPIOA->MODER, GPIOA->AFRH);
  ULOG_INFO("I2S6: GPIOB->MODER=0x%08X, GPIOB->AFRL=0x%08X", GPIOB->MODER, GPIOB->AFRL);
  ULOG_INFO("I2S6: GPIOG->MODER=0x%08X, GPIOG->AFRH=0x%08X", GPIOG->MODER, GPIOG->AFRH);

  /* Ensure peripheral is disabled (SPE=0) before configuration */
  SPI6->CR1 = 0;
  SPI6->CR2 = 0;

  /* Clear all interrupt flags */
  SPI6->IFCR = 0xFFFFFFFF;

  /* Small delay after reset */
  for (volatile uint32_t i = 0; i < 100; i++) {
  }

  /* Configure I2S mode */
  uint32_t i2scfgr = 0;

  /* Enable I2S mode */
  i2scfgr |= SPI_I2SCFGR_I2SMOD;

  /* Configure as Master Transmitter (I2SCFG = 0x2) */
  i2scfgr |= SPI_I2SCFGR_I2SCFG_1; /* Set bit 2 for master transmitter (0x2) */

  /* Configure I2S standard */
  if (config->i2s_standard) {
    i2scfgr |= (0x0 << SPI_I2SCFGR_I2SSTD_Pos); /* Philips standard */
  } else {
    /* For non-I2S mode, use appropriate standard */
    /* Note: Could be MSB justified (0x1), LSB justified (0x2), or PCM (0x3) */
    i2scfgr |= (0x3 << SPI_I2SCFGR_I2SSTD_Pos); /* Default to PCM standard */
  }

  /* Configure data length */
  if (config->data_bits == 16) {
    i2scfgr |= (0x0 << SPI_I2SCFGR_DATLEN_Pos); /* 16-bit data length */
  } else if (config->data_bits == 24) {
    i2scfgr |= (0x1 << SPI_I2SCFGR_DATLEN_Pos); /* 24-bit data length */
  } else if (config->data_bits == 32) {
    i2scfgr |= (0x2 << SPI_I2SCFGR_DATLEN_Pos); /* 32-bit data length */
  }

  /* Configure channel length */
  if (config->channel_bits == 16) {
    i2scfgr &= ~SPI_I2SCFGR_CHLEN; /* 16-bit channel length */
  } else if (config->channel_bits == 32) {
    i2scfgr |= SPI_I2SCFGR_CHLEN; /* 32-bit channel length */
  }

  /* Configure PCM mode if needed */
  if (!config->i2s_mode) {
    i2scfgr |= SPI_I2SCFGR_PCMSYNC;
  }

  /* For I2S Philips standard, set CKPOL = 0 (rising edge) */
  /* CKPOL is bit 1 of I2SCFGR register */
  i2scfgr &= ~(1 << 1); /* Clear CKPOL bit for rising edge */

  /* Write I2S configuration */
  SPI6->I2SCFGR = i2scfgr;
  ULOG_INFO("I2S6: I2SCFGR=0x%08X", SPI6->I2SCFGR);

  /* Configure prescaler for I2S mode */
  /* For I2S Philips standard with MCKOE=0 (no master clock output):
   * F_WS = F_i2s_clk / [32 × (CHLEN + 1) × {(2 × I2SDIV) + ODD}]
   * Where:
   * - F_WS = sample rate (word select frequency)
   * - F_i2s_clk = peripheral clock (PCLK4 = 100MHz for D3 domain)
   * - CHLEN = 0 for 16-bit channel length, 1 for 32-bit channel length
   * - I2SDIV = prescaler value (0-255)
   * - ODD = 0 or 1
   */
  uint32_t i2s_prescaler = calculate_i2s_prescaler(config->sample_rate, config->channel_bits);

  /* Apply prescaler to I2SCFGR register */
  /* Clear existing I2SDIV and ODD bits first */
  SPI6->I2SCFGR &= ~(SPI_I2SCFGR_I2SDIV_Msk | SPI_I2SCFGR_ODD_Msk);
  /* Apply new prescaler values */
  SPI6->I2SCFGR |= i2s_prescaler;

  /* Ensure MCKOE is disabled (no master clock output) */
  SPI6->I2SCFGR &= ~SPI_I2SCFGR_MCKOE;

  ULOG_INFO("I2S6: Updated I2SCFGR with I2S prescaler: 0x%08X", SPI6->I2SCFGR);

  /* Extract I2SDIV and ODD values for logging */
  uint32_t i2sdiv = (i2s_prescaler & SPI_I2SCFGR_I2SDIV_Msk) >> 16;
  uint32_t odd = (i2s_prescaler & SPI_I2SCFGR_ODD_Msk) >> 24;
  uint32_t chlen = (config->channel_bits == 32) ? 1 : 0;
  uint32_t divisor = (2 * i2sdiv) + odd;
  uint32_t denominator = 32 * (chlen + 1); /* 32 for I2S mode */
  uint32_t i2s_clk = 100000000UL;          /* 100 MHz PCLK4 */
  uint32_t actual_sample_rate = i2s_clk / (denominator * divisor);

  ULOG_INFO("I2S6: I2S configuration: I2SDIV=%u, ODD=%u, CHLEN=%u", i2sdiv, odd, chlen);
  ULOG_INFO("I2S6: Target %u Hz, actual %u Hz, divisor=%u, denominator=%u", config->sample_rate, actual_sample_rate,
            divisor, denominator);

  /* Configure SPI for polling mode - According to Table 436, in I2S mode:
   * - CFG1: Only TXDMAEN, RXDMAEN, FTHLV are usable
   * - CFG2: Only AFCNTR, LSBFRST, IOSWP are usable
   * Other fields must be at reset values
   */
  /* CFG1: Set FIFO threshold to 1/4 full (FTHLV=0) for 16-bit data */
  /* Note: MBR field in CFG1 is NOT used in I2S mode, only in SPI mode */
  /* According to Table 434, SPI6 has 8-byte FIFO (not 16-byte) */
  /* For 16-bit data (2 bytes per sample), 8-byte FIFO = 4 samples capacity */
  /* Set FTHLV to 1 (1/2 full) to trigger TXP when FIFO has space for 2 samples */
  SPI6->CFG1 = (0x0 << 28) | (0x1 << 5); /* MBR = 0 (not used), FTHLV = 1 (1/2 full) */

  /* CFG2: Configure for I2S mode with AFCNTR enabled for proper GPIO control */
  /* According to user's documentation: Control of IOs with AFCNTR bit */
  /* AFCNTR = 1: Alternate function GPIOs are controlled by the SPI/I2S peripheral */
  SPI6->CFG2 = SPI_CFG2_AFCNTR; /* Enable AFCNTR for proper GPIO control */

  ULOG_INFO("I2S6: CFG1=0x%08X (FTHLV=1 for 8-byte FIFO), CFG2=0x%08X (AFCNTR enabled)", SPI6->CFG1, SPI6->CFG2);

  /* Clear status register by reading it */
  (void)SPI6->SR;
  ULOG_INFO("I2S6: Initial status SR=0x%08X", SPI6->SR);

  i2s6_initialized = true;
  ULOG_INFO("I2S6: Initialization complete");
  return true;
}

void i2s6_lld_enable(void) {
  if (!i2s6_initialized) {
    i2s6_lld_init(NULL);
  }

  ULOG_INFO("I2S6: Enabling peripheral");

  /* Debug: Check clock configuration */
  ULOG_INFO("I2S6: RCC_D3CCIPR=0x%08X", RCC->D3CCIPR);
  ULOG_INFO("I2S6: RCC_APB4ENR=0x%08X", RCC->APB4ENR);
  ULOG_INFO("I2S6: RCC_APB4RSTR=0x%08X", RCC->APB4RSTR);

  /* Check if SPI6 is out of reset */
  if (RCC->APB4RSTR & RCC_APB4RSTR_SPI6RST) {
    ULOG_ERROR("I2S6: SPI6 is still in reset!");
    RCC->APB4RSTR &= ~RCC_APB4RSTR_SPI6RST;
    ULOG_INFO("I2S6: Cleared SPI6 reset bit");
  }

  /* Enable SPI/I2S peripheral */
  SPI6->CR1 |= SPI_CR1_SPE;
  ULOG_INFO("I2S6: CR1 after SPE=0x%08X", SPI6->CR1);

  /* Check I2S configuration */
  ULOG_INFO("I2S6: I2SCFGR=0x%08X", SPI6->I2SCFGR);
  ULOG_INFO("I2S6: CFG1=0x%08X", SPI6->CFG1);
  ULOG_INFO("I2S6: CFG2=0x%08X", SPI6->CFG2);

  /* MAX98357 startup sequence for I2S mode (SD_MODE=0):
   * According to architecture documentation, SD_MODE is pulled-up (SD_MODE=0 for I2S mode)
   * The only required sequence for startup is that LRCLK must start within 1/2 LRCLK period of BCLK starting.
   * The STM32 I2S peripheral handles this automatically when we start transmission (CSTART).
   */
  ULOG_INFO("I2S6: MAX98357 startup - I2S mode (SD_MODE=0), no delay needed");

  /* Check status register before filling FIFO */
  ULOG_INFO("I2S6: Status before filling FIFO SR=0x%08X", SPI6->SR);

  /* According to STM32H7 reference manual: Ensure TxFIFO is not empty before transmission starts */
  /* Fill TxFIFO with several zero samples to avoid underrun */
  /* According to Table 434, SPI6 has 8-byte FIFO (not 16-byte) */
  /* For 16-bit stereo data (4 bytes per sample: 2 bytes left + 2 bytes right) */
  /* 8-byte FIFO = 2 samples capacity */
  /* With FTHLV=1 (1/2 full), TXP flag triggers when FIFO has space for 2 samples */
  /* Fill with 1 sample (4 bytes) to ensure FIFO is not empty but not overfilled */
  ULOG_INFO("I2S6: Filling 8-byte FIFO with 1 sample (4 bytes)");

  for (int i = 0; i < 1; i++) {
    /* Wait for space in FIFO before writing */
    /* TXP flag indicates space available in TxFIFO (bit 1 of SPI_SR) */
    uint32_t sr = SPI6->SR;
    ULOG_INFO("I2S6: Waiting for TXP flag, SR=0x%08X, TXP=%u", sr, (sr & SPI_SR_TXP) != 0);

    while (!(SPI6->SR & SPI_SR_TXP)) {
      /* Busy wait for TXP flag */
      sr = SPI6->SR;
      /* Check for error flags */
      if (sr & SPI_SR_UDR) {
        SPI6->IFCR = SPI_IFCR_UDRC;
        ULOG_INFO("I2S6: Cleared UDR flag");
      }
      if (sr & SPI_SR_OVR) {
        SPI6->IFCR = SPI_IFCR_OVRC;
        ULOG_INFO("I2S6: Cleared OVR flag");
      }
      /* Also check if transmission has started (TXC should be 0 when transmitting) */
      if ((sr & SPI_SR_TXC) == 0) {
        ULOG_INFO("I2S6: TXC=0, transmission is active");
      }
    }

    SPI6->TXDR = 0; /* Send zero to both channels */
    ULOG_INFO("I2S6: Wrote sample %d to TXDR", i);
  }

  ULOG_INFO("I2S6: Status after filling FIFO SR=0x%08X", SPI6->SR);

  /* Start transmission (CSTART) - This starts BCLK and LRCLK */
  /* IMPORTANT: Start transmission AFTER filling FIFO to avoid underrun */
  SPI6->CR1 |= SPI_CR1_CSTART;
  ULOG_INFO("I2S6: CR1 after CSTART=0x%08X", SPI6->CR1);

  /* Small delay after starting transmission */
  for (volatile uint32_t i = 0; i < 100; i++) {
  }

  /* Check status register after starting transmission */
  ULOG_INFO("I2S6: Status after CSTART SR=0x%08X", SPI6->SR);

  /* Additional debug: Check if CSTART bit is actually set */
  if ((SPI6->CR1 & SPI_CR1_CSTART) == 0) {
    ULOG_ERROR("I2S6: CSTART bit not set after enabling!");
  } else {
    ULOG_INFO("I2S6: CSTART bit successfully set");
  }
}

void i2s6_lld_disable(void) {
  /* Stop transmission */
  SPI6->CR1 &= ~SPI_CR1_CSTART;

  /* Disable SPI/I2S peripheral */
  SPI6->CR1 &= ~SPI_CR1_SPE;
}

bool i2s6_lld_tx_ready(void) {
  /* Check if transmit buffer is empty */
  uint32_t sr = SPI6->SR;

  /* Clear any error flags */
  if (sr & SPI_SR_UDR) {
    SPI6->IFCR = SPI_IFCR_UDRC; /* Clear underrun flag */
  }
  if (sr & SPI_SR_OVR) {
    SPI6->IFCR = SPI_IFCR_OVRC; /* Clear overrun flag */
  }

  /* Check if transmit buffer has space available (TXP flag) */
  /* TXP is set when TxFIFO has enough free location to host 1 data packet */
  return (sr & SPI_SR_TXP) != 0;
}

/* Optimized version without logging for performance-critical paths */
static bool i2s6_lld_tx_ready_fast(void) {
  /* Check if transmit buffer is empty - minimal version */
  uint32_t sr = SPI6->SR;

  /* Clear any error flags quickly */
  if (sr & (SPI_SR_UDR | SPI_SR_OVR)) {
    SPI6->IFCR = SPI_IFCR_UDRC | SPI_IFCR_OVRC;
  }

  /* Check if transmit buffer has space available (TXP flag) */
  return (sr & SPI_SR_TXP) != 0;
}

void i2s6_lld_send_sample(int16_t left_sample, int16_t right_sample) {
  /* Wait for transmit buffer to be empty - optimized version */
  while (!i2s6_lld_tx_ready_fast()) {
    /* Busy wait - minimal overhead */
  }

  /* For I2S Philips standard with 16-bit data in 32-bit frame:
   * - Data is MSB-aligned in 32-bit frame
   * - Right channel data in bits 31:16
   * - Left channel data in bits 15:0
   * But note: The STM32 I2S peripheral expects us to send one 32-bit word
   * containing both channels for stereo mode.
   */
  uint32_t data = ((uint32_t)right_sample << 16) | ((uint32_t)left_sample & 0xFFFF);

  /* Write to transmit register */
  SPI6->TXDR = data;
}

/* Optimized version for high-performance audio playback */
void i2s6_lld_send_sample_fast(int16_t left_sample, int16_t right_sample) {
  /* Wait for transmit buffer to be empty - ultra-fast version */
  while (!(SPI6->SR & SPI_SR_TXP)) {
    /* Busy wait - check only TXP flag */
    /* Clear any error flags that might prevent TXP from being set */
    uint32_t sr = SPI6->SR;
    if (sr & (SPI_SR_UDR | SPI_SR_OVR)) {
      SPI6->IFCR = SPI_IFCR_UDRC | SPI_IFCR_OVRC;
    }
  }

  /* Combine channels and write to transmit register */
  SPI6->TXDR = ((uint32_t)right_sample << 16) | ((uint32_t)left_sample & 0xFFFF);
}

void i2s6_lld_send_sample_32(int32_t left_sample, int32_t right_sample) {
  /* Wait for transmit buffer to be empty */
  while (!i2s6_lld_tx_ready()) {
    /* Busy wait */
  }

  /* For 32-bit I2S:
   * - Send right channel first
   * - Then send left channel
   */
  SPI6->TXDR = (uint32_t)right_sample;

  /* Wait for next transmit buffer to be empty */
  while (!i2s6_lld_tx_ready()) {
    /* Busy wait */
  }

  SPI6->TXDR = (uint32_t)left_sample;
}

uint32_t i2s6_lld_get_status(void) {
  return SPI6->SR;
}

/**
 * @brief Simple sine wave generator for test tone
 */
static int16_t generate_sine_sample(uint32_t phase, uint32_t frequency, uint32_t sample_rate) {
  /* Simple sine approximation using fixed-point math */
  static const int16_t sine_table[] = {
      0,      3212,   6393,   9512,   12540,  15446,  18205,  20787,  23170,  25330,  27246,  28899,  30273,
      31357,  32138,  32610,  32767,  32610,  32138,  31357,  30273,  28899,  27246,  25330,  23170,  20787,
      18205,  15446,  12540,  9512,   6393,   3212,   0,      -3212,  -6393,  -9512,  -12540, -15446, -18205,
      -20787, -23170, -25330, -27246, -28899, -30273, -31357, -32138, -32610, -32767, -32610, -32138, -31357,
      -30273, -28899, -27246, -25330, -23170, -20787, -18205, -15446, -12540, -9512,  -6393,  -3212};

  /* Calculate phase increment per sample */
  uint32_t phase_increment = (frequency * 64) / sample_rate;

  /* Get phase index (0-63) */
  uint32_t phase_index = (phase / phase_increment) & 0x3F;

  return sine_table[phase_index];
}

/**
 * @brief Test function: Generate 440Hz sine wave
 */
void i2s6_test_tone_440hz(void) {
  static uint32_t phase = 0;
  const uint32_t frequency = 440; /* 440 Hz */
  const uint32_t sample_rate = 44100;

  /* Generate sine wave sample */
  int16_t sample = generate_sine_sample(phase, frequency, sample_rate);

  /* Send same sample to both channels (mono) */
  i2s6_lld_send_sample(sample, sample);

  /* Update phase */
  phase++;
  if (phase >= (sample_rate * 64 / frequency)) {
    phase = 0;
  }
}

/**
 * @brief Test function: Generate square wave
 */
void i2s6_test_square_wave(void) {
  static bool high = true;
  static uint32_t counter = 0;
  const uint32_t sample_rate = 44100;
  const uint32_t half_period_samples = sample_rate / 880; /* 440Hz square wave */

  /* Generate square wave */
  int16_t sample = high ? 30000 : -30000;

  /* Send same sample to both channels (mono) */
  i2s6_lld_send_sample(sample, sample);

  /* Update counter and toggle state */
  counter++;
  if (counter >= half_period_samples) {
    counter = 0;
    high = !high;
  }
}

/**
 * @brief Simple test function to verify I2S6 functionality
 * This function should be called in a loop to generate continuous audio
 */
void i2s6_test_continuous(void) {
  /* Initialize I2S6 if not already done */
  if (!i2s6_initialized) {
    i2s6_lld_init(NULL);
    i2s6_lld_enable();
  }

  /* Generate test tone */
  i2s6_test_tone_440hz();
}

/**
 * @brief Simple test function with square wave
 */
void i2s6_test_square_wave_continuous(void) {
  /* Initialize I2S6 if not already done */
  if (!i2s6_initialized) {
    i2s6_lld_init(NULL);
    i2s6_lld_enable();
  }

  /* Generate square wave */
  i2s6_test_square_wave();
}
