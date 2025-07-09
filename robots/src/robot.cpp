#include "../include/robot.hpp"

#include <services.hpp>

#include "../include/lyfco_e1600_robot.hpp"
#if ROBOT_PLATFORM_Sabo
#include "../include/sabo_robot.hpp"
#endif
#include "../include/worx_robot.hpp"
#include "../include/xbot_robot.hpp"
#include "../include/yardforce_robot.hpp"

Robot* GetRobot() {
// TODO: This should look at the flash to determine the mower type.
#if ROBOT_PLATFORM_Sabo
  return new SaboRobot();
#elif ROBOT_PLATFORM_YardForce
  return new YardForceRobot();
#elif ROBOT_PLATFORM_Worx
  return new WorxRobot();
#elif ROBOT_PLATFORM_Lyfco_E1600
  return new Lyfco_E1600Robot();
#elif ROBOT_PLATFORM_xBot
  return new xBotRobot();
#else
#error "Unknown ROBOT_PLATFORM"
#endif
}

void MowerRobot::InitMotors() {
  left_motor_driver_.SetUART(&UARTD1, 115200);
  right_motor_driver_.SetUART(&UARTD4, 115200);
  mower_motor_driver_.SetUART(&UARTD2, 115200);

  left_esc_driver_interface_.Start();
  right_esc_driver_interface_.Start();
  mower_esc_driver_interface_.Start();

  diff_drive.SetDrivers(&left_motor_driver_, &right_motor_driver_);
  mower_service.SetDriver(&mower_motor_driver_);
}
