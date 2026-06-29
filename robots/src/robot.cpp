#include "../include/robot.hpp"

#include <cstring>
#include <globals.hpp>
#include <services.hpp>

#include "../include/lyfco_e1600_robot.hpp"
#include "../include/sabo_robot.hpp"
#include "../include/universal_5s_robot.hpp"
#include "../include/universal_7s_robot.hpp"
#include "../include/worx_robot.hpp"
#include "../include/xbot_robot.hpp"
#include "../include/yardforce_robot.hpp"
#include "../include/yardforce_v4_robot.hpp"

// ── Public detection API ──────────────────────────────────────────────────────

Robot* TryAutoDetectRobot() {
  if (SaboRobot::IsAutoDetected()) return new SaboRobot{};
  if (xBotRobot::IsAutoDetected()) return new xBotRobot{};
  return nullptr;
}

bool BoardSupportsStage2() {
  return YardForceRobot::BoardIsCompatible() || YardForce_V4Robot::BoardIsCompatible() ||
         Universal5SRobot::BoardIsCompatible() || Universal7SRobot::BoardIsCompatible() ||
         WorxRobot::BoardIsCompatible() || Lyfco_E1600Robot::BoardIsCompatible();
}

Robot* GetRobotByName(const char* name) {
  if (YardForceRobot::BoardIsCompatible() && strncmp(name, YardForceRobot::FirmwareName(), 50) == 0)
    return new YardForceRobot{};
  if (YardForce_V4Robot::BoardIsCompatible() && strncmp(name, YardForce_V4Robot::FirmwareName(), 50) == 0)
    return new YardForce_V4Robot{};
  if (Universal5SRobot::BoardIsCompatible() && strncmp(name, Universal5SRobot::FirmwareName(), 50) == 0)
    return new Universal5SRobot{};
  if (Universal7SRobot::BoardIsCompatible() && strncmp(name, Universal7SRobot::FirmwareName(), 50) == 0)
    return new Universal7SRobot{};
  if (WorxRobot::BoardIsCompatible() && strncmp(name, WorxRobot::FirmwareName(), 50) == 0) return new WorxRobot{};
  if (Lyfco_E1600Robot::BoardIsCompatible() && strncmp(name, Lyfco_E1600Robot::FirmwareName(), 50) == 0)
    return new Lyfco_E1600Robot{};
  return nullptr;
}

// ── MowerRobot motor initialisation ──────────────────────────────────────────

void MowerRobot::InitMotors() {
  left_motor_driver_.SetUART(&UARTD1, 115200);
  right_motor_driver_.SetUART(&UARTD4, 115200);

  left_esc_driver_interface_.Start();
  right_esc_driver_interface_.Start();

  diff_drive.SetDrivers(&left_motor_driver_, &right_motor_driver_);
  InitMowerEsc();
}

void MowerRobot::InitMowerEsc() {
  mower_motor_driver_.SetUART(&UARTD2, 115200);
  mower_esc_driver_interface_.Start();
  mower_service.SetDriver(&mower_motor_driver_);
}
