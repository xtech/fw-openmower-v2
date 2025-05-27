#include "../include/robot.hpp"

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
