//
// Created by clemens on 10.01.23.
//

#include "gps_interface.h"

#include <etl/algorithm.h>
#include <ulog.h>

#include <cmath>

namespace xbot {
namespace driver {
namespace gps {

GpsInterface::GpsInterface() {
  datum_lat_ = datum_long_ = NAN;
}
bool GpsInterface::start_driver(UARTDriver *uart, uint32_t baudrate) {
  chDbgAssert(stopped_, "don't start the driver twice");
  chDbgAssert(uart != nullptr, "need to provide a driver");
  if(!stopped_) {
    return false;
  }
  this->uart = uart;
  uart_config.speed = baudrate;
  uart_config.context = this;
  bool uartStarted = uartStart(uart, &uart_config) == MSG_OK;
  if(!uartStarted) {
    return false;
  }


  uart_config.rxchar_cb = [](UARTDriver* uartp, uint16_t c) {
    (void)uartp;
    (void)c;
    static int missing_bytes=0;
    missing_bytes++;
  };

  uart_config.rxend_cb = [](UARTDriver* uartp) {
    (void)uartp;
    static int i = 0;
    i++;
    GpsInterface* instance = reinterpret_cast<const UARTConfigEx*>(uartp->config)->context;
    if(!instance->processing_done) {
      // This is bad, processing is too slow to keep up with updates!
      // We just read into the same buffer again
      uint8_t* next_recv_buffer = (instance->processing_buffer == instance->recv_buffer1) ? instance->recv_buffer2 : instance->recv_buffer1;
      uartStartReceiveI(uartp, RECV_BUFFER_SIZE, next_recv_buffer);
    } else {
      // Swap buffers and read into the next one
      // Get the pointer to the receiving buffer (it's not the processing buffer)
      uint8_t* next_recv_buffer = instance->processing_buffer;
      uartStartReceiveI(uartp, RECV_BUFFER_SIZE, next_recv_buffer);
      instance->processing_buffer = (instance->processing_buffer == instance->recv_buffer1) ? instance->recv_buffer2 : instance->recv_buffer1;
      instance->processing_buffer_len = RECV_BUFFER_SIZE;
      instance->processing_done = false;
      // Signal the processing thread
      if(instance->processing_thread_) {
        chEvtSignalI(instance->processing_thread_, 1);
      }
    }
  };



  stopped_ = false;
  processing_thread_ = chThdCreateStatic(&wa, sizeof(wa), NORMALPRIO, threadHelper, this);

  uartStartReceive(uart, RECV_BUFFER_SIZE, recv_buffer1);
  return true;
}

void GpsInterface::set_state_callback(const GpsInterface::StateCallback &function) {
  state_callback = function;
}
void GpsInterface::set_datum(double datum_lat, double datum_long,
                             double datum_height) {
  datum_lat_ = datum_lat;
  datum_long_ = datum_long;
  datum_u_ = datum_height;
}

bool GpsInterface::send_raw(const void *data, size_t size) {
  // sdWrite(uart, static_cast<const uint8_t*>(data), size);
  uartSendFullTimeout(uart, &size, data, TIME_INFINITE);
  return true;
}

void GpsInterface::threadFunc() {
  while(!stopped_) {
    // Wait for data to arrive, process and mark as done
    chEvtWaitAny(ALL_EVENTS);
    process_bytes(processing_buffer, processing_buffer_len);
    processing_done = true;
  }
}

void GpsInterface::threadHelper(void *instance) {
  auto *gps_interface = static_cast<GpsInterface *>(instance);
  gps_interface->threadFunc();
}

void GpsInterface::send_rtcm(const uint8_t *data, size_t size) {
  send_raw(data, size);
}
}  // namespace gps
}  // namespace driver
}  // namespace xbot
