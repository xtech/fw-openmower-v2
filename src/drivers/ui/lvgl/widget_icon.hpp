/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file widget_icon.hpp
 * @brief Icon widget for LVGL displaying Font Awesome icons with state control
 *
 * This widget wraps an LVGL label to display Font Awesome icons with three states:
 * - ON: Icon visible and steady
 * - OFF: Icon hidden
 * - BLINK: Icon blinking at 600ms period
 *
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-12
 */

#ifndef __UI_LVGL_WIDGET_ICON_HPP
#define __UI_LVGL_WIDGET_ICON_HPP

#include <lvgl.h>

// Forward declare the Font Awesome font
extern "C" {
LV_FONT_DECLARE(font_awesome_14);
}

namespace xbot::driver::ui::lvgl {

/**
 * @brief Icon widget for displaying Font Awesome icons with state control
 *
 * Wraps an LVGL label object to display Font Awesome icons. Supports
 * on/off/blinking states with automatic animation management.
 */
class WidgetIcon {
 public:
  /**
   * @brief Font Awesome 7 Icon definition with UTF-8 encoding
   * Maps icon names to their UTF-8 encoded strings (3 bytes + null terminator)
   */
  struct IconDef {
    const char utf8[4];
  };

  enum class Icon : uint8_t {
    ROS,
    EMERGENCY_WHEEL_LIFT,
    EMERGENCY_GENERIC,
    NO_RTK_FIX,
    RTK_FIX,
    SATELLITE,
    BULLSEYE,
    DOWNLOAD,
    BATTERY_EMPTY,
    BATTERY_QUARTER,
    BATTERY_HALF,
    BATTERY_THREE_QUARTER,
    BATTERY_FULL,
    DOCKED,
    BATTERY_VOLTAGE,
    CHARGE_CURRENT,
    HAND_STOP,
    TIMEOUT
  };

  enum class State : uint8_t { UNDEF, OFF, ON, BLINK };

  /**
   * @brief Font Awesome 7 Icons enumeration
   * Maps icon names to their Icon definitions
   */
  static constexpr IconDef ICONS[] = {
      {"\xEF\x95\x84"},  // ROS - Robot (U+F544)
      {"\xEF\x97\xA1"},  // EMERGENCY_WHEEL_LIFT - Car burst (U+F5E1)
      {"\xEF\x81\xB1"},  // EMERGENCY_GENERIC - Exclamation triangle (U+F071)
      {"\xEF\x81\x9B"},  // NO_RTK_FIX - Location crosshairs with slash (U+F05B)
      {"\xEF\x98\x81"},  // RTK_FIX - location-crosshairs (U+F601)
      {"\xEF\x9E\xBF"},  // SATELLITE - Satellite (U+F7BF)
      {"\xEF\x85\x80"},  // BULLSEYE - Bullseye (U+F140)
      {"\xEF\x80\x99"},  // ARROWS_ROTATE - Download (U+F019)
      {"\xEF\x89\x84"},  // BATTERY_EMPTY - Battery 0% (U+F244)
      {"\xEF\x89\x83"},  // BATTERY_QUARTER - Battery 25% (U+F243)
      {"\xEF\x89\x82"},  // BATTERY_HALF - Battery 50% (U+F242)
      {"\xEF\x89\x81"},  // BATTERY_THREE_QUARTER - Battery 75% (U+F241)
      {"\xEF\x89\x80"},  // BATTERY_FULL - Battery 100% (U+F240)
      {"\xEF\x97\xA7"},  // DOCKED - Charging station (U+F5E7)
      {"\xEF\x97\x9F"},  // BATTERY_VOLTAGE - Car battery (U+F5DF)
      {"\xEF\x82\x8B"},  // CHARGE_CURRENT - Arrow right from bracket (U+F08B)
      {"\xEF\x89\x96"},  // HAND_STOP - Hand stop (U+F06A)
      {"\xEF\x89\x93"}   // TIMEOUT - Hourglass (U+F251)
  };

  /**
   * @brief Constructor with Icon enum - preferred way
   * @param icon Icon to display (use Icon enum)
   * @param parent Parent LVGL object (screen or container)
   * @param align Optional LVGL alignment constant (e.g., LV_ALIGN_TOP_LEFT)
   * @param x_offset Offset from alignment point in x direction (default 0)
   * @param y_offset Offset from alignment point in y direction (default 0)
   * @param state Initial state of the icon (default OFF)
   * @param color Text color of the icon (default black)
   *
   * Icon is created with Font Awesome font, black text color, center aligned.
   * Positioning is applied using standard LVGL alignment constants.
   */
  WidgetIcon(Icon icon, lv_obj_t* parent, lv_align_t align = LV_ALIGN_DEFAULT, lv_coord_t x_offset = 0,
             lv_coord_t y_offset = 0, State state = State::OFF, lv_color_t color = lv_color_black())
      : WidgetIcon(ICONS[static_cast<int>(icon)].utf8, parent, align, x_offset, y_offset, state, color) {
  }

  /**
   * @brief Constructor with icon symbol string - for custom icons
   * @param icon_symbol Font Awesome icon symbol string (e.g., "\xef\x81\xa0")
   * @param parent Parent LVGL object (screen or container)
   * @param align Optional LVGL alignment constant (e.g., LV_ALIGN_TOP_LEFT)
   * @param x_offset Offset from alignment point in x direction (default 0)
   * @param y_offset Offset from alignment point in y direction (default 0)
   * @param state Initial state of the icon (default OFF)
   * @param color Text color of the icon (default black)
   *
   * Icon is created with Font Awesome font, black text color, center aligned.
   * Positioning is applied using standard LVGL alignment constants.
   */
  WidgetIcon(const char* icon_symbol, lv_obj_t* parent, lv_align_t align = LV_ALIGN_DEFAULT, lv_coord_t x_offset = 0,
             lv_coord_t y_offset = 0, State state = State::OFF, lv_color_t color = lv_color_black())
      : label_(nullptr) {
    // Create label for icon display
    label_ = lv_label_create(parent);
    lv_label_set_text_static(label_, icon_symbol);

    // Apply icon styling automatically
    lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_, &font_awesome_14, LV_PART_MAIN);

    // Apply positioning if alignment is specified
    if (align != LV_ALIGN_DEFAULT) {
      lv_obj_align(label_, align, x_offset, y_offset);
    }

    // Set initial state
    SetState(state);
  }

  /**
   * @brief Destructor - clean up animation
   */
  ~WidgetIcon() {
    if (label_) {
      // Stop any running animation
      lv_anim_del(label_, (lv_anim_exec_xcb_t)BlinkCallback_);
      lv_obj_delete(label_);
      label_ = nullptr;
    }
  }

  /**
   * @brief Set icon state
   * @param new_state Desired icon state (ON, OFF, or BLINK)
   *
   * Automatically manages animations:
   * - ON: Stops any blinking and shows icon
   * - OFF: Stops any blinking and hides icon
   * - BLINK: Starts continuous blinking animation
   */
  void SetState(State new_state) {
    if (new_state == state_) {
      return;  // No change needed
    }

    switch (new_state) {
      case State::ON:
        // Stop any blinking animation and show icon
        lv_anim_del(label_, (lv_anim_exec_xcb_t)BlinkCallback_);
        lv_obj_clear_flag(label_, LV_OBJ_FLAG_HIDDEN);
        break;

      case State::OFF:
      case State::UNDEF:
        // Stop animation and hide icon
        lv_anim_del(label_, (lv_anim_exec_xcb_t)BlinkCallback_);
        lv_obj_add_flag(label_, LV_OBJ_FLAG_HIDDEN);
        break;

      case State::BLINK:
        // Start blinking animation
        StartBlinking_();
        break;
    }

    state_ = new_state;
  }

  /**
   * @brief Change the displayed icon
   * @param new_icon New icon to display
   *
   * Updates the icon while preserving the current state (ON/OFF/BLINK).
   * Useful for dynamic icon changes based on status.
   */
  void SetIcon(Icon new_icon) {
    lv_label_set_text_static(label_, ICONS[static_cast<int>(new_icon)].utf8);
  }

  /**
   * @brief Change the displayed icon with custom symbol string
   * @param icon_symbol Font Awesome icon symbol string
   *
   * Updates the icon while preserving the current state.
   * For custom icons not in the predefined Icon enum.
   */
  void SetIcon(const char* icon_symbol) {
    lv_label_set_text_static(label_, icon_symbol);
  }

  /**
   * @brief Set alignment of the icon
   * @param align LVGL alignment constant (e.g., LV_ALIGN_TOP_LEFT)
   * @param x_offset Offset from alignment point in x direction
   * @param y_offset Offset from alignment point in y direction
   *
   * Changes the position of the icon relative to its parent.
   */
  void SetAlign(lv_align_t align, lv_coord_t x_offset = 0, lv_coord_t y_offset = 0) {
    if (label_ != nullptr) {
      lv_obj_align(label_, align, x_offset, y_offset);
    }
  }

 private:
  State state_ = State::UNDEF;
  lv_obj_t* label_ = nullptr;
  lv_anim_t animation_;

  /**
   * @brief LVGL animation callback for blinking effect
   * @param obj The label object being animated
   * @param value Animation value (0 or 1)
   */
  static void BlinkCallback_(void* obj, int32_t value) {
    lv_obj_t* label = static_cast<lv_obj_t*>(obj);
    if (value) {
      lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);  // Show
    } else {
      lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);  // Hide
    }
  }

  /**
   * @brief Start blinking animation with 600ms period
   */
  void StartBlinking_() {
    // Stop previous animation if running
    lv_anim_del(label_, (lv_anim_exec_xcb_t)BlinkCallback_);

    // Configure and start new animation
    lv_anim_init(&animation_);
    lv_anim_set_exec_cb(&animation_, (lv_anim_exec_xcb_t)BlinkCallback_);
    lv_anim_set_var(&animation_, label_);
    lv_anim_set_time(&animation_, 600);          // 600ms period
    lv_anim_set_repeat_delay(&animation_, 600);  // Same as period
    lv_anim_set_values(&animation_, 0, 1);
    lv_anim_set_repeat_count(&animation_, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&animation_);

    // Make sure icon is visible when blinking starts
    lv_obj_clear_flag(label_, LV_OBJ_FLAG_HIDDEN);
  }
};

}  // namespace xbot::driver::ui::lvgl

#endif  // __UI_LVGL_WIDGET_ICON_HPP
