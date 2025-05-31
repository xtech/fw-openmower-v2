#ifndef WORX_ROBOT_HPP
#define WORX_ROBOT_HPP

#include <drivers/charger/bq_2576/bq_2576.hpp>
#include <drivers/input/worx_input_driver.hpp>
#include <services.hpp>

#include "robot.hpp"

using namespace xbot::driver::input;

class WorxRobot : public MowerRobot {
 public:
  void InitPlatform() override;
  bool IsHardwareSupported() override;

  float Power_GetDefaultBatteryFullVoltage() override {
    return 5.0f * 4.2f;
  }

  float Power_GetDefaultBatteryEmptyVoltage() override {
    return 5.0f * 3.3f;
  }

  float Power_GetDefaultChargeCurrent() override {
    return 1.0;
  }

  float Power_GetAbsoluteMinVoltage() override {
    // 3.3V min, 5s pack
    return 5.0f * 3.0;
  }

 private:
  BQ2576 charger_{};
  WorxInputDriver worx_driver_{};
};

#endif  // WORX_ROBOT_HPP
