#ifndef INPUT_SERVICE_HPP
#define INPUT_SERVICE_HPP

#include <etl/array.h>
#include <etl/flat_map.h>
#include <etl/string.h>
#include <etl/utility.h>
#include <lwjson/lwjson.h>

#include <EmergencyServiceBase.hpp>
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

  bool IsHealthy() override {
    return IsRunning() && inputs_configured_;
  }

  static constexpr uint8_t MAX_REDUNDANCY_GROUPS = 4;

  void OnInputChanged(Input& input, const bool active, const uint32_t duration);

  etl::pair<uint16_t, uint32_t> GetEmergencyReasons(uint32_t now);

  uint32_t GetPressDelay(bool long_press = false) {
    return (long_press ? LongPressTime.value : DebounceTime.value) * 1000;
  }

 private:
  etl::atomic<bool> inputs_configured_{false};
  MUTEX_DECL(mutex_);

  // Key must be long enough for the longest driver name (e.g. "yf_cover_ui" = 11 chars).
  etl::flat_map<etl::string<15>, InputDriver*, 3> drivers_;

  // Must not have more than 64 inputs due to the size of various bitmasks.
  constexpr static uint8_t NUM_VIRTUAL_INPUTS = 1;
  etl::vector<Input, 30 + NUM_VIRTUAL_INPUTS> all_inputs_;

  etl::atomic<uint8_t> num_active_lift_{0};
  Input* lift_multiple_input_ = nullptr;

  etl::array<uint8_t, MAX_REDUNDANCY_GROUPS + 1> redundancy_group_refcount_{};

  bool OnRegisterInputConfigsChanged(const void* data, size_t length) override;
  bool InputConfigsJsonCallback(lwjson_stream_parser_t* jsp, lwjson_stream_type_t type, void* data);
  bool OnStart() override;
  void OnStop() override;
  uint32_t OnLoop(uint32_t now_micros, uint32_t last_tick_micros) override;
  void OnSimulatedInputsChanged(const uint64_t& new_value) override;

  bool SendInputEventHelper(Input& input, InputEventType type);
  void SendStatus();
  ServiceSchedule tick_schedule_{*this, 200'000,
                                 XBOT_FUNCTION_FOR_METHOD(InputService, &InputService::SendStatus, this)};

  THD_WORKING_AREA(wa, 3072){};
};

#endif  // INPUT_SERVICE_HPP
