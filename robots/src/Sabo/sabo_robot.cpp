//
// Created by Apehaenger on 03/24/2025
//

#include "robot.hpp"

namespace Robot {

namespace General {
void InitPlatform() {
  // Not used, we could start the GUI driver task here for example
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

namespace Emergency {
std::pair<const Sensor*, size_t> getSensors() {
  static const Sensor sensors[] = {Sensor{"Front-left-wheel", PAL_LINE(GPIOG, 4), false, SensorType::WHEEL},
                                   Sensor{"Front-right-wheel", PAL_LINE(GPIOG, 5), false, SensorType::WHEEL},
                                   Sensor{"Top-stop-button", PAL_LINE(GPIOG, 8), false, SensorType::BUTTON},
                                   Sensor{"Rear-stop-handle", PAL_LINE(GPIOD, 10), false, SensorType::BUTTON}};
  return {sensors, sizeof(sensors) / sizeof(sensors[0])};
}

u_int getLiftPeriod() {
  return 100;
}

u_int getTiltPeriod() {
  return 2500;
}
}  // namespace Emergency

}  // namespace Robot
