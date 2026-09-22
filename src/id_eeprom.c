#include "id_eeprom.h"

#include <string.h>

#include "board_ex.h"
#include "ch.h"
#include "hal.h"

// keep a buffer in the correct RAM for BDMA
CC_SECTION(".ram4") uint8_t i2c4_tx_buffer[1] = {0};
CC_SECTION(".ram4") uint8_t i2c4_rx_buffer[256] = {0};

static uint16_t checksum(void *data, size_t length) {
  uint16_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    if (i % 2 == 0) {
      sum ^= ((uint8_t *)data)[i];
    } else {
      sum ^= (((uint8_t *)data)[i]) << 8;
    }
  }
  return sum;
}

// I2C4 can survive an MCU reset with an EEPROM still driving SDA low.
// Caller holds the bus mutex throughout recovery and the following transfer.
static bool wait_scl_high(void) {
  for (unsigned i = 0; i < 10; ++i) {
    if (palReadLine(LINE_I2C4_SCL)) {
      return true;
    }
    chThdSleepMilliseconds(1);
  }
  return palReadLine(LINE_I2C4_SCL) != 0;
}

static bool recover_eeprom_bus(void) {
  const I2CConfig *config = I2CD4.config;
  i2cStop(&I2CD4);

  // Release both lines before switching to open-drain GPIO. Never drive high.
  palSetLine(LINE_I2C4_SCL);
  palSetLine(LINE_I2C4_SDA);
  palSetLineMode(LINE_I2C4_SCL, PAL_MODE_OUTPUT_OPENDRAIN);
  palSetLineMode(LINE_I2C4_SDA, PAL_MODE_OUTPUT_OPENDRAIN);
  chThdSleepMilliseconds(1);
  bool clock_free = wait_scl_high();
  // UM10204 bus clear: at most nine clocks, with SDA released (NACK).
  // Millisecond sleeps respect the RTOS tickless minimum during startup.
  for (unsigned i = 0; clock_free && !palReadLine(LINE_I2C4_SDA) && i < 9; ++i) {
    palClearLine(LINE_I2C4_SCL);
    chThdSleepMilliseconds(1);
    palSetLine(LINE_I2C4_SCL);
    clock_free = wait_scl_high();
    chThdSleepMilliseconds(1);
  }
  if (clock_free) {
    // STOP: take SDA low while SCL is low, then release SCL followed by SDA.
    palClearLine(LINE_I2C4_SCL);
    palClearLine(LINE_I2C4_SDA);
    chThdSleepMilliseconds(1);
    palSetLine(LINE_I2C4_SCL);
    clock_free = wait_scl_high();
    chThdSleepMilliseconds(1);
    palSetLine(LINE_I2C4_SDA);
    chThdSleepMilliseconds(1);
  }
  bool bus_free = clock_free && palReadLine(LINE_I2C4_SDA);
  palSetLine(LINE_I2C4_SCL);
  palSetLine(LINE_I2C4_SDA);
  palSetLineMode(LINE_I2C4_SCL, PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN);
  palSetLineMode(LINE_I2C4_SDA, PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN);
  // Restart even after an unsuccessful bus clear; a timeout leaves I2C_LOCKED.
  return i2cStart(&I2CD4, config) == HAL_RET_SUCCESS && bus_free;
}

static bool read_eeprom(uint8_t address, uint8_t reg, void *buffer, size_t size, bool verify_checksum) {
  if (size == 0 || size > sizeof(i2c4_rx_buffer) || (verify_checksum && size < 2)) {
    return false;
  }
  bool success = false;
  i2cAcquireBus(&I2CD4);
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    if (I2CD4.state != I2C_READY || !palReadLine(LINE_I2C4_SCL) || !palReadLine(LINE_I2C4_SDA)) {
      if (!recover_eeprom_bus()) {
        chThdSleepMilliseconds(10);
        continue;
      }
    }
    i2c4_tx_buffer[0] = reg;
    msg_t result = i2cMasterTransmitTimeout(&I2CD4, address, i2c4_tx_buffer, 1, i2c4_rx_buffer, size, TIME_MS2I(50));
    if (result == MSG_OK) {
      uint16_t stored_checksum = 0;
      if (verify_checksum) {
        memcpy(&stored_checksum, &i2c4_rx_buffer[size - 2], sizeof(stored_checksum));
      }
      if (!verify_checksum || checksum(i2c4_rx_buffer, size - 2) == stored_checksum) {
        // All transfers use SRAM4 staging, including callers with stack buffers.
        memcpy(buffer, i2c4_rx_buffer, size);
        success = true;
        break;
      }
    } else {
      (void)recover_eeprom_bus();
    }
    chThdSleepMilliseconds(10);
  }
  i2cReleaseBus(&I2CD4);
  return success;
}

bool ID_EEPROM_GetMacAddress(uint8_t *buf, size_t buflen) {
  return read_eeprom(EEPROM_DEVICE_ADDRESS, 0xFA, buf, buflen, false);
}

bool ID_EEPROM_GetBootloaderInfo(struct bootloader_info *buffer) {
  return read_eeprom(EEPROM_DEVICE_ADDRESS, BOOTLOADER_INFO_ADDRESS, buffer, sizeof(*buffer), false);
}

bool ID_EEPROM_GetBoardInfo(struct board_info *buffer) {
  bool success = read_eeprom(EEPROM_DEVICE_ADDRESS, BOARD_INFO_ADDRESS, buffer, sizeof(*buffer), true);
  // An empty record is not an identity, even though all-zero data has a valid XOR checksum.
  success = success && buffer->board_id[0] != '\0' && (uint8_t)buffer->board_id[0] != 0xFF;
  if (!success) {
    memset(buffer, 0, sizeof(*buffer));
    strncpy(buffer->board_id, "N/A", sizeof(buffer->board_id));
  }
  return success;
}

bool ID_EEPROM_GetCarrierBoardInfo(struct carrier_board_info *buffer) {
  bool success = read_eeprom(CARRIER_EEPROM_DEVICE_ADDRESS, CARRIER_BOARD_INFO_ADDRESS, buffer, sizeof(*buffer), true);
  if (!success) {
    memset(buffer, 0, sizeof(*buffer));
    strncpy(buffer->board_id, "N/A", sizeof(buffer->board_id));
  }
  return success;
}
