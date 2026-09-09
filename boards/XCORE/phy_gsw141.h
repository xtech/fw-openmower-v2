#ifndef PHY_GSW141_H
#define PHY_GSW141_H

// MDIO address of the GSW141 on the xcore board.
#define PHY_GSW141_ADDRESS 31U

// Resets and configures the GSW141 PHY. Must run with the MAC's ETH clocks
// already enabled.
void PhyGsw141_Reset(void);

#endif  // PHY_GSW141_H
