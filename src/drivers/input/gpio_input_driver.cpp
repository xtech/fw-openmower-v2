#include "gpio_input_driver.hpp"

#include <ulog.h>

#include <board_utils.hpp>
#include <globals.hpp>
#include <json_stream.hpp>
#include <services.hpp>

namespace xbot::driver::input {

bool GpioInputDriver::OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                         Input& input) {
  if (strcmp(key, "line") == 0) {
    JsonExpectType(STRING);
    input.gpio.line = GetIoLineByName(jsp->data.str.buff);
    if (input.gpio.line == PAL_NOLINE) {
      ULOG_ERROR("Unknown GPIO line \"%s\"", jsp->data.str.buff);
      return false;
    }
    return true;
  } else if (strcmp(key, "active") == 0) {
    JsonExpectType(STRING);
    if (strcmp(jsp->data.str.buff, "high") == 0) {
      input.invert = false;
    } else if (strcmp(jsp->data.str.buff, "low") == 0) {
      input.invert = true;
    } else {
      ULOG_ERROR("Valid values for \"active\" are \"high\" and \"low\"");
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
  for (const auto& input : Inputs()) {
    palSetLineMode(input.gpio.line, PAL_MODE_INPUT);
    palSetLineCallback(input.gpio.line, LineCallback, nullptr);
    palEnableLineEvent(input.gpio.line, PAL_EVENT_MODE_BOTH_EDGES);
  }
  return true;
}

void GpioInputDriver::OnStop() {
  for (const auto& input : Inputs()) {
    palDisableLineEvent(input.gpio.line);
  }
}

void GpioInputDriver::tick() {
  for (auto& input : Inputs()) {
    input.Update(palReadLine(input.gpio.line) == PAL_HIGH);
  }
}

}  // namespace xbot::driver::input
