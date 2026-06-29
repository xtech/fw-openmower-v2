#include "../include/yardforce_v4_robot.hpp"

#include <services.hpp>

#include "../include/yardforce_robot.hpp"

void YardForce_V4Robot::InitMowerEsc() {
  yardforce_mower_driver_.SetUART(&UARTD2, 115200);
  yardforce_mower_debug_.Start();
  mower_service.SetDriver(&yardforce_mower_driver_);
}

void YardForce_V4Robot::InitPlatform() {
  // InitMotors sets up left/right VESC; InitMowerEsc (overridden) sets up YFR4 ESC.
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
}

bool YardForce_V4Robot::BoardIsCompatible() {
  return YardForceRobot::BoardIsCompatible();
}
