#include "worx_input_driver.hpp"

#include <etl/crc16_genibus.h>
#include <ulog.h>

#include <crc_lookup_map.hpp>
#include <json_stream.hpp>

#define IS_BIT_SET(x, bit) ((x & (1 << bit)) != 0)

namespace xbot::driver::input {

#pragma pack(push, 1)
namespace {
struct InputParams {
  uint16_t crc;
  uint8_t bit;
};
}  // namespace
#pragma pack(pop)

static constexpr InputParams available_inputs[] = {
    // Keys
    {crc16("start"), 1},
    {crc16("home"), 2},
    {crc16("back"), 3},

    // Halls
    {crc16("battery_cover"), 9},
    {crc16("stop1"), 10},
    {crc16("trapped1"), 12},
    {crc16("trapped2"), 13},
    {crc16("stop2"), 15},
};

static_assert(HasUniqueCrcs(available_inputs), "CRC16 values are not unique");

bool WorxInputDriver::OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                         Input& input) {
  if (strcmp(key, "id") == 0) {
    JsonExpectType(STRING);
    const auto* id = jsp->data.str.buff;
    if (auto* params = LookupByName(id, available_inputs)) {
      input.worx.bit = params->bit;
      return true;
    }
    ULOG_ERROR("Unknown ID \"%s\"", id);
    return false;
  }
  ULOG_ERROR("Unknown attribute \"%s\"", key);
  return false;
}

bool WorxInputDriver::OnStart() {
  // TODO: Does this need to be configurable?
  i2c_driver_ = &I2CD2;
  return true;
}

void WorxInputDriver::tick() {
  KeypadResponse keypad{};
  if (ReadKeypad(keypad)) {
    for (auto& input : Inputs()) {
      input.Update(IS_BIT_SET(keypad.keys_and_halls, input.worx.bit));
    }
  }
}

bool WorxInputDriver::ReadKeypad(KeypadResponse& response) const {
  static constexpr uint8_t address = 39;
  static constexpr uint8_t request_packet[] = {0x01, 0x01, 0xE0, 0xC1};

  auto* response_ptr = reinterpret_cast<uint8_t*>(&response);
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
