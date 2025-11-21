/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file widget_sensor.hpp
 * @brief Sensor indicator widget for LVGL displaying sensor state with pulsing animation
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-13
 */

#ifndef __UI_LVGL_WIDGET_SENSOR_HPP
#define __UI_LVGL_WIDGET_SENSOR_HPP

#include <lvgl.h>

namespace xbot::driver::ui::lvgl {

/**
 * @brief Sensor indicator widget for displaying sensor state with animation
 *
 * Wraps an LVGL rectangle object to indicate sensor state.
 * Supports idle/triggered/disabled states with automatic animation management.
 *
 * IDLE: Shows small 5x5px indicator box
 * TRIGGERED: Full-size nlinking rectangle
 * DISABLED: Hidden
 */
class WidgetSensor {
 public:
  enum class State : uint8_t { IDLE, TRIGGERED, DISABLED };

  /**
   * @brief Constructor with sizing and positioning
   * @param parent Parent LVGL object (screen or container)
   * @param width Width of the sensor indicator in pixels (when triggered)
   * @param height Height of the sensor indicator in pixels (when triggered)
   * @param align Optional LVGL alignment constant (e.g., LV_ALIGN_TOP_LEFT)
   * @param x_offset Offset from alignment point in x direction (default 0)
   * @param y_offset Offset from alignment point in y direction (default 0)
   */
  WidgetSensor(lv_obj_t* parent, lv_coord_t width, lv_coord_t height, lv_align_t align = LV_ALIGN_CENTER,
               lv_coord_t x_offset = 0, lv_coord_t y_offset = 0)
      : state_(State::DISABLED),
        shape_(nullptr),
        width_(width),
        height_(height),
        align_(align),
        x_offset_(x_offset),
        y_offset_(y_offset) {
    // Create rectangle for sensor indicator
    shape_ = lv_obj_create(parent);

    // Apply styling: black background, black border
    lv_obj_set_style_bg_color(shape_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_color(shape_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(shape_, 1, LV_PART_MAIN);
    lv_obj_align(shape_, align_, x_offset_, y_offset_);

    // Start in DISABLED state (hidden)
    lv_obj_add_flag(shape_, LV_OBJ_FLAG_HIDDEN);
  }

  /**
   * @brief Destructor - clean up animation
   */
  ~WidgetSensor() {
    if (shape_) {
      // Stop any running animation
      lv_anim_del(this, (lv_anim_exec_xcb_t)BlinkCallback_);
      lv_obj_delete(shape_);
      shape_ = nullptr;
    }
  }

  /**
   * @brief Set sensor state
   * @param new_state Desired sensor state (IDLE, TRIGGERED, or DISABLED)
   *
   * Automatically manages animations:
   * - IDLE: Shows small 5x5px indicator box (no animation)
   * - TRIGGERED: Animates between visible and hidden
   * - DISABLED: Stops animation and hides rectangle
   */
  void SetState(State new_state) {
    if (new_state == state_) {
      return;  // No change needed
    }

    switch (new_state) {
      case State::IDLE:
        lv_anim_del(this, (lv_anim_exec_xcb_t)BlinkCallback_);
        lv_obj_set_size(shape_, 5, 5);
        lv_obj_set_style_bg_opa(shape_, LV_OPA_0, LV_PART_MAIN);  // No fill
        lv_obj_set_style_border_width(shape_, 1, LV_PART_MAIN);   // Draw X with border
        lv_obj_clear_flag(shape_, LV_OBJ_FLAG_HIDDEN);
        DrawXMarking_();
        break;

      case State::TRIGGERED: StartBlinking_(); break;

      case State::DISABLED:
        lv_anim_del(this, (lv_anim_exec_xcb_t)BlinkCallback_);
        lv_obj_add_flag(shape_, LV_OBJ_FLAG_HIDDEN);
        break;
    }

    state_ = new_state;
  }

 private:
  State state_;
  lv_obj_t* shape_ = nullptr;
  lv_coord_t width_;
  lv_coord_t height_;
  lv_align_t align_;
  lv_coord_t x_offset_;
  lv_coord_t y_offset_;
  lv_anim_t animation_;

  /**
   * @brief LVGL animation callback for blinking effect
   * @param obj WidgetSensor instance (set via lv_anim_set_var)
   * @param value Animation value (0-800)
   * - 0-400: Show IDLE state (small box with border)
   * - 400-800: Show TRIGGERED state (full-size filled rectangle)
   */
  static void BlinkCallback_(void* obj, int32_t value) {
    WidgetSensor* ws = static_cast<WidgetSensor*>(obj);
    if (!ws || !ws->shape_) return;

    // Toggle between IDLE and TRIGGERED appearance every 400ms
    int32_t phase = value % 800;

    if (phase < 400) {
      // First half (0-400ms): Show IDLE state (small 5x5 box with border, no fill)
      lv_obj_set_size(ws->shape_, 5, 5);
      lv_obj_set_style_bg_opa(ws->shape_, LV_OPA_0, LV_PART_MAIN);  // No fill
      lv_obj_set_style_border_width(ws->shape_, 1, LV_PART_MAIN);   // Show border
      lv_obj_set_style_border_color(ws->shape_, lv_color_make(255, 0, 0), LV_PART_MAIN);
    } else {
      // Second half (400-800ms): Show TRIGGERED state (full-size filled rectangle)
      lv_obj_set_size(ws->shape_, ws->width_, ws->height_);
      lv_obj_set_style_bg_opa(ws->shape_, LV_OPA_100, LV_PART_MAIN);  // Full fill
      lv_obj_set_style_border_width(ws->shape_, 0, LV_PART_MAIN);     // No border
    }

    lv_obj_align(ws->shape_, ws->align_, ws->x_offset_, ws->y_offset_);
  }

  void DrawXMarking_() {
    // Create a visual X by using border style with transparent background
    // and setting border width to 1px on all sides
    if (!shape_) return;

    // Set border color to match primary theme color
    lv_color_t border_color = lv_color_make(255, 0, 0);  // Red X for visibility
    lv_obj_set_style_border_color(shape_, border_color, LV_PART_MAIN);
    lv_obj_set_style_border_width(shape_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(shape_, LV_BORDER_SIDE_FULL, LV_PART_MAIN);
  }

  void StartBlinking_() {
    // Stop previous animation if running
    lv_anim_del(this, (lv_anim_exec_xcb_t)BlinkCallback_);

    // Configure and start new blink animation
    // Animates between IDLE state (first 400ms) and TRIGGERED state (next 400ms)
    lv_anim_init(&animation_);
    lv_anim_set_exec_cb(&animation_, (lv_anim_exec_xcb_t)BlinkCallback_);
    lv_anim_set_var(&animation_, this);
    lv_anim_set_time(&animation_, 800);
    lv_anim_set_repeat_delay(&animation_, 0);
    lv_anim_set_values(&animation_, 0, 800);
    lv_anim_set_repeat_count(&animation_, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&animation_);

    // Make sure shape is visible when animation starts
    lv_obj_clear_flag(shape_, LV_OBJ_FLAG_HIDDEN);
  }
};

}  // namespace xbot::driver::ui::lvgl

#endif  // __UI_LVGL_WIDGET_SENSOR_HPP
