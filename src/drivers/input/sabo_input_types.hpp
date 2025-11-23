#ifndef SABO_INPUT_TYPES_HPP
#define SABO_INPUT_TYPES_HPP

#include <cstdint>

namespace xbot::driver::input::sabo {

// Sabo input types
enum class Type : uint8_t { SENSOR, BUTTON };

// GPIO Sensor indices - shared between SaboInputDriver and Input union
enum class SensorId : uint8_t {
  LIFT_FL = 0,
  LIFT_FR = 1,
  STOP_TOP = 2,
  STOP_REAR = 3,
};

}  // namespace xbot::driver::input::sabo

#endif  // SABO_INPUT_TYPES_HPP
