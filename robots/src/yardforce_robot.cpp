#include "../include/yardforce_robot.hpp"

#include <services.hpp>

void YardForceRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
}

bool YardForceRobot::BoardIsCompatible() {
  return (strncmp("hw-openmower-yardforce", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
          carrier_board_info.version_major == 1) ||
         strncmp("hw-xbot-devkit", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0;
}
