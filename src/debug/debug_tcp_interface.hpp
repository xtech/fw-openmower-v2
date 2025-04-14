//
// Created by clemens on 10.11.24.
//

#ifndef DEBUG_TCP_INTERFACE_HPP
#define DEBUG_TCP_INTERFACE_HPP
#include <cstdint>

#include "debuggable_driver.hpp"

class DebugTCPInterface {
 public:
  DebugTCPInterface(uint16_t listen_port, DebuggableDriver *driver);

  void Start();

  void SetDriver(DebuggableDriver *driver);

 private:
  THD_WORKING_AREA(waThread, 1024);
  MUTEX_DECL(socket_mutex_);
  uint16_t listen_port_ = 0;
  DebuggableDriver *driver_ = nullptr;
  int current_client_socket_ = -1;
  uint8_t input_buffer_[1024]{};
  void ThreadFunc();

  void OnRawDriverData(const uint8_t *data, size_t size);

  static void ThreadFuncHelper(void *instance);
};

#endif  // DEBUG_TCP_INTERFACE_HPP
