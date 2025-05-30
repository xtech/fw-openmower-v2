#ifndef WORX_INPUT_DRIVER_HPP
#define WORX_INPUT_DRIVER_HPP

#include <etl/vector.h>
#include <hal.h>

#include <services.hpp>

#include "input_driver.hpp"

namespace xbot::driver::input {
class WorxInputDriver : public InputDriver {
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

 private:
#pragma pack(push, 1)
  struct KeypadResponse {
    uint8_t unknown1;
    uint16_t keys_and_halls;
    uint32_t unknown2;
    uint16_t crc;
  };
#pragma pack(pop)

  struct WorxInput : public Input {
    uint8_t bit;
  };

  I2CDriver* i2c_driver_;

  etl::vector<WorxInput, 16> inputs_;

  bool ReadKeypad(KeypadResponse& response);

  void tick();
  ServiceSchedule tick_schedule_{input_service, 20'000,
                                 XBOT_FUNCTION_FOR_METHOD(WorxInputDriver, &WorxInputDriver::tick, this)};
};
}  // namespace xbot::driver::input

#endif  // WORX_INPUT_DRIVER_HPP
