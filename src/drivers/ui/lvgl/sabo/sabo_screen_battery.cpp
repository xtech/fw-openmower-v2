/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_screen_battery.cpp
 * @brief Battery Screen with BMS information and cell visualization
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-12-29
 */

#include "sabo_screen_battery.hpp"

#include <cmath>

namespace xbot::driver::ui::lvgl::sabo {

using namespace xbot::driver::sabo::types;

SaboScreenBattery::SaboScreenBattery() : ScreenBase<ScreenId, ButtonId>(ScreenId::BATTERY) {
  if (robot != nullptr) {
    sabo_robot_ = static_cast<SaboRobot*>(robot);
    bms_data_ = sabo_robot_->GetBmsData();
  }
}

SaboScreenBattery::~SaboScreenBattery() {
  // Destructor implementation
}

void SaboScreenBattery::Create(lv_color_t bg_color) {
  ScreenBase::Create(bg_color);

  // Title (stays at top, NOT scrollable)
  lv_obj_t* title = lv_label_create(screen_);
  lv_label_set_text(title, "Battery Information");
  lv_obj_set_style_text_font(title, &orbitron_16b, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

  // Separator line (stays at top, NOT scrollable)
  lv_obj_t* separator = lv_obj_create(screen_);
  lv_obj_set_size(separator, defs::LCD_WIDTH, 1);
  lv_obj_set_pos(separator, 0, 19);
  lv_obj_set_style_border_width(separator, 1, LV_PART_MAIN);

  // Scrollable content container
  content_container_ = lv_obj_create(screen_);
  lv_obj_set_size(content_container_, defs::LCD_WIDTH, defs::LCD_HEIGHT - 20);
  lv_obj_set_pos(content_container_, 0, 20);
  lv_obj_add_flag(content_container_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(content_container_, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(content_container_, LV_DIR_VER);
  lv_obj_set_style_border_width(content_container_, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(content_container_, 0, LV_PART_MAIN);

  int y_pos = 2;

  // Create BMS information section
  CreateBmsInfoSection(content_container_, y_pos);

  y_pos += 10;  // Spacing

  // Create battery visualization section
  CreateBatteryVisualization(content_container_, y_pos);

  is_created_ = true;
}

void SaboScreenBattery::CreateBmsInfoSection(lv_obj_t* parent, int& y_pos) {
  int32_t xv1 = 35;
  int32_t xv2 = (defs::LCD_WIDTH / 2) + 35;

  // Manufacturer Name
  lv_obj_t* mfr_name_label = lv_label_create(parent);
  lv_label_set_text(mfr_name_label, "Mfr.:");
  lv_obj_set_style_text_font(mfr_name_label, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(mfr_name_label, 0, y_pos);

  mfr_val_label_ = lv_label_create(parent);
  lv_label_set_text(mfr_val_label_, "N/A");
  lv_obj_set_style_text_font(mfr_val_label_, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(mfr_val_label_, xv1, y_pos);

  // Device
  lv_obj_t* dev_name_label = lv_label_create(parent);
  lv_label_set_text(dev_name_label, "Dev.:");
  lv_obj_set_style_text_font(dev_name_label, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(dev_name_label, defs::LCD_WIDTH / 2, y_pos);

  dev_val_label_ = lv_label_create(parent);
  lv_label_set_text(dev_val_label_, "N/A");
  lv_obj_set_style_text_font(dev_val_label_, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(dev_val_label_, xv2, y_pos);

  y_pos += 14;

  // Version
  lv_obj_t* ver_name_label = lv_label_create(parent);
  lv_label_set_text(ver_name_label, "Ver.:");
  lv_obj_set_style_text_font(ver_name_label, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(ver_name_label, 0, y_pos);

  ver_val_label_ = lv_label_create(parent);
  lv_label_set_text(ver_val_label_, "N/A");
  lv_obj_set_style_text_font(ver_val_label_, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(ver_val_label_, xv1, y_pos);

  // S/N
  lv_obj_t* sn_name_label = lv_label_create(parent);
  lv_label_set_text(sn_name_label, "S/N:");
  lv_obj_set_style_text_font(sn_name_label, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(sn_name_label, defs::LCD_WIDTH / 2, y_pos);

  ser_val_label_ = lv_label_create(parent);
  lv_label_set_text(ser_val_label_, "N/A");
  lv_obj_set_style_text_font(ser_val_label_, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(ser_val_label_, xv2, y_pos);

  y_pos += 14;

  // Manufacturer Date
  lv_obj_t* date_name_label = lv_label_create(parent);
  lv_label_set_text(date_name_label, "Date:");
  lv_obj_set_style_text_color(date_name_label, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_text_font(date_name_label, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(date_name_label, 0, y_pos);

  dat_val_label_ = lv_label_create(parent);
  lv_label_set_text(dat_val_label_, "N/A");
  lv_obj_set_style_text_color(dat_val_label_, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_text_font(dat_val_label_, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(dat_val_label_, xv1, y_pos);

  y_pos += 14;

  // Design voltage
  lv_obj_t* design_v_name_label = lv_label_create(parent);
  lv_label_set_text(design_v_name_label, "Volt:");
  lv_obj_set_style_text_font(design_v_name_label, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(design_v_name_label, 0, y_pos);

  design_v_val_label_ = lv_label_create(parent);
  lv_label_set_text(design_v_val_label_, "N/A");
  lv_obj_set_style_text_font(design_v_val_label_, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(design_v_val_label_, xv1, y_pos);

  // Design capacity
  lv_obj_t* design_ah_name_label = lv_label_create(parent);
  lv_label_set_text(design_ah_name_label, "Cap.:");
  lv_obj_set_style_text_font(design_ah_name_label, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(design_ah_name_label, defs::LCD_WIDTH / 2, y_pos);

  design_ah_val_label_ = lv_label_create(parent);
  lv_label_set_text(design_ah_val_label_, "N/A");
  lv_obj_set_style_text_font(design_ah_val_label_, &orbitron_12, LV_PART_MAIN);
  lv_obj_set_pos(design_ah_val_label_, xv2, y_pos);

  y_pos += 14;
}

void SaboScreenBattery::CreateBatteryVisualization(lv_obj_t* parent, int& y_pos) {
  // Battery Visualization Header
  lv_obj_t* header = lv_label_create(parent);
  lv_label_set_text(header, "7S3P Cell Voltages:");
  lv_obj_set_style_text_font(header, &orbitron_16b, LV_PART_MAIN);
  lv_obj_set_style_text_color(header, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_pos(header, 5, y_pos);
  y_pos += 18;

  // Cell voltage labels above visualization
  int cell_x = 10;
  const int cell_width = 30;
  const int cell_spacing = 5;

  for (int i = 0; i < 7; i++) {
    cell_voltage_labels_[i] = lv_label_create(parent);
    lv_label_set_text_fmt(cell_voltage_labels_[i], "C%d", i + 1);
    lv_obj_set_style_text_color(cell_voltage_labels_[i], lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(cell_voltage_labels_[i], &orbitron_12, LV_PART_MAIN);
    lv_obj_set_pos(cell_voltage_labels_[i], cell_x + (cell_width + cell_spacing) * i, y_pos);
  }
  y_pos += 15;

  // Battery visualization (7 cells in series, each representing 3 parallel cells)
  const int cell_height = 40;
  for (int i = 0; i < 7; i++) {
    cell_visualization_[i] = lv_obj_create(parent);
    lv_obj_set_size(cell_visualization_[i], cell_width, cell_height);
    lv_obj_set_pos(cell_visualization_[i], cell_x + (cell_width + cell_spacing) * i, y_pos);

    // Default styling (will be updated based on voltage)
    lv_obj_set_style_bg_color(cell_visualization_[i], lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_border_color(cell_visualization_[i], lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(cell_visualization_[i], 1, LV_PART_MAIN);
    lv_obj_set_style_radius(cell_visualization_[i], 2, LV_PART_MAIN);

    // Add cell number label inside
    lv_obj_t* cell_num = lv_label_create(cell_visualization_[i]);
    lv_label_set_text_fmt(cell_num, "%d", i + 1);
    lv_obj_set_style_text_color(cell_num, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(cell_num, &orbitron_12, LV_PART_MAIN);
    lv_obj_center(cell_num);
  }
  y_pos += cell_height + 10;

  // Cell difference labels below visualization
  for (int i = 0; i < 6; i++) {
    cell_diff_labels_[i] = lv_label_create(parent);
    lv_label_set_text_fmt(cell_diff_labels_[i], "Δ%d-%d", i + 1, i + 2);
    lv_obj_set_style_text_color(cell_diff_labels_[i], lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(cell_diff_labels_[i], &orbitron_12, LV_PART_MAIN);
    lv_obj_set_pos(cell_diff_labels_[i], cell_x + (cell_width + cell_spacing) * i + cell_width / 2 - 10, y_pos);
  }
  y_pos += 20;
}

void SaboScreenBattery::Tick() {
  UpdateBmsInfo();
  UpdateCellVoltages();
}

void SaboScreenBattery::UpdateBmsInfo() {
  static bool done_static_data = false;
  if (bms_data_ == nullptr || !is_created_) {
    return;
  }

  // BMS has a couple of static data which will not change during runtime
  if (!done_static_data) {
    if (bms_data_->mfr_name[0] != '\0') {
      lv_label_set_text_fmt(mfr_val_label_, "%s", bms_data_->mfr_name);
    }

    if (bms_data_->dev_name[0] != '\0') {
      lv_label_set_text_fmt(dev_val_label_, "%s", bms_data_->dev_name);
    }

    if (bms_data_->dev_version[0] != '\0') {
      lv_label_set_text_fmt(ver_val_label_, "%s", bms_data_->dev_version);
    }

    if (bms_data_->serial_number != 0) {
      lv_label_set_text_fmt(ser_val_label_, "%u", bms_data_->serial_number);
    }

    if (bms_data_->mfr_date[0] != '\0') {
      lv_label_set_text_fmt(dat_val_label_, "%s", bms_data_->mfr_date);
    }

    if (bms_data_->design_voltage_v > 0.0f) {
      chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "%.1f V", bms_data_->design_voltage_v);
      lv_label_set_text(design_v_val_label_, shared_text_buffer_);
    }

    if (bms_data_->design_capacity_ah > 0.0f) {
      chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "%.2f Ah", bms_data_->design_capacity_ah);
      lv_label_set_text(design_ah_val_label_, shared_text_buffer_);
    }

    done_static_data = true;
  }
}

void SaboScreenBattery::UpdateCellVoltages() {
  if (bms_data_ == nullptr || bms_data_->cell_count == 0) {
    // Show N/A for all cells
    for (int i = 0; i < 7; i++) {
      if (cell_voltage_labels_[i]) {
        lv_label_set_text_fmt(cell_voltage_labels_[i], "C%d: N/A", i + 1);
      }
      if (i < 6 && cell_diff_labels_[i]) {
        lv_label_set_text(cell_diff_labels_[i], "N/A");
      }
      if (cell_visualization_[i]) {
        lv_obj_set_style_bg_color(cell_visualization_[i], lv_color_hex(0xCCCCCC), LV_PART_MAIN);
      }
    }
    return;
  }

  // Update cell voltage labels and visualization
  float min_voltage = 5.0f;  // Start high
  float max_voltage = 0.0f;  // Start low
  int valid_cells = 0;

  // First pass: find min and max voltages
  for (uint8_t i = 0; i < bms_data_->cell_count && i < 7; i++) {
    if (bms_data_->cell_voltage_v[i] > 0.0f) {
      min_voltage = (bms_data_->cell_voltage_v[i] < min_voltage) ? bms_data_->cell_voltage_v[i] : min_voltage;
      max_voltage = (bms_data_->cell_voltage_v[i] > max_voltage) ? bms_data_->cell_voltage_v[i] : max_voltage;
      valid_cells++;
    }
  }

  // If no valid cells, show N/A
  if (valid_cells == 0) {
    for (int i = 0; i < 7; i++) {
      if (cell_voltage_labels_[i]) {
        lv_label_set_text_fmt(cell_voltage_labels_[i], "C%d: N/A", i + 1);
      }
    }
    return;
  }

  const float voltage_range = max_voltage - min_voltage;

  // Second pass: update labels and visualization
  for (uint8_t i = 0; i < bms_data_->cell_count && i < 7; i++) {
    if (cell_voltage_labels_[i]) {
      if (bms_data_->cell_voltage_v[i] > 0.0f) {
        chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "C%d: %.3fV", i + 1,
                   (double)bms_data_->cell_voltage_v[i]);
        lv_label_set_text(cell_voltage_labels_[i], shared_text_buffer_);
      } else {
        lv_label_set_text_fmt(cell_voltage_labels_[i], "C%d: N/A", i + 1);
      }
    }

    // Update visualization color based on voltage
    if (cell_visualization_[i] && bms_data_->cell_voltage_v[i] > 0.0f) {
      // Color gradient from green (balanced) to red (imbalanced)
      // Normalize voltage to 0-1 range within min-max
      float normalized = 0.5f;       // Default middle (green)
      if (voltage_range > 0.001f) {  // Avoid division by zero
        normalized = (bms_data_->cell_voltage_v[i] - min_voltage) / voltage_range;
      }

      // Create color gradient: green (0.0) -> yellow (0.5) -> red (1.0)
      lv_color_t color;
      if (normalized < 0.5f) {
        // Green to yellow
        uint8_t r = static_cast<uint8_t>(255 * (normalized * 2.0f));
        color = lv_color_make(r, 255, 0);
      } else {
        // Yellow to red
        uint8_t g = static_cast<uint8_t>(255 * ((1.0f - normalized) * 2.0f));
        color = lv_color_make(255, g, 0);
      }

      lv_obj_set_style_bg_color(cell_visualization_[i], color, LV_PART_MAIN);
    } else if (cell_visualization_[i]) {
      // No voltage data - gray
      lv_obj_set_style_bg_color(cell_visualization_[i], lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    }
  }

  // Update cell difference labels
  for (int i = 0; i < 6 && i < static_cast<int>(bms_data_->cell_count) - 1; i++) {
    if (cell_diff_labels_[i] && bms_data_->cell_voltage_v[i] > 0.0f && bms_data_->cell_voltage_v[i + 1] > 0.0f) {
      float diff = bms_data_->cell_voltage_v[i] - bms_data_->cell_voltage_v[i + 1];
      chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "%.1fmV", (double)(diff * 1000.0f));
      lv_label_set_text(cell_diff_labels_[i], shared_text_buffer_);

      // Color code differences: green for small diff, red for large diff
      if (fabs(diff) < 0.005f) {  // < 5mV difference
        lv_obj_set_style_text_color(cell_diff_labels_[i], lv_color_hex(0x00AA00), LV_PART_MAIN);
      } else if (fabs(diff) < 0.01f) {  // < 10mV difference
        lv_obj_set_style_text_color(cell_diff_labels_[i], lv_color_hex(0xFFAA00), LV_PART_MAIN);
      } else {  // >= 10mV difference
        lv_obj_set_style_text_color(cell_diff_labels_[i], lv_color_hex(0xFF0000), LV_PART_MAIN);
      }
    } else if (cell_diff_labels_[i]) {
      lv_label_set_text(cell_diff_labels_[i], "N/A");
      lv_obj_set_style_text_color(cell_diff_labels_[i], lv_color_black(), LV_PART_MAIN);
    }
  }
}
}  // namespace xbot::driver::ui::lvgl::sabo
