#ifndef LYFCO_E1600_ROBOT_HPP
#define LYFCO_E1600_ROBOT_HPP

#include <drivers/motor/vesc/VescDriver.h>

#include <debug/debug_tcp_interface.hpp>
#include <drivers/charger/bq_2576/bq_2576.hpp>

#include "robot.hpp"

class Lyfco_E1600Robot : public Robot {
 public:
  void InitPlatform() override;
  bool IsHardwareSupported() override;

  float Power_GetDefaultBatteryFullVoltage() override {
    return 7.0f * 4.2f;
  }

  float Power_GetDefaultBatteryEmptyVoltage() override {
    return 7.0f * 3.3f;
  }

  float Power_GetDefaultChargeCurrent() override {
    return 2.5;
  }

  float Power_GetAbsoluteMinVoltage() override {
    // 3.3V min, 7s pack
    return 7.0f * 3.0f;
  }

 private:
  BQ2576 charger_{};
  xbot::driver::motor::VescDriver left_motor_driver_{};
  xbot::driver::motor::VescDriver right_motor_driver_{};
  xbot::driver::motor::VescDriver mower_motor_driver_{};

  DebugTCPInterface left_esc_driver_interface_{65102, &left_motor_driver_};
  DebugTCPInterface mower_esc_driver_interface_{65103, &mower_motor_driver_};
  DebugTCPInterface right_esc_driver_interface_{65104, &right_motor_driver_};
};

#endif  // LYFCO_E1600_ROBOT_HPP
