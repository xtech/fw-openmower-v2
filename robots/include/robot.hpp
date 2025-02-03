//
// Created by clemens on 27.01.25.
//

#ifndef ROBOT_HPP
#define ROBOT_HPP

#include <hal.h>

namespace Robot {

namespace General {
[[maybe_unused]] void InitPlatform();
}

namespace Gps {
[[maybe_unused]] UARTDriver* GetUart();
}

namespace Power {

[[maybe_unused]] I2CDriver* GetPowerI2CD();

/**
 * Return the maximum voltage for this robot.
 */
[[maybe_unused]] float GetMaxVoltage();
/**
 * Return the charing current for this robot
 */
[[maybe_unused]] float GetChargeCurrent();
/**
 * Return the minimum voltage before shutting down as much as possible
 */
[[maybe_unused]] float GetMinVoltage();
}  // namespace Power
};  // namespace Robot

#endif  // ROBOT_HPP
