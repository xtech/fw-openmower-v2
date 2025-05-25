#ifndef INPUT_DRIVER_HPP
#define INPUT_DRIVER_HPP

#include <etl/atomic.h>
#include <etl/string.h>
#include <lwjson/lwjson.h>

#include <xbot-service/Service.hpp>

using namespace xbot::service;

namespace xbot::driver::input {
struct Input {
  enum class EmergencyMode {
    NONE,
    EMERGENCY,
    TRAPPED,
  };

  // Configuration
  size_t idx;
  etl::string<20> name;
  bool invert = false;
  EmergencyMode emergency_mode;

  // State
  bool IsActive() const {
    return active;
  }

  bool Update(bool new_active);

 private:
  etl::atomic<bool> active = false;
};

class InputDriver {
 public:
  explicit InputDriver(Service& service) : service_(service){};
  virtual Input& AddInput() = 0;
  virtual void ClearInputs() = 0;
  virtual bool OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                  Input& input) = 0;
  virtual bool OnStart() {
    return true;
  };
  virtual void OnStop(){};

 protected:
  Service& service_;
};
}  // namespace xbot::driver::input

#endif  // INPUT_DRIVER_HPP
