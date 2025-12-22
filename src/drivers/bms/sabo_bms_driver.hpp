/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_bms_driver.hpp
 * @brief Sabo BMS driver header
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-12-20
 */

#ifndef SABO_BMS_DRIVER_HPP
#define SABO_BMS_DRIVER_HPP

#include "bms_driver.hpp"
#include "robots/include/sabo_common.hpp"
#include "sabo_bms_driver_debug.hpp"

using namespace xbot::driver::sabo::config;

namespace xbot::driver::bms {

/**
 * @brief Sabo BMS driver
 *
 * Stock Sabo FW read I2C addresses 0x0b, 0x09, 0x0D, 0x05 and 0x06
 * But my current setup only response on 0x0b and 0x09
 * 0x0b got identified as SBS/SMBus Smart Battery device
 * 0x09 TODO
 */
class SaboBmsDriver : public BmsDriver {
 public:
  struct Data {
    char mfr_name[33]{};       // Read only once
    uint16_t mfr_date{};       // Days since 1970-01-01 (UTC), read only once
    char dev_name[33]{};       // Read only once
    char dev_chemistry[33]{};  // Read only once
    uint16_t serial_number{};  // Read only once
  };

  struct StaticCache {
    bool valid_mfg_name{false};
    bool valid_device_name{false};
    bool valid_chemistry{false};
    bool valid_mfg_date{false};
    bool valid_serial{false};

    char mfg_name[33]{};
    char device_name[33]{};
    char chemistry[33]{};
    uint16_t mfg_date_raw{0};
    uint16_t serial_raw{0};
  };

  struct DynamicSnapshot {
    bool valid_pack_voltage{false};
    bool valid_pack_current{false};
    bool valid_temperature{false};
    bool valid_rel_soc{false};
    bool valid_remaining_capacity{false};
    bool valid_full_charge_capacity{false};
    bool valid_cycle_count{false};
    bool valid_cell_voltages{false};

    uint16_t pack_voltage_mv{0};
    int16_t pack_current_ma{0};
    int32_t temperature_centi_c{0};
    uint16_t rel_soc_percent{0};
    uint16_t remaining_capacity_mah{0};
    uint16_t full_charge_capacity_mah{0};
    uint16_t cycle_count{0};
    uint16_t cell_mv[7]{};
  };

  /**
   * @brief Constructor
   * @param bms_cfg Pointer to BMS hardware configuration
   */
  explicit SaboBmsDriver(const Bms* bms_cfg);

  bool Init() override;
  void Tick() override;

  bool DumpDevice();

  // Cache: read once in Init(), never refreshed.
  const StaticCache& GetStaticCache() const {
    return static_cache_;
  }

  // Snapshot: updated by Poll(); Tick() can call Poll() later.
  const DynamicSnapshot& GetDynamicSnapshot() const {
    return snapshot_;
  }
  bool Poll();

  // Normal (non-debug) read API: only expose values that are present/usable on this pack.
  bool ReadPackVoltageMilliV(uint16_t& out_mv);
  bool ReadPackCurrentMilliA(int16_t& out_ma);
  bool ReadTemperatureCentiC(int32_t& out_centi_c);
  bool ReadRelativeSoCPercent(uint16_t& out_percent);
  bool ReadRemainingCapacityMilliAh(uint16_t& out_mah);
  bool ReadFullChargeCapacityMilliAh(uint16_t& out_mah);
  bool ReadCycleCount(uint16_t& out_cycles);

  // Vendor-specific: observed at cmds 0x3C..0x42, values look like per-cell mV for 7S packs.
  bool ReadCellVoltagesMilliV(uint16_t out_mv[7]);

 private:
  static constexpr uint8_t DEVICE_ADDRESS = 0x0B;
  const xbot::driver::sabo::config::Bms* bms_cfg_;

  bool configured_{false};
  bool present_{false};

  StaticCache static_cache_{};
  DynamicSnapshot snapshot_{};
  Data data_{};

  static constexpr size_t READ_BUFFER_SIZE = 64;  // Buffer for raw SPI data
  static uint8_t rx_buffer_[READ_BUFFER_SIZE];

  static constexpr unsigned i2c_retries = 3;
  static constexpr unsigned i2c_retry_delay_ms = 2;

  friend bool debug::DumpDevice(SaboBmsDriver& driver);

  msg_t ReadRegisterRaw(uint8_t reg, uint8_t* rx, size_t rx_len);
  msg_t ReadRegister(uint8_t reg, uint8_t& result);
  msg_t ReadRegister(uint8_t reg, uint16_t& result);
  msg_t ReadRegister(uint8_t reg, int16_t& result);
  msg_t ReadBlock(uint8_t cmd, uint8_t* data, size_t data_capacity, size_t& out_len);

  bool LoadStaticCacheOnce();
  static int16_t ScaleCurrentRawToMilliA(int16_t raw);

  bool ReadSbsTemperature(uint8_t cmd, uint16_t& out_raw, int32_t& out_centi_c);
  bool ReadSbsTemperatureCentiC(uint8_t cmd, int32_t& out_centi_c);
  bool ReadSbsVoltageMilliV(uint8_t cmd, uint16_t& out_mv);
  bool ReadSbsCurrentMilliA(uint8_t cmd, int16_t& out_ma);
  bool ReadSbsCapacityMilliAh(uint8_t cmd, uint16_t& out_mah);
  bool ReadSbsMinutes(uint8_t cmd, uint16_t& out_minutes);
};

}  // namespace xbot::driver::bms

#endif  // SABO_BMS_DRIVER_HPP
