#include "../include/robot_310mkii_robot.hpp"

#include <services.hpp>

void Robot310MKIIRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
}

bool Robot310MKIIRobot::IsHardwareSupported() {
  return strncmp("hw-openmower-310mkii", carrier_board_info.board_id,
                 sizeof(carrier_board_info.board_id)) == 0;
}
