#include "yard_force_input_driver.hpp"

#include <ulog.h>

#include <json_stream.hpp>

namespace xbot::driver::input {

bool YardForceInputDriver::OnInputConfigValue(lwjson_stream_parser_t *jsp, const char *key, lwjson_stream_type_t type,
                                              Input &input) {
  // TODO: We probably want to take strings here and map them to the ID.
  if (strcmp(key, "button") == 0) {
    input.yardforce.type = Input::Type::BUTTON;
    return JsonGetNumber(jsp, type, input.yardforce.button.id);
  } else if (strcmp(key, "hall") == 0) {
    input.yardforce.type = Input::Type::HALL;
    return JsonGetNumber(jsp, type, input.yardforce.hall.bit);
  }
  ULOG_ERROR("Unknown attribute \"%s\"", key);
  return false;
}

}  // namespace xbot::driver::input
