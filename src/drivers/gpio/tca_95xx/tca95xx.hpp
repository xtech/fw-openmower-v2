/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file tca95x.hpp
 * @brief TCA9534/TCA9535 I2C GPIO Expander Driver base class and definitions
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-02-20
 */

#ifndef TCA95XX_HPP
#define TCA95XX_HPP

#include <cstdint>

#include "../gpio_expander.hpp"
#include "hal.h"

namespace xbot::driver::gpio {

struct TCA95xxConfig {
  I2CDriver* i2c;   // Pointer to ChibiOS I2C driver
  uint8_t address;  // I2C address
};

struct TCA95xxRegisterMap {
  uint8_t input_port_reg;
  uint8_t output_port_reg;
  uint8_t config_reg;
  uint8_t port_count;  // TCA9534=1, TCA9535=2
  uint8_t pin_count;   // TCA9534=8, TCA9535=16
};

/**
 * @brief TCA95x GPIO Expander Base Driver
 *
 * Supports TCA9534 (8-bit) and TCA9535 (16-bit) I2C GPIO expanders.
 */
class TCA95xxDriver : public GpioExpanderDriver {
 public:
  TCA95xxDriver(const TCA95xxConfig* config, const TCA95xxRegisterMap* reg_map)
      : config_(config), reg_map_(reg_map), output_state_(0), config_state_(0) {
  }

  bool Init() override;
  bool ReadPort(uint16_t* value) override;
  bool WritePort(uint16_t value) override;
  bool WritePin(uint8_t pin, bool value) override;
  bool ConfigPort(uint16_t direction_mask) override;

 protected:
  const TCA95xxConfig* config_;
  const TCA95xxRegisterMap* reg_map_;
  uint16_t output_state_;
  uint16_t config_state_;

  bool ReadRegister(uint8_t reg, uint16_t* value);
  bool WriteRegister(uint8_t reg, uint16_t value);
};

}  // namespace xbot::driver::gpio

#endif  // TCA95XX_HPP
