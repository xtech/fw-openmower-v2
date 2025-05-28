#include "../include/sabo_robot.hpp"

#include <services.hpp>

#define LINE_EMERGENCY_1 LINE_GPIO13  // Front left wheel lift (Hall)
#define LINE_EMERGENCY_2 LINE_GPIO12  // Front right wheel lift (Hall)
#define LINE_EMERGENCY_3 LINE_GPIO11  // Top stop button (Hall)
#define LINE_EMERGENCY_4 LINE_GPIO10  // Back-handle stop (Capacitive)

void SaboRobot::InitPlatform() {
  // Front left wheel lift (Hall)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_1,
                              .invert = false,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(1000),
                              .active = false});
  // Front right wheel lift (Hall)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_2,
                              .invert = false,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(1000),
                              .active = false});
  // Top stop button (Hall)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_3,
                              .invert = false,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(10),
                              .active = false});
  // Back-handle stop (Capacitive)
  emergency_driver_.AddInput({.gpio_line = LINE_EMERGENCY_4,
                              .invert = false,
                              .active_since = 0,
                              .timeout_duration = TIME_MS2I(10),
                              .active = false});
  emergency_driver_.Start();

  InitMotors();
  charger_.setI2C(&I2CD1);
  power_service.SetDriver(&charger_);
}

bool SaboRobot::IsHardwareSupported() {
  // FIXME: Fix EEPROM reading and check EEPROM
  return true;
}
