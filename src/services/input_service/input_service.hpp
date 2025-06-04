#ifndef INPUT_SERVICE_HPP
#define INPUT_SERVICE_HPP

#include <etl/flat_map.h>
#include <etl/string.h>
#include <lwjson/lwjson.h>

#include <InputServiceBase.hpp>
#include <drivers/input/input_driver.hpp>

using namespace xbot::driver::input;
using namespace xbot::service;

struct input_config_json_data_t;

class InputService : public InputServiceBase {
 public:
  explicit InputService(uint16_t service_id) : InputServiceBase(service_id, wa, sizeof(wa)) {
  }

  void RegisterInputDriver(const char* id, InputDriver* driver) {
    drivers_.emplace(id, driver);
  }

  void OnInputsChangedEvent() {
    SendStatus();
  }

  void OnInputChanged(Input& input, const bool active, const uint32_t duration);

  uint16_t GetEmergencyReasons(uint32_t now);

 private:
  MUTEX_DECL(mutex_);

  etl::flat_map<etl::string<10>, InputDriver*, 3> drivers_;

  // Must not have more than 64 inputs due to the size of various bitmasks.
  etl::vector<Input*, 30> all_inputs_;

  bool OnRegisterInputConfigsChanged(const void* data, size_t length) override;
  bool InputConfigsJsonCallback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type, void* data);
  bool OnStart() override;
  void OnStop() override;
  void OnLoop(uint32_t now_micros, uint32_t last_tick_micros) override;
  void OnSimulatedInputsChanged(const uint64_t& new_value) override;

  bool SendInputEventHelper(Input& input, InputEventType type);
  void SendStatus();
  ServiceSchedule tick_schedule_{*this, 200'000,
                                 XBOT_FUNCTION_FOR_METHOD(InputService, &InputService::SendStatus, this)};

  THD_WORKING_AREA(wa, 2500){};
};

#endif  // INPUT_SERVICE_HPP
