//
// Created by clemens on 05.11.24.
//
#include "board_ex.h"

#include "ch.h"
#include "hal.h"

static I2CConfig i2c1Config = {0};
static I2CConfig i2c2Config = {0};
static I2CConfig i2c4Config = {0};

void initBoardPeriphs(void) {
  /**
   * Init I2C1
   */
  i2cAcquireBus(&I2CD1);

  // Calculated depending on clock source, check reference manual
  // 100kHz
  // i2c1Config.timingr = 0x60404E72;
  // 20kHz:
  i2c1Config.timingr = 0xE020D5F2;

  if (i2cStart(&I2CD1, &i2c1Config) != HAL_RET_SUCCESS) {
    while (1)
      ;
  }
  i2cReleaseBus(&I2CD1);
  /**
   * Init I2C4 with 100kHz
   */
  i2cAcquireBus(&I2CD2);

  // Calculated depending on clock source, check reference manual
  i2c2Config.timingr = 0x60404E72;

  if (i2cStart(&I2CD2, &i2c2Config) != HAL_RET_SUCCESS) {
    while (1)
      ;
  }
  i2cReleaseBus(&I2CD2);
  /**
   * Init I2C4 with 100kHz
   */
  i2cAcquireBus(&I2CD4);

  // Calculated depending on clock source, check reference manual
  i2c4Config.timingr = 0xE14;

  if (i2cStart(&I2CD4, &i2c4Config) != HAL_RET_SUCCESS) {
    while (1)
      ;
  }
  i2cReleaseBus(&I2CD4);
}