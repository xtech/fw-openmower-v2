#include "yard_force_input_driver.hpp"

#include <ulog.h>

#include <crc_lookup_map.hpp>
#include <json_stream.hpp>

namespace xbot::driver::input {

#pragma pack(push, 1)
namespace {
struct InputParams {
  uint16_t crc;
  bool button : 1;
  uint8_t id_or_bit : 7;
};
}  // namespace
#pragma pack(pop)

static constexpr InputParams available_inputs[] = {
    // Buttons
    {crc16("home"), true, 2},
    {crc16("play"), true, 3},
    {crc16("s1"), true, 4},
    {crc16("s2"), true, 5},
    {crc16("lock"), true, 6},

    // Halls
    {crc16("stop1"), false, 1},
    {crc16("stop2"), false, 2},
    {crc16("lift"), false, 3},
    {crc16("bump"), false, 4},
    {crc16("liftx"), false, 5},
    {crc16("rbump"), false, 6},
};

static_assert(HasUniqueCrcs(available_inputs), "CRC16 values are not unique");

bool YardForceInputDriver::OnInputConfigValue(lwjson_stream_parser_t *jsp, const char *key, lwjson_stream_type_t type,
                                              Input &input) {
  if (strcmp(key, "id") == 0) {
    JsonExpectType(STRING);
    const auto *id = jsp->data.str.buff;
    if (auto *params = LookupByName(id, available_inputs)) {
      input.yardforce.button = params->button;
      input.yardforce.id_or_bit = params->id_or_bit;
      return true;
    }
    ULOG_ERROR("Unknown ID \"%s\"", id);
    return false;
  }
  ULOG_ERROR("Unknown attribute \"%s\"", key);
  return false;
}

}  // namespace xbot::driver::input
