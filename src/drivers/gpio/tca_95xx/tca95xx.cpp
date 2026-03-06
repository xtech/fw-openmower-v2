/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file tca95xx.cpp
 * @brief TCA95xx I2C GPIO Expander Driver Implementation
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-02-22
 */

#include "tca95xx.hpp"

#include <cstring>

#include "ulog.h"

namespace xbot::driver::gpio {

// I2C retry settings
namespace {
constexpr unsigned kI2cRetries = 3;
constexpr unsigned kI2cRetryDelayMs = 2;
}  // namespace

bool TCA95xxDriver::Init() {
  if (config_ == nullptr) {
    ULOG_ERROR("TCA95xxDriver: Config not set");
    return false;
  }

  if (config_->i2c == nullptr) {
    ULOG_ERROR("TCA95xxDriver: I2C driver not set");
    return false;
  }

  if (reg_map_ == nullptr) {
    ULOG_ERROR("TCA95xxDriver: Register map not set");
    return false;
  }

  // Probe device by reading configuration register
  if (!ReadRegister(reg_map_->config_reg, &config_state_)) {
    ULOG_ERROR("TCA95xxDriver: Device not found at address 0x%02X", config_->address);
    return false;
  }

  present_ = true;

  ULOG_INFO("TCA95xxDriver: Found at I2C addr 0x%02X (ports: %u, pins: %u)\n", config_->address, reg_map_->port_count,
            reg_map_->pin_count);

  return true;
}

bool TCA95xxDriver::ReadRegister(uint8_t reg, uint16_t* value) {
  if (config_ == nullptr || config_->i2c == nullptr || value == nullptr || reg_map_ == nullptr) {
    return false;
  }

  i2cAcquireBus(config_->i2c);
  bool success = false;
  for (unsigned attempt = 0; attempt < kI2cRetries; attempt++) {
    uint8_t rx_data[reg_map_->port_count] = {};
    const msg_t msg = i2cMasterTransmit(config_->i2c, config_->address, &reg, 1, rx_data, reg_map_->port_count);
    if (msg == MSG_OK) {
      *value = 0;
      for (unsigned i = 0; i < reg_map_->port_count; i++) {
        *value |= static_cast<uint16_t>(rx_data[i]) << (i * 8);
      }
      success = true;
      break;
    }
    chThdSleepMilliseconds(kI2cRetryDelayMs);
  }
  i2cReleaseBus(config_->i2c);

  return success;
}

bool TCA95xxDriver::WriteRegister(uint8_t reg, uint16_t value) {
  if (config_ == nullptr || config_->i2c == nullptr || reg_map_ == nullptr) {
    return false;
  }

  i2cAcquireBus(config_->i2c);
  uint8_t num_tx_bytes = 1 + reg_map_->port_count;  // Register address + data bytes
  uint8_t tx[num_tx_bytes] = {reg, static_cast<uint8_t>(value & 0xFF)};
  if (reg_map_->port_count == 2) {
    tx[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
  }
  bool success = false;
  for (unsigned attempt = 0; attempt < kI2cRetries; attempt++) {
    const msg_t msg = i2cMasterTransmit(config_->i2c, config_->address, tx, num_tx_bytes, nullptr, 0);
    if (msg == MSG_OK) {
      success = true;
      break;
    }
    chThdSleepMilliseconds(kI2cRetryDelayMs);
  }

  i2cReleaseBus(config_->i2c);
  return success;
}

bool TCA95xxDriver::ReadPort(uint16_t* value) {
  if (!present_) {
    return false;
  }

  return ReadRegister(reg_map_->input_port_reg, value);
}

bool TCA95xxDriver::WritePort(uint16_t value) {
  if (!present_) {
    return false;
  }

  output_state_ = value;
  return WriteRegister(reg_map_->output_port_reg, value);
}

bool TCA95xxDriver::WritePin(uint8_t pin, bool value) {
  if (!present_) {
    return false;
  }

  // Check if pin is within valid range
  if (pin >= reg_map_->pin_count) {
    return false;
  }

  // Calculate mask for the pin
  uint16_t pin_mask = 1U << pin;

  // Update output state
  if (value) {
    output_state_ |= pin_mask;  // Set pin high
  } else {
    output_state_ &= ~pin_mask;  // Set pin low
  }

  // Write updated output state to hardware
  return WriteRegister(reg_map_->output_port_reg, output_state_);
}

bool TCA95xxDriver::ConfigPort(uint16_t direction_mask) {
  if (!present_) {
    return false;
  }

  config_state_ = direction_mask;
  return WriteRegister(reg_map_->config_reg, direction_mask);
}

}  // namespace xbot::driver::gpio
