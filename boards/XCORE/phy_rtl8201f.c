#include "phy_rtl8201f.h"

#include "ch.h"
#include "hal.h"

void PhyRtl8201f_ConfigureResetLine(void) {
  // board.h configures RESET_PHY (PF3) open-drain with a pull-up, which is the
  // most this board-agnostic configuration can do: on the original xcore the
  // net is also pulled up externally, because the GSW141 runs stand-alone as a
  // switch and must come out of reset without the MCU's help.
  //
  // On xcore-lite nothing else drives the net, so the pull-up is all that
  // holds the RTL8201F out of reset -- a high-impedance high that noise can
  // pull below the PHYRSTB threshold. Once the variant is known, drive the
  // line push-pull instead. ODR is already high here, so this is glitch-free,
  // and there is no contention risk: measured on hardware, the STM32's ~40k
  // internal pull-up alone brings the line to a valid high, which it could not
  // do if anything else were driving it.
  palSetLineMode(LINE_RESET_PHY, PAL_MODE_OUTPUT_PUSHPULL);
}

void PhyRtl8201f_Reset(void) {
  // Idempotent, and keeps this reset sequence correct on its own rather than
  // depending on BoardPhy_DetectVariant() having run first. Without a real
  // drive on the line, palClearLine() below would leave PHYRSTB pulled up
  // instead of asserted and the PHY would never see the reset.
  PhyRtl8201f_ConfigureResetLine();

  // RTL8201F datasheet 9.1.3 "Power On and PHY Reset Sequence": PHYRSTB is
  // active low and must be held for at least 10ms, and the PHY register set
  // isn't guaranteed accessible over MDIO until 150ms after PHYRSTB is
  // de-asserted (point B in Figure 20). Unlike the GSW141, no vendor
  // register unlock sequence is needed.
  palClearLine(LINE_RESET_PHY);
  chThdSleep(TIME_MS2I(15));
  palSetLine(LINE_RESET_PHY);
  chThdSleep(TIME_MS2I(150));
}
