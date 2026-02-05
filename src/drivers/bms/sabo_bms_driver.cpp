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
 * @date 2025-12-27
 */

#include "sabo_bms_driver.hpp"

#include <ch.h>
#include <chprintf.h>
#include <ctype.h>
#include <ulog.h>

#include <cstdio>
#include <cstring>

#include "json_utils.hpp"
#include "sbs_debug.hpp"

#ifdef USE_SEGGER_RTT
extern "C" {
#include <SEGGER_RTT_streams.h>
}
#endif

using namespace xbot::json;
using namespace xbot::driver;

namespace xbot::driver::bms {

namespace {

static void BmsPrintLine(const char* line) {
  if (line == nullptr) {
    return;
  }
#ifdef USE_SEGGER_RTT
  constexpr unsigned kRttUpIndex = 0;
  (void)SEGGER_RTT_Write(kRttUpIndex, line, ::strlen(line));
#else
  ULOG_INFO("%s", line);
#endif
}

static void PrintLineCb(void* /*ctx*/, const char* line) {
  BmsPrintLine(line);
}

static uint32_t GetI2cErrorsCb(void* ctx) {
  auto* i2c = static_cast<I2CDriver*>(ctx);
  if (i2c == nullptr) return 0;
  return i2cGetErrors(i2c);
}

}  // namespace

SaboBmsDriver::SaboBmsDriver(const Bms* bms_cfg)
    : BmsDriver(kCellCount),
      bms_cfg_(bms_cfg),
      sbs_(SbsProtocol::ReadByteFn::create<SaboBmsDriver, &SaboBmsDriver::SbsReadByte>(*this),
           SbsProtocol::ReadWordFn::create<SaboBmsDriver, &SaboBmsDriver::SbsReadWord>(*this),
           SbsProtocol::ReadInt16Fn::create<SaboBmsDriver, &SaboBmsDriver::SbsReadInt16>(*this),
           SbsProtocol::ReadBlockFn::create<SaboBmsDriver, &SaboBmsDriver::SbsReadBlock>(*this)) {
}

msg_t SaboBmsDriver::SbsReadByte(uint8_t cmd, uint8_t& out) {
  return ReadRegister(cmd, out);
}

msg_t SaboBmsDriver::SbsReadWord(uint8_t cmd, uint16_t& out) {
  return ReadRegister(cmd, out);
}

msg_t SaboBmsDriver::SbsReadInt16(uint8_t cmd, int16_t& out) {
  return ReadRegister(cmd, out);
}

msg_t SaboBmsDriver::SbsReadBlock(uint8_t cmd, uint8_t* data, size_t data_capacity, size_t& out_len) {
  return ReadBlock(cmd, data, data_capacity, out_len);
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
  return true;
}

void SaboBmsDriver::Tick() {
  static uint8_t check_cnt = 10;  // Need some retries to detect presence
  static systime_t last_log_time = 0;

  if (!configured_ || (!IsPresent() && !check_cnt)) return;

  // Acquire bus and request DMA resource
  i2cAcquireBus(bms_cfg_->i2c);
  if (!IsPresent()) {
    SetPresent(probe());
    check_cnt--;
    if (!IsPresent()) {
      i2cReleaseBus(bms_cfg_->i2c);
      return;
    } else {
      size_t len = 0;

      // Read some static generic data which will not change during runtime

      // ManufacturerName (0x20) block
      sbs_.ReadBlock(SbsProtocol::Command::ManufacturerName, reinterpret_cast<uint8_t*>(data_.mfr_name),
                     sizeof(data_.mfr_name), len);
      rtrim(data_.mfr_name);

      // DeviceName (0x21) block
      sbs_.ReadBlock(SbsProtocol::Command::DeviceName, reinterpret_cast<uint8_t*>(data_.dev_name),
                     sizeof(data_.dev_name), len);
      rtrim(data_.dev_name);

      // DeviceChemistry (0x22) block
      // "FSM-BMZ, 30710, V2.00" has some kind of version string in it
      sbs_.ReadBlock(SbsProtocol::Command::DeviceChemistry, reinterpret_cast<uint8_t*>(data_.dev_version),
                     sizeof(data_.dev_version), len);
      rtrim(data_.dev_version);
      chsnprintf(data_.dev_chemistry, sizeof(data_.dev_chemistry), "LION");

      uint16_t tmp_uint16 = 0;

      // ManufacturerDate (0x1B) word
      if (sbs_.ReadWord(SbsProtocol::Command::ManufacturerDate, tmp_uint16) == MSG_OK) {
        SbsProtocol::SbsDateToYMDString(tmp_uint16, data_.mfr_date, sizeof(data_.mfr_date));
      }

      // SerialNumber (0x1C) word
      if (sbs_.ReadWord(SbsProtocol::Command::SerialNumber, tmp_uint16) == MSG_OK) {
        data_.serial_number = tmp_uint16;
      }

      // DesignCapacity (0x18) word
      if (sbs_.ReadWord(SbsProtocol::Command::DesignCapacity, tmp_uint16) == MSG_OK) {
        data_.design_capacity_ah = (float)tmp_uint16 / 1000.0f;
      }

      // DesignVoltage (0x19) word
      if (sbs_.ReadWord(SbsProtocol::Command::DesignVoltage, tmp_uint16) == MSG_OK) {
        data_.design_voltage_v = (float)tmp_uint16 / 1000.0f;
      }

      ULOG_INFO("BMS '%s, %s, %s, S/N %u' found at I2C addr 0x%02X", data_.mfr_name, data_.dev_name,
                data_.dev_chemistry, (unsigned)data_.serial_number, (unsigned)DEVICE_ADDRESS);
    }
  }

  // Read dynamic data (update only when value is >0 / !=0)
  uint16_t u16 = 0;
  int16_t i16 = 0;

  // Temperature (0x08) word: 0.1 Kelvin
  if (ReadRegister(0x08, u16) == MSG_OK && u16 > 0U) {
    data_.temperature_c = ((float)u16 * 0.1f) - 273.15f;
  }

  // Voltage (0x09) word: mV
  if (sbs_.ReadWord(SbsProtocol::Command::Voltage, u16) == MSG_OK && u16 > 0U) {
    data_.pack_voltage_v = (float)u16 / 1000.0f;
  }

  // Current (0x0A) word: pack-specific scaling (10 mA/LSB observed)
  if (sbs_.ReadInt16(SbsProtocol::Command::Current, i16) == MSG_OK) {
    data_.pack_current_a = (float)ScaleCurrentRawToMilliA(i16) / 1000.0f;
  }

  // Relative SoC (0x0D) word: percent
  if (ReadRegister(0x0D, u16) == MSG_OK) {
    float soc = (float)u16 / 100.0f;
    if (soc < 0.0f) soc = 0.0f;
    if (soc > 1.0f) soc = 1.0f;
    data_.battery_soc = soc;
  }

  // RemainingCapacity (0x0F) word: mAh
  if (ReadRegister(0x0F, u16) == MSG_OK && u16 > 0U) {
    data_.remaining_capacity_ah = (float)u16 / 1000.0f;
  }

  // FullChargeCapacity (0x10) word: mAh
  if (ReadRegister(0x10, u16) == MSG_OK && u16 > 0U) {
    data_.full_charge_capacity_ah = (float)u16 / 1000.0f;
  }

  // BatteryStatus (0x16) word
  // "FSM-BMZ, 30710, V2.00" seem to support only some of the status bits.
  if (ReadRegister(0x16, u16) == MSG_OK) {
    // Bit 11, 0x0800 looks like a TERMINATE_DISCHARGE_ALARM flag, SoC < 5% ?
    (u16 & 0b0000100000000000)
        ? SbsProtocol::SetBatteryStatus(data_.battery_status, SbsProtocol::BatteryStatusBit::AlarmTerminateDischarge)
        : SbsProtocol::ResetBatteryStatus(data_.battery_status, SbsProtocol::BatteryStatusBit::AlarmTerminateDischarge);
    // Bit 7-6 looks like a CHARGING flag => /DISCHARGING
    ((u16 & 0b11000000) && data_.pack_current_a > 0.0f)
        ? SbsProtocol::ResetBatteryStatus(data_.battery_status, SbsProtocol::BatteryStatusBit::StatusDischarging)
        : SbsProtocol::SetBatteryStatus(data_.battery_status, SbsProtocol::BatteryStatusBit::StatusDischarging);
    // Bit 5 looks like a /FULLY_CHARGED flag
    (u16 & 0b00100000)
        ? SbsProtocol::ResetBatteryStatus(data_.battery_status, SbsProtocol::BatteryStatusBit::StatusFullyCharged)
        : SbsProtocol::SetBatteryStatus(data_.battery_status, SbsProtocol::BatteryStatusBit::StatusFullyCharged);
    // Bit 1 looks like a FULLY_DISCHARGED flag, SoC < 20% ?
    (u16 & 0b00000010)
        ? SbsProtocol::SetBatteryStatus(data_.battery_status, SbsProtocol::BatteryStatusBit::StatusFullyDischarged)
        : SbsProtocol::ResetBatteryStatus(data_.battery_status, SbsProtocol::BatteryStatusBit::StatusFullyDischarged);
    // Bit 0 looks like a TERMINATE_DISCHARGE_ALARM flag, SoC < 5% ?
    /*(u16 & 0b00000001)
        ? SbsProtocol::SetBatteryStatus(data_.battery_status, SbsProtocol::BatteryStatusBit::AlarmTerminateDischarge)
        : SbsProtocol::ResetBatteryStatus(data_.battery_status,
       SbsProtocol::BatteryStatusBit::AlarmTerminateDischarge);*/
  }

  // CycleCount (0x17) word
  if (ReadRegister(0x17, u16) == MSG_OK && u16 > 0U) {
    data_.cycle_count = u16;
  }

  // Per-cell voltages: vendor-specific 0x3C..0x42, mV
  const uint8_t cell_count = (data_.cell_count > (uint8_t)kMaxCells) ? (uint8_t)kMaxCells : data_.cell_count;
  for (uint8_t i = 0; i < cell_count; i++) {
    const uint8_t cmd = (uint8_t)(0x3CU + i);
    uint16_t mv = 0;
    if (ReadRegister(cmd, mv) == MSG_OK && mv > 0U) {
      data_.cell_voltage_v[i] = (float)mv / 1000.0f;
    }
  }

  // Log BMS data every minute for analysis and BatteryStatus monitoring
  const systime_t now = chVTGetSystemTimeX();
  if (chVTTimeElapsedSinceX(last_log_time) > TIME_S2I(60)) {
    last_log_time = now;

    // Format battery_status as hex and binary
    char status_bin[17];
    for (int i = 0; i < 16; i++) {
      status_bin[i] = (data_.battery_status & (1 << (15 - i))) ? '1' : '0';
    }
    status_bin[16] = '\0';

    ULOG_INFO("BMS Data: t=%lu V=%.2f I=%.3f T=%.1f SOC=%.1f%% RemCap=%.3fAh FullCap=%.3fAh Status=0x%04X (%s)",
              (unsigned long)TIME_I2MS(now), (double)data_.pack_voltage_v, (double)data_.pack_current_a,
              (double)data_.temperature_c, (double)(data_.battery_soc * 100.0f), (double)data_.remaining_capacity_ah,
              (double)data_.full_charge_capacity_ah, (unsigned)data_.battery_status, status_bin);
  }
  i2cReleaseBus(bms_cfg_->i2c);
}

const char* SaboBmsDriver::GetExtraDataJson() const {
  // FIXME: Quite large JSON buffer. Optimize as streamed output?
  static char json_buf[512];

  json_buf[0] = '\0';

  if (!IsPresent()) {
    return json_buf;
  }

  int chars = 0;
  char esc_str[60] = {0};

  EscapeStringInto(data_.mfr_name, esc_str, sizeof(esc_str));
  chars += chsnprintf(json_buf, sizeof(json_buf), "{\"mfr_name\":\"%s\"", esc_str);

  // Manufacturer date: already formatted as YYYY-MM-DD.
  chars += chsnprintf(json_buf + chars, sizeof(json_buf) - chars, ",\"mfr_date\":\"%s\"", data_.mfr_date);

  EscapeStringInto(data_.dev_name, esc_str, sizeof(esc_str));
  chars += chsnprintf(json_buf + chars, sizeof(json_buf) - chars, ",\"dev_name\":\"%s\"", esc_str);

  EscapeStringInto(data_.dev_version, esc_str, sizeof(esc_str));
  chars += chsnprintf(json_buf + chars, sizeof(json_buf) - chars, ",\"dev_version\":\"%s\"", esc_str);

  EscapeStringInto(data_.dev_chemistry, esc_str, sizeof(esc_str));
  chars += chsnprintf(json_buf + chars, sizeof(json_buf) - chars, ",\"dev_chemistry\":\"%s\"", esc_str);

  chars += chsnprintf(json_buf + chars, sizeof(json_buf) - chars,
                      ",\"serial_number\":%u,\"design_capacity_ah\":%.3f,\"design_voltage_v\":%.3f",
                      (unsigned)data_.serial_number, (double)data_.design_capacity_ah, (double)data_.design_voltage_v);

  chars += chsnprintf(json_buf + chars, sizeof(json_buf) - chars,
                      ",\"full_charge_capacity_ah\":%.3f,\"remaining_capacity_ah\":%.3f,\"cycle_count\":%u",
                      (double)data_.full_charge_capacity_ah, (double)data_.remaining_capacity_ah,
                      (unsigned)data_.cycle_count);

  // Cell voltages
  chars += chsnprintf(json_buf + chars, sizeof(json_buf) - chars, ",\"cell_count\":%u,\"cell_voltage_v\": [",
                      (unsigned)data_.cell_count);
  for (uint8_t i = 0; i < data_.cell_count; i++) {
    chars += chsnprintf(json_buf + chars, sizeof(json_buf) - chars, "%.3f%s", (double)data_.cell_voltage_v[i],
                        (i + 1U < data_.cell_count) ? "," : "");
  }
  chars += chsnprintf(json_buf + chars, sizeof(json_buf) - chars, "]}");

  if (chars <= 0) {
    json_buf[0] = '\0';
  }

  return json_buf;
}

bool SaboBmsDriver::probe() {
  if (!configured_ || bms_cfg_ == nullptr || bms_cfg_->i2c == nullptr) return false;

  msg_t msg = MSG_RESET;

  // Voltage (0x09) word
  uint16_t battery_voltage = 0;
  msg = sbs_.ReadWord(SbsProtocol::Command::Voltage, battery_voltage);
  if (msg == MSG_OK && battery_voltage > 0) {
    return true;
  }

  // BatteryStatus (0x16) word
  uint16_t battery_status = 0;
  msg = sbs_.ReadWord(SbsProtocol::Command::BatteryStatus, battery_status);
  if (msg == MSG_OK) {
    return true;
  }

  return false;
}

bool SaboBmsDriver::DumpDevice() {
  if (bms_cfg_ == nullptr || bms_cfg_->i2c == nullptr) {
    return false;
  }

  {
    char line[128];
    std::snprintf(line, sizeof(line), "BMS dump addr=0x%02X\r\n", (unsigned)DEVICE_ADDRESS);
    BmsPrintLine(line);
  }

  debug::SbsDebugCallbacks cb{};
  cb.ctx = bms_cfg_->i2c;
  cb.print_line = &PrintLineCb;
  cb.get_i2c_errors = &GetI2cErrorsCb;

  debug::SbsDebugOptions opt{};
  opt.include_cmd_scan = true;
  opt.list_unknown_nonzero = true;
  opt.suppress_cmds_0x3c_0x42 = true;  // printed separately as cell voltages

  if (!debug::DumpSbsDevice(sbs_, cb, opt)) {
    return false;
  }

  // Vendor-specific: observed per-cell voltages (mV) at 0x3C..0x42 (7 values for 7S packs).
  {
    BmsPrintLine("BMS cell voltages cmds 0x3C..0x42\r\n");

    bool all_ok = true;
    uint32_t sum_mv = 0;
    uint16_t min_mv = 0xFFFFU;
    uint16_t max_mv = 0U;

    for (uint8_t i = 0; i < 7U; i++) {
      const uint8_t reg = (uint8_t)(0x3CU + i);
      uint16_t mv = 0;
      const msg_t msg = ReadRegister(reg, mv);
      const bool ok = (msg == MSG_OK);
      all_ok = all_ok && ok;

      if (ok) {
        sum_mv += mv;
        min_mv = (mv < min_mv) ? mv : min_mv;
        max_mv = (mv > max_mv) ? mv : max_mv;
      }

      char line[160];
      if (!ok) {
        std::snprintf(line, sizeof(line), "  Cell%u (0x%02X) NACK msg=%d\r\n", (unsigned)(i + 1U), (unsigned)reg,
                      (int)msg);
      } else {
        std::snprintf(line, sizeof(line), "  Cell%u (0x%02X) ACK  = %u mV = %lu.%03lu V\r\n", (unsigned)(i + 1U),
                      (unsigned)reg, (unsigned)mv, (unsigned long)((uint32_t)mv / 1000U),
                      (unsigned long)((uint32_t)mv % 1000U));
      }
      BmsPrintLine(line);
    }

    if (all_ok) {
      const uint16_t delta_mv = (uint16_t)(max_mv - min_mv);
      char line[192];
      std::snprintf(line, sizeof(line), "  CellSum=%lu.%03lu V  Min=%u  Max=%u  Delta=%u mV\r\n",
                    (unsigned long)(sum_mv / 1000U), (unsigned long)(sum_mv % 1000U), (unsigned)min_mv,
                    (unsigned)max_mv, (unsigned)delta_mv);
      BmsPrintLine(line);
    }
  }

  return true;
}

msg_t SaboBmsDriver::ReadRegisterRaw(uint8_t reg, uint8_t* rx, size_t rx_len) {
  if (bms_cfg_ == nullptr || bms_cfg_->i2c == nullptr || rx == nullptr || rx_len == 0) return MSG_RESET;

  msg_t last_msg = MSG_RESET;
  for (unsigned attempt = 0; attempt < i2c_retries; attempt++) {
    const msg_t msg = i2cMasterTransmit(bms_cfg_->i2c, DEVICE_ADDRESS, &reg, 1, rx, rx_len);
    last_msg = msg;
    if (msg == MSG_OK) break;
    chThdSleepMilliseconds(i2c_retry_delay_ms);
  }

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

  return last_msg;
}

int16_t SaboBmsDriver::ScaleCurrentRawToMilliA(int16_t raw) {
  const int32_t scaled = (int32_t)raw * 10;
  if (scaled > 32767) return 32767;
  if (scaled < -32768) return -32768;
  return (int16_t)scaled;
}

void SaboBmsDriver::rtrim(char* str) {
  if (*str == 0) return;

  // Trailing spaces
  char* end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end)) end--;

  end[1] = '\0';
}

}  // namespace xbot::driver::bms
