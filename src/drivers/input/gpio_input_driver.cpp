#include "gpio_input_driver.hpp"

#include <etl/to_arithmetic.h>
#include <ulog.h>

#include "../../globals.hpp"
#include "../../services/service_ext.hpp"

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
    gpio_input.line = etl::to_arithmetic<ioline_t>(jsp->data.prim.buff, strlen(jsp->data.prim.buff));
    return true;
  } else {
    ULOG_ERROR("Unknown attribute \"%s\"", key);
    return false;
  }
}

bool GpioInputDriver::OnStart() {
  for (auto& input : inputs_) {
    palSetLineMode(input.line, PAL_MODE_INPUT);
    palSetLineCallback(input.line, GpioInputDriver::LineCallback, this);
    palEnableLineEvent(input.line, PAL_EVENT_MODE_BOTH_EDGES);
  }
  return true;
}

void GpioInputDriver::OnStop() {
  for (auto& input : inputs_) {
    palDisableLineEvent(input.line);
  }
}

void GpioInputDriver::tick() {
  for (auto& input : inputs_) {
    input.Update(palReadLine(input.line) ^ input.invert);
  }
}

void GpioInputDriver::LineCallback(void* arg) {
  auto* service = static_cast<xbot::service::ServiceExt*>(arg);
  service->SendEvent(Events::GPIO_TRIGGERED);
}

}  // namespace xbot::driver::input
