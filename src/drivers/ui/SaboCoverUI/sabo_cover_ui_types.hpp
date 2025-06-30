//
// Created by Apehaenger on 5/31/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_TYPES_HPP
#define OPENMOWER_SABO_COVER_UI_TYPES_HPP

#include "ch.h"

namespace xbot::driver::ui::sabo {

struct SPICfg {
  SPIDriver* instance = nullptr;  // SPI-Instance like &SPID1
  struct {
    ioline_t sck = PAL_NOLINE;   // Clock (SCK/CLK)
    ioline_t miso = PAL_NOLINE;  // Master In Slave Out (MISO/RX)
    ioline_t mosi = PAL_NOLINE;  // Master Out Slave In (MOSI/TX)
    ioline_t cs = PAL_NOLINE;    // Chip Select (/CS)
  } pins;
};

struct CaboCfg {
  SPICfg spi;  // SPI configuration for the LCD
               // Shift Register Pins for HC165 parallel load and HC595 or HEF4794BT latch/load
  struct {
    ioline_t latch_load;
    ioline_t oe = PAL_NOLINE;
    ioline_t btn_cs = PAL_NOLINE;  // Button Chip Select (74HC165 /CE) only avail for Series-II CoverUI
    ioline_t inp_cs = PAL_NOLINE;  // Input Chip Select (74HC165 /CE) only avail in HW >= v0.2
  } pins;
};

struct LCDCfg {
  SPICfg spi;  // SPI configuration for the LCD
  struct {
    ioline_t dc = PAL_NOLINE;   // Data/Command (D/C)
    ioline_t rst = PAL_NOLINE;  // Reset (/RST)
    ioline_t backlight = PAL_NOLINE;
  } pins;
};

struct CoverUICfg {
  CaboCfg cabo_cfg;  // SPI and control pins for the CoverUI (LEDs, Buttons, Display
  LCDCfg lcd_cfg;    // SPI and control pins for the CoverUI LCD (Chip Select, Data/Command, Reset)
};

// Same bit order as in CoverUI Series-II (HEF4794BT). Series-I driver need to map the bits
enum class LEDID : uint8_t { AUTO = 0, MOWING, HOME, PLAY_GN, PLAY_RD };
enum class LEDMode { OFF, ON, BLINK_SLOW, BLINK_FAST };

// Same bit order as in CoverUI Series-II (74HC165, Row 0+1). Series-I driver need to translate the bits
enum class ButtonID {
  UP = 0,
  DOWN,
  LEFT,
  RIGHT,
  OK,
  PLAY,
  S1_SELECT,
  MENU = 8,
  BACK,
  S2_AUTO,
  S2_MOW,
  S2_HOME,
  _FIRST = UP,
  _LAST = S2_HOME
};

enum class SeriesType { Series1, Series2 };

}  // namespace xbot::driver::ui::sabo

#endif  // OPENMOWER_SABO_COVER_UI_TYPES_HPP
