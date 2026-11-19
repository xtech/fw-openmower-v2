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

#include "i2c_utils.hpp"
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
  // The Sabo BMS reports 10 mA per LSB where SBS defines 1 mA; everything else is standard.
  sbs_.SetCurrentLsbMilliA(10);
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
    ULOG_ERROR("SaboBmsDriver: BMS config not set");
    return false;
  }

  if (bms_cfg_->i2c == nullptr) {
    ULOG_ERROR("SaboBmsDriver: I2C driver not set");
    return false;
  }

  configured_ = true;
  return true;
}

void SaboBmsDriver::Tick() {
  if (!configured_) return;

  // Detection. Only the first kProbeWindowTicks try, so a board without a pack does not
  // poll the bus forever; once a pack was seen, a loss is recovered by probing every tick.
  if (!IsPresent()) {
    if (!WasDetected() && probes_left_ == 0U) return;
    if (probes_left_ > 0U) probes_left_--;

    i2cAcquireBus(bms_cfg_->i2c);
    const bool found = sbs_.Probe();
    if (found) {
      ReadDeviceInfo();
      SetPresent(true);
      consecutive_failures_ = 0;
      // last_ok_ms is only advanced by a successful voltage sample below; the detection probe
      // gets away with a BatteryStatus answer, and the retained readings must not look fresh.
      ULOG_INFO("BMS '%s, %s, %s, S/N %u' found at I2C addr 0x%02X", info_.mfr_name, info_.dev_name,
                info_.dev_chemistry, (unsigned)info_.serial_number, (unsigned)DEVICE_ADDRESS);
    }
    i2cReleaseBus(bms_cfg_->i2c);
    return;
  }

  i2cAcquireBus(bms_cfg_->i2c);
  const uint32_t now_ms = TIME_I2MS(chVTGetSystemTimeX());

  // The pack voltage is the canary: it answers or it does not. A missed read keeps the old
  // value, so the count is what decides whether the values are still live.
  if (sbs_.ReadPackVoltageV(live_.pack_voltage_v)) {
    live_.last_ok_ms = now_ms;
    consecutive_failures_ = 0;
  } else if (++consecutive_failures_ >= kFailuresBeforeAbsent) {
    // Mark it absent instead of leaving the last values looking live. Probing continues, so
    // a flaky I2C run recovers by itself.
    ULOG_WARNING("BMS: no answer for %u ticks, values are stale", (unsigned)consecutive_failures_);
    SetPresent(false);
    consecutive_failures_ = 0;
    i2cReleaseBus(bms_cfg_->i2c);
    return;
  }

  float value = 0.0f;
  if (sbs_.ReadCurrentA(value)) live_.pack_current_a = value;
  if (sbs_.ReadTemperatureC(value)) live_.temperature_c = value;
  if (sbs_.ReadRelativeStateOfCharge(value)) live_.battery_soc = value;
  if (sbs_.ReadRemainingCapacityAh(value)) live_.remaining_capacity_ah = value;
  if (sbs_.ReadFullChargeCapacityAh(value)) live_.full_charge_capacity_ah = value;

  uint16_t u16 = 0;
  if (sbs_.ReadCycleCount(u16)) live_.cycle_count = u16;

  // BatteryStatus (0x16). "FSM-BMZ, 30710, V2.00" support only some of the status bits.
  if (sbs_.ReadBatteryStatus(live_.battery_status_raw)) {
    // Bit 11, 0x0800 looks like a TERMINATE_DISCHARGE_ALARM flag, SoC < 5% ?
    (live_.battery_status_raw & 0b0000100000000000)
        ? SbsProtocol::SetBatteryStatus(live_.battery_status, SbsProtocol::BatteryStatusBit::AlarmTerminateDischarge)
        : SbsProtocol::ResetBatteryStatus(live_.battery_status, SbsProtocol::BatteryStatusBit::AlarmTerminateDischarge);
    // Bit 7-6 looks like a CHARGING flag => /DISCHARGING
    ((live_.battery_status_raw & 0b11000000) && live_.pack_current_a > 0.0f)
        ? SbsProtocol::ResetBatteryStatus(live_.battery_status, SbsProtocol::BatteryStatusBit::StatusDischarging)
        : SbsProtocol::SetBatteryStatus(live_.battery_status, SbsProtocol::BatteryStatusBit::StatusDischarging);
    // Bit 5: CHARGE_ACCEPTED_N (active-low) - CLEAR = charger accepted, SET = charger absent/rejected.
    const bool charge_accepted = !(live_.battery_status_raw & (1 << 5));
    (charge_accepted && live_.battery_soc >= 0.99f)
        ? SbsProtocol::SetBatteryStatus(live_.battery_status, SbsProtocol::BatteryStatusBit::StatusFullyCharged)
        : SbsProtocol::ResetBatteryStatus(live_.battery_status, SbsProtocol::BatteryStatusBit::StatusFullyCharged);
    // Bit 1 looks like a FULLY_DISCHARGED flag, SoC < 20% ?
    (live_.battery_status_raw & 0b00000010)
        ? SbsProtocol::SetBatteryStatus(live_.battery_status, SbsProtocol::BatteryStatusBit::StatusFullyDischarged)
        : SbsProtocol::ResetBatteryStatus(live_.battery_status, SbsProtocol::BatteryStatusBit::StatusFullyDischarged);
  }

  // Per-cell voltages: vendor-specific 0x3C.., mV
  const uint8_t cell_count = ClampCells(info_.cell_count);
  for (uint8_t i = 0; i < cell_count; i++) {
    uint16_t mv = 0;
    if (ReadRegister((uint8_t)(0x3CU + i), mv) == MSG_OK && mv > 0U) {
      live_.cell_voltage_v[i] = (float)mv / 1000.0f;
    }
  }

  i2cReleaseBus(bms_cfg_->i2c);
}

void SaboBmsDriver::ReadDeviceInfo() {
  // Static data about the pack: read once when it is detected. Start from a fresh object so
  // a failed read after a pack swap cannot keep the previous pack's metadata; the cell count
  // comes from the configuration, not from the pack.
  const uint8_t cell_count = info_.cell_count;
  info_ = DeviceInfo{};
  info_.cell_count = cell_count;

  size_t len = 0;

  // ManufacturerName (0x20) block
  sbs_.ReadBlock(SbsProtocol::Command::ManufacturerName, reinterpret_cast<uint8_t*>(info_.mfr_name),
                 sizeof(info_.mfr_name), len);
  rtrim(info_.mfr_name);

  // DeviceName (0x21) block
  sbs_.ReadBlock(SbsProtocol::Command::DeviceName, reinterpret_cast<uint8_t*>(info_.dev_name), sizeof(info_.dev_name),
                 len);
  rtrim(info_.dev_name);

  // DeviceChemistry (0x22) block
  // "FSM-BMZ, 30710, V2.00" has some kind of version string in it
  sbs_.ReadBlock(SbsProtocol::Command::DeviceChemistry, reinterpret_cast<uint8_t*>(info_.dev_version),
                 sizeof(info_.dev_version), len);
  rtrim(info_.dev_version);
  chsnprintf(info_.dev_chemistry, sizeof(info_.dev_chemistry), "LION");

  uint16_t tmp_uint16 = 0;

  // ManufacturerDate (0x1B) word
  if (sbs_.ReadWord(SbsProtocol::Command::ManufacturerDate, tmp_uint16) == MSG_OK) {
    SbsProtocol::SbsDateToYMDString(tmp_uint16, info_.mfr_date, sizeof(info_.mfr_date));
  }

  // SerialNumber (0x1C) word
  if (sbs_.ReadWord(SbsProtocol::Command::SerialNumber, tmp_uint16) == MSG_OK) {
    info_.serial_number = tmp_uint16;
  }

  // DesignCapacity (0x18) word
  if (sbs_.ReadWord(SbsProtocol::Command::DesignCapacity, tmp_uint16) == MSG_OK) {
    info_.design_capacity_ah = (float)tmp_uint16 / 1000.0f;
  }

  // DesignVoltage (0x19) word
  if (sbs_.ReadWord(SbsProtocol::Command::DesignVoltage, tmp_uint16) == MSG_OK) {
    info_.design_voltage_v = (float)tmp_uint16 / 1000.0f;
  }
}

// Keep a serialization failure until the caller can report it as an RPC error.
static void AppendJson(char* buf, size_t size, int& used, const char* fmt, ...) {
  if (used < 0) return;

  const size_t offset = static_cast<size_t>(used);
  const size_t remaining = size - offset;
  va_list args;
  va_start(args, fmt);
  const int written = chvsnprintf(buf + offset, remaining, fmt, args);
  va_end(args);

  // chvsnprintf reports the requested length, excluding the terminating NUL.
  if (written < 0 || static_cast<size_t>(written) >= remaining) {
    used = -1;
    return;
  }
  used += written;
}

int SaboBmsDriver::BuildDeviceInfo(char* out, size_t capacity) const {
  if (out == nullptr || capacity == 0U) return -1;
  out[0] = '\0';
  if (!IsPresent()) return 0;

  int chars = 0;
  // An SBS text block holds up to 32 bytes, each of which can expand to \u00XX.
  char esc_str[6U * (sizeof(info_.mfr_name) - 1U) + 1U];

  EscapeStringInto(info_.mfr_name, esc_str, sizeof(esc_str));
  AppendJson(out, capacity, chars, "{\"mfr_name\":\"%s\"", esc_str);

  // Manufacturer date: already formatted as YYYY-MM-DD.
  AppendJson(out, capacity, chars, ",\"mfr_date\":\"%s\"", info_.mfr_date);

  EscapeStringInto(info_.dev_name, esc_str, sizeof(esc_str));
  AppendJson(out, capacity, chars, ",\"dev_name\":\"%s\"", esc_str);

  EscapeStringInto(info_.dev_version, esc_str, sizeof(esc_str));
  AppendJson(out, capacity, chars, ",\"dev_version\":\"%s\"", esc_str);

  EscapeStringInto(info_.dev_chemistry, esc_str, sizeof(esc_str));
  AppendJson(out, capacity, chars, ",\"dev_chemistry\":\"%s\"", esc_str);

  AppendJson(out, capacity, chars,
             ",\"serial_number\":%u,\"design_capacity_ah\":%.3f,\"design_voltage_v\":%.3f,\"cell_count\":%u}",
             (unsigned)info_.serial_number, (double)info_.design_capacity_ah, (double)info_.design_voltage_v,
             (unsigned)ClampCells(info_.cell_count));

  return chars;
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

  i2cAcquireBus(bms_cfg_->i2c);

  if (!debug::DumpSbsDevice(sbs_, cb, opt)) {
    i2cReleaseBus(bms_cfg_->i2c);
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

  i2cReleaseBus(bms_cfg_->i2c);

  return true;
}

msg_t SaboBmsDriver::I2cMasterTransmit(const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t rx_len) {
  chDbgAssert(bms_cfg_->i2c && bms_cfg_->i2c->mutex.owner == chThdGetSelfX(), "NEED TO OWN THE I2C");
  return xbot::i2c::TransmitWithRecovery(bms_cfg_->i2c, DEVICE_ADDRESS, tx, tx_len, rx, rx_len, "BMS");
}

msg_t SaboBmsDriver::ReadRegisterRaw(uint8_t reg, uint8_t* rx, size_t rx_len) {
  chDbgAssert(bms_cfg_->i2c && bms_cfg_->i2c->mutex.owner == chThdGetSelfX(), "NEED TO OWN THE I2C");
  if (bms_cfg_ == nullptr || bms_cfg_->i2c == nullptr || rx == nullptr || rx_len == 0) return MSG_RESET;

  // Single attempt: the I2C helper (xbot::i2c::TransmitWithRecovery) retries a
  // failed transfer a few times and recovers the bus afterwards, so retrying
  // here would only multiply the attempts (and the bus load while the pack is
  // absent).
  return I2cMasterTransmit(&reg, 1, rx, rx_len);
}

msg_t SaboBmsDriver::ReadRegister(uint8_t reg, uint8_t& result) {
  chDbgAssert(bms_cfg_->i2c && bms_cfg_->i2c->mutex.owner == chThdGetSelfX(), "NEED TO OWN THE I2C");
  uint8_t rx = 0;
  const msg_t msg = ReadRegisterRaw(reg, &rx, 1);
  if (msg == MSG_OK) result = rx;
  return msg;
}

msg_t SaboBmsDriver::ReadRegister(uint8_t reg, uint16_t& result) {
  chDbgAssert(bms_cfg_->i2c && bms_cfg_->i2c->mutex.owner == chThdGetSelfX(), "NEED TO OWN THE I2C");
  uint8_t rx[2] = {0, 0};
  const msg_t msg = ReadRegisterRaw(reg, rx, 2);
  if (msg == MSG_OK) result = (uint16_t)rx[0] | (uint16_t)((uint16_t)rx[1] << 8);
  return msg;
}

msg_t SaboBmsDriver::ReadRegister(uint8_t reg, int16_t& result) {
  chDbgAssert(bms_cfg_->i2c && bms_cfg_->i2c->mutex.owner == chThdGetSelfX(), "NEED TO OWN THE I2C");
  uint16_t u = 0;
  const msg_t msg = ReadRegister(reg, u);
  if (msg == MSG_OK) result = (int16_t)u;
  return msg;
}

msg_t SaboBmsDriver::ReadBlock(uint8_t cmd, uint8_t* data, size_t data_capacity, size_t& out_len) {
  chDbgAssert(bms_cfg_->i2c && bms_cfg_->i2c->mutex.owner == chThdGetSelfX(), "NEED TO OWN THE I2C");
  out_len = 0;
  if (!bms_cfg_ || !bms_cfg_->i2c || !data || !data_capacity) return MSG_RESET;

  // SMBus block read: device returns [len][data0][data1]...[data(len-1)]
  // Read the maximum (1 + 32) in one transaction.
  const size_t rx_max = etl::min((int)data_capacity, 33);

  // Single attempt (the I2C helper retries and recovers, see ReadRegisterRaw).
  memset(data, 0, rx_max);
  const msg_t msg = I2cMasterTransmit(&cmd, 1, data, rx_max);
  if (msg != MSG_OK) {
    return msg;
  }

  // Validate SMBus length byte.
  const uint8_t len_u8 = data[0];
  if (len_u8 > 32U) {
    return MSG_RESET;
  }
  const size_t len = (size_t)len_u8;

  // Ensure we actually received the whole payload in this single transaction.
  if ((1U + len) > rx_max) {
    return MSG_RESET;
  }

  // Shift payload in-place (data[0] was the length byte).
  if (len > 0U) {
    memmove(data, &data[1], len);
  }
  out_len = len;
  if (len < data_capacity) {
    data[len] = 0;
  }

  return MSG_OK;
}

void SaboBmsDriver::rtrim(char* str) {
  if (*str == 0) return;

  // Trailing spaces
  char* end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end)) end--;

  end[1] = '\0';
}

}  // namespace xbot::driver::bms
