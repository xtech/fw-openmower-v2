#ifndef BOARD_EX_H
#define BOARD_EX_H

#define BOARD_PHY_ADDRESS 31
#define BOARD_PHY_RESET()                      \
  do {                                         \
    uint8_t tries = 0;                         \
    uint32_t i = 0;                            \
    while (1) {                                \
      mii_write(&ETHD1, 0x1f, 0xFA00);         \
      uint32_t value = mii_read(&ETHD1, 0x00); \
      if (value & 0x1) {                       \
        break;                                 \
      }                                        \
      i = STM32_SYS_CK / 1000;                 \
      while (i-- > 0) {                        \
        asm("nop");                            \
      };                                       \
      tries++;                                 \
      if (tries == 100) {                      \
        mii_write(&ETHD1, 0x1F, 0xFA00);       \
        mii_write(&ETHD1, 0x01, 0xFFFF);       \
        i = STM32_SYS_CK / 10;                 \
        while (i-- > 0) {                      \
          asm("nop");                          \
        };                                     \
      }                                        \
      if (tries > 200) {                       \
        palClearLine(LINE_RESET_PHY);          \
        i = STM32_SYS_CK / 10;                 \
        while (i-- > 0) {                      \
          asm("nop");                          \
        };                                     \
        palSetLine(LINE_RESET_PHY);            \
        tries = 0;                             \
      }                                        \
    }                                          \
    i = STM32_SYS_CK / 10;                     \
    while (i-- > 0) {                          \
      asm("nop");                              \
    };                                         \
                                               \
    mii_write(&ETHD1, 0x1F, 0xF100);           \
    mii_write(&ETHD1, 0x00, 0x40B3);           \
    mii_write(&ETHD1, 0x1F, 0xF410);           \
    mii_write(&ETHD1, 0x00, 0x2A05);           \
  } while (0)

// eeprom address = 0b1010011
#define EEPROM_DEVICE_ADDRESS 0x53
// carrier eeprom address = 0b1010000
#define CARRIER_EEPROM_DEVICE_ADDRESS 0x50

// Define the fallback IP settings for this board (if DHCP fails)
// 10.0.0.254
#define FALLBACK_IP_ADDRESS 0x0A0000FE
// 10.0.0.1
#define FALLBACK_GATEWAY 0x0A000001
// 255.255.255.0
#define FALLBACK_NETMASK 0xFFFFFF00


#define SPID_IMU SPID3
#ifndef __ASSEMBLER__
void initBoardPeriphs(void);
#endif
#endif