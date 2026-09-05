#ifndef BOARD_PHY_H
#define BOARD_PHY_H

// board_ex.h pulls this in from board.h, which some projects also reach from
// assembly; guard on both spellings so it stays a no-op there.
#if !defined(_FROM_ASM_) && !defined(__ASSEMBLER__)

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Brings up whichever Ethernet PHY this board variant fits: gives the PHY's
// reset line its final configuration, then re-runs the MAC's PHY init so it
// actually resets and addresses the chip that is present.
//
// Called by InitBoardVariant() once GetBoardVariant() can answer; not meant to
// be called directly from the application.
//
// The re-run is needed because ChibiOS's halInit() drives the Ethernet MAC
// driver's init (and with it BoardPhy_Reset() / BoardPhy_GetAddress()) long
// before the ID EEPROM can be read. Both hooks deliberately do nothing while
// the variant is BOARD_VARIANT_NOT_YET_DETECTED, so that first pass is
// harmless by construction and this second one is the one that counts.
void BoardPhy_Init(void);

// Board-specific PHY reset, wired up as BOARD_PHY_RESET() in board_ex.h.
void BoardPhy_Reset(void);

// Board-specific PHY address, wired up as BOARD_PHY_ADDRESS in board_ex.h.
uint32_t BoardPhy_GetAddress(void);

#ifdef __cplusplus
}
#endif

#endif /* !defined(_FROM_ASM_) && !defined(__ASSEMBLER__) */

#endif  // BOARD_PHY_H
