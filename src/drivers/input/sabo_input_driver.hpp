/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sabo_input_driver.hpp
 * @brief Input driver for Sabo robot GPIO sensors
 *
 * Direct GPIO sensor reading without ROS configuration.
 * Supports configurable active-low/active-high sensors and heartbeat timeouts.
 * Works immediately on startup without waiting for ROS InputService configuration.
 *
 * @author Apehaenger <joerg@ebeling.ws>
 * @date 2025-11-14
 */

#ifndef SABO_INPUT_DRIVER_HPP
#define SABO_INPUT_DRIVER_HPP

#include <etl/array_view.h>
#include <hal.h>

#include <atomic>

#include "input_driver.hpp"
#include "services.hpp"

namespace xbot::driver::input {

/**
 * @brief Input driver for Sabo robot GPIO sensors
 *
 * Reads GPIO sensor states without waiting for ROS configuration.
 */
class SaboInputDriver : public InputDriver {
  using InputDriver::InputDriver;

 public:
  explicit SaboInputDriver();

  bool OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                          Input& input) override;

  bool GetSensorState(xbot::driver::sabo::types::SensorId sensor_id);

  uint16_t GetHeartbeatFrequency() const {
    // Convert count per interval to Hz (frequency)
    uint32_t interval_ms = TIME_I2MS(HEARTBEAT_CHECK_INTERVAL);
    return (heartbeat_last_ * 1000) / interval_ms;
  }

  /**
   * @brief Enable/disable button blocking for InputScreen button testing without triggering inputs
   * @param block true to block all buttons, false to allow normal operation
   */
  void SetBlockButtons(bool block) {
    block_buttons_ = block;
  }

 private:
  // Sensor configuration for normal hall sensors
  struct SaboGpioSensor {
    ioline_t line;        // GPIO line
    bool invert = false;  // true = sensor active on LOW, false = sensor active on HIGH
  };

  // Static Input objects for all inputs (sensors and buttons)
  // static Input inputs_[NUM_TOTAL_INPUTS];

  // Current sensor configuration
  etl::array_view<const SaboGpioSensor> sensors_;

  // Heartbeat sensor (STOP_REAR) - monitored via EXTI
  const ioline_t heartbeat_line_ = LINE_GPIO10;
  const uint16_t heartbeat_min_ = 10;           // Minimum heartbeats per tick cycle = 10 = 50Hz (@200ms interval)
  static volatile uint16_t heartbeat_counter_;  // ISR increments this (volatile for ISR safety)
  uint16_t heartbeat_last_ = 0;                 // Last stable reading (captured in timer callback)
  static constexpr systime_t HEARTBEAT_CHECK_INTERVAL = TIME_MS2I(200);  // 200ms interval
  virtual_timer_t heartbeat_timer_;  // Independent continuous timer for heartbeat monitoring

  // Button blocking for not sending button inputs during InputScreen button testing
  bool block_buttons_ = false;

  // ISR for heartbeat monitoring (static counter, thus object pointer unused)
  static void HeartbeatISR(void* arg);

  // Timer callback for continuous heartbeat handler (static, receives 'this' pointer)
  static void HeartbeatTimerCallback(virtual_timer_t* vtp, void* arg);

  void Tick();

  ServiceSchedule tick_schedule_{input_service, 20'000,
                                 XBOT_FUNCTION_FOR_METHOD(SaboInputDriver, &SaboInputDriver::Tick, this)};
};

}  // namespace xbot::driver::input

#endif  // SABO_INPUT_DRIVER_HPP
