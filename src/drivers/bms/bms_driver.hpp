/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file bms_driver.hpp
 * @brief Base class for Battery Management System drivers
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-12-18
 */

#ifndef BMS_DRIVER_HPP
#define BMS_DRIVER_HPP

namespace xbot::driver::bms {

/**
 * @brief Base class for BMS drivers
 *
 * Provides a minimal interface for battery management system access.
 * Implementations should handle chip-specific communication and data parsing.
 */
class BmsDriver {
 public:
  virtual ~BmsDriver() = default;

  virtual bool Init() = 0;

  virtual void Tick() = 0;
};

}  // namespace xbot::driver::bms

#endif  // BMS_DRIVER_HPP
