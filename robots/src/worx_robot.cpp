#include "../include/worx_robot.hpp"

#include <globals.hpp>
#include <services.hpp>

void WorxRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
  input_service.RegisterInputDriver("worx", &worx_driver_);
}

bool WorxRobot::IsHardwareSupported() {
  // First batch of universal boards have a non-working EEPROM
  // so we assume that the firmware is compatible, if the xcore is the first batch and no carrier was found.
  if (carrier_board_info.board_info_version == 0 &&
      strncmp("N/A", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      strncmp("xcore", board_info.board_id, sizeof(board_info.board_id)) == 0 &&
      ((board_info.version_major == 1 && board_info.version_minor == 1 && board_info.version_patch == 7) ||
       (board_info.version_major == 1 && board_info.version_minor == 0 && board_info.version_patch == 3))) {
    return true;
  }

  // Else, we accept universal boards
  return strncmp("hw-openmower-universal", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0;
}
