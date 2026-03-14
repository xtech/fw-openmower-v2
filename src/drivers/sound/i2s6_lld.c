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

/* Default configuration for MAX98357 */
static const i2s6_config_t default_config = {
    .sample_rate = 44100, /* 44.1 kHz */
    .data_bits = 16,      /* 16-bit data */
    .channel_bits = 16,   /* 16-bit per channel */
    .master_mode = true,  /* Master mode */
    .i2s_standard = true, /* I2S Philips standard */
    .i2s_mode = true,     /* I2S mode (not PCM) */
};

static bool i2s6_initialized = false;

/**
 * @brief Calculate I2S prescaler value for desired sample rate
 *
 * For I2S Philips standard without master clock output (MCKOE=0):
 * Fs = I2SxCLK / ((16*2)*((2*I2SDIV)+ODD))
 * Where:
 * - Fs = sample rate
 * - I2SxCLK = peripheral clock (PCLK4 = 100MHz for D3 domain)
 * - I2SDIV = prescaler value (0-255)
 * - ODD = 0 or 1
 *
 * We need to find I2SDIV and ODD such that Fs is as close as possible to desired sample rate.
 *
 * Note: This function is not used in STM32H7 I2S implementation which uses MBR field instead.
 * Keeping it commented for reference.
 */
/*
static uint32_t calculate_prescaler(uint32_t sample_rate) {
  // PCLK4 frequency for D3 domain (from mcuconf.h)
  const uint32_t i2s_clk = 100000000UL; // 100 MHz PCLK4

  // Target bit clock frequency
  uint32_t target_freq = sample_rate * 32; // 16 bits * 2 channels

  // Calculate ideal divider
  uint32_t ideal_divider = i2s_clk / target_freq;

  // I2SDIV = (ideal_divider / 2) - 1
  uint32_t i2sdiv = (ideal_divider / 2);
  if (i2sdiv > 0) {
    i2sdiv--;
  }

  // Calculate ODD bit
  uint32_t odd = 0;
  uint32_t actual_divider = 2 * ((2 * i2sdiv) + odd);
  uint32_t actual_freq = i2s_clk / (32 * actual_divider);

  // Try with ODD=1 if it gives better accuracy
  uint32_t odd_divider = 2 * ((2 * i2sdiv) + 1);
  uint32_t odd_freq = i2s_clk / (32 * odd_divider);

  if (abs((int32_t)odd_freq - (int32_t)sample_rate) < abs((int32_t)actual_freq - (int32_t)sample_rate)) {
    odd = 1;
    actual_divider = odd_divider;
    actual_freq = odd_freq;
  }

  // Clamp I2SDIV to 8-bit range
  if (i2sdiv > 255) {
    i2sdiv = 255;
  }

  ULOG_INFO("I2S6: sample_rate=%u, i2sdiv=%u, odd=%u, actual_freq=%u", sample_rate, i2sdiv, odd, actual_freq);

  // Return prescaler value with ODD bit
  return i2sdiv | (odd << 8);
}
*/

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

  /* Configure I2S standard (Philips) */
  if (config->i2s_standard) {
    i2scfgr |= (0x0 << SPI_I2SCFGR_I2SSTD_Pos); /* Philips standard */
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

  /* Write I2S configuration */
  SPI6->I2SCFGR = i2scfgr;
  ULOG_INFO("I2S6: I2SCFGR=0x%08X", SPI6->I2SCFGR);

  /* Configure prescaler for I2S mode */
  /* For STM32H7 in I2S mode, we need to configure the clock using MBR field in CFG1 */
  /* Target bit clock frequency = sample_rate * 32 (16 bits * 2 channels) */
  uint32_t pclk4_freq = 100000000UL; /* 100 MHz PCLK4 */

  /* Find appropriate MBR value (see RM0433 Table 436) */
  /* For audio, we prefer standard sample rates: 48kHz or 44.1kHz */
  /* With PCLK4=100MHz:
   * - MBR=5 (divide by 64): 100MHz/64 = 1.5625MHz -> 48.828kHz
   * - MBR=6 (divide by 128): 100MHz/128 = 781.25kHz -> 24.414kHz
   * For 44.1kHz target, MBR=5 gives 48.828kHz (error 4.7kHz)
   * MBR=6 gives 24.414kHz (error 19.7kHz)
   * So MBR=5 is better for 44.1kHz target
   */
  uint32_t mbr_value = 5; /* Default to divide by 64 for ~48.8kHz */

  /* Calculate which MBR gives closest sample rate to target */
  uint32_t best_mbr = 5;
  uint32_t best_error = 0xFFFFFFFF;

  ULOG_INFO("I2S6: Calculating best MBR for target %u Hz (PCLK4=%u Hz)", config->sample_rate, pclk4_freq);

  for (uint32_t test_mbr = 0; test_mbr <= 7; test_mbr++) {
    uint32_t test_divider = 2 << test_mbr; /* 2^(test_mbr+1) */
    uint32_t test_bit_clock = pclk4_freq / test_divider;
    uint32_t test_sample_rate = test_bit_clock / 32;
    uint32_t error = abs((int32_t)test_sample_rate - (int32_t)config->sample_rate);

    ULOG_INFO("I2S6: MBR=%u: divider=%u, bit_clock=%u, sample_rate=%u, error=%u", test_mbr, test_divider,
              test_bit_clock, test_sample_rate, error);

    if (error < best_error) {
      best_error = error;
      best_mbr = test_mbr;
      ULOG_INFO("I2S6: New best: MBR=%u, error=%u", best_mbr, best_error);
    }
  }

  mbr_value = best_mbr;

  /* Calculate actual bit clock frequency */
  uint32_t actual_divider = 2 << mbr_value; /* 2^(mbr_value+1) */
  uint32_t actual_bit_clock = pclk4_freq / actual_divider;
  uint32_t actual_sample_rate = actual_bit_clock / 32;

  ULOG_INFO("I2S6: Target %u Hz, MBR=%u (div by %u), actual %u Hz", config->sample_rate, mbr_value, actual_divider,
            actual_sample_rate);

  /* Configure SPI for polling mode - According to Table 436, in I2S mode:
   * - CFG1: Only TXDMAEN, RXDMAEN, FTHLV are usable
   * - CFG2: Only AFCNTR, LSBFRST, IOSWP are usable
   * Other fields must be at reset values
   */
  /* CFG1: Set FIFO threshold to 1/4 full (FTHLV=0) for 16-bit data */
  /* Set MBR for clock division */
  SPI6->CFG1 = (mbr_value << 28) | (0x0 << 5); /* MBR = calculated value, FTHLV = 0 (1/4 full) */

  /* CFG2: Set to reset value (0) as per Table 436 */
  SPI6->CFG2 = 0;

  ULOG_INFO("I2S6: CFG1=0x%08X, CFG2=0x%08X", SPI6->CFG1, SPI6->CFG2);

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

  /* Enable SPI/I2S peripheral */
  SPI6->CR1 |= SPI_CR1_SPE;
  ULOG_INFO("I2S6: CR1 after SPE=0x%08X", SPI6->CR1);

  /* Small delay after enabling peripheral */
  for (volatile uint32_t i = 0; i < 100; i++) {
  }

  /* Start transmission first (CSTART) - According to STM32 programming examples */
  SPI6->CR1 |= SPI_CR1_CSTART;
  ULOG_INFO("I2S6: CR1 after CSTART=0x%08X", SPI6->CR1);

  /* Small delay after starting transmission */
  for (volatile uint32_t i = 0; i < 100; i++) {
  }

  /* Check status register before filling FIFO */
  ULOG_INFO("I2S6: Status after CSTART SR=0x%08X", SPI6->SR);

  /* According to programming example: Ensure TxFIFO is not empty before transmission starts */
  /* Fill TxFIFO with several zero samples to avoid underrun */
  /* FIFO is 16 entries deep, fill at least 4 samples (1/4 full threshold) */
  for (int i = 0; i < 8; i++) {
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

void i2s6_lld_send_sample(int16_t left_sample, int16_t right_sample) {
  /* Wait for transmit buffer to be empty */
  while (!i2s6_lld_tx_ready()) {
    /* Busy wait */
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
