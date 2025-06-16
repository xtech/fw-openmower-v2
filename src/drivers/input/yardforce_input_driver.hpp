#ifndef YARDFORCE_INPUT_DRIVER_HPP
#define YARDFORCE_INPUT_DRIVER_HPP

#include <services.hpp>

#include "input_driver.hpp"

namespace xbot::driver::input {
class YardForceInputDriver : public InputDriver {
 public:
  explicit YardForceInputDriver() = default;
  bool OnInputConfigValue(lwjson_stream_parser_t *jsp, const char *key, lwjson_stream_type_t type,
                          Input &input) override;
};
}  // namespace xbot::driver::input

#endif  // YARDFORCE_INPUT_DRIVER_HPP
