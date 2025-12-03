#ifndef WORX_INPUT_DRIVER_HPP
#define WORX_INPUT_DRIVER_HPP

#include <hal.h>

#include <services.hpp>

#include "input_driver.hpp"

namespace xbot::driver::input {
class WorxInputDriver : public InputDriver {
  using InputDriver::InputDriver;

 public:
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

  I2CDriver* i2c_driver_ = nullptr;

  bool ReadKeypad(KeypadResponse& response) const;

  void tick();
  ServiceSchedule tick_schedule_{input_service, 20'000,
                                 XBOT_FUNCTION_FOR_METHOD(WorxInputDriver, &WorxInputDriver::tick, this)};
};
}  // namespace xbot::driver::input

#endif  // WORX_INPUT_DRIVER_HPP
