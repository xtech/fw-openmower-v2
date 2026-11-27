#ifndef YARDFORCE_ROBOT_HPP
#define YARDFORCE_ROBOT_HPP

#include <cstdint>
#include <drivers/adc/adc1.hpp>
#include <drivers/charger/bq_2576/bq_2576.hpp>
#include <drivers/ui/yf_cover_ui/yf_cover_ui.hpp>
#include <type_traits>

#include "robot.hpp"

/**
 * YardForce platform.
 *
 * Both variants share the same carrier board and platform code, only the mower ESC differs, so
 * the mower driver is a template parameter:
 *   YardForceRobot    = YardForceRobotT<VescDriver>    -> YardForce Std. (VESC mower ESC)
 *   YardForce_V4Robot = YardForceRobotT<YFR4escDriver> -> YardForce Rev4 (YFR4 mower ESC)
 *
 * Detection: the mower ESC on UARTD2 tells the variant (see mower_esc_probe.hpp), so neither
 * variant waits for the MetaService / Stage 2.
 */
template <typename MowerDriverT>
class YardForceRobotT : public VariantNamed<YardForceRobotT<MowerDriverT>, MowerRobotBase<MowerDriverT>> {
 public:
  static bool BoardIsCompatible();
  /**
   * Stage 1 detection for the YardForce family: the carrier board is shared, the protocol of
   * the mower ESC on UARTD2 decides. The probe result is cached, so asking both variants costs
   * one probe.
   */
  static bool IsAutoDetected();

  static const char* Name() {
    if constexpr (std::is_same_v<MowerDriverT, xbot::driver::motor::YFR4escDriver>) {
      return "YardForce-V4";
    } else {
      return "YardForce";
    }
  }

  void InitPlatform() override;

  bool HasSound() const override {
    return true;
  }

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

// YardForce Std. with a VESC mower ESC
using YardForceRobot = YardForceRobotT<xbot::driver::motor::VescDriver>;
// YardForce Rev4 with a YFR4 mower ESC
using YardForce_V4Robot = YardForceRobotT<xbot::driver::motor::YFR4escDriver>;

#endif  // YARDFORCE_ROBOT_HPP
