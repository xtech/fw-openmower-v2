#ifndef UNIVERSAL_ROBOT_HPP
#define UNIVERSAL_ROBOT_HPP

#include <drivers/charger/bq_2576/bq_2576.hpp>

#include "robot.hpp"

/**
 * @class UniversalRobot
 * @brief Intermediate base class for Universal robot platform variants.
 */
class UniversalRobot : public MowerRobot {
 public:
  void InitPlatform() override;
  bool IsHardwareSupported() override;

  float Power_GetDefaultChargeCurrent() override {
    return 0.5;
  }

  float Power_GetMaxChargeCurrent() override {
    return 5.0;
  }

  virtual ChargerDriver::ReChargeVoltage Power_GetDefaultReChargeVoltage() {
    // Allow the voltage to drop a bit more, since we have a load attached during charging
    return ChargerDriver::ReChargeVoltage::PERCENT_95_2;
  }

 protected:
  UniversalRobot() = default;  // Intermediate/abstract class

  virtual BQ2576* GetCharger() = 0;
};

#endif  // UNIVERSAL_ROBOT_HPP
