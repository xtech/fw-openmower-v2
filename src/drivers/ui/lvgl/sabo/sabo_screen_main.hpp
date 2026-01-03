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

// clang-format off
#include <ch.h>   // Includes chconf.h which defines CHPRINTF_USE_FLOAT
#include <hal.h>  // Defines BaseSequentialStream
#include <chprintf.h>
// clang-format on
#include <lvgl.h>
#include <ulog.h>

#include <cstdint>

#include "../../SaboCoverUI/sabo_cover_ui_controller.hpp"
#include "../screen_base.hpp"
#include "../widget_icon.hpp"
#include "../widget_textbar.hpp"
#include "robots/include/sabo_common.hpp"
#include "robots/include/sabo_robot.hpp"
#include "services.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
LV_FONT_DECLARE(orbitron_16b);
}

namespace xbot::driver::ui::lvgl::sabo {

using namespace xbot::driver::sabo::types;
using GpsState = xbot::driver::gps::GpsDriver::GpsState;

class SaboScreenMain : public ScreenBase<ScreenId, ButtonId> {
 public:
  SaboScreenMain() : ScreenBase<ScreenId, ButtonId>(ScreenId::MAIN) {
    if (robot != nullptr) {
      sabo_robot_ = static_cast<SaboRobot*>(robot);
      bms_data_ = sabo_robot_->GetBmsData();
    }
  }

  ~SaboScreenMain() override {
    // Unregister from emergency change events
    chEvtUnregister(&mower_events, &emergency_event_listener_);

    // Delete dynamically allocated WidgetIcon objects
    delete icon_satellite_;
    delete icon_download_;
    delete icon_gps_mode_;
    delete icon_bullseye_;
    delete icon_battery_voltage_;
    delete icon_current_;
    delete icon_battery_;
    delete icon_docked_;

    // Delete WidgetTextBar objects
    delete accuracy_bar_;
    delete battery_voltage_bar_;
    delete current_bar_;

    // Delete emergency icons
    for (auto& [type, icon] : emergency_icons_) {
      if (icon) delete icon;
    }
  }

  void Create(lv_color_t bg_color = lv_color_white(), lv_color_t fg_color = lv_color_black()) override {
    ScreenBase::Create(bg_color, fg_color);
    lv_obj_set_style_text_font(screen_, &orbitron_12, LV_PART_MAIN);

    // Register for emergency change events
    chEvtRegister(&mower_events, &emergency_event_listener_, Events::GLOBAL);

    // Topbar container
    CreateTopbar();

    // Content Area
    int32_t y = 24;

    // Satellites in use icon
    icon_satellite_ =
        new WidgetIcon(WidgetIcon::Icon::SATELLITE, screen_, LV_ALIGN_TOP_LEFT, 0, y - 2, WidgetIcon::State::ON);
    sats_value_ = lv_label_create(screen_);
    lv_label_set_text_static(sats_value_, "0");
    lv_obj_align(sats_value_, LV_ALIGN_TOP_LEFT, 17, y);

    // NTRIP actuality icon
    icon_download_ =
        new WidgetIcon(WidgetIcon::Icon::DOWNLOAD, screen_, LV_ALIGN_TOP_MID, -55, y - 2, WidgetIcon::State::ON);
    ntrip_value_ = lv_label_create(screen_);
    lv_label_set_text_static(ntrip_value_, "N/A");
    lv_obj_align(ntrip_value_, LV_ALIGN_TOP_LEFT, 75, y);

    // GPS Mode icon
    icon_gps_mode_ =
        new WidgetIcon(WidgetIcon::Icon::NO_RTK_FIX, screen_, LV_ALIGN_TOP_RIGHT, -84, y - 2, WidgetIcon::State::BLINK);
    gps_mode_value_ = lv_label_create(screen_);
    lv_label_set_text_static(gps_mode_value_, "N/A");
    lv_obj_align(gps_mode_value_, LV_ALIGN_TOP_LEFT, defs::LCD_WIDTH - 80, y);

    // GPS Accuracy
    y += 18;
    icon_bullseye_ =
        new WidgetIcon(WidgetIcon::Icon::BULLSEYE, screen_, LV_ALIGN_TOP_LEFT, 0, y, WidgetIcon::State::ON);
    accuracy_bar_ =
        new WidgetTextBar(screen_, "N/A", LV_ALIGN_TOP_LEFT, 20, y - 1, defs::LCD_WIDTH - 20, 18, &orbitron_12);

    // Battery Voltage
    y += 32;
    icon_battery_voltage_ =
        new WidgetIcon(WidgetIcon::Icon::BATTERY_VOLTAGE, screen_, LV_ALIGN_TOP_LEFT, 0, y, WidgetIcon::State::ON);
    battery_voltage_bar_ =
        new WidgetTextBar(screen_, "N/A", LV_ALIGN_TOP_LEFT, 20, y - 1, defs::LCD_WIDTH - 20, 18, &orbitron_12);

    // Charge Current
    y += 22;
    icon_current_ = new WidgetIcon(WidgetIcon::Icon::CURRENT, screen_, LV_ALIGN_TOP_LEFT, 0, y, WidgetIcon::State::ON);
    current_bar_ =
        new WidgetTextBar(screen_, "N/A", LV_ALIGN_TOP_LEFT, 20, y - 1, defs::LCD_WIDTH - 20, 18, &orbitron_12);

    // Adapter Voltage (bottom left) and Charger Status (bottom right) labels below charge current bar
    y += 20;
    adapter_voltage_label_ = lv_label_create(screen_);
    lv_label_set_text_static(adapter_voltage_label_, "");
    lv_obj_align(adapter_voltage_label_, LV_ALIGN_TOP_LEFT, 0, y);

    // Charger status label
    charger_status_label_ = lv_label_create(screen_);
    lv_label_set_text_static(charger_status_label_, "");
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
   * - Phase 3: Current widgets (icon, current)
   * - Phase 4: Dock widgets (docked state, charge, adapter)
   * - Phase 5: Robot state + Emergency check (event-driven)
   * Complete cycle: 180ms (still faster than human perception)
   */
  void Tick() override {
    static uint8_t update_phase = 0;

    switch (update_phase) {
      case 0: UpdateGpsData(); break;
      case 1: UpdateGpsBar(); break;
      case 2: UpdateBatteryWidgets(); break;
      case 3: UpdateCurrentWidgets(); break;
      case 4: UpdateDockWidgets(); break;
      case 5: UpdateRobotState(); break;
    }

    // Do always Emergency check
    eventflags_t flags = chEvtGetAndClearFlags(&emergency_event_listener_);
    if (flags & (MowerEvents::EMERGENCY_CHANGED)) {
      UpdateEmergencyWidgets();
    } else {
      // Get SaboInputDriver sensors for changes directly, for pre-ROS as well as pre-Emergency display
      if (sabo_robot_ != nullptr) {
        uint8_t current_bits = sabo_robot_->GetSensorBits();

        if (current_bits != last_sensor_bits_) {
          last_sensor_bits_ = current_bits;
          UpdateEmergencyWidgets();
        }
      }
    }

    update_phase = (update_phase + 1) % 5;  // Cycle through 5 phases

    // DEBUG: BMS testing only
    /*static systime_t last_test_time = 0;
    const systime_t now = chVTGetSystemTime();
    if (chVTTimeElapsedSinceX(last_test_time) > TIME_MS2I(5000)) {
      last_test_time = now;
      if (sabo_robot_ != nullptr) {
        sabo_robot_->DumpBms();
      }
    }*/
  }

 private:
  SaboRobot* sabo_robot_ = nullptr;
  const xbot::driver::bms::Data* bms_data_ = nullptr;

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

  // State tracking
  uint8_t last_sensor_bits_ = 0;
  bool is_docked_ = false;
  bool last_is_docked_ = true;
  bool has_bms_current_ = false;

  // GPS display widgets
  WidgetIcon* icon_satellite_ = nullptr;
  WidgetIcon* icon_download_ = nullptr;
  WidgetIcon* icon_gps_mode_ = nullptr;
  WidgetIcon* icon_bullseye_ = nullptr;
  WidgetTextBar* accuracy_bar_ = nullptr;
  lv_obj_t* sats_value_ = nullptr;
  lv_obj_t* gps_mode_value_ = nullptr;
  lv_obj_t* ntrip_value_ = nullptr;

  // Power display widgets
  WidgetIcon* icon_battery_voltage_ = nullptr;
  WidgetIcon* icon_current_ = nullptr;
  WidgetTextBar* battery_voltage_bar_ = nullptr;
  WidgetTextBar* current_bar_ = nullptr;
  lv_obj_t* adapter_voltage_label_ = nullptr;
  lv_obj_t* charger_status_label_ = nullptr;

  // Robot state label
  lv_obj_t* state_label_ = nullptr;

  // Shared text buffer for formatting (used by all update methods)
  static constexpr size_t SHARED_TEXT_BUFFER_SIZE = 32;
  char shared_text_buffer_[SHARED_TEXT_BUFFER_SIZE];

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
    icon_docked_ = new WidgetIcon(WidgetIcon::Icon::DOCKED, topbar_, LV_ALIGN_TOP_RIGHT, -3, 0, WidgetIcon::State::ON,
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
      chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "%.2fm", accuracy);
      const auto acc_percent = GetPercent(accuracy, 0.01f, 1.00f, true);
      accuracy_bar_->SetValues(acc_percent, shared_text_buffer_);
      last_accuracy = accuracy;
    }
  }

  /**
   * @brief Update battery widgets (icon, voltage bar)
   */
  void UpdateBatteryWidgets() {
    if (sabo_robot_ == nullptr) return;

    // Static variables for caching and change detection
    static float last_battery_volts;
    static int32_t last_battery_percent;
    static WidgetIcon::Icon last_battery_icon = WidgetIcon::Icon::BATTERY_FULL;
    static WidgetIcon::State last_battery_state = WidgetIcon::State::ON;

    // Prefer BMS pack voltage over power-service voltage. Reduced precision (0.1V resolution)
    const float battery_volts = bms_data_ != nullptr && bms_data_->pack_voltage_v > 10.0f
                                    ? std::round(bms_data_->pack_voltage_v * 10.0f) / 10.0f
                                    : std::round(power_service.GetBatteryVolts() * 10.0f) / 10.0f;

    // Prefer BMS battery_soc over calculated battery percentage
    const auto battery_percent = bms_data_ != nullptr && bms_data_->battery_soc > 0.0f
                                     ? static_cast<int32_t>(bms_data_->battery_soc * 100)
                                     : GetPercent(battery_volts, sabo_robot_->Power_GetAbsoluteMinVoltage(),
                                                  sabo_robot_->Power_GetDefaultBatteryFullVoltage());

    if (battery_percent != last_battery_percent) {
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
    }

    // Update battery %/V label only if changed (0.1V resolution)
    if (battery_volts != last_battery_volts || battery_percent != last_battery_percent) {
      chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "%d%% / %.1fV", battery_percent, battery_volts);
      battery_voltage_bar_->SetValuesOnBarChange(battery_percent, shared_text_buffer_);

      last_battery_volts = battery_volts;
      last_battery_percent = battery_percent;
    }
  }

  /**
   * @brief Update current related widgets
   */
  void UpdateCurrentWidgets() {
    if (sabo_robot_ == nullptr) return;

    has_bms_current_ = (bms_data_ != nullptr && bms_data_->pack_current_a != 0.0f);

    // Static variables for caching and change detection
    static float last_current;

    if (!is_docked_ && !last_is_docked_ && !has_bms_current_) {
      return;  // Not docked, no BMS current available, skip update
    }

    // Get current values with reduced precision (0.1V / 0.01A resolution)
    const float current = has_bms_current_ ? std::round(bms_data_->pack_current_a * 100.0f) / 100.0f
                                           : std::round(power_service.GetChargeCurrent() * 100.0f) / 100.0f;

    if (current != last_current) {
      chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "%.2fA", current);
      const auto current_percent = GetPercent(fabs(current), 0.0f, sabo_robot_->Power_GetDefaultChargeCurrent());
      current_bar_->SetValuesOnBarChange(current_percent, shared_text_buffer_);
      current_bar_->SetBaseDir(current < 0.0f ? LV_BASE_DIR_RTL : LV_BASE_DIR_LTR);
      last_current = current;
    }
  }

  /**
   * @brief Update dock-related widgets (docked state, adapter voltage, charge current, charger status)
   */
  void UpdateDockWidgets() {
    if (sabo_robot_ == nullptr) return;

    // Static variables for caching and change detection
    static float last_adapter_volts;
    static CHARGER_STATUS last_charger_status = CHARGER_STATUS::UNKNOWN;

    // Get current values with reduced precision (0.1V / 0.01A resolution)
    const float adapter_volts = std::round(power_service.GetAdapterVolts() * 10.0f) / 10.0f;
    const CHARGER_STATUS charger_status = power_service.GetChargerStatus();
    is_docked_ = adapter_volts > 10.0f;

    // Update docked state only if changed
    if (is_docked_ != last_is_docked_) {
      icon_docked_->SetState(is_docked_ ? WidgetIcon::State::ON : WidgetIcon::State::OFF);

      if (is_docked_) {
        icon_current_->SetState(WidgetIcon::State::ON);
        current_bar_->Show();
        lv_obj_remove_flag(adapter_voltage_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(charger_status_label_, LV_OBJ_FLAG_HIDDEN);

        // Update adapter voltage label only if changed (0.1V resolution)
        if (adapter_volts != last_adapter_volts) {
          chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "%.1fV", adapter_volts);
          lv_label_set_text(adapter_voltage_label_, shared_text_buffer_);
          last_adapter_volts = adapter_volts;
        }

        // Update charger status only if changed
        if (charger_status != last_charger_status) {
          lv_label_set_text(charger_status_label_, ChargerDriver::statusToString(charger_status));
          last_charger_status = charger_status;
        }
      } else if (!has_bms_current_) {
        icon_current_->SetState(WidgetIcon::State::OFF);
        current_bar_->Hide();
      } else {
        lv_obj_add_flag(adapter_voltage_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(charger_status_label_, LV_OBJ_FLAG_HIDDEN);
      }

      last_is_docked_ = is_docked_;
    }
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
   * Called when emergency state changes (MowerEvents::EMERGENCY_CHANGED), or
   * when input state changes
   */
  void UpdateEmergencyWidgets() {
    static uint16_t triggered_emergency_reasons_ = 0;
    static uint16_t last_sensor_reasons_ = 0;
    uint16_t service_reasons = emergency_service.GetEmergencyReasons();
    uint16_t display_reasons = 0;

    // Track initially triggered emergency reasons
    if (last_emergency_reasons_ == 0 && service_reasons != 0) {
      triggered_emergency_reasons_ = service_reasons;
    } else if (service_reasons == 0) {
      triggered_emergency_reasons_ = 0;
    }

    // SaboInputDriver sensor states as emergency reasons using cached bits
    if (last_sensor_bits_ &
        ((1 << static_cast<uint8_t>(SensorId::STOP_TOP)) | (1 << static_cast<uint8_t>(SensorId::STOP_REAR)))) {
      display_reasons |= EmergencyReason::STOP;
    }
    if (last_sensor_bits_ &
        ((1 << static_cast<uint8_t>(SensorId::LIFT_FL)) | (1 << static_cast<uint8_t>(SensorId::LIFT_FR)))) {
      display_reasons |= EmergencyReason::LIFT;
    }

    if (service_reasons == last_emergency_reasons_ && display_reasons == last_sensor_reasons_) {
      return;
    }
    last_emergency_reasons_ = service_reasons;
    last_sensor_reasons_ = display_reasons;

    display_reasons |= service_reasons | triggered_emergency_reasons_;
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
            if (service_reasons & EmergencyReason::TIMEOUT_HIGH_LEVEL)
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
    lv_label_set_text(state_label_, "Robot State");
    lv_obj_set_style_text_font(state_label_, &orbitron_16b, LV_PART_MAIN);
    lv_obj_align(state_label_, LV_ALIGN_BOTTOM_MID, defs::LCD_WIDTH / 4, -2);
    lv_label_set_long_mode(state_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(state_label_, defs::LCD_WIDTH - 6);
  }

};  // class SaboScreenMain

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_MAIN_HPP_
