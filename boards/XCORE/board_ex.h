#ifndef BOARD_EX_H
#define BOARD_EX_H

#include "board_phy.h"

// The xcore and xcore-lite boards are identical except for the Ethernet
// PHY (GSW141 vs RTL8201F). BoardPhy_GetAddress()/BoardPhy_Reset() dispatch
// to the correct one at runtime, based on the ID EEPROM's board_id -- see
// board_phy.h for why this has to happen as a second pass, after
// InitBoardVariant() has identified the board and BoardPhy_Init() has
// re-invoked macInit().
#define BOARD_PHY_ADDRESS BoardPhy_GetAddress()
#define BOARD_PHY_RESET() BoardPhy_Reset()

#define BOARD_HAS_RGB_STATUS 1
#define BOARD_HAS_RGB_HEARTBEAT 1

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
