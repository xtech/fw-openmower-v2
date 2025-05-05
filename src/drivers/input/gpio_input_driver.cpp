#include "gpio_input_driver.hpp"

#include <etl/to_arithmetic.h>

#include <drivers/input/gpio_input_driver.hpp>

#include "ulog.h"

namespace xbot::driver::input {

#define lw_json_expect_type(expected)          \
  if (type != LWJSON_STREAM_TYPE_##expected) { \
    return false;                              \
  }

bool GpioInputDriver::OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                         Input& input) {
  auto& gpio_input = static_cast<GpioInput&>(input);
  if (strcmp(key, "line") == 0) {
    lw_json_expect_type(NUMBER);
    gpio_input.line = etl::to_arithmetic<ioline_t>(jsp->data.prim.buff, LWJSON_CFG_STREAM_PRIMITIVE_MAX_LEN);
    return true;
  } else {
    ULOG_ERROR("Unknown attribute \"%s\"", key);
    return false;
  }
}

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
