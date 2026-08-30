#ifndef YARDFORCE_V4_ROBOT_HPP
#define YARDFORCE_V4_ROBOT_HPP

#include <drivers/motor/yfr4esc/YFR4escDriver.h>

#include "yardforce_robot.hpp"

// YardForce V4 uses the YFR4 ESC for the mower motor instead of VESC.
// Inherits common YardForce logic, overrides InitMowerEsc() to wire YFR4.
// Selection happens at runtime (unified binary) via BoardIsCompatible().
class YardForce_V4Robot : public YardForceRobot {
 public:
  static const char* FirmwareName() {
    return "YardForce-V4";
  }
  void InitMowerEsc() override {
    yfr4_mower_driver_.SetUART(&UARTD2, 115200);
    yfr4_mower_debug_.Start();
    mower_service.SetDriver(&yfr4_mower_driver_);
  }

 private:
  xbot::driver::motor::YFR4escDriver yfr4_mower_driver_{};
  DebugTCPInterface yfr4_mower_debug_{65103, &yfr4_mower_driver_};
};

#endif  // YARDFORCE_V4_ROBOT_HPP
