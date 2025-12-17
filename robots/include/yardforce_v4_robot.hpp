#ifndef YARDFORCE_V4_ROBOT_HPP
#define YARDFORCE_V4_ROBOT_HPP

#include <drivers/charger/bq_2576/bq_2576.hpp>

#include "robot.hpp"

class YardForce_V4Robot : public MowerRobot {
 public:
  void InitPlatform() override;
  bool IsHardwareSupported() override;

  UARTDriver* GPS_GetUartPort() override {
#ifndef STM32_UART_USE_USART6
#error STM32_UART_USE_USART6 must be enabled for the YardForce build to work
#endif
    return &UARTD6;
  }

  float Power_GetDefaultBatteryFullVoltage() override {
    return 7.0f * 4.2f;
  }

  float Power_GetDefaultBatteryEmptyVoltage() override {
    return 7.0f * 3.3f;
  }

  float Power_GetDefaultChargeCurrent() override {
    return 0.5;
  }

  float Power_GetAbsoluteMinVoltage() override {
    // 3.3V min, 7s pack
    return 7.0f * 3.0;
  }

 private:
  BQ2576 charger_{};
};

#endif  // YARDFORCE_V4_ROBOT_HPP
