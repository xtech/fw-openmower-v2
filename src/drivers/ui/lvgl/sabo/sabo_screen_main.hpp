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
 * - Content area: Progressbars and other dynamic content
 * - Robot state indicator: Idle, Mowing, Pause, Area Recording, Emergency (future)
 *
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-30
 */

#ifndef LVGL_SABO_SCREEN_MAIN_HPP_
#define LVGL_SABO_SCREEN_MAIN_HPP_

#include <chprintf.h>
#include <lvgl.h>
#include <ulog.h>

#include "../../SaboCoverUI/sabo_cover_ui_controller.hpp"
#include "../screen_base.hpp"
#include "../widget_icon.hpp"
#include "../widget_textbar.hpp"
#include "robots/include/sabo_common.hpp"
#include "robots/include/sabo_robot.hpp"
#include "services.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
}

namespace xbot::driver::ui::lvgl::sabo {

using GpsState = xbot::driver::gps::GpsDriver::GpsState;

class SaboScreenMain : public ScreenBase<ScreenId, ButtonId> {
 public:
  SaboScreenMain() : ScreenBase<ScreenId, ButtonId>(ScreenId::MAIN) {
    if (robot != nullptr) sabo_robot_ = static_cast<SaboRobot*>(robot);
  }

  ~SaboScreenMain() = default;

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

    // Battery Voltage
    y += 32;
    // Icon
    WidgetIcon* icon_battery_voltage_ =
        new WidgetIcon(WidgetIcon::Icon::BATTERY_VOLTAGE, screen_, LV_ALIGN_TOP_LEFT, 0, y, lv_color_black());
    icon_battery_voltage_->SetState(WidgetIcon::State::ON);
    // Bar
    battery_voltage_bar_ =
        new WidgetTextBar(screen_, "N/A", LV_ALIGN_TOP_LEFT, 20, y - 1, defs::LCD_WIDTH - 20, 18, &orbitron_12);

    // Charge Current
    y += 22;
    // Icon
    icon_charge_current_ =
        new WidgetIcon(WidgetIcon::Icon::CHARGE_CURRENT, screen_, LV_ALIGN_TOP_LEFT, 0, y, lv_color_black());
    icon_charge_current_->SetState(WidgetIcon::State::ON);
    // Bar
    charge_current_bar_ =
        new WidgetTextBar(screen_, "N/A", LV_ALIGN_TOP_LEFT, 20, y - 1, defs::LCD_WIDTH - 20, 18, &orbitron_12);

    // Adapter Voltage (bottom left) and Charger Status (bottom right) labels below charge current bar
    y += 20;
    adapter_voltage_label_ = lv_label_create(screen_);
    lv_label_set_text_static(adapter_voltage_label_, "");
    lv_obj_set_style_text_color(adapter_voltage_label_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(adapter_voltage_label_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(adapter_voltage_label_, LV_ALIGN_TOP_LEFT, 0, y);

    charger_status_label_ = lv_label_create(screen_);
    lv_label_set_text_static(charger_status_label_, "");
    lv_obj_set_style_text_color(charger_status_label_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(charger_status_label_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(charger_status_label_, LV_ALIGN_TOP_RIGHT, 0, y);

    // Create bottombar for robot state (will be updated by Ticker widget later)
    CreateBottombar();
  }

  /**
   * @brief Update icon states based on current system state
   * Called periodically from display controller's Tick()
   */
  void Tick() override {
    UpdateGpsWidgets();
    UpdatePowerWidgets();
  }

 private:
  SaboRobot* sabo_robot_ = nullptr;

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

  // Power display widgets
  WidgetIcon* icon_charge_current_ = nullptr;
  WidgetTextBar* battery_voltage_bar_ = nullptr;
  WidgetTextBar* charge_current_bar_ = nullptr;
  lv_obj_t* adapter_voltage_label_ = nullptr;
  lv_obj_t* charger_status_label_ = nullptr;
  bool last_is_docked_ = false;  // Track docked state to show/hide dock-relevant widgets

  /**
   * @brief Calculate percentage from value within min/max range
   * @tparam T Numeric type (int, float, double, etc.)
   * @param value Current value
   * @param min Minimum value (0%)
   * @param max Maximum value (100%)
   * @param invert If true, inverts the percentage (max=0%, min=100%)
   * @return Percentage (0-100)
   */
  template <typename T>
  [[nodiscard]] int32_t GetPercent(T value, T min, T max, bool invert = false) {
    if (value <= min) return invert ? 100 : 0;
    if (value >= max) return invert ? 0 : 100;

    int32_t percent = static_cast<int32_t>((value - min) * 100 / (max - min));
    return invert ? 100 - percent : percent;
  }

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
    float accuracy = gps_state.position_h_accuracy;
    auto acc_percent = GetPercent(accuracy, 0.01f, 1.00f, true);
    static char accuracy_text[16];
    chsnprintf(accuracy_text, sizeof(accuracy_text), "%.2f m", accuracy);
    accuracy_bar_->SetValue(acc_percent, accuracy_text);

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
   * @brief Update power display widgets with current power data
   * Updates battery voltage bar, charge current bar, and topbar icons (docked, battery state)
   */
  void UpdatePowerWidgets() {
    if (sabo_robot_ == nullptr) return;

    // Read current power state from power service
    float battery_volts = power_service.GetBatteryVolts();
    float adapter_volts = power_service.GetAdapterVolts();
    float charge_current = power_service.GetChargeCurrent();

    // Determine docked status (adapter voltage indicates connection)
    bool is_docked = adapter_volts > 10.0f;

    // Battery voltage bar
    auto battery_percent = GetPercent(battery_volts, sabo_robot_->Power_GetAbsoluteMinVoltage(),
                                      sabo_robot_->Power_GetDefaultBatteryFullVoltage());
    static char battery_text[16];
    chsnprintf(battery_text, sizeof(battery_text), "%.1f V", battery_volts);
    battery_voltage_bar_->SetValue(battery_percent, battery_text);

    // Update docked related widgets
    if (is_docked) {
      // Charge current bar
      auto charge_percent = GetPercent(charge_current, 0.0f, sabo_robot_->Power_GetDefaultChargeCurrent());
      static char charge_text[16];
      chsnprintf(charge_text, sizeof(charge_text), "%.2f A", charge_current);
      charge_current_bar_->SetValue(charge_percent, charge_text);

      // Enable docked relevant widgets
      icon_charge_current_->SetState(WidgetIcon::State::ON);
      charge_current_bar_->Show();
      lv_obj_clear_flag(adapter_voltage_label_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(charger_status_label_, LV_OBJ_FLAG_HIDDEN);

      last_is_docked_ = true;
    } else {
      // When not docked, show 0% charge bar
      charge_current_bar_->SetValue(0, "0.00 A");

      // Hide docked relevant widgets
      icon_charge_current_->SetState(WidgetIcon::State::OFF);
      charge_current_bar_->Hide();
      lv_obj_add_flag(adapter_voltage_label_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(charger_status_label_, LV_OBJ_FLAG_HIDDEN);

      last_is_docked_ = false;
    }

    // Adapter voltage label (bottom left)
    static char adapter_text[16];
    chsnprintf(adapter_text, sizeof(adapter_text), "%.1f V", adapter_volts);
    lv_label_set_text(adapter_voltage_label_, adapter_text);

    // Update Charger Status Label (bottom right) with actual charger status
    lv_label_set_text(charger_status_label_, ChargerDriver::statusToString(power_service.GetChargerStatus()));

    // Update Topbar Icons
    if (is_docked) {
      icon_docked_->SetState(WidgetIcon::State::ON);
    } else {
      icon_docked_->SetState(WidgetIcon::State::OFF);
    }

    // Battery icon with 4 states based on battery percentage
    WidgetIcon::Icon battery_icon = WidgetIcon::Icon::BATTERY_FULL;
    WidgetIcon::State battery_state = WidgetIcon::State::ON;

    if (battery_percent <= 0) {
      battery_icon = WidgetIcon::Icon::BATTERY_EMPTY;
      battery_state = WidgetIcon::State::BLINK;  // Blink when critically low
    } else if (battery_percent <= 25) {
      battery_icon = WidgetIcon::Icon::BATTERY_EMPTY;
    } else if (battery_percent <= 50) {
      battery_icon = WidgetIcon::Icon::BATTERY_QUARTER;
    } else if (battery_percent <= 75) {
      battery_icon = WidgetIcon::Icon::BATTERY_HALF;
    } else if (battery_percent < 100) {
      battery_icon = WidgetIcon::Icon::BATTERY_THREE_QUARTER;
    } else {
      battery_icon = WidgetIcon::Icon::BATTERY_FULL;
    }

    icon_battery_->SetIcon(battery_icon);
    icon_battery_->SetState(battery_state);
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

    // Placeholder for robot state
    lv_obj_t* state_label = lv_label_create(bottombar_);
    lv_label_set_text_static(state_label, "TODO: Robot State");
    lv_obj_set_style_text_color(state_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(state_label, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(state_label, LV_ALIGN_CENTER, 0, 0);
  }

};  // class SaboScreenMain

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_MAIN_HPP_
