//
// Created by clemens on 27.01.25.
//

#ifndef ROBOT_HPP
#define ROBOT_HPP

#include <etl/string_view.h>
#include <hal.h>

namespace Robot {

namespace General {
[[maybe_unused]] void InitPlatform();
}

namespace GPS {
[[maybe_unused]] UARTDriver *GetUartPort();
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

namespace Emergency {

// We've different sensors types which are handled differently
enum class SensorType { WHEEL, BUTTON };

// Low-level sensor configuration for any kind of emergency sensor like wheel or button sensors.
struct Sensor {
  etl::string_view name;  // Sensor name ie used in emergency reason string
  ioline_t line;          // ChibiOS PAL_LINE (e.g., PAL_LINE(GPIOA, 3))
  bool active_low;        // true = active-low, false = active-high
  SensorType type;
};

/**
 * @brief Returns all emergency sensors.
 * @return Pair of [sensor_array, count]. If no sensors exist, returns [nullptr, 0].
 */
[[maybe_unused]] std::pair<const Sensor*, size_t> getSensors();

[[maybe_unused]] u_int getLiftPeriod();
[[maybe_unused]] u_int getTiltPeriod();

}  // namespace Emergency

};  // namespace Robot

#endif  // ROBOT_HPP
