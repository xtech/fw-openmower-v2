#ifndef BOARD_EX_H
#define BOARD_EX_H

#define BOARD_PHY_ADDRESS 31
#define BOARD_PHY_RESET()                                               \
  do {                                                                  \
    uint32_t i;                                                         \
    bool valid = true;                                                  \
    palSetLine(LINE_HEARTBEAT_LED_RED);                                 \
                                                                        \
    do {                                                                \
      bool was_hardware_reset = READ_REG(RCC->RSR) & (RCC_RSR_PORRSTF); \
      if (was_hardware_reset || !valid) {                               \
        i = STM32_SYS_CK / 100;                                         \
        while (i-- > 0) {                                               \
          asm("nop");                                                   \
        }                                                               \
                                                                        \
        palToggleLine(LINE_HEARTBEAT_LED_RED);                          \
        palClearLine(LINE_RESET_PHY);                                   \
        i = STM32_SYS_CK / 20;                                          \
        while (i-- > 0) {                                               \
          asm("nop");                                                   \
        }                                                               \
        palSetLine(LINE_RESET_PHY);                                     \
        i = STM32_SYS_CK / 20;                                          \
        while (i-- > 0) {                                               \
          asm("nop");                                                   \
        }                                                               \
        mii_write(&ETHD1, 0x1F, 0xF100);                                \
        mii_write(&ETHD1, 0x00, 0x40B3);                                \
        mii_write(&ETHD1, 0x1F, 0xF410);                                \
        mii_write(&ETHD1, 0x00, 0x2A05);                                \
      }                                                                 \
                                                                        \
      valid = true;                                                     \
      mii_write(&ETHD1, 0x1f, 0xFA00);                                  \
      uint32_t value = mii_read(&ETHD1, 0x00);                          \
      bool is_initialized = value & 0x1;                                \
      valid &= is_initialized;                                          \
      if (valid) {                                                      \
        mii_write(&ETHD1, 0x1F, 0xF100);                                \
        valid &= mii_read(&ETHD1, 0x1F) == 0xF100;                      \
        valid &= mii_read(&ETHD1, 0x00) == 0x40B3;                      \
      }                                                                 \
      if (valid) {                                                      \
        mii_write(&ETHD1, 0x1F, 0xF410);                                \
        valid &= mii_read(&ETHD1, 0x1F) == 0xF410;                      \
        valid &= mii_read(&ETHD1, 0x00) == 0x2A05;                      \
      }                                                                 \
    } while (!valid);                                                   \
                                                                        \
    palSetLine(LINE_HEARTBEAT_LED_RED);                                 \
                                                                        \
    WRITE_REG(RCC->RSR, RCC_RSR_RMVF);                                  \
  } while (0)

#define BOARD_HAS_RGB_STATUS 1
#define BOARD_HAS_RGB_HEARTBEAT 1
#define BOARD_STATUS_LED_INVERTED
#define BOARD_HEARTBEAT_LED_INVERTED

#define BOARD_HAS_EEPROM 1
#define EEPROM_DEVICE_ADDRESS 0b1010011
#define CARRIER_EEPROM_DEVICE_ADDRESS 0b1010000

// Define the fallback IP settings for this board (if DHCP fails)
// 10.0.0.254
#define FALLBACK_IP_ADDRESS 0x0A0000FE
// 10.0.0.1
#define FALLBACK_GATEWAY 0x0A000001
// 255.255.255.0
#define FALLBACK_NETMASK 0xFFFFFF00

// Flash information for the bootloader
#define BOOT_ADDRESS 0x8020000
// Available flash pages for user program
#define FLASH_PAGE_COUNT 7
// Size of each flash page in bytes
#define FLASH_PAGE_SIZE_BYTES 0x20000
// Size of flash memory for the user program
#define PROGRAM_FLASH_SIZE_BYTES (FLASH_PAGE_COUNT * FLASH_PAGE_SIZE_BYTES)

#endif