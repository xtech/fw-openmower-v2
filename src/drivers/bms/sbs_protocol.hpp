/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sbs_protocol.hpp
 * @brief SBS protocol helper implementation
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-12-27
 */

#ifndef SBS_PROTOCOL_HPP
#define SBS_PROTOCOL_HPP

// clang-format off
#include <ch.h>   // Includes chconf.h which defines CHPRINTF_USE_FLOAT
#include <hal.h>  // Defines BaseSequentialStream
#include <chprintf.h>
// clang-format on
#include <etl/array.h>
#include <etl/delegate.h>
#include <etl/string_view.h>

#include <cstddef>
#include <cstdint>

namespace xbot::driver::bms {

// Minimal SBS/SMBus protocol helper as of sbdat110 specification.
//
// This class intentionally does NOT know about the physical bus.
// The driver binds the low-level read primitives via delegates.
//
class SbsProtocol {
 public:
  enum class Command : uint8_t {
    // SBS standard word commands
    RemainingCapacityAlarm = 0x01,
    RemainingTimeAlarm = 0x02,
    BatteryMode = 0x03,
    AtRate = 0x04,
    AtRateTimeToFull = 0x05,
    AtRateTimeToEmpty = 0x06,
    AtRateOK = 0x07,

    Temperature = 0x08,
    Voltage = 0x09,
    Current = 0x0A,
    AverageCurrent = 0x0B,
    MaxError = 0x0C,
    RelativeStateOfCharge = 0x0D,
    AbsoluteStateOfCharge = 0x0E,
    RemainingCapacity = 0x0F,
    FullChargeCapacity = 0x10,
    RunTimeToEmpty = 0x11,
    AverageTimeToEmpty = 0x12,
    AverageTimeToFull = 0x13,
    ChargingCurrent = 0x14,
    ChargingVoltage = 0x15,
    BatteryStatus = 0x16,
    CycleCount = 0x17,
    DesignCapacity = 0x18,
    DesignVoltage = 0x19,
    SpecificationInfo = 0x1A,
    ManufacturerDate = 0x1B,
    SerialNumber = 0x1C,

    // SBS standard block commands
    ManufacturerName = 0x20,
    DeviceName = 0x21,
    DeviceChemistry = 0x22,
    ManufacturerData = 0x23,
  };

  // LSB 0-3 of BatteryStatus are an error code (4-bit value), per SBS.
  enum class ErrorCode : uint8_t {
    Ok = 0,
    Busy,
    ReservedCommand,
    UnsupportedCommand,
    AccessDenied,
    DataXFlow,
    BadSize,
    Unknown
  };

  enum class BatteryStatusBit : uint8_t {
    StatusFullyDischarged = 4,
    StatusFullyCharged = 5,
    StatusDischarging = 6,
    StatusInitialized = 7,
    AlarmRemainingTime = 8,
    AlarmRemainingCapacity = 9,
    AlarmReserved1 = 10,
    AlarmTerminateDischarge = 11,
    AlarmOverTemperature = 12,
    AlarmReserved2 = 13,
    AlarmTerminateCharge = 14,
    AlarmOverCharged = 15,
  };

  static ErrorCode BatteryStatusToErrorCode(uint16_t battery_status) {
    const uint8_t ec = (uint8_t)(battery_status & 0x0FU);
    if (ec <= (uint8_t)ErrorCode::BadSize) {
      return static_cast<ErrorCode>(ec);
    }
    return ErrorCode::Unknown;
  }

  static void SetBatteryStatus(uint16_t& battery_status, BatteryStatusBit bit) {
    battery_status |= (uint16_t)(1U << (uint8_t)bit);
    return;
  }

  static void ResetBatteryStatus(uint16_t& battery_status, BatteryStatusBit bit) {
    battery_status &= ~(1U << (uint8_t)bit);
    return;
  }

  static etl::string_view GetCommandStr(Command cmd) {
    struct Entry {
      Command cmd;
      etl::string_view name;
    };

    static constexpr Entry kMap[] = {
        {Command::RemainingCapacityAlarm, "RemCapAlarm"},
        {Command::RemainingTimeAlarm, "RemTimeAlarm"},
        {Command::BatteryMode, "BatteryMode"},
        {Command::AtRate, "AtRate"},
        {Command::AtRateTimeToFull, "AtRateToFull"},
        {Command::AtRateTimeToEmpty, "AtRateToEmpty"},
        {Command::AtRateOK, "AtRateOK"},

        {Command::Temperature, "Temperature"},
        {Command::Voltage, "Voltage"},
        {Command::Current, "Current"},
        {Command::AverageCurrent, "AvgCurrent"},
        {Command::MaxError, "MaxError"},
        {Command::RelativeStateOfCharge, "RelSoC"},
        {Command::AbsoluteStateOfCharge, "AbsSoC"},
        {Command::RemainingCapacity, "RemCap"},
        {Command::FullChargeCapacity, "FullChgCap"},
        {Command::RunTimeToEmpty, "RunTimeEmpty"},
        {Command::AverageTimeToEmpty, "AvgTimeEmpty"},
        {Command::AverageTimeToFull, "AvgTimeFull"},
        {Command::ChargingCurrent, "ChgCurrent"},
        {Command::ChargingVoltage, "ChgVolt"},
        {Command::BatteryStatus, "BattStatus"},
        {Command::CycleCount, "CycleCount"},
        {Command::DesignCapacity, "DesignCap"},
        {Command::DesignVoltage, "DesignVolt"},
        {Command::SpecificationInfo, "SpecInfo"},
        {Command::ManufacturerDate, "MfrDate"},
        {Command::SerialNumber, "Serial"},

        {Command::ManufacturerName, "ManufacturerName"},
        {Command::DeviceName, "DeviceName"},
        {Command::DeviceChemistry, "DeviceChemistry"},
        {Command::ManufacturerData, "ManufacturerData"},
    };

    for (const auto& e : kMap) {
      if (e.cmd == cmd) return e.name;
    }
    return "<unknown>";
  }

  static etl::string_view GetErrorCodeStr(ErrorCode ec) {
    static constexpr etl::array<etl::string_view, 8> kMap = {
        "Ok", "Busy", "ReservedCommand", "UnsupportedCommand", "AccessDenied", "DataXFlow", "BadSize", "Unknown",
    };

    const uint8_t idx = (uint8_t)ec;
    if (idx < kMap.size()) return kMap[idx];
    return "Unknown";
  }

  struct ReadWordResult {
    msg_t msg{MSG_RESET};
    uint16_t value{0};
    bool ok{false};

    bool battery_status_valid{false};
    msg_t battery_status_msg{MSG_RESET};
    uint16_t battery_status{0};
  };

  using ReadByteFn = etl::delegate<msg_t(uint8_t cmd, uint8_t& out)>;
  using ReadWordFn = etl::delegate<msg_t(uint8_t cmd, uint16_t& out)>;
  using ReadInt16Fn = etl::delegate<msg_t(uint8_t cmd, int16_t& out)>;
  using ReadBlockFn = etl::delegate<msg_t(uint8_t cmd, uint8_t* data, size_t data_capacity, size_t& out_len)>;

  SbsProtocol() = default;

  SbsProtocol(ReadByteFn read_byte, ReadWordFn read_word, ReadInt16Fn read_int16, ReadBlockFn read_block)
      : read_byte_(read_byte), read_word_(read_word), read_int16_(read_int16), read_block_(read_block) {
  }

  void Bind(ReadByteFn read_byte, ReadWordFn read_word, ReadInt16Fn read_int16, ReadBlockFn read_block) {
    read_byte_ = read_byte;
    read_word_ = read_word;
    read_int16_ = read_int16;
    read_block_ = read_block;
  }

  msg_t ReadByte(Command cmd, uint8_t& out) const {
    if (!read_byte_.is_valid()) return MSG_RESET;
    return read_byte_(static_cast<uint8_t>(cmd), out);
  }

  msg_t ReadWord(Command cmd, uint16_t& out) const {
    if (!read_word_.is_valid()) return MSG_RESET;
    return read_word_(static_cast<uint8_t>(cmd), out);
  }

  // Raw primitive for debugging/scanning: allows reading arbitrary command bytes.
  // Prefer ReadWord(Command, ...) in normal code.
  msg_t ReadWordRaw(uint8_t cmd, uint16_t& out) const {
    if (!read_word_.is_valid()) return MSG_RESET;
    return read_word_(cmd, out);
  }

  msg_t ReadInt16(Command cmd, int16_t& out) const {
    if (!read_int16_.is_valid()) return MSG_RESET;
    return read_int16_(static_cast<uint8_t>(cmd), out);
  }

  msg_t ReadBlock(Command cmd, uint8_t* data, size_t data_capacity, size_t& out_len) const {
    if (!read_block_.is_valid()) return MSG_RESET;
    return read_block_(static_cast<uint8_t>(cmd), data, data_capacity, out_len);
  }

  // Convenience helper: read a word command and on error try to read BatteryStatus.
  // This is useful for debugging "why did a read fail" without scattering fallback code.
  ReadWordResult ReadWordWithBatteryStatus(Command cmd) const {
    ReadWordResult r{};

    uint16_t v = 0;
    const msg_t msg = ReadWord(cmd, v);
    r.msg = msg;
    r.value = v;
    r.ok = (msg == MSG_OK);

    if (!r.ok && cmd != Command::BatteryStatus) {
      uint16_t st = 0;
      r.battery_status_msg = ReadWord(Command::BatteryStatus, st);
      r.battery_status_valid = (r.battery_status_msg == MSG_OK);
      r.battery_status = st;
    }

    return r;
  }

  static constexpr bool IsLeapYear(uint16_t year) {
    return ((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U));
  }

  static uint8_t DaysInMonth(uint16_t year, uint8_t month) {
    static constexpr uint8_t kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1U || month > 12U) return 0;
    if (month == 2U && IsLeapYear(year)) return 29;
    return kDays[month - 1U];
  }

  static bool SbsDateToYMDString(const uint16_t sbs_date, char* out_buf, size_t out_buf_size) {
    if (out_buf == nullptr || out_buf_size < 11) return false;

    const uint16_t day = (uint16_t)(sbs_date & 0x1FU);
    const uint16_t month = (uint16_t)((sbs_date >> 5) & 0x0FU);
    const uint16_t year = (uint16_t)(1980U + ((sbs_date >> 9) & 0x7FU));

    const uint8_t dim = DaysInMonth(year, (uint8_t)month);
    if (month < 1U || month > 12U || day < 1U || dim == 0U || day > (uint16_t)dim) {
      out_buf[0] = '\0';
      return false;
    }

    (void)chsnprintf(out_buf, out_buf_size, "%04u-%02u-%02u", (unsigned)year, (unsigned)month, (unsigned)day);
    return true;
  }

 private:
  ReadByteFn read_byte_{};
  ReadWordFn read_word_{};
  ReadInt16Fn read_int16_{};
  ReadBlockFn read_block_{};
};

}  // namespace xbot::driver::bms

#endif  // SBS_PROTOCOL_HPP
