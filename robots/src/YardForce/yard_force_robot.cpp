//
// Created by clemens on 27.01.25.
//

#include "robot.hpp"

namespace Robot {

namespace General {
void InitPlatform() {
  // Not used, we could star the GUI driver task here for example
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

namespace Emergency {
std::pair<const Sensor*, size_t> getSensors() {
  return {nullptr, 0};  // No sensors defined yet
}

u_int getLiftPeriod() {
  return 100;
}

u_int getTiltPeriod() {
  return 2500;
}
}  // namespace Emergency

}  // namespace Robot
