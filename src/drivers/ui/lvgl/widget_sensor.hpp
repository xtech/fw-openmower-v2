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
 * @brief Sensor indicator widget for displaying sensor state with pulsing animation
 *
 * Wraps an LVGL rectangle object to indicate sensor state.
 * Supports idle/triggered/disabled states with automatic animation management.
 *
 * IDLE: Shows small 2x2px indicator dot
 * TRIGGERED: Rectangle grows from 2x2px to full width×height and back (filling animation)
 * DISABLED: Hidden
 */
class WidgetSensor {
 public:
  enum class State : uint8_t { IDLE, TRIGGERED, DISABLED };

  /**
   * @brief Constructor with sizing and positioning
   * @param parent Parent LVGL object (screen or container)
   * @param width Width of the sensor indicator in pixels (if active)
   * @param height Height of the sensor indicator in pixels (if active)
   * @param align Optional LVGL alignment constant (e.g., LV_ALIGN_TOP_LEFT)
   * @param x_offset Offset from alignment point in x direction (default 0)
   * @param y_offset Offset from alignment point in y direction (default 0)
   *
   * IDLE: Shows small 2x2px indicator dot
   * TRIGGERED: Pulsing filled rectangle
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
    // Small 2x2px dot for IDLE state
    lv_obj_set_size(shape_, 2, 2);

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
      lv_anim_del(this, (lv_anim_exec_xcb_t)PulseCallback_);
      lv_obj_delete(shape_);
      shape_ = nullptr;
    }
  }

  /**
   * @brief Set sensor state
   * @param new_state Desired sensor state (IDLE, TRIGGERED, or DISABLED)
   *
   * Automatically manages animations:
   * - IDLE: Shows small 2x2px indicator dot (no animation)
   * - TRIGGERED: Animates rectangle from 2x2px to full size and back (filling animation)
   * - DISABLED: Stops animation and hides rectangle
   */
  void SetState(State new_state) {
    if (new_state == state_) {
      return;  // No change needed
    }

    switch (new_state) {
      case State::IDLE:
        lv_anim_del(this, (lv_anim_exec_xcb_t)PulseCallback_);
        lv_obj_set_size(shape_, 2, 2);
        lv_obj_set_style_bg_opa(shape_, LV_OPA_100, LV_PART_MAIN);
        lv_obj_clear_flag(shape_, LV_OBJ_FLAG_HIDDEN);
        break;

      case State::TRIGGERED: StartPulsing_(); break;

      case State::DISABLED:
        lv_anim_del(this, (lv_anim_exec_xcb_t)PulseCallback_);
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
   * @brief LVGL animation callback for grow-and-shrink effect
   * @param obj The WidgetSensor instance (set via lv_anim_set_var)
   * @param value Animation value (0-200, represents expansion phase)
   * - 0..100: grow from 2x2 to full width×height
   * - 100..200: shrink from full size back to 2x2
   */
  static void PulseCallback_(void* obj, int32_t value) {
    WidgetSensor* ws = static_cast<WidgetSensor*>(obj);
    if (!ws || !ws->shape_) return;

    // Normalize value to 0-200 range (growth then shrink)
    int32_t v = value;
    if (v < 0) v = 0;
    if (v > 200) v = 200;

    // Map to 0-100 for size interpolation
    int32_t phase = (v <= 100) ? v : (200 - v);

    lv_coord_t min_w = 2, min_h = 2;
    lv_coord_t new_w = min_w + (phase * (ws->width_ - min_w)) / 100;
    lv_coord_t new_h = min_h + (phase * (ws->height_ - min_h)) / 100;

    lv_obj_set_size(ws->shape_, new_w, new_h);
    // Re-align after size change, using stored alignment and offsets
    lv_obj_align(ws->shape_, ws->align_, ws->x_offset_, ws->y_offset_);
  }

  void StartPulsing_() {
    // Stop previous animation if running
    lv_anim_del(this, (lv_anim_exec_xcb_t)PulseCallback_);

    // Configure and start new animation
    lv_anim_init(&animation_);
    lv_anim_set_exec_cb(&animation_, (lv_anim_exec_xcb_t)PulseCallback_);
    lv_anim_set_var(&animation_, this);
    lv_anim_set_time(&animation_, 1400);       // 700ms grow + 700ms shrink
    lv_anim_set_repeat_delay(&animation_, 0);  // Pause between cycles
    lv_anim_set_values(&animation_, 0, 200);   // 0-100 grow, 100-200 shrink
    lv_anim_set_repeat_count(&animation_, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&animation_);

    // Make sure shape is visible when animation starts
    lv_obj_clear_flag(shape_, LV_OBJ_FLAG_HIDDEN);
  }
};

}  // namespace xbot::driver::ui::lvgl

#endif  // __UI_LVGL_WIDGET_SENSOR_HPP
