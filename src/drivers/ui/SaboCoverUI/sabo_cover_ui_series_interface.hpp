//
// Created by Apehaenger on 6/4/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_SERIES_INTERFACE_HPP
#define OPENMOWER_SABO_COVER_UI_SERIES_INTERFACE_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_types.hpp"

namespace xbot::driver::ui {

using sabo::LEDID;
using sabo::SeriesType;

class SaboCoverUISeriesInterface {
 public:
  virtual ~SaboCoverUISeriesInterface() = default;

  virtual SeriesType GetType() const = 0;

  // Get the current button row mask
  virtual uint8_t GetButtonRowMask() const = 0;

  // Process the received button column data dependent on the current row and advance row to the next column
  virtual uint8_t ProcessButtonCol(const uint16_t cur_btn_mask, const uint8_t rx_data) = 0;

  virtual uint8_t MapLEDIDToBit(const LEDID id) const = 0;
  // virtual bool IsButtonPressed(uint16_t stable_mask, ButtonID btn) const = 0;
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_SERIES_INTERFACE_HPP
