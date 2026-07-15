#ifndef YARDFORCE_V4_ROBOT_HPP
#define YARDFORCE_V4_ROBOT_HPP

#include "yardforce_robot.hpp"

// YardForce V4 uses the YFR4 ESC for the mower motor instead of VESC.
// The platform selection is done at compile-time via ROBOT_PLATFORM=YardForce_V4:
//  - MowerRobot picks YFR4escDriver via #ifdef ROBOT_PLATFORM_YardForce_V4
//  - robot.cpp instantiates this class via CREATE_ROBOT(YardForce_V4)
// Apart from the mower ESC, everything else is identical to YardForceRobot.
class YardForce_V4Robot : public YardForceRobot {};

#endif  // YARDFORCE_V4_ROBOT_HPP
