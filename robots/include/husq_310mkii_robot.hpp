#ifndef HUSQ_310MKII_ROBOT_HPP
#define HUSQ_310MKII_ROBOT_HPP

#include <drivers/charger/bq_2576/bq_2576.hpp>

#include "robot.hpp"

class husq310MKIIRobot : public MowerRobot {
 public:
  void InitPlatform() override;
  bool IsHardwareSupported() override;

  UARTDriver* GPS_GetUartPort() override {
#ifndef STM32_UART_USE_USART6
#error STM32_UART_USE_USART6 must be enabled for the Robot310MKII build to work
#endif
    return &UARTD6;
  }

  float Power_GetDefaultBatteryFullVoltage() override {
    return 5.0f * 4.2f;
  }

  float Power_GetDefaultBatteryEmptyVoltage() override {
    return 5.0f * 3.3f;
  }

  float Power_GetDefaultChargeCurrent() override {
    return 0.5f;
  }

  float Power_GetAbsoluteMinVoltage() override {
    return 5.0f * 3.0f;
  }

  float Power_GetMaxChargeCurrent() override {
    return 0.5f;
  }

 private:
  BQ2576 charger_{249000, 20100};
};

#endif  // ROBOT_310MKII_ROBOT_HPP
