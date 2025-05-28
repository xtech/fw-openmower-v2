#ifndef LYFCO_E1600_ROBOT_HPP
#define LYFCO_E1600_ROBOT_HPP

#include <drivers/charger/bq_2576/bq_2576.hpp>

#include "robot.hpp"

class Lyfco_E1600Robot : public MowerRobot {
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
};

#endif  // LYFCO_E1600_ROBOT_HPP
