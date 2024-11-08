//
// Created by clemens on 10.01.23.
//

#include "gps_interface.h"

#include <etl/algorithm.h>
#include <ulog.h>

#include <cmath>



namespace xbot::driver::gps {

GpsDriver::GpsDriver() {
  datum_lat_ = datum_long_ = NAN;
}
bool GpsDriver::StartDriver(UARTDriver *uart, uint32_t baudrate) {
  chDbgAssert(stopped_, "don't start the driver twice");
  chDbgAssert(uart != nullptr, "need to provide a driver");
  if(!stopped_) {
    return false;
  }
  this->uart_ = uart;
  uart_config_.speed = baudrate;
  uart_config_.context = this;
  bool uartStarted = uartStart(uart, &uart_config_) == MSG_OK;
  if(!uartStarted) {
    return false;
  }


  uart_config_.rxchar_cb = [](UARTDriver* uartp, uint16_t c) {
    (void)uartp;
    (void)c;
    static int missing_bytes=0;
    missing_bytes++;
  };

  uart_config_.rxend_cb = [](UARTDriver* uartp) {
    (void)uartp;
    static int i = 0;
    i++;
    GpsDriver* instance = reinterpret_cast<const UARTConfigEx*>(uartp->config)->context;
    if(!instance->processing_done_) {
      // This is bad, processing is too slow to keep up with updates!
      // We just read into the same buffer again
      uint8_t* next_recv_buffer = (instance->processing_buffer_ == instance->recv_buffer1_) ? instance->recv_buffer2_ : instance->recv_buffer1_;
      uartStartReceiveI(uartp, RECV_BUFFER_SIZE, next_recv_buffer);
    } else {
      // Swap buffers and read into the next one
      // Get the pointer to the receiving buffer (it's not the processing buffer)
      uint8_t* next_recv_buffer = instance->processing_buffer_;
      uartStartReceiveI(uartp, RECV_BUFFER_SIZE, next_recv_buffer);
      instance->processing_buffer_ = (instance->processing_buffer_ == instance->recv_buffer1_) ? instance->recv_buffer2_ : instance->recv_buffer1_;
      instance->processing_buffer_len_ = RECV_BUFFER_SIZE;
      instance->processing_done_ = false;
      // Signal the processing thread
      if(instance->processing_thread_) {
        chEvtSignalI(instance->processing_thread_, 1);
      }
    }
  };



  stopped_ = false;
  processing_thread_ = chThdCreateStatic(&thd_wa_, sizeof(thd_wa_), NORMALPRIO, threadHelper, this);

  uartStartReceive(uart, RECV_BUFFER_SIZE, recv_buffer1_);
  return true;
}

void GpsDriver::SetStateCallback(const GpsDriver::StateCallback &function) {
  state_callback_ = function;
}
void GpsDriver::SetDatum(double datum_lat, double datum_long,
                             double datum_height) {
  datum_lat_ = datum_lat;
  datum_long_ = datum_long;
  datum_u_ = datum_height;
}

bool GpsDriver::send_raw(const void *data, size_t size) {
  // sdWrite(uart, static_cast<const uint8_t*>(data), size);
  uartSendFullTimeout(uart_, &size, data, TIME_INFINITE);
  return true;
}

void GpsDriver::threadFunc() {
  while(!stopped_) {
    // Wait for data to arrive, process and mark as done
    chEvtWaitAny(ALL_EVENTS);
    ProcessBytes(processing_buffer_, processing_buffer_len_);
    processing_done_ = true;
  }
}

void GpsDriver::threadHelper(void *instance) {
  auto *gps_interface = static_cast<GpsDriver *>(instance);
  gps_interface->threadFunc();
}

void GpsDriver::SendRTCM(const uint8_t *data, size_t size) {
  send_raw(data, size);
}
} // namespace xbot::driver::gps
