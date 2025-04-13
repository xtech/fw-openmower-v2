#ifndef WORX_INPUT_DRIVER_HPP
#define WORX_INPUT_DRIVER_HPP

#include <etl/vector.h>
#include <hal.h>

#include "input_driver.hpp"

namespace xbot::driver::input {
class WorxInputDriver : public InputDriver {
 public:
  Input& AddInput() override {
    return inputs_.emplace_back();
  }
  void ClearInputs() override {
    inputs_.clear();
  };
  bool OnStart() override;
  void tick() override;

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
};
}  // namespace xbot::driver::input

#endif  // WORX_INPUT_DRIVER_HPP
