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
  chMtxObjectInit(&processing_mutex);
}
bool GpsInterface::start_driver(UARTDriver *uart, uint32_t baudrate) {
  chDbgAssert(stopped_, "don't start the driver twice");
  chDbgAssert(uart != nullptr, "need to provide a driver");
  if(!stopped_) {
    return false;
  }
  this->uart = uart;
  uart_config.speed = baudrate;
  bool uartStarted = uartStart(uart, &uart_config) == MSG_OK;
  if(!uartStarted) {
    return false;
  }

  stopped_ = false;
  processing_thread_ = chThdCreateStatic(&wa, sizeof(wa), NORMALPRIO, threadHelper, this);
  // Allow higher prio in order to not lose buffers. Active time in this thread is very minimal (it's just swapping buffers)
  chThdCreateStatic(&recvThdWa, sizeof(recvThdWa), NORMALPRIO+1, receivingThreadHelper, this);
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
  // size_t bytes_to_process = 1;
  while(!stopped_) {
    // Wait for data to arrive
    chEvtWaitAny(ALL_EVENTS);
    // Lock the processing buffer and process the data
    chMtxLock(&processing_mutex);
    // try to read as much as possible in order to not interrupt on single bytes
    // size_t processed = 0;
    // while(processed < processing_buffer_len) {
      // const size_t in = etl::min(bytes_to_process, processing_buffer_len-processed);
      // bytes_to_process = process_bytes(processing_buffer+processed, in);
      // processed+=in;
    // }
    process_bytes(processing_buffer, processing_buffer_len);
    chMtxUnlock(&processing_mutex);
  }
}


void GpsInterface::receivingThreadFunc() {
  while(!stopped_) {
    // try to read as much as possible in order to not interrupt on single bytes
    size_t read = RECV_BUFFER_SIZE;
    // Get the pointer to the receiving buffer (it's not the processing buffer)
    uint8_t* buffer = (processing_buffer == recv_buffer1) ? recv_buffer2 : recv_buffer1;
    memset(buffer, 0, RECV_BUFFER_SIZE);
    uartReceiveTimeout(uart, &read, buffer, TIME_S2I(1));
    // swap buffers and instantly restart DMA
    bool locked = chMtxTryLock(&processing_mutex);
    if(!locked) {
      ULOG_WARNING("GPS processing too slow to keep up with incoming data");
      chMtxLock(&processing_mutex);
    }
    // Swap the buffers
    processing_buffer = buffer;
    processing_buffer_len = read;
    chMtxUnlock(&processing_mutex);
    // Signal the processing thread
    if(processing_thread_) {
      chEvtSignal(processing_thread_, 1);
    }
  }
}


void GpsInterface::threadHelper(void *instance) {
  auto *gps_interface = static_cast<GpsInterface *>(instance);
  gps_interface->threadFunc();
}
void GpsInterface::receivingThreadHelper(void *instance) {
  auto *gps_interface = static_cast<GpsInterface *>(instance);
  gps_interface->receivingThreadFunc();
}

void GpsInterface::send_rtcm(const uint8_t *data, size_t size) {
  send_raw(data, size);
}
}  // namespace gps
}  // namespace driver
}  // namespace xbot
