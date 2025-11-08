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

#include "../../SaboCoverUI/sabo_cover_ui_defs.hpp"
#include "../input_device_base.hpp"

namespace xbot::driver::ui {

using namespace xbot::driver::ui::sabo;

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
class SaboInputDeviceKeypad : public lvgl::InputDeviceBase<ButtonID> {
 public:
  using ButtonCheckCallback = etl::delegate<bool(ButtonID)>;

  explicit SaboInputDeviceKeypad(ButtonCheckCallback callback = ButtonCheckCallback())
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

    // Check for pressed buttons and map to LVGL keys
    // Priority order: UP, DOWN, LEFT, RIGHT, OK, BACK

    if (button_check_callback_(ButtonID::UP)) {
      data->key = LV_KEY_PREV;  // Navigate to previous item in group
      data->state = LV_INDEV_STATE_PRESSED;
      last_key_ = LV_KEY_PREV;
      return;
    }

    if (button_check_callback_(ButtonID::DOWN)) {
      data->key = LV_KEY_NEXT;  // Navigate to next item in group
      data->state = LV_INDEV_STATE_PRESSED;
      last_key_ = LV_KEY_NEXT;
      return;
    }

    if (button_check_callback_(ButtonID::LEFT)) {
      data->key = LV_KEY_LEFT;  // Edit focused widget
      data->state = LV_INDEV_STATE_PRESSED;
      last_key_ = LV_KEY_LEFT;
      return;
    }

    if (button_check_callback_(ButtonID::RIGHT)) {
      data->key = LV_KEY_RIGHT;  // Edit focused widget
      data->state = LV_INDEV_STATE_PRESSED;
      last_key_ = LV_KEY_RIGHT;
      return;
    }

    if (button_check_callback_(ButtonID::OK)) {
      data->key = LV_KEY_ENTER;
      data->state = LV_INDEV_STATE_PRESSED;
      last_key_ = LV_KEY_ENTER;
      return;
    }

    if (button_check_callback_(ButtonID::BACK)) {
      data->key = LV_KEY_ESC;
      data->state = LV_INDEV_STATE_PRESSED;
      last_key_ = LV_KEY_ESC;
      return;
    }

    // No button pressed - report last key as released
    data->key = last_key_;
    data->state = LV_INDEV_STATE_RELEASED;
  }

 private:
  ButtonCheckCallback button_check_callback_;  // Button check callback delegate
  uint32_t last_key_ = 0;                      // Track last pressed key for release event
};

}  // namespace xbot::driver::ui

#endif  // SABO_INPUT_DEVICE_KEYPAD_HPP_
