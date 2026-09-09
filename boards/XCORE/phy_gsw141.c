#include "phy_gsw141.h"

#include "ch.h"
#include "hal.h"

// Moved verbatim from the old BOARD_PHY_RESET() macro in board_ex.h.
//
// On a real power-on reset (or if the previous attempt left the PHY in an
// unexpected state), pulses RESET_PHY and re-applies GSW141's
// vendor-specific extended-register unlock sequence. On a warm MCU reset
// (e.g. rebooting into the bootloader from the user program) the PHY was
// never powered down, so this skips straight to verifying the extended
// registers are still as expected, saving the reset/settle delay.
void PhyGsw141_Reset(void) {
  uint32_t i;
  bool valid = true;
  palSetLine(LINE_HEARTBEAT_LED_RED);

  do {
    bool was_hardware_reset = READ_REG(RCC->RSR) & (RCC_RSR_PORRSTF);
    if (was_hardware_reset || !valid) {
      i = STM32_SYS_CK / 100;
      while (i-- > 0) {
        asm("nop");
      }

      palToggleLine(LINE_HEARTBEAT_LED_RED);
      palClearLine(LINE_RESET_PHY);
      i = STM32_SYS_CK / 20;
      while (i-- > 0) {
        asm("nop");
      }
      palSetLine(LINE_RESET_PHY);
      i = STM32_SYS_CK / 20;
      while (i-- > 0) {
        asm("nop");
      }
      mii_write(&ETHD1, 0x1F, 0xF100);
      mii_write(&ETHD1, 0x00, 0x40B3);
      mii_write(&ETHD1, 0x1F, 0xF410);
      mii_write(&ETHD1, 0x00, 0x2A05);
    }

    valid = true;
    mii_write(&ETHD1, 0x1f, 0xFA00);
    uint32_t value = mii_read(&ETHD1, 0x00);
    bool is_initialized = value & 0x1;
    valid &= is_initialized;
    if (valid) {
      mii_write(&ETHD1, 0x1F, 0xF100);
      valid &= mii_read(&ETHD1, 0x1F) == 0xF100;
      valid &= mii_read(&ETHD1, 0x00) == 0x40B3;
    }
    if (valid) {
      mii_write(&ETHD1, 0x1F, 0xF410);
      valid &= mii_read(&ETHD1, 0x1F) == 0xF410;
      valid &= mii_read(&ETHD1, 0x00) == 0x2A05;
    }
  } while (!valid);

  palSetLine(LINE_HEARTBEAT_LED_RED);

  WRITE_REG(RCC->RSR, RCC_RSR_RMVF);
}
