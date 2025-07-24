//
// Created by clemens on 23.06.25.
//

#ifndef WORX_DISPLAY_DRIVER_HPP
#define WORX_DISPLAY_DRIVER_HPP

#include <ch.h>
#include <u8g2.h>

class WorxDisplayDriver {
 public:
  WorxDisplayDriver() = default;

  void Start();

 private:
  THD_WORKING_AREA(thd_wa_, 1024){};
  thread_t *processing_thread_ = nullptr;

  u8g2_t u8g2{};

  void threadFunc();

  static void threadHelper(void *instance);
};

#endif  // WORX_DISPLAY_DRIVER_HPP
