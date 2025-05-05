//
// Created by clemens on 27.01.25.
//

#include <drivers/motor/vesc/VescDriver.h>

#include <debug/debug_tcp_interface.hpp>
#include <drivers/charger/bq_2576/bq_2576.hpp>
#include <drivers/emergency/gpio_emergency_driver.hpp>
#include <globals.hpp>
#include <services.hpp>

#include "robot.hpp"

namespace Robot {

static BQ2576 charger{};
static GPIOEmergencyDriver emergencyDriver{};

static VescDriver left_motor_driver{};
static VescDriver right_motor_driver{};
static VescDriver mower_motor_driver{};

static DebugTCPInterface left_esc_driver_interface_{65102, &left_motor_driver};
static DebugTCPInterface mower_esc_driver_interface_{65103, &mower_motor_driver};
static DebugTCPInterface right_esc_driver_interface_{65104, &right_motor_driver};

namespace General {
void InitPlatform() {
  emergencyDriver.AddInput({.gpio_line = LINE_EMERGENCY_1,
                            .invert = true,
                            .active_since = 0,
                            .timeout_duration = TIME_MS2I(10),
                            .active = false});
  emergencyDriver.AddInput({.gpio_line = LINE_EMERGENCY_2,
                            .invert = true,
                            .active_since = 0,
                            .timeout_duration = TIME_MS2I(10),
                            .active = false});
  emergencyDriver.AddInput({.gpio_line = LINE_EMERGENCY_3,
                            .invert = true,
                            .active_since = 0,
                            .timeout_duration = TIME_MS2I(2000),
                            .active = false});
  emergencyDriver.AddInput({.gpio_line = LINE_EMERGENCY_4,
                            .invert = true,
                            .active_since = 0,
                            .timeout_duration = TIME_MS2I(2000),
                            .active = false});
  emergencyDriver.Start();

  left_motor_driver.SetUART(&UARTD1, 115200);
  right_motor_driver.SetUART(&UARTD4, 115200);
  mower_motor_driver.SetUART(&UARTD2, 115200);
  left_esc_driver_interface_.Start();
  right_esc_driver_interface_.Start();
  mower_esc_driver_interface_.Start();

  diff_drive.SetDrivers(&left_motor_driver, &right_motor_driver);
  mower_service.SetDriver(&mower_motor_driver);

  charger.setI2C(&I2CD1);
  power_service.SetDriver(&charger);
}

bool IsHardwareSupported() {
  // Accept YardForce 1.x.x boards
  if (strncmp("hw-openmower-yardforce", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      carrier_board_info.version_major == 1) {
    return true;
  }

  // Accept early testing boards
  if (strncmp("hw-xbot-devkit", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0) {
    return true;
  }

  return false;
}

}  // namespace General

namespace GPS {
UARTDriver* GetUartPort() {
#ifndef STM32_UART_USE_USART6
#error STM32_SERIAL_USE_UART6 must be enabled for the YardForce build to work
#endif
  return &UARTD6;
}
}  // namespace GPS

namespace Power {

float GetDefaultBatteryFullVoltage() {
  return 7.0f * 4.2f;
}

float GetDefaultBatteryEmptyVoltage() {
  return 7.0f * 3.3f;
}

float GetDefaultChargeCurrent() {
  return 0.5;
}

float GetAbsoluteMinVoltage() {
  // 3.3V min, 7s pack
  return 7.0f * 3.0;
}

}  // namespace Power
}  // namespace Robot
