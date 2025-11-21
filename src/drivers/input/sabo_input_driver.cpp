/*
 * OpenMower V2 Firmware
 * Part of the OpenMower V2 Firmware (https://github.com/xtech/fw-openmower-v2)
 *
 * Copyright (C) 2025 The OpenMower Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sabo_input_driver.hpp"

#include <ulog.h>

#include <globals.hpp>
#include <robots/include/sabo_robot.hpp>

namespace xbot::driver::input {

// Input SaboInputDriver::inputs_[SaboInputDriver::NUM_TOTAL_INPUTS];

// Static member definition
volatile uint16_t SaboInputDriver::heartbeat_counter_{0};

SaboInputDriver::SaboInputDriver() {
  // Define static Input objects
  static const SaboGpioSensor HW_V0_1[] = {
      {LINE_GPIO13},  // LIFT_FL
      {LINE_GPIO12},  // LIFT_FR
      {LINE_GPIO11},  // STOP_TOP
                      // STOP_REAR is handled separately as heartbeat sensor
  };

  static const SaboGpioSensor HW_V0_3[] = {
      {LINE_GPIO13, true},  // LIFT_FL
      {LINE_GPIO12, true},  // LIFT_FR
      {LINE_GPIO11, true},  // STOP_TOP
                            // STOP_REAR is handled separately as heartbeat sensor
  };

  // Initialize sensor definitions based on hardware version
  if (carrier_board_info.version_major == 0 && carrier_board_info.version_minor <= 2) {
    // HW v0.1
    auto size_v01 = sizeof(HW_V0_1) / sizeof(HW_V0_1[0]);
    sensors_ = etl::array_view<const SaboGpioSensor>(HW_V0_1, size_v01);
  } else {
    // HW v0.3 and later
    auto size_v03 = sizeof(HW_V0_3) / sizeof(HW_V0_3[0]);
    sensors_ = etl::array_view<const SaboGpioSensor>(HW_V0_3, size_v03);
  }

  // Setup GPIO for regular sensors
  for (size_t i = 0; i < sensors_.size(); ++i) {
    palSetLineMode(sensors_[i].line, PAL_MODE_INPUT);
  }

  // Setup EXTI for heartbeat sensor (STOP_REAR)
  if (heartbeat_line_ != PAL_NOLINE) {
    palSetLineMode(heartbeat_line_, PAL_MODE_INPUT);
    palSetLineCallback(heartbeat_line_, &SaboInputDriver::HeartbeatISR, nullptr);
    palEnableLineEvent(heartbeat_line_, PAL_EVENT_MODE_RISING_EDGE);
  }

  // Initialize and start continuous virtual timer for heartbeat monitoring
  chVTObjectInit(&heartbeat_timer_);
  chVTSetContinuous(&heartbeat_timer_, HEARTBEAT_CHECK_INTERVAL, HeartbeatTimerCallback, this);
}

bool SaboInputDriver::GetSensorState(SensorId sensor_id) {
  uint8_t idx = static_cast<uint8_t>(sensor_id);

  // Handle regular GPIO sensors (LIFT_FL, LIFT_FR, STOP_TOP)
  if (idx < sensors_.size()) {
    const auto& sensor = sensors_[idx];
    return (palReadLine(sensor.line) == PAL_HIGH) ^ sensor.invert;
  }

  // Handle STOP_REAR heartbeat sensor
  if (sensor_id == SensorId::STOP_REAR) {
    // Heartbeat pulse stop when pressed (button pressed = no pulses)
    return (heartbeat_last_ < heartbeat_min_);
  }

  return false;  // Invalid sensor index
}

bool SaboInputDriver::OnStart() {
  // Create Input objects for each GPIO sensor
  /*ULOG_INFO("SaboInputDriver: Initializing %u sensors", sensors_.size());
  ULOG_INFO("SaboInputDriver: sensors_ size = %u, NUM_GPIO_SENSORS = %u", sensors_.size(), NUM_GPIO_SENSORS);
  for (uint8_t i = 0; i < sensors_.size(); ++i) {
    const auto& sensor = sensors_[i];
    inputs_[i].idx = i;
    inputs_[i].gpio.line = sensor.line;
    inputs_[i].invert = sensor.invert;
    AddInput(&inputs_[i]);
    ULOG_INFO("SaboInputDriver: Added sensor [%u] %s", i, INPUTID_STRINGS[i]);
  }*/

  // Create Input objects for each CoverUI button
  /*using xbot::driver::ui::sabo::ALL_BUTTONS;
  using xbot::driver::ui::sabo::NUM_BUTTONS;
  ULOG_INFO("SaboInputDriver: Initializing %u buttons", NUM_BUTTONS);
  for (uint8_t btn_idx = 0; btn_idx < NUM_BUTTONS; ++btn_idx) {
    auto* input = new Input();
    input->idx = static_cast<uint8_t>(sensors_.size()) + btn_idx;  // Button indices start after sensors
    input->invert = false;                                         // Buttons are handled by CoverUI debouncing

    AddInput(input);
    auto button_id = static_cast<InputId>(input->idx);
    ULOG_INFO("SaboInputDriver: Added button [%u] %s", input->idx, SaboInputDriver::InputIdToString(button_id));
  }*/
  return true;
}

void SaboInputDriver::OnStop() {
  // Disable heartbeat EXTI
  palDisableLineEvent(heartbeat_line_);

  ClearInputs();
  sensors_ = etl::array_view<const SaboGpioSensor>();
}

void SaboInputDriver::Tick() {
  // Read GPIO sensor states and update Input objects
  /*for (auto& input : Inputs()) {
    if (input.idx >= sensors_.size()) {
      // This is a button input (handled below)
      continue;
    }

    const auto& sensor = sensors_[input.idx];
    bool new_state = ReadGpioSensor(sensor.line, sensor.invert);

    // Debug: Log sensor state changes
    if (new_state != input.IsActive()) {
      auto sensor_id = static_cast<InputId>(input.idx);
      ULOG_INFO("SaboInputDriver: Sensor[%u] (%s) -> %s", input.idx, InputIdToString(sensor_id),
                new_state ? "ACTIVE" : "INACTIVE");
    }

    input.Update(new_state);
  }*/

  // TODO: Use block_buttons_

  // Read CoverUI button states
  /*if (robot != nullptr) {
    auto* sabo_robot = static_cast<SaboRobot*>(robot);
    uint16_t buttons_mask = sabo_robot->GetCoverUIButtonsMask();

    using xbot::driver::ui::sabo::ALL_BUTTONS;
    using xbot::driver::ui::sabo::NUM_BUTTONS;

    for (uint8_t btn_idx = 0; btn_idx < NUM_BUTTONS; ++btn_idx) {
      auto button_id = ALL_BUTTONS[btn_idx];
      uint8_t input_idx = sensors_.size() + btn_idx;

      // Find the corresponding Input object
      for (auto& input : Inputs()) {
        if (input.idx == input_idx) {
          // Extract bit from button mask (button_id is the bit position)
          bool button_pressed = (buttons_mask & (1 << static_cast<uint8_t>(button_id))) != 0;
          input.Update(button_pressed);
          break;
        }
      }
    }
  }*/
}

void SaboInputDriver::HeartbeatISR(void* arg) {
  (void)arg;  // Unused - we use static counter for performance
  heartbeat_counter_++;
}

void SaboInputDriver::HeartbeatTimerCallback(virtual_timer_t* vtp, void* arg) {
  (void)vtp;  // Unused parameter, required by ChibiOS timer signature
  SaboInputDriver* driver = static_cast<SaboInputDriver*>(arg);
  if (!driver) return;

  // Read and reset the volatile counter
  uint16_t current = driver->heartbeat_counter_;
  driver->heartbeat_counter_ = 0;
  driver->heartbeat_last_ = current;

  // Note: No logging here - timer callback runs in ISR context
}

bool SaboInputDriver::OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                         Input& input) {
  (void)jsp;
  (void)type;
  (void)input;

  if (strcmp(key, "name") == 0) {
    // For now, we just log a warning that the name parameter is ignored
    ULOG_WARNING("SaboInputDriver: 'name' parameter is ignored (hardcoded in firmware)");
    return true;
  }

  ULOG_ERROR("SaboInputDriver: Unknown config key '%s'", key);
  return false;
}

}  // namespace xbot::driver::input
