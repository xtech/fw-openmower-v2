//
// Created by clemens on 7/7/24.
//

#ifndef ID_EEPROM_H
#define ID_EEPROM_H

#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"

#define BOOTLOADER_INFO_ADDRESS 0x00
#define BOARD_INFO_ADDRESS 0x40
#define CARRIER_BOARD_INFO_ADDRESS 0x00

#pragma pack(push, 1)
struct bootloader_info {
  uint8_t image_present;
  uint32_t image_size;
  uint32_t image_sha256[8];
} __attribute__((packed));
#pragma pack(pop)

#pragma pack(push, 1)
struct board_info {
  uint8_t board_info_version;
  char board_id[32];
  uint8_t version_major;
  uint8_t version_minor;
  uint16_t version_patch;
  uint16_t checksum;
} __attribute__((packed));
#pragma pack(pop)

#pragma pack(push, 1)
struct carrier_board_info {
  uint8_t board_info_version;
  char board_id[32];
  uint8_t version_major;
  uint8_t version_minor;
  uint16_t version_patch;
  uint16_t checksum;
} __attribute__((packed));
#pragma pack(pop)

void ID_EEPROM_Init(void);
bool ID_EEPROM_GetMacAddress(uint8_t* buf, size_t buflen);
bool ID_EEPROM_GetBootloaderInfo(struct bootloader_info* buffer);
bool ID_EEPROM_GetBoardInfo(struct board_info* buffer);
bool ID_EEPROM_GetCarrierBoardInfo(struct carrier_board_info* buffer);
bool ID_EEPROM_SaveBootloaderInfo(struct bootloader_info* buffer);

#endif  // ID_EEPROM_H
