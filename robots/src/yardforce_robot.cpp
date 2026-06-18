#include "../include/yardforce_robot.hpp"

#include <services.hpp>

void YardForceRobot::InitPlatform() {
  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);

#if !STM32_UART_USE_UART7
#error STM32_UART_USE_UART7 must be enabled for the YF Cover UI to work
#endif
  input_service.RegisterInputDriver("yf_cover_ui", &yf_cover_ui_);
  yf_cover_ui_.Start(&UARTD7);
}

bool YardForceRobot::IsHardwareSupported() {
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
