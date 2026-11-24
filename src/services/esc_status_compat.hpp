#ifndef ESC_STATUS_COMPAT_HPP
#define ESC_STATUS_COMPAT_HPP

#include <cstdint>
#include <drivers/motor/motor_driver.hpp>

/**
 * @brief Compile-time check that a service ESC status enum still matches the driver enum.
 *
 * The values exist twice: in the service definition (the generated enum, sent to the high
 * level) and in MotorDriver::ESCState::ESCStatus (what the drivers set).
 */
template <typename T>
constexpr bool EscStatusMatchesDriverEnum() {
  using Driver = xbot::driver::motor::MotorDriver::ESCState::ESCStatus;
  return static_cast<uint8_t>(T::DISCONNECTED) == static_cast<uint8_t>(Driver::ESC_STATUS_DISCONNECTED) &&
         static_cast<uint8_t>(T::ERROR) == static_cast<uint8_t>(Driver::ESC_STATUS_ERROR) &&
         static_cast<uint8_t>(T::STALLED) == static_cast<uint8_t>(Driver::ESC_STATUS_STALLED) &&
         static_cast<uint8_t>(T::OK) == static_cast<uint8_t>(Driver::ESC_STATUS_OK) &&
         static_cast<uint8_t>(T::RUNNING) == static_cast<uint8_t>(Driver::ESC_STATUS_RUNNING);
}

#endif  // ESC_STATUS_COMPAT_HPP
