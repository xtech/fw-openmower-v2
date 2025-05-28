#include "../include/yardforce_robot.hpp"

#include <services.hpp>

#define LINE_EMERGENCY_1 LINE_GPIO13
#define LINE_EMERGENCY_2 LINE_GPIO12
#define LINE_EMERGENCY_3 LINE_GPIO11
#define LINE_EMERGENCY_4 LINE_GPIO10

void YardForceRobot::InitPlatform() {
  // Front left wheel lift (Hall)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_1,
                              .invert = true,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(10),
                              .active = false});
  // Front right wheel lift (Hall)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_2,
                              .invert = true,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(10),
                              .active = false});
  // Top stop button (Hall)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_3,
                              .invert = true,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(2000),
                              .active = false});
  // Back-handle stop (Capacitive)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_4,
                              .invert = true,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(2000),
                              .active = false});
  emergency_driver_.Start();

  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
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
