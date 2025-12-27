/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sbs_debug.hpp"

#include <cstdio>

#include "sbs_protocol.hpp"

namespace xbot::driver::bms::debug {

namespace {

static void PrintLine(const SbsDebugCallbacks& cb, const char* line) {
  if (cb.print_line == nullptr) return;
  cb.print_line(cb.ctx, line);
}

static uint32_t GetI2cErrors(const SbsDebugCallbacks& cb) {
  if (cb.get_i2c_errors == nullptr) return 0;
  return cb.get_i2c_errors(cb.ctx);
}

static void AppendSignedCentiToBuf(char* dst, size_t dst_cap, int32_t centi) {
  const int32_t i = centi / 100;
  const int32_t f = (centi >= 0) ? (centi % 100) : (-(centi % 100));
  (void)std::snprintf(dst, dst_cap, " = %ld.%02ld", (long)i, (long)f);
}

static bool IsKnownSbsCmd(uint8_t cmd) {
  // Known SBS word commands 0x01..0x1C and block commands 0x20..0x23.
  if (cmd >= 0x01U && cmd <= 0x1CU) return true;
  if (cmd >= 0x20U && cmd <= 0x23U) return true;
  return false;
}

}  // namespace

bool DumpSbsDevice(const SbsProtocol& sbs, const SbsDebugCallbacks& cb, const SbsDebugOptions& opt) {
  // Probe device (minimal sanity): Voltage or BatteryStatus should return something non-zero.
  {
    uint16_t v = 0;
    const msg_t msg_v = sbs.ReadWord(SbsProtocol::Command::Voltage, v);
    if (!(msg_v == MSG_OK && v > 0U)) {
      uint16_t st = 0;
      const msg_t msg_st = sbs.ReadWord(SbsProtocol::Command::BatteryStatus, st);
      if (!(msg_st == MSG_OK && st != 0U)) {
        char line[160];
        std::snprintf(line, sizeof(line), "SBS probe failed msg_v=%d msg_st=%d i2c_err=0x%08lX\r\n", (int)msg_v,
                      (int)msg_st, (unsigned long)GetI2cErrors(cb));
        PrintLine(cb, line);
        return false;
      }
    }
  }

  // Known (and save) SBS word commands.
  {
    constexpr SbsProtocol::Command kWordCommands[] = {
        SbsProtocol::Command::RemainingCapacityAlarm,
        SbsProtocol::Command::RemainingTimeAlarm,
        SbsProtocol::Command::BatteryMode,
        SbsProtocol::Command::AtRate,
        SbsProtocol::Command::AtRateTimeToFull,
        SbsProtocol::Command::AtRateTimeToEmpty,
        SbsProtocol::Command::AtRateOK,
        SbsProtocol::Command::Temperature,
        SbsProtocol::Command::Voltage,
        SbsProtocol::Command::Current,
        SbsProtocol::Command::AverageCurrent,
        SbsProtocol::Command::MaxError,
        SbsProtocol::Command::RelativeStateOfCharge,
        SbsProtocol::Command::AbsoluteStateOfCharge,
        SbsProtocol::Command::RemainingCapacity,
        SbsProtocol::Command::FullChargeCapacity,
        SbsProtocol::Command::RunTimeToEmpty,
        SbsProtocol::Command::AverageTimeToEmpty,
        SbsProtocol::Command::AverageTimeToFull,
        SbsProtocol::Command::ChargingCurrent,
        SbsProtocol::Command::ChargingVoltage,
        SbsProtocol::Command::BatteryStatus,
        SbsProtocol::Command::CycleCount,
        SbsProtocol::Command::DesignCapacity,
        SbsProtocol::Command::DesignVoltage,
        SbsProtocol::Command::SpecificationInfo,
        SbsProtocol::Command::ManufacturerDate,
        SbsProtocol::Command::SerialNumber,
    };

    for (const auto cmd : kWordCommands) {
      const uint8_t cmd_u8 = (uint8_t)cmd;
      const auto r = sbs.ReadWordWithBatteryStatus(cmd);
      const uint32_t err = GetI2cErrors(cb);

      char line[240];
      if (r.ok) {
        char extra[96]{};
        switch (cmd) {
          case SbsProtocol::Command::Temperature: {
            // SBS temperature is typically 0.1 Kelvin.
            const int32_t centi_c = (int32_t)r.value * 10 - 27315;
            char tmp[48]{};
            AppendSignedCentiToBuf(tmp, sizeof(tmp), centi_c);
            std::snprintf(extra, sizeof(extra), "%s C", tmp);
          } break;

          case SbsProtocol::Command::Voltage:
          case SbsProtocol::Command::ChargingVoltage:
          case SbsProtocol::Command::DesignVoltage: {
            const uint32_t mv = (uint32_t)r.value;
            std::snprintf(extra, sizeof(extra), " = %lu.%03lu V", (unsigned long)(mv / 1000U),
                          (unsigned long)(mv % 1000U));
          } break;

          case SbsProtocol::Command::Current:
          case SbsProtocol::Command::AverageCurrent:
          case SbsProtocol::Command::AtRate:
          case SbsProtocol::Command::ChargingCurrent: {
            const int16_t ma = (int16_t)r.value;
            const int32_t ma_10 = (int32_t)ma * 10;
            std::snprintf(extra, sizeof(extra), " = %d mA = %ld mA@10mA/LSB", (int)ma, (long)ma_10);
          } break;

          case SbsProtocol::Command::RemainingCapacity:
          case SbsProtocol::Command::FullChargeCapacity:
          case SbsProtocol::Command::DesignCapacity:
          case SbsProtocol::Command::RemainingCapacityAlarm: {
            std::snprintf(extra, sizeof(extra), " = %u mAh", (unsigned)r.value);
          } break;

          case SbsProtocol::Command::RemainingTimeAlarm:
          case SbsProtocol::Command::AtRateTimeToFull:
          case SbsProtocol::Command::AtRateTimeToEmpty:
          case SbsProtocol::Command::RunTimeToEmpty:
          case SbsProtocol::Command::AverageTimeToEmpty:
          case SbsProtocol::Command::AverageTimeToFull: {
            const uint16_t minutes = r.value;
            std::snprintf(extra, sizeof(extra), " = %uh%02um", (unsigned)(minutes / 60U), (unsigned)(minutes % 60U));
          } break;

          case SbsProtocol::Command::BatteryStatus: {
            const auto ec = SbsProtocol::BatteryStatusToErrorCode(r.value);
            std::snprintf(extra, sizeof(extra), " ec=%s", SbsProtocol::GetErrorCodeStr(ec).data());
          } break;

          case SbsProtocol::Command::ManufacturerDate: {
            const uint16_t raw = r.value;
            const uint16_t day = (uint16_t)(raw & 0x1FU);
            const uint16_t month = (uint16_t)((raw >> 5) & 0x0FU);
            const uint16_t year = (uint16_t)(1980U + ((raw >> 9) & 0x7FU));
            std::snprintf(extra, sizeof(extra), " = %04u-%02u-%02u", (unsigned)year, (unsigned)month, (unsigned)day);
          } break;

          default: break;
        }

        std::snprintf(line, sizeof(line), "  0x%02X %-16s ACK  = 0x%04X (%u)%s\r\n", (unsigned)cmd_u8,
                      SbsProtocol::GetCommandStr(cmd).data(), (unsigned)r.value, (unsigned)r.value, extra);
      } else {
        if (r.battery_status_valid) {
          const auto ec = SbsProtocol::BatteryStatusToErrorCode(r.battery_status);
          std::snprintf(line, sizeof(line), "  0x%02X %-16s NACK msg=%d i2c_err=0x%08lX  BattStatus=0x%04X ec=%s\r\n",
                        (unsigned)cmd_u8, SbsProtocol::GetCommandStr(cmd).data(), (int)r.msg, (unsigned long)err,
                        (unsigned)r.battery_status, SbsProtocol::GetErrorCodeStr(ec).data());
        } else {
          std::snprintf(line, sizeof(line), "  0x%02X %-16s NACK msg=%d i2c_err=0x%08lX\r\n", (unsigned)cmd_u8,
                        SbsProtocol::GetCommandStr(cmd).data(), (int)r.msg, (unsigned long)err);
        }
      }
      PrintLine(cb, line);
    }
  }

  // Known SBS block commands.
  {
    constexpr SbsProtocol::Command kBlockCommands[] = {
        SbsProtocol::Command::ManufacturerName,
        SbsProtocol::Command::DeviceName,
        SbsProtocol::Command::DeviceChemistry,
        SbsProtocol::Command::ManufacturerData,
    };

    uint8_t buf[33]{};
    for (const auto cmd : kBlockCommands) {
      size_t len = 0;
      const msg_t msg = sbs.ReadBlock(cmd, buf, sizeof(buf), len);
      const uint32_t err = GetI2cErrors(cb);
      const uint8_t cmd_u8 = (uint8_t)cmd;

      char line[320];
      if (msg != MSG_OK) {
        std::snprintf(line, sizeof(line), "  0x%02X %-16s NACK msg=%d i2c_err=0x%08lX\r\n", (unsigned)cmd_u8,
                      SbsProtocol::GetCommandStr(cmd).data(), (int)msg, (unsigned long)err);
        PrintLine(cb, line);
        continue;
      }

      // Render as ASCII (printable only) plus hex.
      char ascii[34]{};
      const size_t n = (len < 33U) ? len : 33U;
      for (size_t i = 0; i < n; i++) {
        const uint8_t c = buf[i];
        ascii[i] = (c >= 32 && c <= 126) ? (char)c : '.';
      }
      ascii[n] = '\0';

      // Hex dump (truncate to keep line bounded).
      char hex[3 * 16 + 1]{};
      const size_t hex_n = (n < 16U) ? n : 16U;
      size_t hex_pos = 0;
      for (size_t i = 0; i < hex_n && (hex_pos + 3) < sizeof(hex); i++) {
        const int w = std::snprintf(&hex[hex_pos], sizeof(hex) - hex_pos, "%02X ", (unsigned)buf[i]);
        if (w <= 0) break;
        hex_pos += (size_t)w;
      }

      std::snprintf(line, sizeof(line), "  0x%02X %-16s ACK  len=%u \"%s\"  hex: %s\r\n", (unsigned)cmd_u8,
                    SbsProtocol::GetCommandStr(cmd).data(), (unsigned)len, ascii, hex);
      PrintLine(cb, line);
    }
  }

  if (!opt.include_cmd_scan) {
    return true;
  }

  // Capability scan: word-read every command byte 0x00..0xFF (skip known block cmds 0x20..0x23).
  {
    PrintLine(cb, "BMS cmd scan (word read) 0x00..0xFF: ++=ACK --=NACK BB=block\r\n");

    uint8_t ok[256]{};
    uint16_t val[256]{};

    char line[256];
    for (uint16_t base = 0U; base <= 0xF0U; base += 0x10U) {
      int pos = std::snprintf(line, sizeof(line), "0x%02X:", (unsigned)base);
      for (uint16_t j = 0U; j < 0x10U; j++) {
        const uint8_t cmd = (uint8_t)(base + j);
        if (cmd >= 0x20U && cmd <= 0x23U) {
          pos += std::snprintf(&line[pos], sizeof(line) - (size_t)pos, " BB");
          ok[cmd] = 2U;
          continue;
        }

        uint16_t v = 0;
        const msg_t msg = sbs.ReadWordRaw(cmd, v);
        const bool ack = (msg == MSG_OK);
        ok[cmd] = ack ? 1U : 0U;
        val[cmd] = v;
        pos += std::snprintf(&line[pos], sizeof(line) - (size_t)pos, ack ? " ++" : " --");
      }
      std::snprintf(&line[pos], sizeof(line) - (size_t)pos, "\r\n");
      PrintLine(cb, line);
    }

    if (opt.list_unknown_nonzero) {
      PrintLine(cb, "BMS cmd scan: non-zero ACK (unknown cmds)\r\n");
      for (uint16_t cmd_u16 = 0U; cmd_u16 <= 0xFFU; cmd_u16++) {
        const uint8_t cmd = (uint8_t)cmd_u16;
        if (ok[cmd] != 1U) continue;
        if (IsKnownSbsCmd(cmd)) continue;
        if (opt.suppress_cmds_0x3c_0x42 && (cmd >= 0x3CU && cmd <= 0x42U)) continue;
        const uint16_t v = val[cmd];
        if (v == 0U) continue;

        char l[200];
        std::snprintf(l, sizeof(l), "  0x%02X <unknown> ACK  = 0x%04X (%u)\r\n", (unsigned)cmd, (unsigned)v,
                      (unsigned)v);
        PrintLine(cb, l);
      }
    }
  }

  return true;
}

}  // namespace xbot::driver::bms::debug
