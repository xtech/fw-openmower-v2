//
// Created by clemens on 10.11.24.
//

#include "debug_tcp_interface.hpp"
#include "lwip/sockets.h"
#include <cstddef>

DebugTCPInterface::DebugTCPInterface(uint16_t listen_port, DebuggableDriver *driver) {
  chDbgAssert(listen_port > 0, "port invalid");
  chDbgAssert(driver != nullptr, "invalid driver");
  listen_port_ = listen_port;
  driver_ = driver;
}
void DebugTCPInterface::Start() {
  driver_->SetRawDataCallback(etl::delegate<void(const uint8_t*, size_t)>::create<DebugTCPInterface, &DebugTCPInterface::OnRawDriverData>(*this));
  chThdCreateStatic(waThread, sizeof(waThread), NORMALPRIO, &ThreadFuncHelper, this);
}
void DebugTCPInterface::ThreadFunc() {
  chRegSetThreadName("DebugTCPInterface");

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    return;
  }

  // Bind the socket to a port
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(listen_port_);

  if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    lwip_close(sockfd);
    return;
  }

  // Listen for incoming connections
  if (listen(sockfd, 0) < 0) {
    lwip_close(sockfd);
    return;
  }

  while (1) {
    // Keep the client socket in a global variable so that the callback can use it
    int incoming = accept(sockfd, nullptr, nullptr);
    if (incoming < 0) {
      continue;
    }


    int flag = 1;
    int result = setsockopt(incoming, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    if (result < 0) {
      close(incoming);
      continue;
    }

    chMtxLock(&socket_mutex_);
    current_client_socket_ = incoming;
    chMtxUnlock(&socket_mutex_);

    // Enable raw mode for the driver
    driver_->SetRawAccessMode(true);

    size_t bytes_received = 0;
    while ((bytes_received = read(current_client_socket_, input_buffer_, sizeof(input_buffer_))) > 0) {
      driver_->RawDataInput(input_buffer_, bytes_received);
    }
    // Enable raw mode for the driver
    driver_->SetRawAccessMode(false);

    // wait for last transmission before invalidating and closing the socket
    chMtxLock(&socket_mutex_);
    current_client_socket_ = -1;
    chMtxUnlock(&socket_mutex_);

    close(incoming);
  }
}
void DebugTCPInterface::OnRawDriverData(const uint8_t *data, size_t size) {
  chMtxLock(&socket_mutex_);
  if (current_client_socket_ >= 0) {
    write(current_client_socket_, data, size);
  }
  chMtxUnlock(&socket_mutex_);
}
void DebugTCPInterface::ThreadFuncHelper(void *instance) {
  static_cast<DebugTCPInterface*>(instance)->ThreadFunc();
}
