//
// Created by Apehaenger on 03/24/2025
//

#include <drivers/charger/bq_2576/bq_2576.hpp>
#include <drivers/emergency/gpio_emergency_driver.hpp>
#include <globals.hpp>

#include "robot.hpp"

namespace Robot {

static BQ2576 charger{};
static GPIOEmergencyDriver emergencyDriver{};

namespace General {
void InitPlatform() {
  // Front left wheel lift (Hall)
  emergencyDriver.AddInput({.gpio_line = LINE_EMERGENCY_1,
                            .invert = false,
                            .active_since = 0,
                            .timeout_duration = TIME_MS2I(1000),
                            .active = false});
  // Front right wheel lift (Hall)
  emergencyDriver.AddInput({.gpio_line = LINE_EMERGENCY_2,
                            .invert = false,
                            .active_since = 0,
                            .timeout_duration = TIME_MS2I(1000),
                            .active = false});
  // Top stop button (Hall)
  emergencyDriver.AddInput({.gpio_line = LINE_EMERGENCY_3,
                            .invert = false,
                            .active_since = 0,
                            .timeout_duration = TIME_MS2I(10),
                            .active = false});
  // Back-handle stop (Capacitive)
  emergencyDriver.AddInput({.gpio_line = LINE_EMERGENCY_4,
                            .invert = false,
                            .active_since = 0,
                            .timeout_duration = TIME_MS2I(10),
                            .active = false});
  emergencyDriver.Start();
}
bool IsHardwareSupported() {
  // FIXME: Fix EEPROM reading and check EEPROM
  return true;
}
}  // namespace General

namespace GPS {
UARTDriver* GetUartPort() {
#ifndef STM32_UART_USE_USART6
#error STM32_SERIAL_USE_UART6 must be enabled for the Sabo build to work
#endif
  return &UARTD6;
}
}  // namespace GPS

namespace Power {

I2CDriver* GetPowerI2CD() {
  return &I2CD1;
}

Charger* GetCharger() {
  return &charger;
}

float GetMaxVoltage() {
  return 29.4f;  // As rated on battery pack, which is 7 * 4.2V
}

float GetChargeCurrent() {
  // Battery pack is 7S3P, so max. would be 1.3Ah * 3 = 3.9A
  // 3.9A would be also the max. charge current for the stock PSU!
  return 1.95f;  // Lets stay save and conservative for now
}

float GetMinVoltage() {
  // Stock Sabo battery pack has INR18650-13L (Samsung) which are specified as:
  // Empty = 3.0V, Critical discharge <=2.5V. For now, let's stay save and use 3.3V
  return 7.0f * 3.3;
}

}  // namespace Power

}  // namespace Robot
