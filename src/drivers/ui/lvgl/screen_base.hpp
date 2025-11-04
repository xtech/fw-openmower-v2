/*
 * OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file screen_base.hpp
 * @brief Base class for all screens with button handling support
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-03
 */

#ifndef LVGL_SCREEN_BASE_HPP_
#define LVGL_SCREEN_BASE_HPP_

#include <lvgl.h>

namespace xbot::driver::ui::lvgl {

template <typename ScreenId, typename ButtonId = void>
class ScreenBase {
 public:
  explicit ScreenBase(ScreenId screen_id) : screen_id_(screen_id){};
  virtual ~ScreenBase() = default;

  virtual void Create(lv_color_t bg_color = lv_color_white()) {
    screen_ = lv_obj_create(NULL);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen_, bg_color, LV_PART_MAIN);
  };

  virtual void Show() {
    if (screen_) lv_screen_load(screen_);
  };

  /**
   * @brief Handle button press events
   * @param button_id The ID of the button that was pressed
   * @return true if the button was handled by this screen, false otherwise
   *
   * Override this method in derived classes to implement screen-specific button logic.
   * Return true if the button press was consumed/handled, false if it should be
   * passed to other handlers (e.g., global menu button).
   */
  virtual bool OnButtonPress(ButtonId button_id) {
    (void)button_id;
    return false;  // Default: don't handle any buttons
  }

  /**
   * @brief Periodic update/tick called from display driver
   *
   * Override this method in derived classes for screen-specific periodic updates,
   * animations, or state changes that need to happen on each tick.
   */
  virtual void Tick() {
    // Default: do nothing
  }

  ScreenId GetScreenId() const {
    return screen_id_;
  }

  lv_obj_t* GetLvScreen() const {
    return screen_;
  }

 protected:
  lv_obj_t* screen_ = nullptr;
  ScreenId screen_id_;
};

}  // namespace xbot::driver::ui::lvgl

#endif  // LVGL_SCREEN_BASE_HPP_
