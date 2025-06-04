#ifndef GPIO_INPUT_DRIVER_HPP
#define GPIO_INPUT_DRIVER_HPP

#include <etl/vector.h>
#include <hal.h>

#include "input_driver.hpp"
#include "services.hpp"

namespace xbot::driver::input {
class GpioInputDriver : public InputDriver {
  using InputDriver::InputDriver;

 public:
  Input& AddInput() override {
    return inputs_.emplace_back();
  }
  void ClearInputs() override {
    inputs_.clear();
  }
  bool OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                          Input& input) override;
  bool OnStart() override;
  void OnStop() override;
  void tick();

 private:
  struct GpioInput : public Input {
    ioline_t line = PAL_NOLINE;
  };

  etl::vector<GpioInput, 4> inputs_;

  ServiceSchedule tick_schedule_{input_service, 1'000'000,
                                 XBOT_FUNCTION_FOR_METHOD(GpioInputDriver, &GpioInputDriver::tick, this)};
};
}  // namespace xbot::driver::input

#endif  // GPIO_INPUT_DRIVER_HPP
