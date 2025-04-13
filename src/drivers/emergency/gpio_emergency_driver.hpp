//
// Created by clemens on 4/11/25.
//

#ifndef GPIOEMERGENCYDRIVER_HPP
#define GPIOEMERGENCYDRIVER_HPP

#include <ch.h>
#include <etl/vector.h>

class GPIOEmergencyDriver {
 public:
  struct Input {
    uint32_t gpio_line;
    bool invert;
    systime_t active_since;
    sysinterval_t timeout_duration;
    bool active;
  };
 private:
  THD_WORKING_AREA(wa_, 1024);
  thread_t* thread_ = nullptr;

  static void ThreadHelper(void *instance);
  static void GPIOCallback(void* instance);
  void ThreadFunc();

  etl::vector<Input, 16> inputs_{};
  MUTEX_DECL(mtx_);
 public:
  void Start();
  void AddInput(const Input& input);
};

#endif  // OPENMOWER_GPIOEMERGENCYDRIVER_HPP
