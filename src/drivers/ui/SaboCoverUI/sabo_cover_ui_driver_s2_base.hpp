//
// Created by Apehaenger on 6/1/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_DRIVER_S2_BASE_HPP
#define OPENMOWER_SABO_COVER_UI_DRIVER_S2_BASE_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_driver_base.hpp"

namespace xbot::driver::ui {

// Base class for CoverUI Series-I/II
class SaboCoverUIDriverS2Base : virtual public SaboCoverUIDriverBase {
 public:
  // Latch LEDs as well as button-row, and load button columns
  void LatchLoad() override {
    uint8_t tx_data = current_led_mask_ | ((current_button_row_ + 1) << 5);  // Bit5/6 for row0/1
    uint8_t rx_data = LatchLoadRaw(tx_data);                                 // Latch LEDs and load button row

    // Buffer / Shift & buffer depending on current row
    if (current_button_row_ == 0) {
      button_states_raw_ = (button_states_raw_ & 0xFF00) | rx_data;
    } else {
      button_states_raw_ = (button_states_raw_ & 0x00FF) | (rx_data << 8);
    }

    // Alternate between row0 and row1
    current_button_row_ ^= 1;
  };

  void PowerOnAnimation() {
    leds_.on_mask = 0x1F;  // All on
    chThdSleepMilliseconds(400);
    leds_.on_mask = 0x00;  // All off
    chThdSleepMilliseconds(800);

    // Knight Rider
    for (int i = 0; i <= 5; i++) {
      leds_.on_mask = (1 << i) - 1;
      chThdSleepMilliseconds(80);
    }
    for (int i = 4; i >= 0; i--) {
      leds_.on_mask = (1 << i) - 1;
      chThdSleepMilliseconds(80);
    }
  }

 protected:
  uint8_t current_button_row_ = 0;  // Alternating button rows

  uint8_t MapLEDIDToBit(LEDID id) const override {
    // ENUM value matches LEDs bit position (for Series-II)
    return (1 << uint8_t(id)) & 0b11111;  // Safety mask to only use the connected LEDs
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_DRIVER_S2_BASE_HPP
