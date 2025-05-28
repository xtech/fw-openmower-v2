#include "../include/robot.hpp"

#include <services.hpp>

#include "../include/lyfco_e1600_robot.hpp"
#include "../include/sabo_robot.hpp"
#include "../include/worx_robot.hpp"
#include "../include/xbot_robot.hpp"
#include "../include/yardforce_robot.hpp"

#define EXPAND(x) x
#define ROBOT_CLASS_NAME(platform) platform##Robot
#define CREATE_ROBOT(platform) new EXPAND(ROBOT_CLASS_NAME(platform))()

Robot* GetRobot() {
  // TODO: This should look at the flash to determine the mower type.
  return CREATE_ROBOT(ROBOT_PLATFORM);
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
