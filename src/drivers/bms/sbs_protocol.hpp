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
 * @date 2025-12-23
 */

#ifndef SBS_PROTOCOL_HPP
#define SBS_PROTOCOL_HPP

#include <ch.h>
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

  // LSB 0-3 of BatteryStatusBits are error codes
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

  static ErrorCode BatteryStatusToErrorCode(uint16_t battery_status) {
    const uint8_t ec = (uint8_t)(battery_status & 0x0FU);
    if (ec <= (uint8_t)ErrorCode::BadSize) {
      return static_cast<ErrorCode>(ec);
    }
    return ErrorCode::Unknown;
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

  enum class BatteryStatusBit : uint16_t {
    FullyDischarged = 1 << 4,
    FullyCharged = 1 << 5,
    Discharging = 1 << 6,
    Initialized = 1 << 7,
    AlarmTimeRemaining = 1 << 8,
    AlarmCapacityRemaining = 1 << 9,
    Reserved1 = 1 << 10,
    AlarmTerminateDischarge = 1 << 11,
    AlarmOverTemperature = 1 << 12,
    Reserved2 = 1 << 13,
    AlarmTerminateCharge = 1 << 14,
    AlarmOverCharged = 1 << 15
  };

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

  // Decode SBS ManufacturerDate (0x1B) raw value to epoch days since 1970-01-01 (UTC).
  // Returns false if the raw fields are invalid/out of range.
  static bool ManufacturerDateRawToEpochDays(uint16_t raw, uint16_t& out_days) {
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

  static uint64_t EpochDaysToUnixTimestamp(uint16_t epoch_days) {
    return (uint64_t)epoch_days * 86400ULL;
  }

 private:
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

  ReadByteFn read_byte_{};
  ReadWordFn read_word_{};
  ReadInt16Fn read_int16_{};
  ReadBlockFn read_block_{};
};

}  // namespace xbot::driver::bms

#endif  // SBS_PROTOCOL_HPP
