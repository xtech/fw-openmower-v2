/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file gpio_expander.hpp
 * @brief Generic GPIO Expander Driver Interface
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-02-22
 */

#ifndef GPIO_EXPANDER_DRIVER_HPP
#define GPIO_EXPANDER_DRIVER_HPP

#include <cstdint>

namespace xbot::driver::gpio {

/**
 * @brief Pin direction mode
 */
enum class PinMode { OUTPUT = 0, INPUT };

class GpioExpanderDriver {
 public:
  virtual ~GpioExpanderDriver() = default;
  virtual bool Init() = 0;
  virtual bool IsPresent() const {
    return present_;
  }
  virtual bool ReadPort(uint16_t* value) = 0;
  virtual bool WritePort(uint16_t value) = 0;
  virtual bool WritePin(uint8_t pin, bool value) = 0;    // Set/clear a single pin
  virtual bool ConfigPort(uint16_t direction_mask) = 0;  // Set port direction

 protected:
  bool present_ = false;
};

}  // namespace xbot::driver::gpio

#endif  // GPIO_EXPANDER_DRIVER_HPP
