#include "simulated_input_driver.hpp"

#include <etl/to_arithmetic.h>
#include <ulog.h>

#include "../../json_stream.hpp"

#define IS_BIT_SET(x, bit) ((x & (1 << bit)) != 0)

namespace xbot::driver::input {

bool SimulatedInputDriver::OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                              Input& input) {
  auto& simulated_input = static_cast<SimulatedInput&>(input);
  if (strcmp(key, "bit") == 0) {
    JsonExpectType(NUMBER);
    simulated_input.bit = etl::to_arithmetic<uint8_t>(jsp->data.prim.buff, strlen(jsp->data.prim.buff));
    return true;
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
