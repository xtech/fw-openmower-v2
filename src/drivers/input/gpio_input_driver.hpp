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
  bool OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                          Input& input) override;
  bool OnStart() override;
  void OnStop() override;
  void tick();

 private:
  ServiceSchedule tick_schedule_{input_service, 1'000'000,
                                 XBOT_FUNCTION_FOR_METHOD(GpioInputDriver, &GpioInputDriver::tick, this)};
};
}  // namespace xbot::driver::input

#endif  // GPIO_INPUT_DRIVER_HPP
