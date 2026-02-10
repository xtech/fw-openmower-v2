#ifndef UNIVERSAL_7S_ROBOT_HPP
#define UNIVERSAL_7S_ROBOT_HPP

#include "universal_robot.hpp"

/**
 * @class Universal7SRobot
 * @brief Universal robot platform variant for 7S (7-series) battery packs.
 */
class Universal7SRobot : public UniversalRobot {
 public:
  float Power_GetDefaultBatteryFullVoltage() override {
    return 7.0f * 4.2f;
  }

  float Power_GetDefaultBatteryEmptyVoltage() override {
    return 7.0f * 3.3f;
  }

  float Power_GetAbsoluteMinVoltage() override {
    // 3.3V min, 7s pack
    return 7.0f * 3.0;
  }
};

#endif  // UNIVERSAL_7S_ROBOT_HPP
