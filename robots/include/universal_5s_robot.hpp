#ifndef UNIVERSAL_5S_ROBOT_HPP
#define UNIVERSAL_5S_ROBOT_HPP

#include "universal_robot.hpp"

/**
 * @class Universal5SRobot
 * @brief Universal robot platform variant for 5S (5-series) battery packs.
 */
class Universal5SRobot : public UniversalRobot {
 public:
  float Power_GetDefaultBatteryFullVoltage() override {
    return 5.0f * 4.2f;
  }

  float Power_GetDefaultBatteryEmptyVoltage() override {
    return 5.0f * 3.3f;
  }

  float Power_GetAbsoluteMinVoltage() override {
    // 3.3V min, 5s pack
    return 5.0f * 3.0;
  }

  float Power_GetDefaultTerminationCurrent() override {
    return 0.5f;
  }

 protected:
  BQ2576* GetCharger() override {
    return &charger_;
  }

 private:
  BQ2576 charger_{249000, 20000};
};

#endif  // UNIVERSAL_5S_ROBOT_HPP
