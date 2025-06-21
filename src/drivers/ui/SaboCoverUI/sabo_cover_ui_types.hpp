//
// Created by Apehaenger on 5/31/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_TYPES_HPP
#define OPENMOWER_SABO_COVER_UI_TYPES_HPP

#include "ch.h"

namespace xbot::driver::ui::sabo {

struct DriverConfig {
  SPIDriver* spi_instance;  // SPI-Instance like &SPID1
  struct {
    ioline_t sck;   // Clock (SCK/CLK)
    ioline_t miso;  // Master In Slave Out (MISO/RX)
    ioline_t mosi;  // Master Out Slave In (MOSI/TX)
  } spi_pins;

  struct {
    ioline_t latch_load;
    ioline_t oe;
    ioline_t btn_cs;  // Button Chip Select (74HC165 /CE) only avail for Series-II CoverUI
    ioline_t inp_cs;  // Input Chip Select (74HC165 /CE) only avail in HW >= v0.2
  } control_pins;
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
