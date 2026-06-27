/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "setup_driver.hpp"

#include <ulog.h>

#include <board_utils.hpp>
#include <globals.hpp>
#include <json_stream.hpp>

namespace xbot::driver::input {

bool SetupDriver::OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                     Input& input) {
  // Parse "line": GPIO pin name (e.g. "GPIO7")
  if (strcmp(key, "line") == 0) {
    JsonExpectType(STRING);
    input.setup.line = GetIoLineByName(jsp->data.str.buff);
    if (input.setup.line == PAL_NOLINE) {
      ULOG_ERROR("Unknown GPIO line \"%s\"", jsp->data.str.buff);
      return false;
    }
    return true;
  }

  // Parse "value": output level to write (0 or 1)
  if (strcmp(key, "value") == 0) {
    return JsonGetNumber(jsp, type, input.setup.value);
  }

  // Parse "id": driver-unique identifier (e.g. "hall_mux")
  if (strcmp(key, "id") == 0) {
    JsonExpectType(STRING);
    input.setup.is_hall_mux = (strcmp(jsp->data.str.buff, "hall_mux") == 0);
    return true;
  }

  ULOG_ERROR("Unknown attribute \"%s\"", key);
  return false;
}

bool SetupDriver::OnStart() {
  for (auto& input : Inputs()) {
    // Refuse to set MUX to OEM IDC on carrier boards that don't have it
    if (input.setup.is_hall_mux && input.setup.value != 0 &&
        (carrier_board_info.version_major < 1 ||
         (carrier_board_info.version_major == 1 && carrier_board_info.version_minor < 2))) {
      ULOG_ERROR("hall_mux set to OEM IDC but carrier board pre v1.2.0");
      return false;
    }

    palSetLineMode(input.setup.line, PAL_MODE_OUTPUT_PUSHPULL);
    palWriteLine(input.setup.line, input.setup.value);
  }
  return true;
}

}  // namespace xbot::driver::input
