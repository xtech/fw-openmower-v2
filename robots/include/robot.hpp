//
// Created by clemens on 27.01.25.
//

#ifndef ROBOT_HPP
#define ROBOT_HPP

#include <hal.h>

#include <drivers/charger/bq_2576/bq_2576.hpp>
#include <drivers/charger/charger.hpp>

namespace Robot {

namespace General {
[[maybe_unused]] void InitPlatform();
bool IsHardwareSupported();
}

namespace GPS {
[[maybe_unused]] UARTDriver *GetUartPort();
}

namespace Power {

[[maybe_unused]] I2CDriver* GetPowerI2CD();
[[maybe_unused]] BQ2576* GetCharger();

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
