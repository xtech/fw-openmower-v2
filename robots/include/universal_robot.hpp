#ifndef UNIVERSAL_ROBOT_HPP
#define UNIVERSAL_ROBOT_HPP

#include <drivers/charger/bq_2576/bq_2576.hpp>

#include "robot.hpp"

class UniversalRobot : public MowerRobot {
 public:
  void InitPlatform() override;
  bool IsHardwareSupported() override;

  UARTDriver* GPS_GetUartPort() override {
#ifndef STM32_UART_USE_USART6
#error STM32_UART_USE_USART6 must be enabled for the YardForce build to work
#endif
    return &UARTD6;
  }

  float Power_GetDefaultChargeCurrent() override {
    return 0.5;
  }

 private:
  BQ2576 charger_{};
};

#endif  // UNIVERSAL_ROBOT_HPP
