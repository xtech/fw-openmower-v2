#ifndef XBOT_ROBOT_HPP
#define XBOT_ROBOT_HPP

#include <drivers/charger/bq_2579/bq_2579.hpp>
#include <drivers/motor/pwm/pwm_motor_driver.hpp>

#include "robot.hpp"

using xbot::driver::motor::PwmMotorDriver;

class xBotRobot : public Robot {
 public:
  void InitPlatform() override;
  bool IsHardwareSupported() override;

  float Power_GetDefaultBatteryFullVoltage() override {
    return 4.0f * 4.2f;
  }

  float Power_GetDefaultBatteryEmptyVoltage() override {
    return 4.0f * 3.3f;
  }

  float Power_GetDefaultChargeCurrent() override {
    return 1.0;
  }

  float Power_GetAbsoluteMinVoltage() override {
    // 3.0V min, 4s pack
    return 4.0f * 3.0;
  }

 private:
  BQ2579 charger_{};
  PwmMotorDriver left_pwm_motor_driver_{};
  PwmMotorDriver right_pwm_motor_driver_{};
};

#endif  // XBOT_ROBOT_HPP
