/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_bms_driver.cpp
 * @brief Sabo BMS driver implementation
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-12-20
 */

#include "sabo_bms_driver.hpp"

#include <ch.h>
#include <ulog.h>

#include <cstdio>
#include <cstring>

namespace xbot::driver::bms {

namespace {

// Convert civil date to days since Unix epoch (1970-01-01), proleptic Gregorian calendar.
// Based on a well-known algorithm by Howard Hinnant.
static int64_t DaysFromCivil(int32_t y, uint32_t m, uint32_t d) {
  y -= (m <= 2);
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = (uint32_t)(y - era * 400);                                 // [0, 399]
  const uint32_t doy = (153 * (m + (m > 2 ? (uint32_t)-3 : 9)) + 2) / 5 + d - 1;  // [0, 365]
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                     // [0, 146096]
  return (int64_t)era * 146097 + (int64_t)doe - 719468;                           // 719468 = days to 1970-01-01
}

static bool SbsMfgDateRawToEpochDays(uint16_t raw, uint16_t& out_days) {
  const uint32_t day = raw & 0x1FU;
  const uint32_t month = (raw >> 5) & 0x0FU;
  const uint32_t year = 1980U + ((raw >> 9) & 0x7FU);

  if (year < 1970U || year > 2107U) return false;
  if (month < 1U || month > 12U) return false;
  if (day < 1U || day > 31U) return false;

  const int64_t days = DaysFromCivil((int32_t)year, month, day);
  if (days < 0 || days > 0xFFFF) return false;
  out_days = (uint16_t)days;
  return true;
}

}  // namespace

// Define static member variable
uint8_t SaboBmsDriver::rx_buffer_[SaboBmsDriver::READ_BUFFER_SIZE];

SaboBmsDriver::SaboBmsDriver(const Bms* bms_cfg) : bms_cfg_(bms_cfg) {
}

bool SaboBmsDriver::Init() {
  if (bms_cfg_ == nullptr) {
    ULOG_ERROR("SaboBmsDriver: BMS config not set\n");
    return false;
  }

  if (bms_cfg_->i2c == nullptr) {
    ULOG_ERROR("SaboBmsDriver: I2C driver not set\n");
    return false;
  }
  configured_ = true;

  return configured_;
}

void SaboBmsDriver::Tick() {
  static uint8_t check_cnt = 10;  // Need some retries to detect presence

  if (!configured_ || (!present_ && !check_cnt)) return;

  if (!present_) {
    check_cnt--;

    size_t len = 0;

    // Read some static generic data which will not change during runtime and use that also as presence probe
    // ManufacturerName (0x20) block
    if (ReadBlock(0x20, reinterpret_cast<uint8_t*>(data_.mfr_name), sizeof(data_.mfr_name), len) == MSG_OK)
      present_ = true;

    // DeviceName (0x21) block
    if (ReadBlock(0x21, reinterpret_cast<uint8_t*>(data_.dev_name), sizeof(data_.dev_name), len) == MSG_OK)
      present_ = true;

    // DeviceChemistry (0x22) block
    if (ReadBlock(0x22, reinterpret_cast<uint8_t*>(data_.dev_chemistry), sizeof(data_.dev_chemistry), len) == MSG_OK)
      present_ = true;

    uint16_t tmp_uint16;

    // ManufacturerDate (0x1B) word
    if (ReadRegister(0x1B, tmp_uint16) == MSG_OK) {
      uint16_t days = 0;
      if (SbsMfgDateRawToEpochDays(tmp_uint16, days)) {
        data_.mfr_date = days;
      }
      present_ = true;
    }

    // SerialNumber (0x1C) word
    if (ReadRegister(0x1C, tmp_uint16) == MSG_OK) {
      data_.serial_number = tmp_uint16;
      present_ = true;
    }

  } else {
    asm volatile("nop");
  }

  //(void)Poll();
}

bool SaboBmsDriver::DumpDevice() {
  return debug::DumpDevice(*this);
}

bool SaboBmsDriver::ReadPackVoltageMilliV(uint16_t& out_mv) {
  return ReadSbsVoltageMilliV(0x09, out_mv);
}

bool SaboBmsDriver::ReadPackCurrentMilliA(int16_t& out_ma) {
  return ReadSbsCurrentMilliA(0x0A, out_ma);
}

bool SaboBmsDriver::ReadTemperatureCentiC(int32_t& out_centi_c) {
  return ReadSbsTemperatureCentiC(0x08, out_centi_c);
}

bool SaboBmsDriver::ReadRelativeSoCPercent(uint16_t& out_percent) {
  return ReadRegister(0x0D, out_percent) == MSG_OK;
}

bool SaboBmsDriver::ReadRemainingCapacityMilliAh(uint16_t& out_mah) {
  return ReadSbsCapacityMilliAh(0x0F, out_mah);
}

bool SaboBmsDriver::ReadFullChargeCapacityMilliAh(uint16_t& out_mah) {
  return ReadSbsCapacityMilliAh(0x10, out_mah);
}

bool SaboBmsDriver::ReadCycleCount(uint16_t& out_cycles) {
  return ReadRegister(0x17, out_cycles) == MSG_OK;
}

bool SaboBmsDriver::ReadCellVoltagesMilliV(uint16_t out_mv[7]) {
  if (out_mv == nullptr) return false;

  bool any_ok = false;
  for (uint8_t i = 0; i < 7U; i++) {
    uint16_t mv = 0;
    const msg_t msg = ReadRegister((uint8_t)(0x3CU + i), mv);
    const bool ok = (msg == MSG_OK);
    out_mv[i] = ok ? mv : 0U;
    any_ok = any_ok || ok;
  }

  return any_ok;
}

bool SaboBmsDriver::Poll() {
  if (!present_) return false;
  if (bms_cfg_ == nullptr || bms_cfg_->i2c == nullptr) return false;

  // Reset validity flags; keep last values only if re-read succeeds.
  snapshot_ = DynamicSnapshot{};

  bool any_ok = false;

  struct WordReg {
    uint8_t cmd;
    uint16_t* dst;
    bool* valid;
  };

  // One loop for the simple word registers.
  WordReg regs[] = {
      {0x09, &snapshot_.pack_voltage_mv, &snapshot_.valid_pack_voltage},
      {0x0D, &snapshot_.rel_soc_percent, &snapshot_.valid_rel_soc},
      {0x0F, &snapshot_.remaining_capacity_mah, &snapshot_.valid_remaining_capacity},
      {0x10, &snapshot_.full_charge_capacity_mah, &snapshot_.valid_full_charge_capacity},
      {0x17, &snapshot_.cycle_count, &snapshot_.valid_cycle_count},
  };

  for (size_t i = 0; i < (sizeof(regs) / sizeof(regs[0])); i++) {
    uint16_t v = 0;
    const bool ok = (ReadRegister(regs[i].cmd, v) == MSG_OK);
    *regs[i].valid = ok;
    *regs[i].dst = ok ? v : 0U;
    any_ok = any_ok || ok;
  }

  // Special decodes: temperature + current.
  {
    uint16_t raw = 0;
    const bool ok = (ReadRegister(0x08, raw) == MSG_OK);
    snapshot_.valid_temperature = ok;
    snapshot_.temperature_centi_c = ok ? ((int32_t)raw * 10 - 27315) : 0;
    any_ok = any_ok || ok;
  }

  {
    uint16_t raw_u = 0;
    const bool ok = (ReadRegister(0x0A, raw_u) == MSG_OK);
    snapshot_.valid_pack_current = ok;
    snapshot_.pack_current_ma = ok ? ScaleCurrentRawToMilliA((int16_t)raw_u) : 0;
    any_ok = any_ok || ok;
  }

  {
    bool cells_any_ok = false;
    for (uint8_t i = 0; i < 7U; i++) {
      uint16_t mv = 0;
      const bool ok = (ReadRegister((uint8_t)(0x3CU + i), mv) == MSG_OK);
      snapshot_.cell_mv[i] = ok ? mv : 0U;
      cells_any_ok = cells_any_ok || ok;
    }
    snapshot_.valid_cell_voltages = cells_any_ok;
    any_ok = any_ok || cells_any_ok;
  }

  return any_ok;
}

msg_t SaboBmsDriver::ReadRegisterRaw(uint8_t reg, uint8_t* rx, size_t rx_len) {
  if (bms_cfg_ == nullptr || bms_cfg_->i2c == nullptr || rx == nullptr || rx_len == 0) return MSG_RESET;

  i2cAcquireBus(bms_cfg_->i2c);

  msg_t last_msg = MSG_RESET;
  for (unsigned attempt = 0; attempt < i2c_retries; attempt++) {
    const msg_t msg = i2cMasterTransmit(bms_cfg_->i2c, DEVICE_ADDRESS, &reg, 1, rx, rx_len);
    last_msg = msg;
    if (msg == MSG_OK) break;
    chThdSleepMilliseconds(i2c_retry_delay_ms);
  }

  i2cReleaseBus(bms_cfg_->i2c);
  return last_msg;
}

msg_t SaboBmsDriver::ReadRegister(uint8_t reg, uint8_t& result) {
  uint8_t rx = 0;
  const msg_t msg = ReadRegisterRaw(reg, &rx, 1);
  if (msg == MSG_OK) result = rx;
  return msg;
}

msg_t SaboBmsDriver::ReadRegister(uint8_t reg, uint16_t& result) {
  uint8_t rx[2] = {0, 0};
  const msg_t msg = ReadRegisterRaw(reg, rx, 2);
  if (msg == MSG_OK) result = (uint16_t)rx[0] | (uint16_t)((uint16_t)rx[1] << 8);
  return msg;
}

msg_t SaboBmsDriver::ReadRegister(uint8_t reg, int16_t& result) {
  uint16_t u = 0;
  const msg_t msg = ReadRegister(reg, u);
  if (msg == MSG_OK) result = (int16_t)u;
  return msg;
}

msg_t SaboBmsDriver::ReadBlock(uint8_t cmd, uint8_t* data, size_t data_capacity, size_t& out_len) {
  out_len = 0;
  if (!bms_cfg_ || !bms_cfg_->i2c || !data || !data_capacity) return MSG_RESET;

  i2cAcquireBus(bms_cfg_->i2c);

  // SMBus block read: device returns [len][data0][data1]...[data(len-1)]
  // Read the maximum (1 + 32) in one transaction.
  const size_t rx_max = etl::min((int)data_capacity, 33);

  msg_t last_msg = MSG_RESET;

  for (unsigned attempt = 0; attempt < i2c_retries; attempt++) {
    memset(data, 0, rx_max);
    const msg_t msg = i2cMasterTransmit(bms_cfg_->i2c, DEVICE_ADDRESS, &cmd, 1, data, rx_max);
    last_msg = msg;
    if (msg != MSG_OK) {
      chThdSleepMilliseconds(i2c_retry_delay_ms);
      continue;
    }

    // Validate SMBus length byte.
    const uint8_t len_u8 = data[0];
    if (len_u8 > 32U) {
      last_msg = MSG_RESET;
      chThdSleepMilliseconds(i2c_retry_delay_ms);
      continue;
    }
    const size_t len = (size_t)len_u8;

    // Ensure we actually received the whole payload in this single transaction.
    if ((1U + len) > rx_max) {
      last_msg = MSG_RESET;
      chThdSleepMilliseconds(i2c_retry_delay_ms);
      continue;
    }

    // Shift payload in-place (data[0] was the length byte).
    if (len > 0U) {
      memmove(data, &data[1], len);
    }
    out_len = len;
    if (len < data_capacity) {
      data[len] = 0;
    }
    break;
  }

  i2cReleaseBus(bms_cfg_->i2c);
  return last_msg;
}

int16_t SaboBmsDriver::ScaleCurrentRawToMilliA(int16_t raw) {
  const int32_t scaled = (int32_t)raw * 10;
  if (scaled > 32767) return 32767;
  if (scaled < -32768) return -32768;
  return (int16_t)scaled;
}

bool SaboBmsDriver::ReadSbsTemperatureCentiC(uint8_t cmd, int32_t& out_centi_c) {
  uint16_t raw = 0;
  return ReadSbsTemperature(cmd, raw, out_centi_c);
}

bool SaboBmsDriver::ReadSbsTemperature(uint8_t cmd, uint16_t& out_raw, int32_t& out_centi_c) {
  // SBS temperature: 0.1 Kelvin
  if (ReadRegister(cmd, out_raw) != MSG_OK) return false;
  // C*100 = raw*10 - 27315
  out_centi_c = (int32_t)out_raw * 10 - 27315;
  return true;
}

bool SaboBmsDriver::ReadSbsVoltageMilliV(uint8_t cmd, uint16_t& out_mv) {
  // SBS voltage: mV
  return ReadRegister(cmd, out_mv) == MSG_OK;
}

bool SaboBmsDriver::ReadSbsCurrentMilliA(uint8_t cmd, int16_t& out_ma) {
  // Pack-specific: observed current appears to be 10 mA/LSB (not 1 mA/LSB).
  int16_t raw = 0;
  if (ReadRegister(cmd, raw) != MSG_OK) return false;

  out_ma = ScaleCurrentRawToMilliA(raw);
  return true;
}

bool SaboBmsDriver::ReadSbsCapacityMilliAh(uint8_t cmd, uint16_t& out_mah) {
  // SBS capacity: mAh
  return ReadRegister(cmd, out_mah) == MSG_OK;
}

bool SaboBmsDriver::ReadSbsMinutes(uint8_t cmd, uint16_t& out_minutes) {
  return ReadRegister(cmd, out_minutes) == MSG_OK;
}

}  // namespace xbot::driver::bms
