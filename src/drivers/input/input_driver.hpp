#ifndef INPUT_DRIVER_HPP
#define INPUT_DRIVER_HPP

#include <etl/atomic.h>
#include <etl/string.h>
#include <lwjson/lwjson.h>

#include <xbot-service/portable/system.hpp>

namespace xbot::driver::input {
struct Input {
  enum class EmergencyMode {
    NONE,
    EMERGENCY,
    TRAPPED,
  };

  // Configuration
  uint8_t idx;
  etl::string<20> name;
  bool invert = false;
  EmergencyMode emergency_mode;

  // State
  bool IsActive() const {
    return active;
  }

  bool Update(bool new_active);

  uint32_t ActiveDuration() {
    return xbot::service::system::getTimeMicros() - active_since;
  }

 private:
  etl::atomic<bool> active = false;
  uint32_t active_since;
};

class InputDriver {
 public:
  explicit InputDriver(){};
  virtual Input& AddInput() = 0;
  virtual void ClearInputs() = 0;
  virtual bool OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                  Input& input) = 0;
  virtual bool OnStart() {
    return true;
  };
  virtual void OnStop(){};
};
}  // namespace xbot::driver::input

#endif  // INPUT_DRIVER_HPP
