//
// Created by Apehaenger on 6/1/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_SERIES2_HPP
#define OPENMOWER_SABO_COVER_UI_SERIES2_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_series_interface.hpp"

namespace xbot::driver::ui {

// Series-II driver
class SaboCoverUISeries2 : public SaboCoverUISeriesInterface {
 public:
  uint8_t GetButtonRowMask() override {
    return ((current_button_row_ + 1) << 5);  // Bit5/6 for row0/1
  };

  // Process the received button column data dependent on the current row and advance row to the next column
  uint8_t ProcessButtonCol(uint16_t cur_btn_mask, uint8_t rx_data) {
    // Buffer / Shift & buffer depending on current row, and alternate between row0 and row1
    if (current_button_row_++ == 0) {
      return (cur_btn_mask & 0xFF00) | rx_data;
    }
    return (cur_btn_mask & 0x00FF) | (rx_data << 8);
  };

  /*
  bool IsButtonPressed(ButtonID btn) {
    // Series-II button bitmask matches ButtonID ENUM
    return (btn_stable_raw_mask_ & (1 << uint8_t(btn))) == 0;  // We do have low-active buttons
  }*/

 protected:
  uint8_t current_button_row_ = 0;  // Alternating button rows

  uint8_t MapLEDIDToBit(LEDID id) const override {
    //  ENUM value matches LEDs bit position (for Series-II)
    return (1 << uint8_t(id)) & 0b11111;  // Safety mask to only use the connected LEDs
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_SERIES2_HPP
