//
// Created by clemens on 05.11.24.
//
#include "board_ex.h"

#include "ch.h"
#include "hal.h"

static I2CConfig i2c1Config = {0};
static I2CConfig i2c2Config = {0};
static I2CConfig i2c4Config = {0};

/*
 * The I2C1/I2C2 TIMINGR values below are calculated for a 130 MHz kernel clock
 * (PCLK1 = HCLK / 2 = SYSCLK / 4, SYSCLK = 520 MHz). Recalculate them if the
 * clock tree changes. I2C4 runs from CSI (4 MHz) and is not affected.
 */
#if (STM32_I2C1CLK != 130000000U) || (STM32_I2C2CLK != 130000000U)
#error "I2C1/I2C2 TIMINGR values in board_ex.c are calculated for a 130 MHz kernel clock"
#endif

void initBoardPeriphs(void) {
  /**
   * Init I2C1
   */
  i2cAcquireBus(&I2CD1);

  // Calculated depending on clock source, check reference manual.
  // Note: TIMINGR bits 27:24 are reserved (RM0468) and must be zero - the older
  // 400 kHz draft (0x0E00A3B3) had them set, cleaned up while rescaling them.
  // 20 kHz:
  // i2c1Config.timingr = 0xE020C9E5;
  // 100 kHz (default, resulting in ~100.9 kHz):
  i2c1Config.timingr = 0x60404A6C;
  // 400 kHz (Fast mode):
  // AH20260921: Works also with BQ charger and Sabo OEM BMS
  // i2c1Config.timingr = 0x00009AA9;
  i2c1Config.cr1 = 0b111100000000;

  if (i2cStart(&I2CD1, &i2c1Config) != HAL_RET_SUCCESS) {
    while (1);
  }
  i2cReleaseBus(&I2CD1);
  /**
   * Init I2C4 with 100kHz
   */
  i2cAcquireBus(&I2CD2);

  // Calculated depending on clock source, check reference manual
  i2c2Config.timingr = 0x60404A6C;
  i2c2Config.cr1 = 0b111100000000;

  if (i2cStart(&I2CD2, &i2c2Config) != HAL_RET_SUCCESS) {
    while (1);
  }
  i2cReleaseBus(&I2CD2);
  /**
   * Init I2C4 with 100kHz
   */
  i2cAcquireBus(&I2CD4);

  // Calculated depending on clock source, check reference manual
  i2c4Config.timingr = 0xE14;
  i2c4Config.cr1 = 0b111100000000;

  if (i2cStart(&I2CD4, &i2c4Config) != HAL_RET_SUCCESS) {
    while (1);
  }
  i2cReleaseBus(&I2CD4);
}