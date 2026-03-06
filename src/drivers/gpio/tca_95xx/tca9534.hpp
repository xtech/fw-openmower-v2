/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file tca9534.hpp
 * @brief TCA9534 I2C GPIO Expander Driver
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-02-20
 */

#ifndef TCA9534_HPP
#define TCA9534_HPP

#include <cstdint>

#include "tca95xx.hpp"

namespace xbot::driver::gpio {

constexpr TCA95xxRegisterMap TCA9534 = {
    .input_port_reg = 0x00, .output_port_reg = 0x01, .config_reg = 0x03, .port_count = 1, .pin_count = 8};

/**
 * @brief TCA9534 Driver (8 GPIO pins)
 */
class TCA9534Driver : public TCA95xxDriver {
 public:
  explicit TCA9534Driver(const TCA95xxConfig* config) : TCA95xxDriver(config, &TCA9534) {
  }
};

}  // namespace xbot::driver::gpio

#endif  // TCA9534_HPP
