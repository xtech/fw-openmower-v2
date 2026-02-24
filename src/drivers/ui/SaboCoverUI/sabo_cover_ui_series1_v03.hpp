//
// Created by Apehaenger on 6/14/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_SERIES1_V03_HPP
#define OPENMOWER_SABO_COVER_UI_SERIES1_V03_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_series_interface.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::sabo::types;

/** CoverUI Series-I driver for Carrierboard v0.3.
 *
 * Yes, we need a separate driver for CoverUI Series-I for each Carrierboard implementation, because CoverUI Series-I is
 * fully dump and highly depends on the Carrierboard wiring.
 */
class SaboCoverUISeries1V03 : public SaboCoverUISeriesInterface {
 public:
  // clang-format off
  static constexpr uint8_t GPIO_LED_PINS     = 0b00011111;  // The lower 5 bits are used for LEDs, in the same order as sabo::types::LedId

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
  static constexpr uint16_t GPIO_BTNS_MASK_BTN_ALL_L    = (GPIO_BTNS_MASK_BTN_SELECT_L | GPIO_BTNS_MASK_BTN_MENUE_L | GPIO_BTNS_MASK_BTN_PLAY_L | GPIO_BTNS_MASK_BTN_RIGHT_L | GPIO_BTNS_MASK_BTN_BACK_L | GPIO_BTNS_MASK_BTN_LEFT_L | GPIO_BTNS_MASK_BTN_UP_L | GPIO_BTNS_MASK_BTN_DOWN_L | GPIO_BTNS_MASK_BTN_OK_L);
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
  uint8_t MapLedIdToMask(const LedId id) const override {
    return 1 << static_cast<uint8_t>(id);  // Direct 1:1 mapping of LedId to GPIO_LED_PINS bitmask
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_SERIES1_V03_HPP
