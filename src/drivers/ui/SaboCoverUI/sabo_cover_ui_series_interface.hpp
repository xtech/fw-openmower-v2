//
// Created by Apehaenger on 6/4/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_SERIES_INTERFACE_HPP
#define OPENMOWER_SABO_COVER_UI_SERIES_INTERFACE_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_defs.hpp"

namespace xbot::driver::ui {

using namespace sabo;

class SaboCoverUISeriesInterface {
 public:
  virtual ~SaboCoverUISeriesInterface() = default;

  virtual SeriesType GetType() const = 0;

  // Get the current button row mask
  virtual uint8_t GetButtonRowMask() const = 0;

  // Process the received button column data dependent on the current row and advance row to the next column
  virtual uint16_t ProcessButtonCol(const uint8_t cur_col_data) = 0;

  // Map ButtonID to a corresponding bitmask of the connected CoverUI Series type
  virtual uint16_t MapButtonIDToMask(const ButtonID id) const = 0;
  virtual uint16_t AllButtonsMask() const = 0;

  virtual uint8_t MapLEDIDToMask(const LEDID id) const = 0;
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_SERIES_INTERFACE_HPP
