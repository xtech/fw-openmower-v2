#include "simulated_input_driver.hpp"

#include <ulog.h>

#include "../../json_stream.hpp"

#define IS_BIT_SET(x, bit) ((x & (1 << bit)) != 0)

namespace xbot::driver::input {

bool SimulatedInputDriver::OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                              Input& input) {
  auto& simulated_input = static_cast<SimulatedInput&>(input);
  if (strcmp(key, "bit") == 0) {
    return JsonGetNumber(jsp, type, simulated_input.bit);
  } else {
    ULOG_ERROR("Unknown attribute \"%s\"", key);
    return false;
  }
}

void SimulatedInputDriver::SetActiveInputs(uint64_t active_inputs_mask) {
  for (auto& input : inputs_) {
    input.Update(IS_BIT_SET(active_inputs_mask, input.bit));
  }
}

}  // namespace xbot::driver::input
