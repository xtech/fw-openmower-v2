#ifndef INPUT_DRIVER_HPP
#define INPUT_DRIVER_HPP

#include <etl/atomic.h>
#include <etl/string.h>

namespace xbot::driver::input {
struct Input {
  enum class EmergencyMode {
    NONE,
    EMERGENCY,
    TRAPPED,
  };

  // Configuration
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
  virtual Input& AddInput() = 0;
  virtual void ClearInputs() = 0;
  virtual bool OnStart() {
    return true;
  };
  virtual void tick() = 0;
};
}  // namespace xbot::driver::input

#endif  // INPUT_DRIVER_HPP
