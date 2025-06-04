#include "gpio_input_driver.hpp"

#include <ulog.h>

#include <board_utils.hpp>
#include <globals.hpp>
#include <json_stream.hpp>
#include <services.hpp>

namespace xbot::driver::input {

bool GpioInputDriver::OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                         Input& input) {
  auto& gpio_input = static_cast<GpioInput&>(input);
  if (strcmp(key, "line") == 0) {
    JsonExpectType(STRING);
    gpio_input.line = GetIoLineByName(jsp->data.str.buff);
    if (gpio_input.line == PAL_NOLINE) {
      ULOG_ERROR("Unknown GPIO line \"%s\"", jsp->data.str.buff);
      return false;
    }
    return true;
  }
  ULOG_ERROR("Unknown attribute \"%s\"", key);
  return false;
}

static void LineCallback(void*) {
  input_service.SendEvent(Events::GPIO_TRIGGERED);
}

bool GpioInputDriver::OnStart() {
  for (const auto& input : inputs_) {
    palSetLineMode(input.line, PAL_MODE_INPUT);
    palSetLineCallback(input.line, LineCallback, nullptr);
    palEnableLineEvent(input.line, PAL_EVENT_MODE_BOTH_EDGES);
  }
  return true;
}

void GpioInputDriver::OnStop() {
  for (const auto& input : inputs_) {
    palDisableLineEvent(input.line);
  }
}

void GpioInputDriver::tick() {
  for (auto& input : inputs_) {
    input.Update((palReadLine(input.line) == PAL_HIGH) ^ input.invert);
  }
}

}  // namespace xbot::driver::input
