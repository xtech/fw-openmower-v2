//
// Created by clemens on 4/11/25.
//

#ifndef OPENMOWER_YARD_FORCE_COVER_UI_DRIVER_HPP
#define OPENMOWER_YARD_FORCE_COVER_UI_DRIVER_HPP

#include <etl/crc16_ccitt.h>

#include <drivers/input/input_driver.hpp>

#include "COBS.h"
#include "ch.h"
#include "hal.h"
#include "ui_board.h"

class YardForceCoverUIDriver : public xbot::driver::input::InputDriver {
 private:
  // Extend the config struct by a pointer to this instance, so that we can access it in callbacks.
  struct UARTConfigEx : UARTConfig {
    YardForceCoverUIDriver *context;
  };

  THD_WORKING_AREA(wa_, 1024);
  thread_t *thread_ = nullptr;
  UARTDriver *uart_ = nullptr;
  UARTConfigEx uart_config_{};
  etl::crc16_ccitt CRC16{};

  volatile uint8_t buffer[255];
  volatile uint8_t buffer_fill = 0;
  volatile bool processing = false;

  bool board_found = false;
  uint8_t encode_decode_buf_[100]{};
  COBS cobs_{};

  static void ThreadHelper(void *instance);
  static void UartRxChar(UARTDriver *driver, uint16_t data);

  void ThreadFunc();

  void ProcessPacket();

  void sendUIMessage(void *msg, size_t size);
  void RequestFWVersion();

 public:
  void Start(UARTDriver *uart);
};

#endif  // OPENMOWER_YARD_FORCE_COVER_UI_DRIVER_HPP
