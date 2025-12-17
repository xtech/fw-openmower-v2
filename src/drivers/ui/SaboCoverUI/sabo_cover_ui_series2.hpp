//
// Created by Apehaenger on 6/1/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_SERIES2_HPP
#define OPENMOWER_SABO_COVER_UI_SERIES2_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_series_interface.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::sabo::types;

// Series-II driver
class SaboCoverUISeries2 : public SaboCoverUISeriesInterface {
 public:
  SeriesType GetType() const override {
    return SeriesType::Series2;
  };

  uint8_t GetButtonRowMask() const override {
    // Toggle between bit 5 (0b00100000) and bit 6 (0b01000000), never 0 or both
    return (1 << (5 + (current_button_row_ & 0x01)));
  };

  // Process the received button column data dependent on the current row and advance row to the next column
  uint16_t ProcessButtonCol(const uint8_t cur_col_data) {
    // Only lower 6 bits are connected to buttons; mask them out and set bits 7-8 high
    uint8_t masked_col = (cur_col_data & 0b00111111) | 0b11000000;
    cur_btn_mask = (cur_btn_mask & (current_button_row_ ? 0xFF00 : 0x00FF)) |
                   (current_button_row_ ? masked_col : (masked_col << 8));
    current_button_row_ ^= 1;  // Toggle between 0 and 1
    return cur_btn_mask;
  };

  // Series-II = 1:1 Mapping
  uint16_t MapButtonIDToMask(const ButtonId id) const override {
    return (1 << uint16_t(id));
  }

  uint16_t AllButtonsMask() const {
    return 0b0001111101111111;
  };

 protected:
  uint8_t current_button_row_ = 0;  // Alternating button rows
  uint16_t cur_btn_mask = 0;        // Current button mask, high-active

  uint8_t MapLedIdToMask(const LedId id) const override {
    //  ENUM value matches LEDs bit position (for Series-II)
    return (1 << uint8_t(id)) & 0b11111;  // Safety mask to only use the connected LEDs
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_SERIES2_HPP
