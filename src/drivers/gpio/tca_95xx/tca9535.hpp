/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file tca9535.hpp
 * @brief TCA9535 I2C GPIO Expander Driver
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-02-20
 */

#ifndef TCA9535_HPP
#define TCA9535_HPP

#include <cstdint>

#include "tca95xx.hpp"

namespace xbot::driver::gpio {

constexpr TCA95xxRegisterMap TCA9535 = {.input_port_reg = 0x00,   // Port 0
                                        .output_port_reg = 0x02,  // Port 0
                                        .config_reg = 0x06,       // Port 0
                                        .port_count = 2,
                                        .pin_count = 16};

/**
 * @brief TCA9535 Driver (16 GPIO pins)
 */
class TCA9535Driver : public TCA95xxDriver {
 public:
  explicit TCA9535Driver(const TCA95xxConfig* config) : TCA95xxDriver(config, &TCA9535) {
  }
};

}  // namespace xbot::driver::gpio

#endif  // TCA9535_HPP
