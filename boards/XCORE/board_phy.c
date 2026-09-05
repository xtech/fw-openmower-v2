#include "board_phy.h"

#include "board_variant.h"
#include "ch.h"
#include "hal.h"
#include "phy_gsw141.h"
#include "phy_rtl8201f.h"

void BoardPhy_Init(void) {
  // Give the RTL8201F's reset line its final configuration. board.h can only
  // set up a configuration that is safe on both boards, so it leaves RESET_PHY
  // open-drain with a pull-up; that is enough to hold the PHY out of reset,
  // but a weak, high-impedance high is exactly what stray coupling can drag
  // low. Driving it push-pull removes that risk for the rest of the run.
  if (GetBoardVariant() == BOARD_VARIANT_XCORE_LITE) {
    PhyRtl8201f_ConfigureResetLine();
  }

  // Re-run the MAC's PHY init, this time with the variant known, so
  // BoardPhy_Reset() / BoardPhy_GetAddress() below do their real work.
  macInit();
}

void BoardPhy_Reset(void) {
  switch (GetBoardVariant()) {
    case BOARD_VARIANT_XCORE_LITE:
      PhyRtl8201f_Reset();
      break;
    case BOARD_VARIANT_XCORE:
      PhyGsw141_Reset();
      break;
    case BOARD_VARIANT_NOT_YET_DETECTED:
    default:
      // halInit()'s automatic MAC init, before the ID EEPROM could be read.
      // BoardPhy_Init() runs the real reset later.
      break;
  }
}

uint32_t BoardPhy_GetAddress(void) {
  switch (GetBoardVariant()) {
    case BOARD_VARIANT_XCORE_LITE:
      return PHY_RTL8201F_ADDRESS;
    case BOARD_VARIANT_XCORE:
      return PHY_GSW141_ADDRESS;
    case BOARD_VARIANT_NOT_YET_DETECTED:
    default:
      return 0U;
  }
}
