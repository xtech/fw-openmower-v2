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

  ULOG_ERROR("Unknown attribute \"%s\"", key);
  return false;
}

bool SetupDriver::OnStart() {
  // Configure each setup line as push-pull output and write its value,
  // e.g. setting a MUX selector before input drivers read their pins.
  for (auto& input : Inputs()) {
    palSetLineMode(input.setup.line, PAL_MODE_OUTPUT_PUSHPULL);
    palWriteLine(input.setup.line, input.setup.value);
  }
  return true;
}

}  // namespace xbot::driver::input
