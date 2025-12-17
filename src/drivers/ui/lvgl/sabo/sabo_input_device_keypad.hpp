/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_input_device_keypad.hpp
 * @brief Keypad input device for Sabo Cover UI
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-04
 */

#ifndef SABO_INPUT_DEVICE_KEYPAD_HPP_
#define SABO_INPUT_DEVICE_KEYPAD_HPP_

#include "../input_device_base.hpp"
#include "robots/include/sabo_common.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::sabo::types;

/**
 * @brief Keypad input device for Sabo Cover UI
 *
 * Maps Sabo button presses to LVGL keypad keys for navigation:
 * - UP → LV_KEY_PREV (navigate to previous item in group)
 * - DOWN → LV_KEY_NEXT (navigate to next item in group)
 * - LEFT → LV_KEY_LEFT (edit focused widget)
 * - RIGHT → LV_KEY_RIGHT (edit focused widget)
 * - OK → LV_KEY_ENTER (activate focused item)
 * - BACK → LV_KEY_ESC (cancel/back)
 */
class SaboInputDeviceKeypad : public lvgl::InputDeviceBase<ButtonId> {
 public:
  using ButtonCheckCallback = etl::delegate<bool(ButtonId)>;

  explicit SaboInputDeviceKeypad(const ButtonCheckCallback& callback = ButtonCheckCallback())
      : button_check_callback_(callback) {
  }

  /**
   * @brief Initialize the keypad input device
   * @return true if successful, false otherwise
   */
  bool Init() {
    return InputDeviceBase::Init(LV_INDEV_TYPE_KEYPAD);
  }

 protected:
  /**
   * @brief Read callback implementation for Sabo buttons
   * @param data The input device data structure to fill
   */
  void ReadCallback(lv_indev_data_t* data) override {
    if (!button_check_callback_) {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }

    // Button mapping with priority order
    static constexpr etl::array<etl::pair<ButtonId, uint32_t>, 6> button_map = {{
        {ButtonId::UP, LV_KEY_PREV},
        {ButtonId::DOWN, LV_KEY_NEXT},
        {ButtonId::LEFT, LV_KEY_LEFT},
        {ButtonId::RIGHT, LV_KEY_RIGHT},
        {ButtonId::OK, LV_KEY_ENTER},
        {ButtonId::BACK, LV_KEY_ESC},
    }};

    for (const auto& mapping : button_map) {
      if (button_check_callback_(mapping.first)) {
        data->key = mapping.second;
        data->state = LV_INDEV_STATE_PRESSED;
        last_key_ = mapping.second;
        return;
      }
    }

    // No button pressed - report last key as released
    data->key = last_key_;
    data->state = LV_INDEV_STATE_RELEASED;
  }

 private:
  const ButtonCheckCallback button_check_callback_;  // Button check callback delegate
  uint32_t last_key_ = 0;                            // Track last pressed key for release event
};

}  // namespace xbot::driver::ui

#endif  // SABO_INPUT_DEVICE_KEYPAD_HPP_
