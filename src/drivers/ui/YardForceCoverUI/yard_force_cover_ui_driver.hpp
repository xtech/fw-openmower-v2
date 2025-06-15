#ifndef YARD_FORCE_COVER_UI_DRIVER_HPP
#define YARD_FORCE_COVER_UI_DRIVER_HPP

#include <etl/crc16_ccitt.h>

#include <drivers/input/yard_force_input_driver.hpp>

#include "ch.h"
#include "hal.h"
#include "ui_board.h"

class YardForceCoverUIDriver {
 private:
  YardForceInputDriver input_driver_{};

  // Extend the config struct by a pointer to this instance, so that we can access it in callbacks.
  struct UARTConfigEx : UARTConfig {
    YardForceCoverUIDriver *context;
  };

  THD_WORKING_AREA(wa_, 1024);
  thread_t *thread_ = nullptr;
  UARTDriver *uart_ = nullptr;
  UARTConfigEx uart_config_{};
  etl::crc16_ccitt CRC16{};

  volatile uint8_t buffer_[255];
  volatile uint8_t buffer_fill_ = 0;
  volatile bool processing_ = false;

  bool board_found_ = false;
  uint8_t encode_decode_buf_[100]{};

  static void ThreadHelper(void *instance);
  static void UartRxChar(UARTDriver *driver, uint16_t data);

  void ThreadFunc();

  void UpdateUILeds();

  void ProcessPacket();

  void sendUIMessage(void *msg, size_t size);
  void RequestFWVersion();

 public:
  void Start(UARTDriver *uart);
  YardForceInputDriver &GetInputDriver() {
    return input_driver_;
  }
};

#endif  // YARD_FORCE_COVER_UI_DRIVER_HPP
