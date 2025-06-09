#ifndef SIMULATED_INPUT_DRIVER_HPP
#define SIMULATED_INPUT_DRIVER_HPP

#include <etl/vector.h>

#include "input_driver.hpp"

namespace xbot::driver::input {
class SimulatedInputDriver : public InputDriver {
  using InputDriver::InputDriver;

 public:
  bool OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                          Input& input) override;
  void SetActiveInputs(uint64_t active_inputs_mask);
};
}  // namespace xbot::driver::input

#endif  // SIMULATED_INPUT_DRIVER_HPP
