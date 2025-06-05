#include "../include/sabo_robot.hpp"

#include <services.hpp>

void SaboRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
}

bool SaboRobot::IsHardwareSupported() {
  // FIXME: Fix EEPROM reading and check EEPROM
  return true;
}
