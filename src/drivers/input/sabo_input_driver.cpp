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

#include <cstdint>
#include <globals.hpp>
#include <json_stream.hpp>
#include <robots/include/sabo_common.hpp>
#include <robots/include/sabo_robot.hpp>

namespace xbot::driver::input {

using namespace xbot::driver::sabo::types;

// Static member definition
volatile uint16_t SaboInputDriver::heartbeat_counter_{0};

// Static lookup tables for input configuration
static const etl::flat_map<etl::string<6>, types::InputType, 2> INPUT_TYPES = {
    {"sensor", types::InputType::SENSOR},
    {"button", types::InputType::BUTTON},
};

static const etl::flat_map<etl::string<9>, types::SensorId, 4> SENSOR_IDS = {
    {"lift_fl", types::SensorId::LIFT_FL},
    {"lift_fr", types::SensorId::LIFT_FR},
    {"stop_top", types::SensorId::STOP_TOP},
    {"stop_rear", types::SensorId::STOP_REAR},
};

static const etl::flat_map<etl::string<6>, types::ButtonId, 12> BUTTON_IDS = {
    {"up", types::ButtonId::UP},
    {"down", types::ButtonId::DOWN},
    {"left", types::ButtonId::LEFT},
    {"right", types::ButtonId::RIGHT},
    {"ok", types::ButtonId::OK},
    {"play", types::ButtonId::PLAY},
    {"select", types::ButtonId::S1_SELECT},
    {"menu", types::ButtonId::MENU},
    {"back", types::ButtonId::BACK},
    {"auto", types::ButtonId::S2_AUTO},
    {"mow", types::ButtonId::S2_MOW},
    {"home", types::ButtonId::S2_HOME},
};

SaboInputDriver::SaboInputDriver(const config::HardwareConfig& hardware_config) : hardware_config(hardware_config) {
  // Setup GPIO for regular sensors
  for (const auto& sensor : hardware_config.sensors) {
    palSetLineMode(sensor.line, PAL_MODE_INPUT);
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
  if (idx < hardware_config.sensors.size()) {
    const auto& sensor = hardware_config.sensors[idx];
    return (palReadLine(sensor.line) == PAL_HIGH) ^ sensor.invert;
  }

  // Handle STOP_REAR heartbeat sensor
  if (sensor_id == SensorId::STOP_REAR) {
    // Heartbeat pulse stop when pressed (button pressed = no pulses)
    return (heartbeat_last_ < heartbeat_min_);
  }

  return false;  // Invalid sensor index
}

void SaboInputDriver::Tick() {
  uint16_t buttons_mask = 0;

  if (robot != nullptr) {
    auto* sabo_robot = static_cast<SaboRobot*>(robot);
    buttons_mask = sabo_robot->GetCoverUIButtonsMask();
  }

  for (auto& input : Inputs()) {
    switch (input.sabo.type) {
      case InputType::SENSOR: {
        bool sensor_state = GetSensorState(input.sabo.id.sensor_id);
        input.Update(sensor_state);
        break;
      }
      case InputType::BUTTON:
        if (!block_buttons_) {
          bool button_pressed = (buttons_mask & (1 << static_cast<uint8_t>(input.sabo.id.button_id)));
          input.Update(button_pressed);
        }
        break;
    }
  }
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
  if (strcmp(key, "type") == 0) {
    JsonExpectType(STRING);
    decltype(INPUT_TYPES)::key_type input_type{jsp->data.str.buff};
    auto type_it = INPUT_TYPES.find(input_type);
    if (type_it == INPUT_TYPES.end()) {
      ULOG_ERROR("Unknown Sabo input type \"%s\"", input_type.c_str());
      return false;
    }
    input.sabo.type = type_it->second;
    return true;
  } else if (strcmp(key, "id") == 0) {
    JsonExpectType(STRING);
    switch (input.sabo.type) {
      case InputType::SENSOR: {
        decltype(SENSOR_IDS)::key_type sensor_id{jsp->data.str.buff};
        auto sensor_it = SENSOR_IDS.find(sensor_id);
        if (sensor_it == SENSOR_IDS.end()) {
          ULOG_ERROR("Unknown Sabo sensor ID \"%s\"", sensor_id.c_str());
          return false;
        }
        input.sabo.id.sensor_id = sensor_it->second;
        return true;
      }
      case InputType::BUTTON: {
        decltype(BUTTON_IDS)::key_type button_id{jsp->data.str.buff};
        auto button_it = BUTTON_IDS.find(button_id);
        if (button_it == BUTTON_IDS.end()) {
          ULOG_ERROR("Unknown Sabo button ID \"%s\"", button_id.c_str());
          return false;
        }
        input.sabo.id.button_id = button_it->second;
        return true;
      }
      default: ULOG_ERROR("Sabo input type not set before ID"); return false;
    }
    return false;
  }

  ULOG_ERROR("SaboInputDriver: Unknown config key '%s'", key);
  return false;
}

}  // namespace xbot::driver::input
