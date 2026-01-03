/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sbs_debug.hpp
 * @brief Generic SBS/SMBus debug dump helpers
 */

#ifndef SBS_DEBUG_HPP
#define SBS_DEBUG_HPP

#include <ch.h>

#include <cstddef>
#include <cstdint>

namespace xbot::driver::bms {

class SbsProtocol;

namespace debug {

struct SbsDebugCallbacks {
  void* ctx{nullptr};

  // Print a fully formatted line (already contains \r\n if desired).
  void (*print_line)(void* ctx, const char* line){nullptr};

  // Retrieve I2C error flags for additional diagnostics (optional).
  uint32_t (*get_i2c_errors)(void* ctx){nullptr};
};

struct SbsDebugOptions {
  bool include_cmd_scan{true};
  bool list_unknown_nonzero{true};
  bool suppress_cmds_0x3c_0x42{false};
};

bool DumpSbsDevice(const SbsProtocol& sbs, const SbsDebugCallbacks& cb, const SbsDebugOptions& opt = {});

}  // namespace debug

}  // namespace xbot::driver::bms

#endif  // SBS_DEBUG_HPP
