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
  virtual ~ScreenBase() {
    // Delete LVGL screen (automatically deletes all LVGL child objects)
    if (screen_) lv_obj_delete(screen_);
  }

  virtual void Create(lv_color_t bg_color = lv_color_white(), lv_color_t fg_color = lv_color_black()) {
    screen_ = lv_obj_create(NULL);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen_, bg_color, LV_PART_MAIN);
    lv_obj_set_style_text_color(screen_, fg_color, LV_PART_MAIN);
  };

  virtual void Show() {
    if (screen_) lv_screen_load(screen_);
    is_visible_ = true;
  };

  /**
   * @brief Hide the screen (but keep in memory)
   */
  virtual void Hide() {
    is_visible_ = false;
  }

  /**
   * @brief Activate screen for input focus - override for screens with interactive elements
   */
  virtual void Activate(lv_group_t* group) {
    current_group_ = group;
    // Base implementation does nothing - screens without buttons don't need group
  }

  /**
   * @brief Deactivate screen input focus - override for screens with interactive elements
   */
  virtual void Deactivate() {
    current_group_ = nullptr;
    // Base implementation does nothing
  }

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

  /**
   * @brief Check if screen is currently visible
   */
  bool IsVisible() const {
    return is_visible_;
  }

  /**
   * @brief Check if screen is currently active (has input focus)
   */
  bool IsActive() const {
    return current_group_ != nullptr;
  }

 protected:
  lv_obj_t* screen_ = nullptr;
  ScreenId screen_id_;
  lv_group_t* current_group_ = nullptr;
  bool is_visible_ = false;
};

}  // namespace xbot::driver::ui::lvgl

#endif  // LVGL_SCREEN_BASE_HPP_
