#include "robot.hpp"
#include <globals.hpp>

namespace Robot {

namespace General {
void InitPlatform() {
  // Not used, we could star the GUI driver task here for example
}

bool IsHardwareSupported() {
  // Accept YardForce 1.x.x boards
  return strncmp("hw-xbot-mainboard", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      carrier_board_info.version_major == 0;
}

}  // namespace General

namespace GPS {
UARTDriver* GetUartPort() {
  return nullptr;
}
}

namespace Power {

I2CDriver* GetPowerI2CD() {
  return &I2CD1;
}

float GetMaxVoltage() {
  return 4.0f * 4.2f;
}

float GetChargeCurrent() {
  return 0.5;
}

float GetMinVoltage() {
  // 3.3V min, 7s pack
  return 4.0f * 3.3;
}

}  // namespace Power
}  // namespace Robot
