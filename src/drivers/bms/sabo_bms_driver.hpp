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
#include "sbs_protocol.hpp"

using namespace xbot::driver::sabo::config;

namespace xbot::driver::bms {

/**
 * @brief Sabo BMS driver
 *
 * Stock Sabo FW read I2C addresses 0x0b, 0x09, 0x0D, 0x05 and 0x06.
 * But my last OEM setup only responded on 0x0b and 0x09.
 * 0x0b got identified as SBS/SMBus Smart Battery device.
 * 0x09 seem to be some other OEM I2c device.
 */
class SaboBmsDriver : public BmsDriver {
 public:
  static constexpr uint8_t kCellCount = 7;

  /**
   * @brief Constructor
   * @param bms_cfg Pointer to Sabo-BMS hardware configuration
   */
  explicit SaboBmsDriver(const Bms* bms_cfg);

  bool Init() override;
  void Tick() override;

  const char* GetExtraDataJson() const override;

  bool DumpDevice();

 private:
  static constexpr uint8_t DEVICE_ADDRESS = 0x0B;
  const xbot::driver::sabo::config::Bms* bms_cfg_;

  SbsProtocol sbs_{};

  bool configured_{false};
  bool probe();

  static constexpr unsigned i2c_retries = 3;
  static constexpr unsigned i2c_retry_delay_ms = 2;

  // SBS protocol delegates (avoid overload ambiguity when binding callbacks).
  msg_t SbsReadByte(uint8_t cmd, uint8_t& out);
  msg_t SbsReadWord(uint8_t cmd, uint16_t& out);
  msg_t SbsReadInt16(uint8_t cmd, int16_t& out);
  msg_t SbsReadBlock(uint8_t cmd, uint8_t* data, size_t data_capacity, size_t& out_len);

  msg_t ReadRegisterRaw(uint8_t reg, uint8_t* rx, size_t rx_len);
  msg_t ReadRegister(uint8_t reg, uint8_t& result);
  msg_t ReadRegister(uint8_t reg, uint16_t& result);
  msg_t ReadRegister(uint8_t reg, int16_t& result);
  msg_t ReadBlock(uint8_t cmd, uint8_t* data, size_t data_capacity, size_t& out_len);

  static int16_t ScaleCurrentRawToMilliA(int16_t raw);

  inline void rtrim(char* str);
};

}  // namespace xbot::driver::bms

#endif  // SABO_BMS_DRIVER_HPP
