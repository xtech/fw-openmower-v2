#include "../include/sabo_robot.hpp"

#include <drivers/gps/nmea_gps_driver.h>

#include <services.hpp>

using namespace xbot::driver::sabo;
using namespace xbot::driver::gps;

SaboRobot::SaboRobot() : hardware_config(GetHardwareConfig(GetHardwareVersion(carrier_board_info))) {
}

void SaboRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  bms_.Init();

  power_service.SetDriver(&charger_);
  power_service.SetDriver(&bms_);
  input_service.RegisterInputDriver("sabo", &sabo_input_driver_);

  cover_ui_.Start();

  // Initialize GPS driver for immediate availability (without waiting for ROS configuration)
  gps_service.LoadAndStartGpsDriver(ProtocolType::NMEA, 0, 921600);
}

bool SaboRobot::IsHardwareSupported() {
  // Accept Sabo 0.1|2|3.x boards
  if (strncmp("hw-openmower-sabo", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      strncmp("xcore", board_info.board_id, sizeof(board_info.board_id)) == 0 && board_info.version_major == 1 &&
      carrier_board_info.version_major == 0 &&
      (carrier_board_info.version_minor == 1 || carrier_board_info.version_minor == 2 ||
       carrier_board_info.version_minor == 3)) {
    return true;
  }

  return false;
}
