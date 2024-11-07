//
// Created by clemens on 10.01.23.
//

#include "gps_interface.h"

#include <etl/algorithm.h>

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
  stopped_ = false;
  this->uart = uart;
  uart_config.speed = baudrate;
  chThdCreateStatic(&wa, sizeof(wa), NORMALPRIO, GpsInterface::threadHelper, this);
  return uartStart(uart, &uart_config) == MSG_OK;
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
  size_t bytes_to_process = 1;
  while(!stopped_) {
    // try to read as much as possible in order to not interrupt on single bytes
    size_t read = sizeof(gps_buffer);
    uartReceiveTimeout(uart, &read, gps_buffer, TIME_MS2I(100));
    size_t processed = 0;
    while(processed < read) {
      size_t in = etl::min(bytes_to_process, read-processed);
      bytes_to_process = process_bytes(gps_buffer+processed, in);
      processed+=in;
    }
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
