/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2026 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SETUP_INPUT_DRIVER_HPP
#define SETUP_INPUT_DRIVER_HPP

#include <hal.h>

#include "input_driver.hpp"

namespace xbot::driver::input {

class SetupDriver : public InputDriver {
  using InputDriver::InputDriver;

 public:
  bool OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                          Input& input) override;
  bool OnStart() override;
};

}  // namespace xbot::driver::input

#endif  // SETUP_INPUT_DRIVER_HPP
