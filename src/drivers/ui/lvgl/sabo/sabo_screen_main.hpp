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
#include "../screen_base.hpp"
#include "../widget_icon.hpp"
#include "../widget_textbar.hpp"
#include "robots/include/sabo_common.hpp"
#include "services.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
}

namespace xbot::driver::ui::lvgl::sabo {

using GpsState = xbot::driver::gps::GpsDriver::GpsState;

class SaboScreenMain : public ScreenBase<ScreenId, ButtonId> {
 public:
  SaboScreenMain() : ScreenBase<ScreenId, ButtonId>(ScreenId::MAIN) {
  }

  ~SaboScreenMain() {
    // WidgetIcon objects clean up their own LVGL objects
    // No explicit cleanup needed here
  }

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    // Topbar container
    CreateTopbar();

    // Content Area
    int32_t y = 24;

    // Satellites in use icon
    WidgetIcon* icon_sats_ =
        new WidgetIcon(WidgetIcon::Icon::SATELLITE, screen_, LV_ALIGN_TOP_LEFT, 0, y - 2, lv_color_black());
    icon_sats_->SetState(WidgetIcon::State::ON);
    // ... value
    sats_value_ = lv_label_create(screen_);
    lv_label_set_text_static(sats_value_, "0");
    lv_obj_set_style_text_color(sats_value_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(sats_value_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(sats_value_, LV_ALIGN_TOP_LEFT, 17, y);

    // NTRIP actuality icon
    WidgetIcon* icon_ntrip_ =
        new WidgetIcon(WidgetIcon::Icon::DOWNLOAD, screen_, LV_ALIGN_TOP_MID, -55, y - 2, lv_color_black());
    icon_ntrip_->SetState(WidgetIcon::State::ON);
    // ... value
    ntrip_value_ = lv_label_create(screen_);
    lv_label_set_text_static(ntrip_value_, "N/A");
    lv_obj_set_style_text_color(ntrip_value_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(ntrip_value_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(ntrip_value_, LV_ALIGN_TOP_LEFT, 75, y);

    // GPS Mode icon
    WidgetIcon* icon_gps_mode_ =
        new WidgetIcon(WidgetIcon::Icon::BULLSEYE, screen_, LV_ALIGN_TOP_RIGHT, -84, y - 2, lv_color_black());
    icon_gps_mode_->SetState(WidgetIcon::State::ON);
    // ... value
    gps_mode_value_ = lv_label_create(screen_);
    lv_label_set_text_static(gps_mode_value_, "RTK FLOAT");
    lv_obj_set_style_text_color(gps_mode_value_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(gps_mode_value_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(gps_mode_value_, LV_ALIGN_TOP_LEFT, defs::LCD_WIDTH - 80, y);

    // GPS Accuracy
    y += 18;
    // Icon
    WidgetIcon* icon_accuracy_ =
        new WidgetIcon(WidgetIcon::Icon::NO_RTK_FIX, screen_, LV_ALIGN_TOP_LEFT, 0, y, lv_color_black());
    icon_accuracy_->SetState(WidgetIcon::State::ON);
    // Bar
    accuracy_bar_ =
        new WidgetTextBar(screen_, "N/A", LV_ALIGN_TOP_LEFT, 20, y - 1, defs::LCD_WIDTH - 20, 18, &orbitron_12);

    // Create bottombar for robot state (will be updated by Ticker widget later)
    CreateBottombar();
  }

  /**
   * @brief Update icon states based on current system state
   * Called periodically from display controller's Tick()
   */
  void Tick() override {
    UpdateGpsWidgets();
  }

 private:
  static constexpr lv_coord_t TOPBAR_HEIGHT = 19;     ///< Height of the top status bar
  static constexpr lv_coord_t BOTTOMBAR_HEIGHT = 16;  ///< Height of the bottom robot state bar

  lv_obj_t* topbar_ = nullptr;
  lv_obj_t* bottombar_ = nullptr;

  // Status icons (initialized in CreateTopbar)
  WidgetIcon* icon_ros_ = nullptr;
  WidgetIcon* icon_emergency_lift_ = nullptr;
  WidgetIcon* icon_emergency_generic_ = nullptr;
  WidgetIcon* icon_gps_ = nullptr;
  WidgetIcon* icon_docked_ = nullptr;
  WidgetIcon* icon_battery_ = nullptr;

  // GPS display widgets
  WidgetTextBar* accuracy_bar_ = nullptr;
  lv_obj_t* sats_value_ = nullptr;
  lv_obj_t* gps_mode_value_ = nullptr;
  lv_obj_t* ntrip_value_ = nullptr;

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
    lv_obj_set_size(topbar_, defs::LCD_WIDTH, TOPBAR_HEIGHT);
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
    icon_gps_ = new WidgetIcon(WidgetIcon::Icon::NO_RTK_FIX, topbar_, LV_ALIGN_TOP_MID, 47, 0);
    icon_gps_->SetState(WidgetIcon::State::BLINK);

    // Right side: Battery icon
    icon_battery_ = new WidgetIcon(WidgetIcon::Icon::BATTERY_FULL, topbar_, LV_ALIGN_TOP_RIGHT, -23, 0);
    icon_battery_->SetState(WidgetIcon::State::ON);

    // Right side: Docking icon
    icon_docked_ = new WidgetIcon(WidgetIcon::Icon::DOCKED, topbar_, LV_ALIGN_TOP_RIGHT, -3, 0);
    icon_docked_->SetState(WidgetIcon::State::ON);
  }

  /**
   * @brief Update GPS display widgets with current GPS data
   */
  void UpdateGpsWidgets() {
    if (!gps_service.IsGpsStateValid()) {
      // GPS state not valid, show default values
      lv_label_set_text_fmt(sats_value_, "%d", 0);
      lv_label_set_text(gps_mode_value_, "N/A");
      accuracy_bar_->SetValue(0);
      return;
    }

    const auto& gps_state = gps_service.GetGpsState();

    // Update satellites count
    lv_label_set_text_fmt(sats_value_, "%d", gps_state.num_sv);

    // Update GPS mode status
    const char* gps_mode = "N/A";
    WidgetIcon::Icon gps_icon = WidgetIcon::Icon::NO_RTK_FIX;
    WidgetIcon::State gps_icon_state = WidgetIcon::State::BLINK;

    switch (gps_state.rtk_type) {
      case GpsState::RTK_FIX:
        gps_mode = "RTK FIX";
        // Topbar GPS icon show RTK FIX
        gps_icon = WidgetIcon::Icon::RTK_FIX;
        gps_icon_state = WidgetIcon::State::ON;
        break;
      case GpsState::RTK_FLOAT: gps_mode = "RTK FLOAT"; break;
      default:
        // If no RTK, check fix type
        switch (gps_state.fix_type) {
          case GpsState::FIX_3D: gps_mode = "3D FIX"; break;
          case GpsState::FIX_2D: gps_mode = "2D FIX"; break;
          case GpsState::GNSS_DR_COMBINED: gps_mode = "GNSS+DR"; break;
          case GpsState::DR_ONLY: gps_mode = "DR ONLY"; break;
          case GpsState::NO_FIX: gps_mode = "NO FIX"; break;
          default: gps_mode = "N/A"; break;
        }
        break;
    }
    lv_label_set_text(gps_mode_value_, gps_mode);
    icon_gps_->SetIcon(gps_icon);
    icon_gps_->SetState(gps_icon_state);

    // Update accuracy progress bar with intuitive percentage bar but m text display
    double accuracy = gps_state.position_h_accuracy;
    double accuracy_for_bar = accuracy;
    if (accuracy_for_bar < 0.01) accuracy_for_bar = 0.01;  // Clamp to minimum (1cm) for bar
    if (accuracy_for_bar > 1.00) accuracy_for_bar = 1.00;  // Clamp to maximum (1m) for bar

    // Convert accuracy to progress value (0.01m = 100%, 1.00m = 0%)
    // Intuitive: full bar = best accuracy, empty bar = worst accuracy
    int32_t progress = static_cast<int32_t>((1.00 - accuracy_for_bar) / 0.99 * 100.0);

    // Display percentage bar with m text (2 decimal places)
    int32_t accuracy_m_int = static_cast<int32_t>(accuracy * 100.0);  // Convert to cm for integer math
    int32_t meters = accuracy_m_int / 100;
    int32_t centimeters = accuracy_m_int % 100;

    // Format the text manually since SetValueFormatted only accepts one value
    static char accuracy_text[16];
    lv_snprintf(accuracy_text, sizeof(accuracy_text), "%d.%02d m", meters, centimeters);
    accuracy_bar_->SetValue(progress, accuracy_text);

    // Update NTRIP status
    uint32_t seconds_since_rtcm = gps_service.GetSecondsSinceLastRtcmPacket();
    if (seconds_since_rtcm == 0) {
      lv_label_set_text(ntrip_value_, "N/A");
    } else if (seconds_since_rtcm > 99) {
      lv_label_set_text(ntrip_value_, ">99 sec");
    } else {
      lv_label_set_text_fmt(ntrip_value_, "%lu sec", seconds_since_rtcm);
    }

    if (!gps_service.IsGpsStateValid()) {
      icon_gps_->SetState(WidgetIcon::State::BLINK);
      return;
    }

    if (gps_state.rtk_type == xbot::driver::gps::GpsDriver::GpsState::RTK_FIX) {
      icon_gps_->SetState(WidgetIcon::State::ON);
    } else {
      icon_gps_->SetState(WidgetIcon::State::BLINK);
    }
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
    lv_obj_set_size(bottombar_, defs::LCD_WIDTH, BOTTOMBAR_HEIGHT);
    lv_obj_set_pos(bottombar_, 0, defs::LCD_HEIGHT - BOTTOMBAR_HEIGHT);
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
