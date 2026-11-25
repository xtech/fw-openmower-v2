#include "../include/robot.hpp"

#include <globals.hpp>
#include <services.hpp>

#include "../include/husq_310mkii_robot.hpp"
#include "../include/lyfco_e1600_robot.hpp"
#include "../include/sabo_robot.hpp"
#include "../include/universal_5s_robot.hpp"
#include "../include/universal_7s_robot.hpp"
#include "../include/universal_8s_robot.hpp"
#include "../include/worx_robot.hpp"
#include "../include/xbot_robot.hpp"
#include "../include/yardforce_robot.hpp"

// ── Public detection API ──────────────────────────────────────────────────────

Robot* TryAutoDetectRobot() {
  // Order matters for boot time (not for correctness): the cheap carrier-EEPROM checks come
  // first, the YardForce probe (blocking, up to 10 x 40 ms while no ESC-YFR4 answers).
  if (SaboRobot::IsAutoDetected()) return new SaboRobot{};
  if (xBotRobot::IsAutoDetected()) return new xBotRobot{};

  // YardForce and YardForce-V4 share the carrier board; the mower ESC on UARTD2 (VESC
  // vs YFR4 protocol, probed once and cached) decides. The two checks are mutually exclusive.
  if (YardForce_V4Robot::IsAutoDetected()) return new YardForce_V4Robot{};
  if (YardForceRobot::IsAutoDetected()) return new YardForceRobot{};
  return nullptr;
}

bool BoardSupportsStage2() {
  return YardForceRobot::BoardIsCompatible() || YardForce_V4Robot::BoardIsCompatible() ||
         Universal5SRobot::BoardIsCompatible() || Universal7SRobot::BoardIsCompatible() ||
         Universal8SRobot::BoardIsCompatible() || WorxRobot::BoardIsCompatible() ||
         Lyfco_E1600Robot::BoardIsCompatible() || husq310MKIIRobot::BoardIsCompatible();
}

Robot* GetRobotByName(etl::string_view name) {
  if (YardForceRobot::BoardIsCompatible() && name == etl::string_view(YardForceRobot::Name()))
    return new YardForceRobot{};
  if (YardForce_V4Robot::BoardIsCompatible() && name == etl::string_view(YardForce_V4Robot::Name()))
    return new YardForce_V4Robot{};
  if (Universal5SRobot::BoardIsCompatible() && name == etl::string_view(Universal5SRobot::Name()))
    return new Universal5SRobot{};
  if (Universal7SRobot::BoardIsCompatible() && name == etl::string_view(Universal7SRobot::Name()))
    return new Universal7SRobot{};
  if (Universal8SRobot::BoardIsCompatible() && name == etl::string_view(Universal8SRobot::Name()))
    return new Universal8SRobot{};
  if (WorxRobot::BoardIsCompatible() && name == etl::string_view(WorxRobot::Name())) return new WorxRobot{};
  if (Lyfco_E1600Robot::BoardIsCompatible() && name == etl::string_view(Lyfco_E1600Robot::Name()))
    return new Lyfco_E1600Robot{};
  if (husq310MKIIRobot::BoardIsCompatible() && name == etl::string_view(husq310MKIIRobot::Name()))
    return new husq310MKIIRobot{};
  return nullptr;
}

// ── MowerRobot motor initialisation ──────────────────────────────────────────

template <typename MowerDriverT, size_t kMowerMotors>
void MowerRobotBase<MowerDriverT, kMowerMotors>::InitMotors() {
  left_motor_driver_.SetUART(&UARTD1, 115200);
  right_motor_driver_.SetUART(&UARTD4, 115200);

  left_esc_driver_interface_.Start();
  right_esc_driver_interface_.Start();

  diff_drive.SetDrivers(&left_motor_driver_, &right_motor_driver_);
  InitMowerEsc();
}

template <typename MowerDriverT, size_t kMowerMotors>
void MowerRobotBase<MowerDriverT, kMowerMotors>::InitMowerEsc() {
  // One mower motor per robot today. Several motors (same type, running in parallel) need a
  // per-motor UART and - more importantly - a MowerService that fans out (see MowerRobotBase
  // in robot.hpp). The assertion keeps the multi-motor case from being half implemented.
  static_assert(kMowerMotors == 1,
                "Multi-motor mowers need a fan-out in the MowerService (see MowerRobotBase in robot.hpp)");

  mower_motor_driver_[0].SetUART(&UARTD2, 115200);
  mower_esc_driver_interface_.Start();
  mower_service.SetDriver(&mower_motor_driver_[0]);
}

// The mower variant is chosen at runtime, so both instantiations are compiled into the binary.
template class MowerRobotBase<xbot::driver::motor::VescDriver, 1>;
template class MowerRobotBase<xbot::driver::motor::YFR4escDriver, 1>;
