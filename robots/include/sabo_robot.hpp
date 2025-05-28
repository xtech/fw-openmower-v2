#ifndef SABO_ROBOT_HPP
#define SABO_ROBOT_HPP

#include <drivers/charger/bq_2576/bq_2576.hpp>
#include <drivers/emergency/gpio_emergency_driver.hpp>

#include "drivers/ui/SaboCoverUI/sabo_cover_ui_controller.hpp"
#include "robot.hpp"

using namespace xbot::driver::ui;

class SaboRobot : public MowerRobot {
 public:
  void InitPlatform() override;
  bool IsHardwareSupported() override;

  UARTDriver* GPS_GetUartPort() override {
#ifndef STM32_UART_USE_USART6
#error STM32_SERIAL_USE_UART6 must be enabled for the Sabo build to work
#endif
    return &UARTD6;
  }

  float Power_GetDefaultBatteryFullVoltage() override {
    return 29.4f;  // As rated on battery pack, which is 7 * 4.2V
  }

  float Power_GetDefaultBatteryEmptyVoltage() override {
    return 7.0f * 3.3f;
  }

  float Power_GetDefaultChargeCurrent() override {
    // Battery pack is 7S3P, so max. would be 1.3Ah * 3 = 3.9A
    // 3.9A would be also the max. charge current for the stock PSU!
    return 1.95f;  // Lets stay save and conservative for now
  }

  float Power_GetAbsoluteMinVoltage() override {
    // Stock Sabo battery pack has INR18650-13L (Samsung) which are specified as:
    // Empty = 3.0V, Critical discharge <=2.5V. For now, let's stay save and use 3.0V
    return 7.0f * 3.0;
  }

 private:
  BQ2576 charger_{};
  GPIOEmergencyDriver emergency_driver_{};
  SaboCoverUIController cover_ui_{};
};

#endif  // SABO_ROBOT_HPP
