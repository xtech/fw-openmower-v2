#ifndef INPUT_SERVICE_HPP
#define INPUT_SERVICE_HPP

#include <etl/flat_map.h>
#include <etl/string.h>

#include <InputServiceBase.hpp>

#include "drivers/input/gpio_input_driver.hpp"
#include "drivers/input/worx_input_driver.hpp"

using namespace xbot::driver::input;
using namespace xbot::service;

class InputService : public InputServiceBase {
 public:
  explicit InputService(uint16_t service_id) : InputServiceBase(service_id, wa, sizeof(wa)) {
  }

 private:
  GpioInputDriver gpio_driver_;
  WorxInputDriver worx_driver_;

  const etl::flat_map<etl::string<4>, InputDriver*, 2> drivers_ = {
      {"gpio", &gpio_driver_},
      {"worx", &worx_driver_},
  };

  bool OnStart() override;
  void tick();
  ManagedSchedule tick_schedule_{scheduler_, IsRunning(), 20'000,
                                 XBOT_FUNCTION_FOR_METHOD(InputService, &InputService::tick, this)};

  THD_WORKING_AREA(wa, 1500) {};
};

#endif  // INPUT_SERVICE_HPP
