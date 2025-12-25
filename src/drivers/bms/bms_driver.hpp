/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file bms_driver.hpp
 * @brief Base class for Battery Management System drivers
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-12-18
 */

#ifndef BMS_DRIVER_HPP
#define BMS_DRIVER_HPP

#include <cstddef>
#include <cstdint>

namespace xbot::driver::bms {

static constexpr size_t kMaxCells = 8;

struct Data {
  // Mostly static data about the BMS and battery pack
  char mfr_name[33]{};
  uint16_t mfr_date_days_utc{};  // days since 1970-01-01 (UTC)
  char dev_name[33]{};
  char dev_chemistry[33]{};
  uint16_t serial_number{};
  float design_capacity_ah{};
  float design_voltage_v{};

  // Dynamic data updated periodically
  float pack_voltage_v{};
  float pack_current_a{};
  float temperature_c{};
  float battery_percentage{};  // 0..1
  float remaining_capacity_ah{};
  float full_charge_capacity_ah{};
  uint16_t cycle_count{};
  uint16_t battery_status{};

  // Cell voltages: only indices [0..cell_count-1] are valid.
  uint8_t cell_count{};
  float cell_voltage_v[kMaxCells]{};
};

/**
 * @brief Base class for BMS drivers
 */
class BmsDriver {
 public:
  explicit BmsDriver(uint8_t num_cells) : num_cells_(ClampCells(num_cells)) {
    data_.cell_count = num_cells_;
  }
  virtual ~BmsDriver() = default;

  virtual bool Init() = 0;
  virtual void Tick() = 0;

  virtual bool IsPresent() const {
    return present_;
  }

  // Pointer remains valid for the lifetime of the driver.
  virtual const Data* GetData() const {
    return &data_;
  }

  // Get JSON representation of extra data (which is mostly static or minor)
  virtual const char* GetExtraDataJson() const = 0;

 protected:
  static uint8_t ClampCells(uint8_t num_cells) {
    return (num_cells > (uint8_t)kMaxCells) ? (uint8_t)kMaxCells : num_cells;
  }

  Data data_{};

  void SetPresent(bool present) {
    present_ = present;
  }

 private:
  const uint8_t num_cells_;
  bool present_{false};
};

}  // namespace xbot::driver::bms

#endif  // BMS_DRIVER_HPP
