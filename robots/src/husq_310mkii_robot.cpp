#include "../include/husq_310mkii_robot.hpp"

#include <services.hpp>

void husq310MKIIRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
}

bool husq310MKIIRobot::IsHardwareSupported() {
  return strncmp("hw-openmower-310mk2", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0;
}
