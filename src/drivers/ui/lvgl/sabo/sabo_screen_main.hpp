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

#include <cstdint>

#include "../../SaboCoverUI/sabo_cover_ui_controller.hpp"
#include "../screen_base.hpp"
#include "../widget_icon.hpp"
#include "../widget_textbar.hpp"
#include "ch.h"
#include "robots/include/sabo_common.hpp"
#include "robots/include/sabo_robot.hpp"
#include "services.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
LV_FONT_DECLARE(orbitron_16b);
}

namespace xbot::driver::ui::lvgl::sabo {

using GpsState = xbot::driver::gps::GpsDriver::GpsState;

class SaboScreenMain : public ScreenBase<ScreenId, ButtonId> {
 public:
  SaboScreenMain() : ScreenBase<ScreenId, ButtonId>(ScreenId::MAIN) {
    if (robot != nullptr) sabo_robot_ = static_cast<SaboRobot*>(robot);
  }

  ~SaboScreenMain() override {
    // Unregister from emergency change events
    chEvtUnregister(&mower_events, &emergency_event_listener_);

    for (auto& [type, icon] : emergency_icons_) {
      if (icon) delete icon;
    }
  }

  void Create(lv_color_t bg_color = lv_color_white()) override {
    ScreenBase::Create(bg_color);

    // Register for emergency change events
    chEvtRegister(&mower_events, &emergency_event_listener_, Events::GLOBAL);

    // Topbar container
    CreateTopbar();

    // Content Area
    int32_t y = 24;

    // Satellites in use icon
    new WidgetIcon(WidgetIcon::Icon::SATELLITE, screen_, LV_ALIGN_TOP_LEFT, 0, y - 2, WidgetIcon::State::ON);
    sats_value_ = lv_label_create(screen_);
    lv_label_set_text_static(sats_value_, "0");
    lv_obj_set_style_text_color(sats_value_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(sats_value_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(sats_value_, LV_ALIGN_TOP_LEFT, 17, y);

    // NTRIP actuality icon
    new WidgetIcon(WidgetIcon::Icon::DOWNLOAD, screen_, LV_ALIGN_TOP_MID, -55, y - 2, WidgetIcon::State::ON);
    ntrip_value_ = lv_label_create(screen_);
    lv_label_set_text_static(ntrip_value_, "N/A");
    lv_obj_set_style_text_color(ntrip_value_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(ntrip_value_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(ntrip_value_, LV_ALIGN_TOP_LEFT, 75, y);

    // GPS Mode icon
    icon_gps_mode_ =
        new WidgetIcon(WidgetIcon::Icon::NO_RTK_FIX, screen_, LV_ALIGN_TOP_RIGHT, -84, y - 2, WidgetIcon::State::BLINK);
    gps_mode_value_ = lv_label_create(screen_);
    lv_label_set_text_static(gps_mode_value_, "N/A");
    lv_obj_set_style_text_color(gps_mode_value_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(gps_mode_value_, &orbitron_12, LV_PART_MAIN);
    lv_obj_align(gps_mode_value_, LV_ALIGN_TOP_LEFT, defs::LCD_WIDTH - 80, y);

    // GPS Accuracy
    y += 18;
    new WidgetIcon(WidgetIcon::Icon::BULLSEYE, screen_, LV_ALIGN_TOP_LEFT, 0, y, WidgetIcon::State::ON);
    accuracy_bar_ =
        new WidgetTextBar(screen_, "N/A", LV_ALIGN_TOP_LEFT, 20, y - 1, defs::LCD_WIDTH - 20, 18, &orbitron_12);

    // Battery Voltage
    y += 32;
    new WidgetIcon(WidgetIcon::Icon::BATTERY_VOLTAGE, screen_, LV_ALIGN_TOP_LEFT, 0, y, WidgetIcon::State::ON);
    battery_voltage_bar_ =
        new WidgetTextBar(screen_, "N/A", LV_ALIGN_TOP_LEFT, 20, y - 1, defs::LCD_WIDTH - 20, 18, &orbitron_12);

    // Charge Current
    y += 22;
    icon_charge_current_ =
        new WidgetIcon(WidgetIcon::Icon::CHARGE_CURRENT, screen_, LV_ALIGN_TOP_LEFT, 0, y, WidgetIcon::State::ON);
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

    // Create state label container with scrolling text
    CreateStateLabel();
  }

  /**
   * @brief Update widget states based on current system state
   * Called periodically from display controller's Tick()
   * 5-phase round-robin updates (30ms intervals) to spread lv_timer_handler() load:
   * - Phase 0: GPS data (satellites, mode, NTRIP)
   * - Phase 1: GPS bar (accuracy display)
   * - Phase 2: Battery widgets (icon, voltage)
   * - Phase 3: Dock widgets (docked state, charge, adapter)
   * - Phase 4: Robot state + Emergency check (event-driven)
   * Complete cycle: 150ms (still faster than human perception)
   */
  void Tick() override {
    static uint8_t update_phase = 0;

    switch (update_phase) {
      case 0: UpdateGpsData(); break;
      case 1: UpdateGpsBar(); break;
      case 2: UpdateBatteryWidgets(); break;
      case 3: UpdateDockWidgets(); break;
      case 4: UpdateRobotState(); break;
    }

    // Do always Emergency check
    eventflags_t flags = chEvtGetAndClearFlags(&emergency_event_listener_);
    if (flags & MowerEvents::EMERGENCY_CHANGED) {
      UpdateEmergencyWidgets();
    }

    update_phase = (update_phase + 1) % 5;  // Cycle through 5 phases
  }

 private:
  SaboRobot* sabo_robot_ = nullptr;

  static constexpr lv_coord_t TOPBAR_HEIGHT = 19;
  static constexpr lv_coord_t STATELABEL_HEIGHT = 23;

  lv_obj_t* topbar_ = nullptr;
  lv_obj_t* state_container_ = nullptr;

  // Status icons (initialized in CreateTopbar)
  WidgetIcon* icon_docked_ = nullptr;
  WidgetIcon* icon_battery_ = nullptr;

  // Emergency tracking (also initialized in CreateTopbar)
  enum class EmergencyIconType : uint8_t { GENERIC = 0, STOP, WHEEL, TIMEOUT_ANY, TIMEOUT_HL };
  etl::flat_map<EmergencyIconType, WidgetIcon*, 5> emergency_icons_;
  event_listener_t emergency_event_listener_;
  uint16_t last_emergency_reasons_ = 0;

  // GPS display widgets
  WidgetIcon* icon_gps_mode_ = nullptr;
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

  // Robot state label
  lv_obj_t* state_label_ = nullptr;

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
   * [ROS] [Emergency Container] [GPS] [Battery] [Docking]
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

    // Emergency icons, create them all to a fixed position (middle left, grouped together)
    emergency_icons_.insert(
        {EmergencyIconType::GENERIC, new WidgetIcon(WidgetIcon::Icon::EMERGENCY_GENERIC, topbar_, LV_ALIGN_TOP_MID, -40,
                                                    0, WidgetIcon::State::BLINK, lv_color_white())});
    emergency_icons_.insert(
        {EmergencyIconType::STOP, new WidgetIcon(WidgetIcon::Icon::HAND_STOP, topbar_, LV_ALIGN_TOP_MID, -20, 0,
                                                 WidgetIcon::State::OFF, lv_color_white())});
    emergency_icons_.insert(
        {EmergencyIconType::WHEEL, new WidgetIcon(WidgetIcon::Icon::EMERGENCY_WHEEL_LIFT, topbar_, LV_ALIGN_TOP_MID, 0,
                                                  0, WidgetIcon::State::OFF, lv_color_white())});
    emergency_icons_.insert(
        {EmergencyIconType::TIMEOUT_ANY, new WidgetIcon(WidgetIcon::Icon::TIMEOUT, topbar_, LV_ALIGN_TOP_MID, 20, 0,
                                                        WidgetIcon::State::BLINK, lv_color_white())});
    // ROS connected get shown separately in left edge
    emergency_icons_.insert(
        {EmergencyIconType::TIMEOUT_HL, new WidgetIcon(WidgetIcon::Icon::ROS, topbar_, LV_ALIGN_TOP_LEFT, 3, 0,
                                                       WidgetIcon::State::BLINK, lv_color_white())});

    // Right side: Battery icon
    icon_battery_ = new WidgetIcon(WidgetIcon::Icon::BATTERY_FULL, topbar_, LV_ALIGN_TOP_RIGHT, -23, 0,
                                   WidgetIcon::State::ON, lv_color_white());

    // Right side: Docking icon
    icon_docked_ = new WidgetIcon(WidgetIcon::Icon::DOCKED, topbar_, LV_ALIGN_TOP_RIGHT, -3, 0, WidgetIcon::State::OFF,
                                  lv_color_white());
  }

  /**
   * @brief Update GPS data widgets (num satellites, mode, NTRIP)
   */
  void UpdateGpsData() {
    static bool last_invalid = false;
    if (!gps_service.IsGpsStateValid()) {
      if (!last_invalid) {
        lv_label_set_text_fmt(sats_value_, "%d", 0);
        icon_gps_mode_->SetIcon(WidgetIcon::Icon::NO_RTK_FIX);
        icon_gps_mode_->SetState(WidgetIcon::State::BLINK);
        lv_label_set_text(gps_mode_value_, "N/A");
        lv_label_set_text(ntrip_value_, "N/A");
        last_invalid = true;
      }
      return;
    }

    last_invalid = false;
    const auto& gps_state = gps_service.GetGpsState();

    // Update satellites count only if changed
    static uint8_t last_num_sats = 0;
    if (gps_state.num_sv != last_num_sats) {
      lv_label_set_text_fmt(sats_value_, "%d", gps_state.num_sv);
      last_num_sats = gps_state.num_sv;
    }

    // Update GPS mode and icon only if changed
    static GpsState last_gps_state = {};
    if (gps_state.rtk_type != last_gps_state.rtk_type || gps_state.fix_type != last_gps_state.fix_type) {
      const char* gps_mode = "N/A";
      WidgetIcon::Icon gps_icon = WidgetIcon::Icon::NO_RTK_FIX;
      WidgetIcon::State gps_icon_state = WidgetIcon::State::BLINK;

      switch (gps_state.rtk_type) {
        case GpsState::RTK_FIX:
          gps_mode = "RTK FIX";
          gps_icon = WidgetIcon::Icon::RTK_FIX;
          gps_icon_state = WidgetIcon::State::ON;
          break;
        case GpsState::RTK_FLOAT: gps_mode = "RTK FLOAT"; break;
        default:
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
      icon_gps_mode_->SetIcon(gps_icon);
      icon_gps_mode_->SetState(gps_icon_state);
    }

    // Update NTRIP seconds only if changed
    static uint32_t last_seconds_since_rtcm = 0;
    uint32_t seconds_since_rtcm = gps_service.GetSecondsSinceLastRtcmPacket();
    if (seconds_since_rtcm != last_seconds_since_rtcm) {
      if (seconds_since_rtcm == 0) {
        lv_label_set_text(ntrip_value_, "N/A");
      } else if (seconds_since_rtcm > 99) {
        lv_label_set_text(ntrip_value_, ">99 sec");
      } else {
        lv_label_set_text_fmt(ntrip_value_, "%lu sec", seconds_since_rtcm);
      }
      last_seconds_since_rtcm = seconds_since_rtcm;
    }

    // Update last_gps_state with minimal copy (only needed fields)
    last_gps_state.rtk_type = gps_state.rtk_type;
    last_gps_state.fix_type = gps_state.fix_type;
  }

  /**
   * @brief Update GPS accuracy bar
   */
  void UpdateGpsBar() {
    if (!gps_service.IsGpsStateValid()) {
      accuracy_bar_->SetValuesOnBarChange(0, "N/A");
      return;
    }

    const auto& gps_state = gps_service.GetGpsState();

    // Update accuracy text only if changed (0.01m resolution)
    static float last_accuracy = -1.0f;
    float accuracy = std::round(gps_state.position_h_accuracy * 100.0f) / 100.0f;
    if (accuracy != last_accuracy) {
      static char accuracy_text[16];
      // Integer formatting is faster than float formatting
      const int accuracy_int = static_cast<int>(accuracy * 100);
      chsnprintf(accuracy_text, sizeof(accuracy_text), "%d.%02d m", accuracy_int / 100, accuracy_int % 100);
      const auto acc_percent = GetPercent(accuracy, 0.01f, 1.00f, true);
      accuracy_bar_->SetValues(acc_percent, accuracy_text);
      last_accuracy = accuracy;
    }
  }

  /**
   * @brief Update battery widgets (icon, voltage bar)
   */
  void UpdateBatteryWidgets() {
    if (sabo_robot_ == nullptr) return;

    // Static variables for caching and change detection
    static float last_battery_volts = -1.0f;
    static WidgetIcon::Icon last_battery_icon = WidgetIcon::Icon::BATTERY_FULL;
    static WidgetIcon::State last_battery_state = WidgetIcon::State::ON;

    // Get current values with reduced precision (0.1V resolution)
    const float battery_volts = std::round(power_service.GetBatteryVolts() * 10.0f) / 10.0f;

    // Calculate battery percentage
    const auto battery_percent = GetPercent(battery_volts, sabo_robot_->Power_GetAbsoluteMinVoltage(),
                                            sabo_robot_->Power_GetDefaultBatteryFullVoltage());

    // Determine battery icon and state based on percentage
    WidgetIcon::Icon battery_icon = WidgetIcon::Icon::BATTERY_FULL;
    WidgetIcon::State battery_state = WidgetIcon::State::ON;

    if (battery_percent <= 0) {
      battery_icon = WidgetIcon::Icon::BATTERY_EMPTY;
      battery_state = WidgetIcon::State::BLINK;
    } else if (battery_percent <= 25) {
      battery_icon = WidgetIcon::Icon::BATTERY_EMPTY;
    } else if (battery_percent <= 50) {
      battery_icon = WidgetIcon::Icon::BATTERY_QUARTER;
    } else if (battery_percent <= 75) {
      battery_icon = WidgetIcon::Icon::BATTERY_HALF;
    } else if (battery_percent < 100) {
      battery_icon = WidgetIcon::Icon::BATTERY_THREE_QUARTER;
    }

    // Update battery icon only if changed
    if (battery_icon != last_battery_icon || battery_state != last_battery_state) {
      icon_battery_->SetIcon(battery_icon);
      icon_battery_->SetState(battery_state);
      last_battery_icon = battery_icon;
      last_battery_state = battery_state;
    }

    // Update battery voltage label only if changed (0.1V resolution)
    if (battery_volts != last_battery_volts) {
      static char battery_text[16];
      // Integer formatting is faster than float formatting
      const int battery_volts_int = static_cast<int>(battery_volts * 10);
      chsnprintf(battery_text, sizeof(battery_text), "%d.%dV", battery_volts_int / 10, battery_volts_int % 10);
      battery_voltage_bar_->SetValuesOnBarChange(battery_percent, battery_text);
      last_battery_volts = battery_volts;
    }
  }

  /**
   * @brief Update dock-related widgets (docked state, adapter voltage, charge current, charger status)
   */
  void UpdateDockWidgets() {
    if (sabo_robot_ == nullptr) return;

    // Static variables for caching and change detection
    static float last_adapter_volts = -1.0f;
    static float last_charge_current = -1.0f;
    static bool last_is_docked = false;
    static CHARGER_STATUS last_charger_status = CHARGER_STATUS::UNKNOWN;

    // Get current values with reduced precision (0.1V / 0.01A resolution)
    const float adapter_volts = std::round(power_service.GetAdapterVolts() * 10.0f) / 10.0f;
    const float charge_current = std::round(power_service.GetChargeCurrent() * 100.0f) / 100.0f;
    const bool is_docked = adapter_volts > 10.0f;
    const CHARGER_STATUS charger_status = power_service.GetChargerStatus();

    // Update docked state only if changed
    if (is_docked != last_is_docked) {
      icon_docked_->SetState(is_docked ? WidgetIcon::State::ON : WidgetIcon::State::OFF);

      if (is_docked) {
        icon_charge_current_->SetState(WidgetIcon::State::ON);
        charge_current_bar_->Show();
        lv_obj_clear_flag(adapter_voltage_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(charger_status_label_, LV_OBJ_FLAG_HIDDEN);
      } else {
        icon_charge_current_->SetState(WidgetIcon::State::OFF);
        charge_current_bar_->Hide();
        lv_obj_add_flag(adapter_voltage_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(charger_status_label_, LV_OBJ_FLAG_HIDDEN);
      }

      last_is_docked = is_docked;
    }

    // Update adapter voltage label only if changed (0.1V resolution)
    if (adapter_volts != last_adapter_volts) {
      static char adapter_text[16];
      const int adapter_volts_int = static_cast<int>(adapter_volts * 10);
      chsnprintf(adapter_text, sizeof(adapter_text), "%d.%dV", adapter_volts_int / 10, adapter_volts_int % 10);
      lv_label_set_text(adapter_voltage_label_, adapter_text);
      last_adapter_volts = adapter_volts;
    }

    // Update charge current only if docked AND changed (0.01A resolution)
    if (is_docked && charge_current != last_charge_current) {
      static char charge_text[16];
      const int charge_current_int = static_cast<int>(charge_current * 100);
      chsnprintf(charge_text, sizeof(charge_text), "%d.%02dA", charge_current_int / 100, charge_current_int % 100);
      const auto charge_percent = GetPercent(charge_current, 0.0f, 10.0f, false);  // 0-10A range
      charge_current_bar_->SetValuesOnBarChange(charge_percent, charge_text);
      last_charge_current = charge_current;
    }

    // Update charger status only if changed
    if (charger_status != last_charger_status) {
      lv_label_set_text(charger_status_label_, ChargerDriver::statusToString(charger_status));
      last_charger_status = charger_status;
    }

    // Update last_is_docked_ for external use
    last_is_docked_ = is_docked;
  }

  /**
   * @brief Update robot state display (placeholder for future implementation)
   */
  void UpdateRobotState() {
    // TODO: Implement robot state display
    // This function is called in phase 4 of the round-robin
  }

  /**
   * @brief Update emergency display widgets based on current emergency state
   * Called when emergency state changes (MowerEvents::EMERGENCY_CHANGED)
   */
  void UpdateEmergencyWidgets() {
    static uint16_t triggered_emergency_reasons_ = 0;
    uint16_t current_reasons = emergency_service.GetEmergencyReasons();

    // Track initially triggered emergency reasons
    if (last_emergency_reasons_ == 0 && current_reasons != 0) {
      triggered_emergency_reasons_ = current_reasons;
    } else if (current_reasons == 0) {
      triggered_emergency_reasons_ = 0;
    }

    if (current_reasons == last_emergency_reasons_) {
      return;
    }
    last_emergency_reasons_ = current_reasons;

    uint16_t display_reasons = current_reasons | triggered_emergency_reasons_;
    for (auto& [type, icon] : emergency_icons_) {
      if (icon != nullptr) {
        WidgetIcon::State new_state = WidgetIcon::State::OFF;
        switch (type) {
          case EmergencyIconType::GENERIC:
            if (display_reasons != 0) new_state = WidgetIcon::State::BLINK;
            break;
          case EmergencyIconType::STOP:
            if (display_reasons & EmergencyReason::STOP) new_state = WidgetIcon::State::BLINK;
            break;
          case EmergencyIconType::WHEEL:
            if (display_reasons & (EmergencyReason::LIFT | EmergencyReason::LIFT_MULTIPLE | EmergencyReason::COLLISION))
              new_state = WidgetIcon::State::BLINK;
            break;
          case EmergencyIconType::TIMEOUT_ANY:
            if (display_reasons & (EmergencyReason::TIMEOUT_HIGH_LEVEL | EmergencyReason::TIMEOUT_INPUTS))
              new_state = WidgetIcon::State::BLINK;
            break;
          case EmergencyIconType::TIMEOUT_HL:
            if (current_reasons & EmergencyReason::TIMEOUT_HIGH_LEVEL)
              new_state = WidgetIcon::State::BLINK;
            else
              new_state = WidgetIcon::State::ON;
            break;
        }
        icon->SetState(new_state);
      }
    }
  }

  /**
   * @brief Create a bordered container with scrolling label for robot state display
   *
   * Layout:
   * [bordered rectangle] [Robot State like IDLE/MOWING/PAUSE/etc]
   * The label uses circular scrolling if text exceeds container width.
   */
  void CreateStateLabel() {
    // Create container with border (no fill) at bottom of screen
    state_container_ = lv_obj_create(screen_);
    lv_obj_set_size(state_container_, defs::LCD_WIDTH, STATELABEL_HEIGHT);
    lv_obj_set_pos(state_container_, 0, defs::LCD_HEIGHT - STATELABEL_HEIGHT);
    lv_obj_set_style_border_width(state_container_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(state_container_, lv_color_black(), LV_PART_MAIN);

    // Create scrolling label
    state_label_ = lv_label_create(screen_);  // Doesn't work with state_container_ as parent. Whyever :-/
    lv_label_set_text(state_label_, "STATE: ToDo");
    lv_obj_set_style_text_color(state_label_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(state_label_, &orbitron_16b, LV_PART_MAIN);
    lv_obj_align(state_label_, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_label_set_long_mode(state_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(state_label_, defs::LCD_WIDTH - 6);
  }

};  // class SaboScreenMain

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_MAIN_HPP_
