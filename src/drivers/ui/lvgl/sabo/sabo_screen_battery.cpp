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
 * @date 2026-01-02
 */

#include "sabo_screen_battery.hpp"

#include <cmath>

extern "C" {
LV_FONT_DECLARE(font_awesome_14);
}

namespace xbot::driver::ui::lvgl::sabo {

using namespace xbot::driver::sabo::types;

SaboScreenBattery::SaboScreenBattery() : ScreenBase<ScreenId, ButtonId>(ScreenId::BATTERY) {
  if (robot != nullptr) {
    sabo_robot_ = static_cast<SaboRobot*>(robot);
    bms_info_ = &sabo_robot_->GetBmsInfo();
    bms_live_ = &sabo_robot_->GetBmsLive();
  }
}

SaboScreenBattery::~SaboScreenBattery() {
  // Clean up WidgetIcon objects
  for (size_t i = 0; i < BATTERY_CELLS; i++) {
    if (cell_status_icons_[i] != nullptr) {
      delete cell_status_icons_[i];
    }
  }
}

void SaboScreenBattery::Create(lv_color_t bg_color, lv_color_t fg_color) {
  if (is_created_) return;  // Prevent multiple calls to Create()

  ScreenBase::Create(bg_color, fg_color);
  lv_obj_set_style_text_font(screen_, &orbitron_12, LV_PART_MAIN);
  lv_obj_add_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(screen_, LV_DIR_VER);

  int y_pos = 0;

  y_pos = CreateBmsInfoSection(screen_, y_pos);
  y_pos += 5;  // Spacing

  y_pos = CreateBmsDataSection(screen_, y_pos);
  y_pos += 5;  // Spacing

  y_pos = CreateBmsCellSection(screen_, y_pos);

  is_created_ = true;
}

int SaboScreenBattery::CreateBmsInfoSection(lv_obj_t* parent, int y_pos) {
  // Title Battery Information
  title_label_ = lv_label_create(screen_);
  lv_label_set_text(title_label_, "Battery Information");
  lv_obj_set_style_text_font(title_label_, &orbitron_16b, LV_PART_MAIN);
  lv_obj_align(title_label_, LV_ALIGN_TOP_MID, 0, y_pos);
  y_pos += 19;

  // Separator line
  lv_obj_t* separator = lv_obj_create(screen_);
  lv_obj_set_size(separator, defs::LCD_WIDTH, 1);
  lv_obj_set_pos(separator, 0, y_pos);
  lv_obj_set_style_border_width(separator, 1, LV_PART_MAIN);
  y_pos += 3;

  const int section_height = (15 * 3) + 2;  // 3 rows of 15px height + 2px padding

  // Create a grid container for BMS information
  lv_obj_t* grid = lv_obj_create(parent);
  lv_obj_set_size(grid, defs::LCD_WIDTH, section_height);
  lv_obj_set_pos(grid, 0, y_pos);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);
  lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(grid, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(grid, 0, LV_PART_MAIN);

  // Ensure grid is not scrollable - only parent container should scroll
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

  // Grid column descriptors: 4 columns - Label1, Value1, Label2, Value2
  static int32_t col_dsc[] = {LV_GRID_CONTENT,  // Label column (Mfr., Ver., Date)
                              LV_GRID_FR(1),    // Value column (auto-expand)
                              LV_GRID_CONTENT,  // Label column (Dev., S/N)
                              LV_GRID_FR(1),    // Value column (auto-expand)
                              LV_GRID_TEMPLATE_LAST};

  // Grid row descriptors: 3 rows, each 15px height
  static int32_t row_dsc[] = {15,  // Row 0: Mfr. and Dev.
                              15,  // Row 1: Ver. and S/N
                              15,  // Row 2: Date (centered across all columns)
                              LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
  y_pos += section_height;

  // Row 0: Manufacturer Name and Device
  CreateLabelGridCell(grid, "Mfr.:", LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  mfr_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  // Device
  CreateLabelGridCell(grid, "Dev.:", LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  dev_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_START, 3, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // Row 1: Version and Serial Number
  CreateLabelGridCell(grid, "Ver.:", LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  ver_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  CreateLabelGridCell(grid, "S/N:", LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  ser_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_START, 3, 1, LV_GRID_ALIGN_CENTER, 1, 1);

  // Row 2: Manufacturer Date (centered across all 4 columns)
  CreateLabelGridCell(grid, "Date:", LV_GRID_ALIGN_END, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
  dat_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, 2, 1);

  return y_pos;
}

int SaboScreenBattery::CreateBmsDataSection(lv_obj_t* parent, int y_pos) {
  // Title Battery Data
  lv_obj_t* title = lv_label_create(screen_);
  lv_label_set_text(title, "Battery Data");
  lv_obj_set_style_text_font(title, &orbitron_16b, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y_pos);
  y_pos += 19;

  // Separator line
  lv_obj_t* separator = lv_obj_create(screen_);
  lv_obj_set_size(separator, defs::LCD_WIDTH, 1);
  lv_obj_set_pos(separator, 0, y_pos);
  lv_obj_set_style_border_width(separator, 1, LV_PART_MAIN);
  y_pos += 3;

  const int section_height = (15 * 5) + 2;  // 5 rows of 15px height + 2px padding

  // Create a grid container for BMS information
  lv_obj_t* grid = lv_obj_create(parent);
  lv_obj_set_size(grid, defs::LCD_WIDTH, section_height);
  lv_obj_set_pos(grid, 0, y_pos);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);
  lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(grid, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(grid, 0, LV_PART_MAIN);

  // Ensure grid is not scrollable - only parent container should scroll
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

  // Grid column descriptors: 4 columns - Label1, Value1, Label2, Value2
  static int32_t col_dsc[] = {LV_GRID_CONTENT,  // Label column (Design, Full charge, Actual)
                              LV_GRID_FR(1),    // Voltage column (auto-expand)
                              LV_GRID_FR(1),    // VCurrent column (auto-expand)
                              LV_GRID_TEMPLATE_LAST};

  // Grid row descriptors: 5 rows, each 15px height
  static int32_t row_dsc[] = {15,  // Row 0: Design
                              15,  // Row 1: Full charge
                              15,  // Row 2: Actual
                              15,  // Row 3: Health
                              15,  // Row 4: SoC
                              LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
  y_pos += section_height;

  // Row 0: Design
  CreateLabelGridCell(grid, "Design:", LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  design_v_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  design_ah_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // Row 1: Full charge
  CreateLabelGridCell(grid, "Full charge:", LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  full_charge_ah_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 1, 1);

  // Row 2: Actual
  CreateLabelGridCell(grid, "Actual:", LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
  actual_v_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);
  actual_ah_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 2, 1);

  // Row 3: Health
  CreateLabelGridCell(grid, "Health:", LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);
  health_cycles_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);
  health_pct_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 3, 1);

  // Row 4: SoC
  CreateLabelGridCell(grid, "SoC:", LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
  soc_val_label_ = CreateLabelGridCell(grid, "N/A", LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 4, 1);

  return y_pos;
}

int SaboScreenBattery::CreateBmsCellSection(lv_obj_t* parent, int y_pos) {
  // Title Cell Data
  lv_obj_t* title = lv_label_create(screen_);
  lv_label_set_text(title, "Cell Data");
  lv_obj_set_style_text_font(title, &orbitron_16b, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y_pos);
  y_pos += 19;

  // Separator line
  lv_obj_t* separator = lv_obj_create(screen_);
  lv_obj_set_size(separator, defs::LCD_WIDTH, 1);
  lv_obj_set_pos(separator, 0, y_pos);
  lv_obj_set_style_border_width(separator, 1, LV_PART_MAIN);
  y_pos += 3;

  const int grid_rows = BATTERY_CELLS + 1;  // Header row + data rows
  const int row_height = 15;
  const int section_height = (row_height * grid_rows) + 2;  // +2px padding

  // Create a grid container for cell data
  lv_obj_t* grid = lv_obj_create(parent);
  lv_obj_set_size(grid, defs::LCD_WIDTH, section_height);
  lv_obj_set_pos(grid, 0, y_pos);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);
  lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(grid, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(grid, 0, LV_PART_MAIN);

  // Ensure grid is not scrollable - only parent container should scroll
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

  // Grid column descriptors: 4 columns - Cell number, Voltage, Delta, Status
  static int32_t col_dsc[] = {LV_GRID_FR(1),  // Cell number
                              LV_GRID_FR(1),  // Voltage
                              LV_GRID_FR(1),  // Delta
                              LV_GRID_FR(1),  // Status (Icon)
                              LV_GRID_TEMPLATE_LAST};

  // Grid row descriptors: static array with maximum possible rows (8 cells + header = 9 rows)
  static int32_t row_dsc[] = {row_height,  // Row 0: Header
                              row_height,  // Row 1: Cell 1
                              row_height,  // Row 2: Cell 2
                              row_height,  // Row 3: Cell 3
                              row_height,  // Row 4: Cell 4
                              row_height,  // Row 5: Cell 5
                              row_height,  // Row 6: Cell 6
                              row_height,  // Row 7: Cell 7
                              LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
  y_pos += section_height;

  // Create header row
  CreateLabelGridCell(grid, "Cell", LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  CreateLabelGridCell(grid, "Voltage", LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  CreateLabelGridCell(grid, "Delta", LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  CreateLabelGridCell(grid, "Status", LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // Init data rows and cells
  for (size_t i = 0; i < BATTERY_CELLS; i++) {
    last_cell_diffs_[i] = kUnknownDelta;
    int grid_row = i + 1;  // +1 because row 0 is header

    // Cell number (static, no need to store)
    chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "%d", i + 1);
    CreateLabelGridCell(grid, shared_text_buffer_, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, grid_row, 1);

    // Voltage label (store in array for later updates)
    cell_voltage_labels_[i] =
        CreateLabelGridCell(grid, "", LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, grid_row, 1);

    // Delta label (store in array for later updates)
    cell_diff_labels_[i] = CreateLabelGridCell(grid, "", LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, grid_row, 1);

    // Status label (icon) - create with FontAwesome font
    WidgetIcon* icon = new WidgetIcon(WidgetIcon::Icon::UNDEF, grid, LV_ALIGN_DEFAULT, 0, 0, WidgetIcon::State::ON);
    lv_obj_set_grid_cell(icon->GetLvObj(), LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_CENTER, grid_row, 1);
    cell_status_icons_[i] = icon;
  }

  return y_pos;
}

void SaboScreenBattery::Tick() {
  UpdateStaleIndicator();
  UpdateBmsInfo();
  UpdateBmsData();
  UpdateCellVoltages();
}

void SaboScreenBattery::UpdateStaleIndicator() {
  if (!is_created_ || title_label_ == nullptr || sabo_robot_ == nullptr) return;

  const bool stale = sabo_robot_->IsBmsDataStale();
  if (!stale) {
    if (stale_shown_) {
      lv_label_set_text(title_label_, "Battery Information");
      stale_shown_ = false;
    }
    return;
  }

  // The values below are the last ones the pack reported - say so instead of pretending
  // they are live.
  const uint32_t age = sabo_robot_->GetBmsAgeMs();
  if (age == UINT32_MAX) {
    lv_label_set_text(title_label_, "NO LIVE DATA");
  } else {
    lv_label_set_text_fmt(title_label_, "STALE DATA %us", (unsigned)(age / 1000U));
  }
  stale_shown_ = true;
}

void SaboScreenBattery::UpdateBmsInfo() {
  if (!is_created_ || bms_info_ == nullptr) return;
  const uint32_t generation = bms_info_->generation;
  if (generation == 0U || generation == displayed_generation_) return;
  displayed_generation_ = generation;

  lv_label_set_text(mfr_val_label_, bms_info_->mfr_name[0] ? bms_info_->mfr_name : "N/A");
  lv_label_set_text(dev_val_label_, bms_info_->dev_name[0] ? bms_info_->dev_name : "N/A");
  lv_label_set_text(ver_val_label_, bms_info_->dev_version[0] ? bms_info_->dev_version : "N/A");
  lv_label_set_text(dat_val_label_, bms_info_->mfr_date[0] ? bms_info_->mfr_date : "N/A");
  if (bms_info_->serial_number != 0) {
    lv_label_set_text_fmt(ser_val_label_, "%u", bms_info_->serial_number);
  } else {
    lv_label_set_text(ser_val_label_, "N/A");
  }
  if (bms_info_->design_voltage_v > 0.0f) {
    chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "%.1fV", bms_info_->design_voltage_v);
    lv_label_set_text(design_v_val_label_, shared_text_buffer_);
  } else {
    lv_label_set_text(design_v_val_label_, "N/A");
  }
  if (bms_info_->design_capacity_ah > 0.0f) {
    chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "%.2fAh", bms_info_->design_capacity_ah);
    lv_label_set_text(design_ah_val_label_, shared_text_buffer_);
  } else {
    lv_label_set_text(design_ah_val_label_, "N/A");
  }
}

// All quantities shown here are non-negative. -1 represents a missing reading.
void SaboScreenBattery::UpdateReading(lv_obj_t* label, float value, float& previous, const char* format) {
  if (value == previous) return;
  previous = value;
  if (value < 0.0f) {
    lv_label_set_text(label, "N/A");
    return;
  }
  chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, format, value);
  lv_label_set_text(label, shared_text_buffer_);
}

void SaboScreenBattery::UpdateBmsData() {
  if (bms_info_ == nullptr || !is_created_) return;

  UpdateReading(full_charge_ah_val_label_, bms_live_->full_charge_capacity_ah, last_full_charge_capacity_ah_, "%.2fAh");
  UpdateReading(actual_ah_val_label_, bms_live_->remaining_capacity_ah, last_remaining_capacity_ah_, "%.2fAh");
  const float voltage = bms_live_->pack_voltage_v;
  UpdateReading(actual_v_val_label_, voltage > 0.0f ? voltage : -1.0f, last_pack_voltage_v_, "%.1fV");

  if (bms_live_->cycle_count != last_cycle_count_) {
    last_cycle_count_ = bms_live_->cycle_count;
    if (last_cycle_count_ >= 0) {
      lv_label_set_text_fmt(health_cycles_val_label_, "%u cycles", static_cast<unsigned>(last_cycle_count_));
    } else {
      lv_label_set_text(health_cycles_val_label_, "N/A");
    }
  }

  /* A pack that reports 0 Ah full-charge capacity has 0 % health, while a missing design
     capacity means the health is unknown - say that instead of leaving a stale percentage
     on the screen. */
  const bool health_known = bms_info_->design_capacity_ah > 0.0f && bms_live_->full_charge_capacity_ah >= 0.0f;
  const uint8_t health_pct = health_known ? static_cast<uint8_t>(std::round(100.0f / bms_info_->design_capacity_ah *
                                                                            bms_live_->full_charge_capacity_ah))
                                          : kHealthUnknown;
  if (health_pct != last_health_pct_) {
    if (health_known) {
      lv_label_set_text_fmt(health_pct_val_label_, "%u%%", health_pct);
    } else {
      lv_label_set_text(health_pct_val_label_, "N/A");
    }
    last_health_pct_ = health_pct;
  }

  if (bms_live_->battery_soc != last_battery_soc_) {
    last_battery_soc_ = bms_live_->battery_soc;
    if (last_battery_soc_ >= 0.0f) {
      lv_label_set_text_fmt(soc_val_label_, "%u%%", static_cast<unsigned>(last_battery_soc_ * 100));
    } else {
      lv_label_set_text(soc_val_label_, "N/A");
    }
  }
}

void SaboScreenBattery::UpdateCellVoltages() {
  if (!is_created_ || bms_info_ == nullptr || bms_info_->cell_count == 0) {
    return;
  }

  // Update cell voltages
  for (size_t i = 0; i < BATTERY_CELLS; i++) {
    float voltage = std::round(bms_live_->cell_voltage_v[i] * 1000.0f) / 1000.0f;  // Round to 3 decimal places
    if (voltage != last_cell_voltages_[i]) {
      if (voltage > 0.0f) {
        chsnprintf(shared_text_buffer_, SHARED_TEXT_BUFFER_SIZE, "%.3fV", voltage);
        lv_label_set_text(cell_voltage_labels_[i], shared_text_buffer_);
      } else {
        lv_label_set_text(cell_voltage_labels_[i], "N/A");
      }
      last_cell_voltages_[i] = voltage;
    }
  }

  // Update cell differences (delta to previous cell) and status icons
  for (size_t i = 1; i < BATTERY_CELLS; i++) {
    if (last_cell_voltages_[i] == 0.0f || last_cell_voltages_[i - 1] == 0.0f) {
      if (last_cell_diffs_[i] != kUnknownDelta) {
        lv_label_set_text(cell_diff_labels_[i], "N/A");
        if (cell_status_icons_[i]) cell_status_icons_[i]->SetIcon(WidgetIcon::Icon::UNDEF);
        last_cell_diffs_[i] = kUnknownDelta;
      }
      continue;
    }
    int diff = static_cast<int>(std::round((last_cell_voltages_[i] - last_cell_voltages_[i - 1]) * 1000.0f));
    if (diff != last_cell_diffs_[i]) {
      lv_label_set_text_fmt(cell_diff_labels_[i], "%dmV", diff);
      last_cell_diffs_[i] = diff;

      // Update status icon based on delta using class constants
      if (!cell_status_icons_[i]) {
        continue;
      }
      int abs_diff = std::abs(diff);
      if (abs_diff < DELTA_WARNING_THRESHOLD_MV) {
        cell_status_icons_[i]->SetIcon(WidgetIcon::Icon::CHECK);
      } else if (abs_diff < DELTA_ERROR_THRESHOLD_MV) {
        cell_status_icons_[i]->SetIcon(WidgetIcon::Icon::EMERGENCY_GENERIC);  // Warning triangle
      } else {
        cell_status_icons_[i]->SetIcon(WidgetIcon::Icon::SQUARE_XMARK);
      }
    }
  }
}

lv_obj_t* SaboScreenBattery::CreateLabelGridCell(lv_obj_t* parent, const char* text, lv_grid_align_t column_align,
                                                 int32_t col_pos, int32_t col_span, lv_grid_align_t row_align,
                                                 int32_t row_pos, int32_t row_span) {
  if (parent == nullptr) return nullptr;

  lv_obj_t* obj = lv_label_create(parent);
  lv_label_set_text(obj, text);
  lv_obj_set_grid_cell(obj, column_align, col_pos, col_span, row_align, row_pos, row_span);
  return obj;
}

}  // namespace xbot::driver::ui::lvgl::sabo
