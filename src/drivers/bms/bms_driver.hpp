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

#include <ch.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "sbs_protocol.hpp"

namespace xbot::driver::bms {

static constexpr size_t kMaxCells = 8;

/**
 * @brief Data that describes the pack and never changes while it is connected.
 *
 * Read once when the pack is detected (see SaboBmsDriver::Tick), so a consumer can hand it
 * out again at any time - e.g. as the answer of the BMS service's GetDeviceInfo RPC.
 */
struct DeviceInfo {
  uint32_t generation{};  ///< Incremented after each successful detection.
  char mfr_name[33]{};
  // SBS ManufacturerDate (0x1B) formatted as YYYY-MM-DD (UTC).
  // Empty string means "unknown".
  char mfr_date[11]{};
  char dev_name[33]{};
  char dev_version[33]{};
  char dev_chemistry[33]{};
  uint16_t serial_number{};
  float design_capacity_ah{};
  float design_voltage_v{};
  uint8_t cell_count{};
};

/**
 * @brief Data that the pack reports periodically.
 *
 * Everything here may change from tick to tick, and stays at its last value when a read
 * fails. AgeMs() measures the age of the last successful voltage sample.
 */
struct LiveData {
  // Signed readings start at NaN. Non-negative quantities use -1 until sampled.
  float pack_voltage_v{-1.0f};
  float pack_current_a{NAN};
  float temperature_c{NAN};

  // The (relative) battery_soc is used to estimate the amount of charge remaining in the battery.
  // The problem with this paradigm is that the tank size is variable.
  // As standardized battery packs come into service, physical size will have less to do with the actual capacity.
  float battery_soc{-1.0f};  // 0..1

  // remaining_capacity_ah returns the battery's remaining capacity in absolute terms but relative to
  // a specific discharge rate. This information is a numeric indication of remaining charge which can also be
  // represented by the battery_soc and may be in a better form for use by power management systems.
  // battery_soc return values in percentage format which is a relative representation while
  // remaining_capacity_ah returns a more absolute value defined at a specific discharge value.
  float remaining_capacity_ah{-1.0f};

  // The FullChargeCapacity() function provides the user with a means of understanding the "tank size" of their battery.
  // This information, along with information about the original capacity of the battery, can be presented to the user
  // as an indication of battery wear
  float full_charge_capacity_ah{-1.0f};

  int32_t cycle_count{-1};  ///< All uint16_t cycle counts, including zero, are valid.
  bool has_battery_status{};
  uint16_t battery_status{0};  // SBS/SMBus BatteryStatus word, but without error bits.
  // The BatteryStatus register (0x16) exactly as the pack reported it: the low nibble is
  // the SBS error code, the vendor bits above it are what battery_status is derived from.
  uint16_t battery_status_raw{0};

  // Cell indices are fixed on the wire. Zero means this pack has not supplied that cell yet.
  float cell_voltage_v[kMaxCells]{};

  // System time of the last tick that read the pack voltage: the age a consumer uses to
  // decide whether the values above are still current.
  systime_t last_ok_tick{};
};

/**
 * @brief Base class for BMS drivers
 */
class BmsDriver {
 public:
  explicit BmsDriver(uint8_t num_cells) : num_cells_(ClampCells(num_cells)) {
    info_.cell_count = num_cells_;
  }
  virtual ~BmsDriver() = default;

  virtual bool Init() = 0;
  virtual void Tick() = 0;

  virtual bool IsPresent() const {
    return present_;
  }

  /// True once a pack was detected. A board without one never shows stale data.
  bool WasDetected() const {
    return ever_seen_;
  }

  // Both stay valid for the lifetime of the driver.
  virtual const DeviceInfo& GetDeviceInfo() const {
    return info_;
  }

  virtual const LiveData& GetLiveData() const {
    return live_;
  }

  // Age of the live values in ms, i.e. how long ago the pack was last read successfully.
  uint32_t AgeMs() const {
    if (live_.pack_voltage_v < 0.0f) return UINT32_MAX;
    return TIME_I2MS(chVTTimeElapsedSinceX(live_.last_ok_tick));
  }

  // Get JSON representation of the device info (which is static for one pack).
  // Writes into the buffer the caller provides, so no driver keeps a static JSON buffer.
  // Capacity includes the terminator. Returns the payload length, 0 if no pack is present,
  // or -1 if the JSON could not be serialized in full.
  virtual int BuildDeviceInfo(char* out, size_t capacity) const = 0;

 protected:
  static uint8_t ClampCells(uint8_t num_cells) {
    return (num_cells > (uint8_t)kMaxCells) ? (uint8_t)kMaxCells : num_cells;
  }

  DeviceInfo info_{};
  LiveData live_{};

  void SetPresent(bool present) {
    present_ = present;
    if (present) {
      ever_seen_ = true;
    }
  }

 private:
  const uint8_t num_cells_;
  bool present_{false};
  bool ever_seen_{false};
};

}  // namespace xbot::driver::bms

#endif  // BMS_DRIVER_HPP
