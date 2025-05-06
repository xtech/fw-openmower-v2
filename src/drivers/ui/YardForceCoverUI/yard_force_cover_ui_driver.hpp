//
// Created by clemens on 4/11/25.
//

#ifndef OPENMOWER_YARD_FORCE_COVER_UI_DRIVER_HPP
#define OPENMOWER_YARD_FORCE_COVER_UI_DRIVER_HPP

#include "ch.h"
#include "hal.h"
#include <FastCRC.h>
#include <etl/delegate.h>

class YardForceCoverUIDriver {
 private:
  // Extend the config struct by a pointer to this instance, so that we can access it in callbacks.
  struct UARTConfigEx : UARTConfig {
    YardForceCoverUIDriver *context;
  };

  THD_WORKING_AREA(wa_, 1024);
  thread_t* thread_ = nullptr;
  UARTDriver* uart_ = nullptr;
  UARTConfigEx uart_config_{};
  FastCRC16 CRC16{};

  volatile uint8_t buffer[255];
  volatile uint8_t buffer_fill = 0;
  volatile bool processing = false;

  static void ThreadHelper(void *instance);
  static void UartRxChar(UARTDriver *driver, uint16_t data);

  void ThreadFunc();

  void ProcessPacket();


 public:
  enum class Button {

  };
  typedef etl::delegate<void(const GpsState &new_state)> StateCallback;
  void Start(UARTDriver* uart);

};

#endif  // OPENMOWER_YARD_FORCE_COVER_UI_DRIVER_HPP
