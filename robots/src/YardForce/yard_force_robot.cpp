//
// Created by clemens on 27.01.25.
//

#include <drivers/charger/bq_2576/bq_2576.hpp>
#include <globals.hpp>

#include "robot.hpp"
#include <drivers/emergency/gpio_emergency_driver.hpp>

namespace Robot {

static BQ2576 charger{};
static GPIOEmergencyDriver emergencyDriver{};

namespace General {
void InitPlatform() {
  emergencyDriver.AddInput({
      .gpio_line = LINE_EMERGENCY_1,
      .invert = true,
      .active_since = 0,
      .timeout_duration = TIME_MS2I(10),
      .active = false
  });
  emergencyDriver.AddInput({
      .gpio_line = LINE_EMERGENCY_2,
      .invert = true,
      .active_since = 0,
      .timeout_duration = TIME_MS2I(10),
      .active = false
  });
  emergencyDriver.AddInput({
      .gpio_line = LINE_EMERGENCY_3,
      .invert = true,
      .active_since = 0,
      .timeout_duration = TIME_MS2I(500),
      .active = false
  });
  emergencyDriver.AddInput({
      .gpio_line = LINE_EMERGENCY_4,
      .invert = true,
      .active_since = 0,
      .timeout_duration = TIME_MS2I(500),
      .active = false
  });
  emergencyDriver.Start();
}

bool IsHardwareSupported() {
  // Accept YardForce 1.x.x boards
  return strncmp("hw-openmower-yardforce", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      carrier_board_info.version_major == 1;
}

}  // namespace General

namespace GPS {
UARTDriver* GetUartPort() {
#ifndef STM32_UART_USE_USART6
#error STM32_SERIAL_USE_UART6 must be enabled for the YardForce build to work
#endif
  return &UARTD6;
}
}

namespace Power {

I2CDriver* GetPowerI2CD() {
  return &I2CD1;
}

Charger* GetCharger() {
  return &charger;
}

float GetMaxVoltage() {
  return 7.0f * 4.2f;
}

float GetChargeCurrent() {
  return 0.5;
}

float GetMinVoltage() {
  // 3.3V min, 7s pack
  return 7.0f * 3.3;
}

}  // namespace Power
}  // namespace Robot
