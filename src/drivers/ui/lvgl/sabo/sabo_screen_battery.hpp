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
 * @date 2025-12-29
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
   * @brief Activate screen for input focus - adds content container to LVGL group
   * This enables scrolling through the LVGL group instead of direct button handling
   * When group is deactivated (e.g., menu opens), scrolling is automatically disabled
   */
  void Activate(lv_group_t* group) override {
    ScreenBase::Activate(group);
    if (group && content_container_) {
      lv_group_add_obj(group, content_container_);
      lv_group_focus_obj(content_container_);
    }
  }

  /**
   * @brief Deactivate screen input focus - removes content container from LVGL group
   */
  void Deactivate() override {
    if (content_container_) {
      lv_group_remove_obj(content_container_);
    }
    ScreenBase::Deactivate();
  }

  /**
   * @brief Handle button presses - scroll content when UP/DOWN pressed
   * This is called by the display controller when a button is pressed
   * Only receives input when the group is active (menu is closed)
   */
  bool OnButtonPress(ButtonId button_id) override {
    if (!content_container_) {
      return false;
    }

    switch (button_id) {
      case ButtonId::UP: lv_obj_scroll_by_bounded(content_container_, 0, 20, LV_ANIM_OFF); return true;
      case ButtonId::DOWN: lv_obj_scroll_by_bounded(content_container_, 0, -20, LV_ANIM_OFF); return true;
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

  lv_obj_t* content_container_ = nullptr;
  bool is_created_ = false;

  // BMS info labels
  lv_obj_t* mfr_val_label_ = nullptr;
  lv_obj_t* dev_val_label_ = nullptr;
  lv_obj_t* ver_val_label_ = nullptr;
  lv_obj_t* ser_val_label_ = nullptr;
  lv_obj_t* dat_val_label_ = nullptr;
  lv_obj_t* design_v_val_label_ = nullptr;
  lv_obj_t* design_ah_val_label_ = nullptr;

  lv_obj_t* design_capacity_label_ = nullptr;
  lv_obj_t* design_voltage_label_ = nullptr;

  // Cell voltage labels (7 cells)
  lv_obj_t* cell_voltage_labels_[7] = {nullptr};
  lv_obj_t* cell_diff_labels_[6] = {nullptr};  // 6 differences between 7 cells

  // Battery visualization objects (7S3P representation)
  lv_obj_t* cell_visualization_[7] = {nullptr};  // 7 cells in series

  // Shared text buffer for formatting
  static constexpr size_t SHARED_TEXT_BUFFER_SIZE = 64;
  char shared_text_buffer_[SHARED_TEXT_BUFFER_SIZE];
  etl::string<64> tmp_string_;

  /**
   * @brief Create BMS information section
   * @param parent Parent container
   * @param y_pos Starting Y position (updated to position after this section)
   */
  void CreateBmsInfoSection(lv_obj_t* parent, int& y_pos);

  /**
   * @brief Create battery visualization section (7S3P)
   * @param parent Parent container
   * @param y_pos Starting Y position (updated to position after this section)
   */
  void CreateBatteryVisualization(lv_obj_t* parent, int& y_pos);

  /**
   * @brief Update BMS information display
   */
  void UpdateBmsInfo();

  /**
   * @brief Update cell voltages and visualization
   */
  void UpdateCellVoltages();
};

}  // namespace xbot::driver::ui::lvgl::sabo

#endif  // LVGL_SABO_SCREEN_BATTERY_HPP_
