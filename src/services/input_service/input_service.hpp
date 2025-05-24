#ifndef INPUT_SERVICE_HPP
#define INPUT_SERVICE_HPP

#include <etl/flat_map.h>
#include <etl/string.h>
#include <lwjson/lwjson.h>

#include <InputServiceBase.hpp>

#include "drivers/input/gpio_input_driver.hpp"
#include "drivers/input/worx_input_driver.hpp"

using namespace xbot::driver::input;
using namespace xbot::service;

struct input_config_json_data_t;

class InputService : public InputServiceBase {
 public:
  explicit InputService(uint16_t service_id) : InputServiceBase(service_id, wa, sizeof(wa)) {
    mutex::initialize(&mutex_);
  }

  mutex_t mutex_;

  const etl::ivector<Input*>& GetAllInputs() const {
    return all_inputs_;
  }

 private:
  GpioInputDriver gpio_driver_{*this};
  WorxInputDriver worx_driver_{*this};

  const etl::flat_map<etl::string<4>, InputDriver*, 2> drivers_ = {
      {"gpio", &gpio_driver_},
      {"worx", &worx_driver_},
  };

  etl::vector<Input*, 30> all_inputs_;

  bool OnRegisterInputConfigsChanged(const void* data, size_t length) override;
  bool InputConfigsJsonCallback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type, void* data);
  bool OnStart() override;
  void OnStop() override;
  void OnLoop(uint32_t now_micros, uint32_t last_tick_micros) override;

  THD_WORKING_AREA(wa, 2500) {};
};

#endif  // INPUT_SERVICE_HPP
