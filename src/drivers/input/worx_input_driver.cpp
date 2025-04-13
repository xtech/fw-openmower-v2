#include "worx_input_driver.hpp"

#include <etl/crc16_genibus.h>
#include <etl/flat_map.h>
#include <etl/string.h>
#include <ulog.h>

#define IS_BIT_SET(x, bit) ((x & (1 << bit)) != 0)

namespace xbot::driver::input {

static const etl::flat_map<etl::string<13>, uint8_t, 8> INPUT_BITS = {
    // Keys
    {"start", 1},
    {"home", 2},
    {"back", 3},

    // Halls
    {"battery_cover", 9},
    {"stop1", 10},
    {"trapped1", 12},
    {"trapped2", 13},
    {"stop2", 15},
};

bool WorxInputDriver::OnStart() {
  // TODO: Does this need to be configurable?
  i2c_driver_ = &I2CD2;
  return true;
}

void WorxInputDriver::tick() {
  KeypadResponse keypad{};
  if (ReadKeypad(keypad)) {
    for (auto& input : inputs_) {
      input.Update(IS_BIT_SET(keypad.keys_and_halls, input.bit));
    }
  }
}

bool WorxInputDriver::ReadKeypad(KeypadResponse& response) {
  static const uint8_t address = 39;
  static const uint8_t request_packet[] = {0x01, 0x01, 0xE0, 0xC1};

  uint8_t* response_ptr = (uint8_t*)&response;
  i2cAcquireBus(i2c_driver_);
  bool ok = i2cMasterTransmit(i2c_driver_, address, request_packet, sizeof(request_packet), nullptr, 0) == MSG_OK &&
            i2cMasterReceive(i2c_driver_, address, response_ptr, sizeof(response)) == MSG_OK;
  i2cReleaseBus(i2c_driver_);
  if (!ok) return false;

  etl::crc16_genibus crc;
  crc.add(response_ptr, response_ptr + sizeof(response) - 2);
  return response.crc == crc.value();
}

}  // namespace xbot::driver::input
