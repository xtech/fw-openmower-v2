#ifndef UNIVERSAL_8S_ROBOT_HPP
#define UNIVERSAL_8S_ROBOT_HPP

#include "universal_robot.hpp"

/**
 * @class Universal8SRobot
 * @brief Universal robot platform variant for 8S (8-series) battery packs.
 */
class Universal8SRobot : public UniversalRobot {
 public:
  float Power_GetDefaultBatteryFullVoltage() override {
    return 8.0f * 4.2f;
  }

  float Power_GetDefaultBatteryEmptyVoltage() override {
    return 8.0f * 3.3f;
  }

  float Power_GetAbsoluteMinVoltage() override {
    // 3.3V min, 8s pack
    return 8.0f * 3.0;
  }

 protected:
  BQ2576* GetCharger() override {
    return &charger_;
  }

 private:
  BQ2576 charger_{249000, 12100};
};

#endif  // UNIVERSAL_8S_ROBOT_HPP
