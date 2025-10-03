#include "../include/yardforce_v4_robot.hpp"

#include <services.hpp>

void YardForce_V4Robot::InitPlatform() {
  // Initialize default motors (VESC for left/right + mower)
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
}

bool YardForce_V4Robot::IsHardwareSupported() {
  // Accept YardForce 1.x.x boards
  if (strncmp("hw-openmower-yardforce", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      carrier_board_info.version_major == 1) {
    return true;
  }

  // Accept early testing boards
  if (strncmp("hw-xbot-devkit", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0) {
    return true;
  }

  return false;
}
