//
// Created by Apehaenger on 6/1/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_SERIES2V01_HPP
#define OPENMOWER_SABO_COVER_UI_SERIES2V01_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_series2.hpp"

namespace xbot::driver::ui {

/** CoverUI Series-II driver for Carrierboard v0.1.
 *
 * We need a separate driver for CoverUI Series-II at Carrierboard v0.1, because HW v0.1 needs to latch and load in one
 * Full Duplex cycle and this shifts the button row.
 */

class SaboCoverUISeries2V01 : public SaboCoverUISeries2 {
 public:
  // Process the received button column data dependent on the current row and advance row to the next column
  uint16_t ProcessButtonCol(const uint8_t cur_col_data) {
    // Only lower 6 bits are connected to buttons; mask them out and set bits 7-8 high
    uint8_t masked_col = (cur_col_data & 0b00111111) | 0b11000000;
    // Toggle between 0 and 1 before applying column data, because HW v0.1 does latch and load in one Full Duplex cycle
    current_button_row_ ^= 1;
    cur_btn_mask = (cur_btn_mask & (current_button_row_ ? 0xFF00 : 0x00FF)) |
                   (current_button_row_ ? masked_col : (masked_col << 8));
    return cur_btn_mask;
  };
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_SERIES2V01_HPP
