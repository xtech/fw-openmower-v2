/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_cover_ui_series1_gpio.hpp
 * @brief Template-based Sabo CoverUI Series-I Driver for GPIO Expander based Hardware
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-05-06
 */

#ifndef OPENMOWER_SABO_COVER_UI_SERIES1_GPIO_HPP
#define OPENMOWER_SABO_COVER_UI_SERIES1_GPIO_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_series_interface.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::sabo::types;

/**
 * @brief Template-based CoverUI Series-I driver for GPIO Expander based Carrierboards.
 *
 * This template is used for Carrierboard versions that use GPIO expanders (TCA9534/TCA9535)
 * for LED and button control. The template parameters allow configuration of different
 * LED bit positions across hardware revisions.
 *
 * @tparam LED_MASK Bitmask of GPIO pins used for LEDs
 * @tparam LED_OFFSET Bit offset for LED position mapping (0 for V03, 9 for V04)
 */
template <uint16_t LED_MASK, uint8_t LED_OFFSET>
class SaboCoverUISeries1GPIO : public SaboCoverUISeriesInterface {
 public:
  // clang-format off
  static constexpr uint16_t GPIO_LED_PINS = LED_MASK;

  // Button bits of GPIO Buttons expander (TCA9535)
  static constexpr uint16_t GPIO_BTNS_MASK_BTN_UP_L     = (1 << 0);
  static constexpr uint16_t GPIO_BTNS_MASK_BTN_DOWN_L   = (1 << 1);
  static constexpr uint16_t GPIO_BTNS_MASK_BTN_LEFT_L   = (1 << 2);
  static constexpr uint16_t GPIO_BTNS_MASK_BTN_RIGHT_L  = (1 << 3);
  static constexpr uint16_t GPIO_BTNS_MASK_BTN_OK_L     = (1 << 4);
  static constexpr uint16_t GPIO_BTNS_MASK_BTN_PLAY_L   = (1 << 5);
  static constexpr uint16_t GPIO_BTNS_MASK_BTN_MENUE_L  = (1 << 6);
  static constexpr uint16_t GPIO_BTNS_MASK_BTN_BACK_L   = (1 << 7);
  static constexpr uint16_t GPIO_BTNS_MASK_BTN_SELECT_L = (1 << 8);  // Series-I Button "Select"
  static constexpr uint16_t GPIO_BTNS_MASK_BTN_ALL_L    = (GPIO_BTNS_MASK_BTN_UP_L | GPIO_BTNS_MASK_BTN_DOWN_L |
                                                           GPIO_BTNS_MASK_BTN_LEFT_L | GPIO_BTNS_MASK_BTN_RIGHT_L |
                                                           GPIO_BTNS_MASK_BTN_OK_L | GPIO_BTNS_MASK_BTN_PLAY_L |
                                                           GPIO_BTNS_MASK_BTN_MENUE_L | GPIO_BTNS_MASK_BTN_BACK_L |
                                                           GPIO_BTNS_MASK_BTN_SELECT_L);
  // clang-format on

  SeriesType GetType() const override {
    return SeriesType::Series1;
  }

  uint8_t GetButtonRowMask() const override {
    return 0;  // Series-I doesn't has button rows
  }

  uint16_t ProcessButtonCol(const uint8_t cur_col_data) override {
    return cur_col_data;  // Series-I doesn't has button rows and thus doesn't use ProcessButtonCol()
  }

  // Series-I individual mapping
  uint16_t MapButtonIDToMask(const ButtonId id) const override {
    switch (id) {
      case ButtonId::UP: return GPIO_BTNS_MASK_BTN_UP_L;
      case ButtonId::DOWN: return GPIO_BTNS_MASK_BTN_DOWN_L;
      case ButtonId::LEFT: return GPIO_BTNS_MASK_BTN_LEFT_L;
      case ButtonId::RIGHT: return GPIO_BTNS_MASK_BTN_RIGHT_L;
      case ButtonId::OK: return GPIO_BTNS_MASK_BTN_OK_L;
      case ButtonId::PLAY: return GPIO_BTNS_MASK_BTN_PLAY_L;
      case ButtonId::MENU: return GPIO_BTNS_MASK_BTN_MENUE_L;
      case ButtonId::BACK: return GPIO_BTNS_MASK_BTN_BACK_L;
      case ButtonId::S1_SELECT: return GPIO_BTNS_MASK_BTN_SELECT_L;
      default: return 0;  // Doesn't exists for Series-I
    }
  }

  uint16_t AllButtonsMask() const {
    return GPIO_BTNS_MASK_BTN_ALL_L;
  }

 protected:
  uint16_t MapLedIdToMask(const LedId id) const override {
    return 1 << (static_cast<uint8_t>(id) +
                 LED_OFFSET);  // LED_OFFSET shifted 1:1 mapping of LedId to GPIO_LED_PINS bitmask
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_SERIES1_GPIO_HPP
