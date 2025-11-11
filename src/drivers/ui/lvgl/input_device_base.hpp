/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file input_device_base.hpp
 * @brief Base class for LVGL input device wrappers
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-04
 */

#ifndef LVGL_INPUT_DEVICE_BASE_HPP_
#define LVGL_INPUT_DEVICE_BASE_HPP_

#include <lvgl.h>

namespace xbot::driver::ui::lvgl {

/**
 * @brief Base class for LVGL input device wrappers
 *
 * Provides a generic interface for creating and managing LVGL input devices.
 * Derived classes implement the specific button/key reading logic for their platform.
 *
 * @tparam ButtonId The button ID enum type for the specific platform
 */
template <typename ButtonId>
class InputDeviceBase {
 public:
  InputDeviceBase() = default;
  virtual ~InputDeviceBase() = default;

  /**
   * @brief Initialize the input device and register it with LVGL
   * @param type The type of input device (KEYPAD, BUTTON, ENCODER, etc.)
   * @return true if successful, false otherwise
   */
  bool Init(lv_indev_type_t type) {
    indev_ = lv_indev_create();
    if (!indev_) {
      return false;
    }

    lv_indev_set_type(indev_, type);
    lv_indev_set_read_cb(indev_, ReadCallbackWrapper);
    lv_indev_set_user_data(indev_, this);

    return true;
  }

  /**
   * @brief Get the LVGL input device instance
   * @return Pointer to lv_indev_t
   */
  lv_indev_t* GetIndev() const {
    return indev_;
  }

  /**
   * @brief Set the group for keypad navigation
   * @param group The LVGL group to associate with this input device
   */
  void SetGroup(lv_group_t* group) {
    if (indev_) {
      lv_indev_set_group(indev_, group);
    }
  }

 protected:
  /**
   * @brief Read callback that must be implemented by derived classes
   * @param data The input device data structure to fill
   *
   * Derived classes should:
   * - Check button/key states
   * - Fill data->state (LV_INDEV_STATE_PRESSED or LV_INDEV_STATE_RELEASED)
   * - For keypad: Fill data->key with the appropriate LV_KEY_* value
   * - For button: Fill data->btn_id with the button index
   */
  virtual void ReadCallback(lv_indev_data_t* data) = 0;

 private:
  /**
   * @brief Static wrapper for the LVGL read callback
   * @param indev The input device
   * @param data The input device data structure to fill
   */
  static void ReadCallbackWrapper(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* instance = static_cast<InputDeviceBase*>(lv_indev_get_user_data(indev));
    if (instance) {
      instance->ReadCallback(data);
    }
  }

  lv_indev_t* indev_ = nullptr;
};

}  // namespace xbot::driver::ui::lvgl

#endif  // LVGL_INPUT_DEVICE_BASE_HPP_
