/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_screen_battery.hpp
 * @brief Battery Screen with BMS information and cell visualization
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2026-01-02
 */

#ifndef LVGL_SABO_SCREEN_BATTERY_HPP_
#define LVGL_SABO_SCREEN_BATTERY_HPP_

// clang-format off
#include <ch.h>   // Includes chconf.h which defines CHPRINTF_USE_FLOAT
#include <hal.h>  // Defines BaseSequentialStream
#include <chprintf.h>
// clang-format on
#include <etl/string.h>
#include <lvgl.h>
#include <ulog.h>

#include <cstdint>

#include "../../SaboCoverUI/sabo_cover_ui_controller.hpp"
#include "../screen_base.hpp"
#include "../widget_icon.hpp"
#include "robots/include/sabo_common.hpp"
#include "robots/include/sabo_robot.hpp"
#include "services.hpp"

extern "C" {
LV_FONT_DECLARE(orbitron_12);
LV_FONT_DECLARE(orbitron_16b);
}

namespace xbot::driver::ui::lvgl::sabo {

using namespace xbot::driver::sabo::types;

class SaboScreenBattery : public ScreenBase<ScreenId, ButtonId> {
 public:
  SaboScreenBattery();
  ~SaboScreenBattery() override;

  void Create(lv_color_t bg_color = lv_color_white()) override;

  /**
   * @brief Activate screen for input focus
   * This enables scrolling through the LVGL group instead of direct button handling
   * When group is deactivated (e.g., menu opens), scrolling is automatically disabled
   */
  void Activate(lv_group_t* group) override {
    ScreenBase::Activate(group);
    if (group && screen_) {
      lv_group_add_obj(group, screen_);
      lv_group_focus_obj(screen_);
    }
  }

  /**
   * @brief Deactivate screen input focus - removes content container from LVGL group
   */
  void Deactivate() override {
    if (screen_) {
      lv_group_remove_obj(screen_);
    }
    ScreenBase::Deactivate();
  }

  /**
   * @brief Handle button presses - scroll content when UP/DOWN pressed
   * This is called by the display controller when a button is pressed
   * Only receives input when the group is active (menu is closed)
   */
  bool OnButtonPress(ButtonId button_id) override {
    if (!screen_) {
      return false;
    }

    switch (button_id) {
      case ButtonId::UP: lv_obj_scroll_by_bounded(screen_, 0, 20, LV_ANIM_OFF); return true;
      case ButtonId::DOWN: lv_obj_scroll_by_bounded(screen_, 0, -20, LV_ANIM_OFF); return true;
      default: return false;
    }
  }

  /**
   * @brief Update BMS data display
   * Called periodically from display controller's Tick()
   */
  void Tick() override;

 private:
  SaboRobot* sabo_robot_ = nullptr;
  const xbot::driver::bms::Data* bms_data_ = nullptr;

  bool is_created_ = false;

  // BMS info labels
  lv_obj_t* mfr_val_label_ = nullptr;
  lv_obj_t* dev_val_label_ = nullptr;
  lv_obj_t* ver_val_label_ = nullptr;
  lv_obj_t* ser_val_label_ = nullptr;
  lv_obj_t* dat_val_label_ = nullptr;

  // BMS data labels
  lv_obj_t* design_v_val_label_ = nullptr;
  lv_obj_t* design_ah_val_label_ = nullptr;
  lv_obj_t* full_charge_ah_val_label_ = nullptr;
  lv_obj_t* actual_v_val_label_ = nullptr;
  lv_obj_t* actual_ah_val_label_ = nullptr;
  lv_obj_t* health_cycles_val_label_ = nullptr;
  lv_obj_t* health_pct_val_label_ = nullptr;
  lv_obj_t* soc_val_label_ = nullptr;

  // Cell voltages
  constexpr static size_t BATTERY_CELLS = 7;
  lv_obj_t* cell_voltage_labels_[BATTERY_CELLS] = {nullptr};
  lv_obj_t* cell_diff_labels_[BATTERY_CELLS] = {nullptr};
  WidgetIcon* cell_status_icons_[BATTERY_CELLS] = {nullptr};

  // State tracking for updates
  bool bms_info_printed_ = false;
  float last_cell_voltages_[BATTERY_CELLS] = {0};
  int last_cell_diffs_[BATTERY_CELLS] = {0};

  // BMS data state tracking
  float last_design_voltage_v_ = 0;
  float last_design_capacity_ah_ = 0;
  float last_full_charge_capacity_ah_ = 0;
  float last_pack_voltage_v_ = 0;
  float last_remaining_capacity_ah_ = 0;
  uint16_t last_cycle_count_ = 0;
  uint8_t last_health_pct_ = 0;
  float last_battery_soc_ = -1.0f;

  // Threshold constants for cell delta status
  static constexpr int DELTA_WARNING_THRESHOLD_MV = 50;  // 50mV warning threshold
  static constexpr int DELTA_ERROR_THRESHOLD_MV = 100;   // 100mV error threshold

  // Shared text buffer for formatting
  static constexpr size_t SHARED_TEXT_BUFFER_SIZE = 64;
  char shared_text_buffer_[SHARED_TEXT_BUFFER_SIZE];
  etl::string<64> tmp_string_;

  // Creates a grid cell using an existing object (obj) as the base
  void CreateLabelGridCell(lv_obj_t* obj, lv_obj_t* parent, const char* text, lv_grid_align_t column_align,
                           int32_t col_pos, int32_t col_span, lv_grid_align_t row_align, int32_t row_pos,
                           int32_t row_span);

  // Creates a new grid cell as a child of parent
  lv_obj_t* CreateLabelGridCell(lv_obj_t* parent, const char* text, lv_grid_align_t column_align, int32_t col_pos,
                                int32_t col_span, lv_grid_align_t row_align, int32_t row_pos, int32_t row_span);

  /**
   * @brief Create BMS information section like Manufacturer, Device, Version, ...
   * @param parent Parent container
   * @param y_pos Starting Y position
   * @return New Y position after section (for further content placement)
   */
  int CreateBmsInfoSection(lv_obj_t* parent, int y_pos);

  /**
   * @brief Create BMS data section like Voltages, Currents, SOC, ...
   * @param parent Parent container
   * @param y_pos Starting Y position
   * @return New Y position after section (for further content placement)
   */
  int CreateBmsDataSection(lv_obj_t* parent, int y_pos);

  /**
   * @brief Create BMS Cell section like Voltages, Deltas, Status
   * @param parent Parent container
   * @param y_pos Starting Y position
   * @return New Y position after section (for further content placement)
   */
  int CreateBmsCellSection(lv_obj_t* parent, int y_pos);

  /**
   * @brief Update BMS information display
   */
  void UpdateBmsInfo();

  /**
   * @brief Update BMS data display
   */
  void UpdateBmsData();

  /**
   * @brief Update cell voltages and visualization
   */
  void UpdateCellVoltages();
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_BATTERY_HPP_
