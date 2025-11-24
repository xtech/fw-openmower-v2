/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_screen_main.hpp
 * @brief Main Screen with status icons topbar and content area
 *
 * The main screen displays:
 * - Topbar: Fixed status icons (ROS, emergencies, GPS, battery, docked, ...)
 * - Content area: Progressbars and other dynamic content (future expansion)
 * - Robot state indicator: Idle, Mowing, Pause, Area Recording, Emergency (future)
 *
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-12
 */

#ifndef LVGL_SABO_SCREEN_MAIN_HPP_
#define LVGL_SABO_SCREEN_MAIN_HPP_

#include <lvgl.h>
#include <ulog.h>

#include "../../SaboCoverUI/sabo_cover_ui_controller.hpp"
#include "../../SaboCoverUI/sabo_cover_ui_defs.hpp"
#include "../screen_base.hpp"
#include "../widget_icon.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
}

namespace xbot::driver::ui::lvgl::sabo {

using namespace xbot::driver::ui::sabo;

class SaboScreenMain : public ScreenBase<ScreenId, ButtonID> {
 public:
  SaboScreenMain() : ScreenBase<ScreenId, ButtonID>(ScreenId::MAIN) {
  }

  ~SaboScreenMain() {
    // WidgetIcon objects clean up their own LVGL objects
    // No explicit cleanup needed here
  }

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    // Create topbar container (fixed at top of screen)
    CreateTopbar();

    // TODO: Create content area between topbar and bottombar
    // - Progressbars for battery %, charge current
    // - GPS quality indicator
    content_area_ = lv_obj_create(screen_);
    lv_obj_set_size(content_area_, display::LCD_WIDTH, display::LCD_HEIGHT - TOPBAR_HEIGHT - BOTTOMBAR_HEIGHT);
    lv_obj_set_pos(content_area_, 0, TOPBAR_HEIGHT);
    lv_obj_set_style_bg_color(content_area_, bg_color, LV_PART_MAIN);
    lv_obj_set_style_border_width(content_area_, 0, LV_PART_MAIN);

    // Placeholder text for content area
    lv_obj_t* placeholder = lv_label_create(content_area_);
    lv_label_set_text(placeholder, "TODO");
    lv_obj_set_style_text_color(placeholder, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(placeholder, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(placeholder, LV_ALIGN_CENTER, 0, 0);

    // Create bottombar for robot state (will be updated by Ticker widget later)
    CreateBottombar();
  }

  /**
   * @brief Update icon states based on current system state
   * Called periodically from display controller's Tick()
   */
  void Tick() override {
    // TODO: Update icon states from ROS data
  }

 private:
  static constexpr lv_coord_t TOPBAR_HEIGHT = 19;     ///< Height of the top status bar
  static constexpr lv_coord_t BOTTOMBAR_HEIGHT = 16;  ///< Height of the bottom robot state bar

  lv_obj_t* topbar_ = nullptr;
  lv_obj_t* bottombar_ = nullptr;
  lv_obj_t* content_area_ = nullptr;

  // Status icons (initialized in CreateTopbar)
  WidgetIcon* icon_ros_ = nullptr;
  WidgetIcon* icon_emergency_lift_ = nullptr;
  WidgetIcon* icon_emergency_generic_ = nullptr;
  WidgetIcon* icon_gps_ = nullptr;
  WidgetIcon* icon_docked_ = nullptr;
  WidgetIcon* icon_battery_ = nullptr;

  /**
   * @brief Create the fixed topbar with status icons
   *
   * Layout:
   * [ROS] [List-Emergency] [Generic-Emergency] ... [Docking] [Battery]
   *
   * Each icon can be in state ON, OFF, or BLINK depending on subsystem status
   */
  void CreateTopbar() {
    // Create topbar container with black background and checkerboard pattern
    topbar_ = lv_obj_create(screen_);
    lv_obj_set_size(topbar_, display::LCD_WIDTH, TOPBAR_HEIGHT);
    lv_obj_set_pos(topbar_, 0, 0);
    lv_obj_set_style_bg_color(topbar_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(topbar_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(topbar_, 2, LV_PART_MAIN);

    // Left side: ROS/Engine icon
    icon_ros_ = new WidgetIcon(WidgetIcon::Icon::ROS, topbar_, LV_ALIGN_TOP_LEFT, 3, 0);
    icon_ros_->SetState(WidgetIcon::State::BLINK);

    // Center-left: List-Emergency (Wheel-Lift)
    icon_emergency_lift_ = new WidgetIcon(WidgetIcon::Icon::EMERGENCY_WHEEL_LIFT, topbar_, LV_ALIGN_TOP_MID, -10, 0);
    icon_emergency_lift_->SetState(WidgetIcon::State::ON);

    // Center-right: Generic Emergency
    icon_emergency_generic_ = new WidgetIcon(WidgetIcon::Icon::EMERGENCY_GENERIC, topbar_, LV_ALIGN_TOP_MID, 10, 0);
    icon_emergency_generic_->SetState(WidgetIcon::State::ON);

    // GPS icon
    icon_gps_ = new WidgetIcon(WidgetIcon::Icon::GPS, topbar_, LV_ALIGN_TOP_MID, 47, 0);
    icon_gps_->SetState(WidgetIcon::State::BLINK);

    // Right side: Battery icon
    icon_battery_ = new WidgetIcon(WidgetIcon::Icon::BATTERY_FULL, topbar_, LV_ALIGN_TOP_RIGHT, -23, 0);
    icon_battery_->SetState(WidgetIcon::State::ON);

    // Right side: Docking icon
    icon_docked_ = new WidgetIcon(WidgetIcon::Icon::DOCKED, topbar_, LV_ALIGN_TOP_RIGHT, -3, 0);
    icon_docked_->SetState(WidgetIcon::State::ON);
  }

  /**
   * @brief Create the fixed bottombar for robot state display
   *
   * Layout:
   * [black bar] [Robot State: IDLE/MOWING/PAUSE/etc] (via Ticker widget later)
   *
   * This bar will be updated by the Ticker widget with dynamic robot state from ROS
   */
  void CreateBottombar() {
    // Create bottombar container with black background
    bottombar_ = lv_obj_create(screen_);
    lv_obj_set_size(bottombar_, display::LCD_WIDTH, BOTTOMBAR_HEIGHT);
    lv_obj_set_pos(bottombar_, 0, display::LCD_HEIGHT - BOTTOMBAR_HEIGHT);
    lv_obj_set_style_bg_color(bottombar_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(bottombar_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bottombar_, 1, LV_PART_MAIN);

    // Placeholder for robot state (will be replaced by Ticker widget)
    lv_obj_t* state_label = lv_label_create(bottombar_);
    lv_label_set_text_static(state_label, "IDLE");
    lv_obj_set_style_text_color(state_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(state_label, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(state_label, LV_ALIGN_CENTER, 0, 0);
  }

};  // class SaboScreenMain

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_MAIN_HPP_
