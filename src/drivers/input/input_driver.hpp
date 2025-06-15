#ifndef INPUT_DRIVER_HPP
#define INPUT_DRIVER_HPP

#include <etl/atomic.h>
#include <etl/string.h>
#include <hal.h>
#include <lwjson/lwjson.h>

#include <xbot-service/portable/system.hpp>

namespace xbot::driver::input {
struct Input {
  enum { VIRTUAL = 255 };
  enum class Type : uint8_t { BUTTON, HALL };

  // Configuration
  uint8_t idx;
  bool invert = false;
  uint16_t emergency_reason = 0;
  uint16_t emergency_delay_ms = 0;

  union {
    struct {
      ioline_t line;
    } gpio;

    struct {
      uint8_t bit;
    } worx;

    struct {
      Input::Type type;
      union {
        struct {
          uint8_t id;
        } button;
        struct {
          uint8_t bit;
        } hall;
      };
    } yardforce;
  };

  // State
  bool IsActive() const {
    return active;
  }

  bool Update(bool new_active, uint32_t predate = 0);
  bool GetAndClearPendingEmergency();

  void InjectPress(bool long_press = false);
  void InjectPress(uint32_t duration);

  uint32_t ActiveDuration(const uint32_t now) const {
    return now - active_since;
  }

 private:
  etl::atomic<bool> active{false};
  etl::atomic<bool> emergency_pending{false};
  uint32_t active_since{0};
  Input* next_for_driver_{nullptr};

  friend class InputDriver;
  friend struct InputIterable;
};

struct InputIterable {
  Input* head;

  struct Iterator {
    Input* ptr;

    Input& operator*() const {
      return *ptr;
    }

    Iterator& operator++() {
      ptr = ptr->next_for_driver_;
      return *this;
    }

    bool operator!=(const Iterator& other) const {
      return ptr != other.ptr;
    }
  };

  Iterator begin() const {
    return Iterator{head};
  }

  Iterator end() const {
    return Iterator{nullptr};
  }
};

class InputDriver {
 public:
  virtual ~InputDriver() = default;
  explicit InputDriver() = default;

  void AddInput(Input* input);
  void ClearInputs();
  InputIterable Inputs() {
    return InputIterable{inputs_head_};
  }

  virtual bool OnInputConfigValue(lwjson_stream_parser_t* jsp, const char* key, lwjson_stream_type_t type,
                                  Input& input) = 0;

  virtual bool OnStart() {
    return true;
  };
  virtual void OnStop(){};

 private:
  Input* inputs_head_ = nullptr;
};
}  // namespace xbot::driver::input

#endif  // INPUT_DRIVER_HPP
