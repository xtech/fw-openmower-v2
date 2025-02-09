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

bool ID_EEPROM_GetMacAddress(uint8_t *buf, size_t buflen) {
  chDbgAssert(buflen <= sizeof(i2c4_rx_buffer), "I2C4 RX buffer too small");
  i2cAcquireBus(&I2CD4);

  uint8_t reg = 0xFA;
  i2c4_tx_buffer[0] = reg;

  bool success = i2cMasterTransmit(&I2CD4, EEPROM_DEVICE_ADDRESS, i2c4_tx_buffer, 1, i2c4_rx_buffer, buflen) == MSG_OK;

  if (success) {
    memcpy(buf, i2c4_rx_buffer, buflen);
  }
  i2cReleaseBus(&I2CD4);
  return success;
}

bool ID_EEPROM_GetBootloaderInfo(struct bootloader_info *buffer) {
  i2cAcquireBus(&I2CD4);

  uint8_t reg = BOOTLOADER_INFO_ADDRESS;
  i2c4_tx_buffer[0] = reg;

  bool success = i2cMasterTransmit(&I2CD4, EEPROM_DEVICE_ADDRESS, i2c4_tx_buffer, 1, (uint8_t *)buffer,
                                   sizeof(struct bootloader_info)) == MSG_OK;
  i2cReleaseBus(&I2CD4);
  return success;
}

bool ID_EEPROM_GetBoardInfo(struct board_info *buffer) {
  i2cAcquireBus(&I2CD4);

  uint8_t reg = BOARD_INFO_ADDRESS;
  i2c4_tx_buffer[0] = reg;
  bool success = i2cMasterTransmit(&I2CD4, EEPROM_DEVICE_ADDRESS, i2c4_tx_buffer, 1, (uint8_t *)buffer,
                                   sizeof(struct board_info)) == MSG_OK;
  i2cReleaseBus(&I2CD4);

  // Checksum mismatch, fill with default values
  if (!success || checksum(buffer, sizeof(struct board_info) - 2) != buffer->checksum) {
    memset(buffer, 0, sizeof(struct board_info));
    strncpy(buffer->board_id, "N/A", sizeof(buffer->board_id));
  }

  return success;
}
bool ID_EEPROM_GetCarrierBoardInfo(struct carrier_board_info *buffer) {
  i2cAcquireBus(&I2CD4);

  uint8_t reg = CARRIER_BOARD_INFO_ADDRESS;
  i2c4_tx_buffer[0] = reg;

  bool success = i2cMasterTransmit(&I2CD4, CARRIER_EEPROM_DEVICE_ADDRESS, i2c4_tx_buffer, 1, (uint8_t *)buffer,
                                   sizeof(struct carrier_board_info)) == MSG_OK;
  i2cReleaseBus(&I2CD4);

  // Checksum mismatch, fill with default values
  if (!success || checksum(buffer, sizeof(struct carrier_board_info) - 2) != buffer->checksum) {
    memset(buffer, 0, sizeof(struct carrier_board_info));
    strncpy(buffer->board_id, "N/A", sizeof(buffer->board_id));
  }

  return success;
}
