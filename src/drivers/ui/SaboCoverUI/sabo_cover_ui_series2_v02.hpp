/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_cover_ui_series2_v02.hpp
 * @brief Sabo CoverUI Series-II Driver for Hardware v0.2
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-03-03
 */

#ifndef OPENMOWER_SABO_COVER_UI_SERIES2_V02_HPP
#define OPENMOWER_SABO_COVER_UI_SERIES2_V02_HPP

#include "sabo_cover_ui_series2.hpp"

namespace xbot::driver::ui {

/** CoverUI Series-II driver for Carrierboard v0.2
 *
 * We need a separate driver for CoverUI Series-II at Carrierboard v0.2,
 * because HW v0.2 latches and load in two separate cycles and this shifts the button row.
 */
class SaboCoverUISeries2V02 : public SaboCoverUISeries2 {
 public:
  // HW v0.2 latches and load in two separate cycles, so we need invert the button row toggle
  uint8_t GetButtonRowMask() const override {
    // Toggle between bit 5 (0b00100000) and bit 6 (0b01000000), never 0 or both
    return (1 << (5 + (1 ^ (current_button_row_ & 0x01))));
  };
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_SERIES2_V02_HPP
