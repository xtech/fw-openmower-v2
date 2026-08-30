#include "../include/worx_robot.hpp"

#include <globals.hpp>
#include <services.hpp>

#include "../include/universal_robot.hpp"

void WorxRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
  input_service.RegisterInputDriver("worx", &worx_driver_);
}

bool WorxRobot::BoardIsCompatible() {
  return UniversalRobot::BoardIsCompatible();
}
