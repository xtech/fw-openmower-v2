#ifndef YARDFORCE_ROBOT_HPP
#define YARDFORCE_ROBOT_HPP

#include <drivers/adc/adc1.hpp>
#include <drivers/charger/bq_2576/bq_2576.hpp>
#include <drivers/ui/yf_cover_ui/yf_cover_ui.hpp>

#include "robot.hpp"

class YardForceRobot : public MowerRobot {
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
    return 1.0;
  }

  float Power_GetMaxChargeCurrent() override {
    return 1.0;
  }

  float Power_GetAbsoluteMinVoltage() override {
    // 3.0V min, 7s pack
    return 7.0f * 3.0;
  }

 protected:
  void RegisterAdcSensors();

  BQ2576 charger_{249000, 14040};
  xbot::driver::ui::YFCoverUI yf_cover_ui_{};

  // V_CHARGER: Rtop=33k, Rbot=2k7 → scale = (33000+2700)/2700 = 13.2222
  static constexpr float ADC_CHARGER_VOLTAGE_SCALE = 13.2222f;
  // V_BATTERY: Rtop=32k4, Rbot=3k24 → scale = (32400+3240)/3240 = 11.0
  static constexpr float ADC_BATTERY_VOLTAGE_SCALE = 11.0f;
};

#endif  // YARDFORCE_ROBOT_HPP
