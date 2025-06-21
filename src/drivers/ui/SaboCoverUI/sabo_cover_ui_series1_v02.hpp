//
// Created by Apehaenger on 6/14/25.
//

#ifndef OPENMOWER_SABO_COVER_UI_SERIES1_V02_HPP
#define OPENMOWER_SABO_COVER_UI_SERIES1_V02_HPP

#include "ch.h"
#include "hal.h"
#include "sabo_cover_ui_series_interface.hpp"

namespace xbot::driver::ui {

/** CoverUI Series-I driver for Carrierboard v0.2.
 *
 * Yes, we need a separate driver for CoverUI Series-I for each Carrierboard implementation, because CoverUI Series-I is
 * fully stupid and highly depends on the Carrierboard wiring.
 * And because I messed up the control pins in HW v0.2 it will be most likely not compatible for later Carrierboards.
 */
class SaboCoverUISeries1V02 : public SaboCoverUISeriesInterface {
 public:
  // clang-format off
  // LED bits of 74HC595 shift register
  static constexpr uint8_t SR_MASK_LED_AUTO_L    = (1 << 3);
  static constexpr uint8_t SR_MASK_LED_HOME_L    = (1 << 4);
  static constexpr uint8_t SR_MASK_LED_PLAY_RD_L = (1 << 5);
  static constexpr uint8_t SR_MASK_LED_MOW_L     = (1 << 6);
  static constexpr uint8_t SR_MASK_LED_PLAY_GN_L = (1 << 7);
  static constexpr uint8_t SR_MASK_LED_ALL_L     = (SR_MASK_LED_AUTO_L | SR_MASK_LED_HOME_L | SR_MASK_LED_PLAY_RD_L | SR_MASK_LED_MOW_L | SR_MASK_LED_PLAY_GN_L);

  // Button bits of 74HC165 parallel-input shift register
  static constexpr uint16_t INP_MASK_BTN_SELECT_L = (1 << 0);  // Series-I Button "Select"
  static constexpr uint16_t INP_MASK_BTN_MENUE_L  = (1 << 1);
  static constexpr uint16_t INP_MASK_BTN_PLAY_L   = (1 << 3);
  static constexpr uint16_t INP_MASK_BTN_RIGHT_L  = (1 << 4);
  static constexpr uint16_t INP_MASK_BTN_BACK_L   = (1 << 5);
  static constexpr uint16_t INP_MASK_BTN_LEFT_L   = (1 << 6);
  static constexpr uint16_t INP_MASK_BTN_UP_L     = (1 << 7);
  static constexpr uint16_t INP_MASK_BTN_DOWN_L   = (1 << 8);
  static constexpr uint16_t INP_MASK_BTN_OK_L     = (1 << 10); // Modified JP2
  static constexpr uint16_t INP_MASK_BTN_ALL_L    = (INP_MASK_BTN_SELECT_L | INP_MASK_BTN_MENUE_L | INP_MASK_BTN_PLAY_L | INP_MASK_BTN_RIGHT_L | INP_MASK_BTN_BACK_L | INP_MASK_BTN_LEFT_L | INP_MASK_BTN_UP_L | INP_MASK_BTN_DOWN_L | INP_MASK_BTN_OK_L);
  // clang-format on

  SeriesType GetType() const override {
    return SeriesType::Series1;
  };

  uint8_t GetButtonRowMask() const override {
    return 0;  // Series-I doesn't has button rows
  };

  uint16_t ProcessButtonCol(const uint8_t cur_col_data) override {
    return cur_col_data;  // Series-I doesn't has button rows and thus doesn't use ProcessButtonCol()
  };

  // Series-I individual mapping
  uint16_t MapButtonIDToMask(const ButtonID id) const override {
    switch (id) {
      case ButtonID::UP: return INP_MASK_BTN_UP_L;
      case ButtonID::DOWN: return INP_MASK_BTN_DOWN_L;
      case ButtonID::LEFT: return INP_MASK_BTN_LEFT_L;
      case ButtonID::RIGHT: return INP_MASK_BTN_RIGHT_L;
      case ButtonID::OK: return INP_MASK_BTN_OK_L;
      case ButtonID::PLAY: return INP_MASK_BTN_PLAY_L;
      case ButtonID::MENU: return INP_MASK_BTN_MENUE_L;
      case ButtonID::BACK: return INP_MASK_BTN_BACK_L;
      case ButtonID::S1_SELECT: return INP_MASK_BTN_SELECT_L;
      default: return 0;  // Doesn't exists for Series-I
    }
  }

 protected:
  uint8_t MapLEDIDToMask(const LEDID id) const override {
    switch (id) {
      case LEDID::AUTO: return SR_MASK_LED_AUTO_L;
      case LEDID::HOME: return SR_MASK_LED_HOME_L;
      case LEDID::PLAY_RD: return SR_MASK_LED_PLAY_RD_L;
      case LEDID::MOWING: return SR_MASK_LED_MOW_L;
      case LEDID::PLAY_GN: return SR_MASK_LED_PLAY_GN_L;
      default: return 0;
    }
  }
};

}  // namespace xbot::driver::ui
#endif  // OPENMOWER_SABO_COVER_UI_SERIES1_V02_HPP
