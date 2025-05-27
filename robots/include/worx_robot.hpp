#ifndef WORX_ROBOT_HPP
#define WORX_ROBOT_HPP

#include "robot.hpp"

class WorxRobot : public Robot {
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
};

#endif  // WORX_ROBOT_HPP
