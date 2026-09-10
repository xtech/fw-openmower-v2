#ifndef PHY_RTL8201F_H
#define PHY_RTL8201F_H

// MDIO address of the RTL8201F on xcore-lite. The PHY latches its address
// from the LED0/LED1 strapping pins (RTL8201F datasheet Table 7); both are
// tied to GND on this board, which selects address 0. Confirmed on hardware:
// PHYSID1/PHYSID2 read 0x001C/0xC816 at address 0, and all of 1..31 read
// 0xFFFF.
#define PHY_RTL8201F_ADDRESS 0U

// Switches RESET_PHY from the board-agnostic open-drain-plus-pull-up that
// board.h sets up to a push-pull drive, so the de-asserted level is actively
// held rather than left high-impedance. Call once the board is known to be an
// xcore-lite; harmless to repeat.
void PhyRtl8201f_ConfigureResetLine(void);

// Resets the RTL8201F PHY via RESET_PHY. Must run with the MAC's ETH clocks
// already enabled, and with the RTOS running (uses chThdSleep()).
void PhyRtl8201f_Reset(void);

#endif  // PHY_RTL8201F_H
