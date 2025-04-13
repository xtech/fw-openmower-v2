#include "gpio_input_driver.hpp"

#include <drivers/input/gpio_input_driver.hpp>

#include "ulog.h"

namespace xbot::driver::input {

bool GpioInputDriver::OnStart() {
  for (auto& input : inputs_) {
    palSetLineMode(input.line, PAL_MODE_INPUT);
  }
  return true;
}

void GpioInputDriver::tick() {
  for (auto& input : inputs_) {
    input.Update(palReadLine(input.line) ^ input.invert);
  }
}

}  // namespace xbot::driver::input
