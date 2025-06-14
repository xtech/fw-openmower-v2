//
// Created by Apehaenger on 6/14/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_SERIES1_HPP
#define OPENMOWER_SABO_COVER_UI_SERIES1_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_series_interface.hpp"

namespace xbot::driver::ui {

// Series-I driver
class SaboCoverUISeries1 : public SaboCoverUISeriesInterface {
 public:
  SeriesType GetType() const override {
    return SeriesType::Series1;
  };

  uint8_t GetButtonRowMask() const override {
    return ((current_button_row_ + 1) << 5);  // Bit5/6 for row0/1
  };

  // Process the received button column data dependent on the current row and advance row to the next column
  uint8_t ProcessButtonCol(const uint16_t cur_btn_mask, const uint8_t rx_data) {
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

  uint8_t MapLEDIDToBit(const LEDID id) const override {
    switch (id) {
      case LEDID::AUTO: return 1 << 3;      // Bit 3
      case LEDID::HOME: return 1 << 4;      // Bit 4
      case LEDID::START_RD: return 1 << 5;  // Bit 5
      case LEDID::MOWING: return 1 << 6;    // Bit 6
      case LEDID::START_GN: return 1 << 7;  // Bit 7
      default: return 0;
    }
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_SERIES1_HPP
