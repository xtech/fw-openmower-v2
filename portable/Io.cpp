//
// Created by clemens on 7/14/24.
//
#include <lwip/sockets.h>

#include <xbot-service/Io.hpp>
#include <xbot-service/Lock.hpp>
#include <xbot-service/portable/thread.hpp>
#include <xbot/config.hpp>

namespace xbot::service {

// Keep track of the first registered service.
// All services have a pointer to the next one, so we can loop through all
// of them (for IO)
ServiceIo* firstService_ = nullptr;

/**
 * The IO thread is used to receive data for registered services.
 * On data reception, the handlePacket method will be called for the service.
 */
#ifdef XBOT_ENABLE_STATIC_STACK
static THD_WORKING_AREA(waIoThread, 128000);
#endif
XBOT_THREAD_TYPEDEF io_thread_{};

fd_set socket_set;

void runIo(void* arg) {
  while (true) {
    timeval select_to{.tv_sec = 1, .tv_usec = 0};
    FD_ZERO(&socket_set);
    // Add all sockets to the set and then wait for one to receive data
    int max_sd = 0;
    for (ServiceIo* service = firstService_; service != nullptr;
         service = service->next_service_) {
      FD_SET(service->udp_socket_, &socket_set);
      if (service->udp_socket_ + 1 > max_sd) {
        max_sd = service->udp_socket_ + 1;
      }
    }

    int res = select(max_sd, &socket_set, NULL, NULL, &select_to);
    if (res <= 0) {
      // Timeout, continue
      continue;
    }

    for (ServiceIo* service = firstService_; service != nullptr;
         service = service->next_service_) {
      // Check, if the FD was ready
      if (FD_ISSET(service->udp_socket_, &socket_set)) {
        // Check, if service is running
        packet::PacketPtr packet = nullptr;
        if (sock::receivePacket(&service->udp_socket_, &packet)) {
          {
            Lock lk(&service->state_mutex_);
            if (service->stopped) {
              // service not running, free packet and go to next one
              packet::freePacket(packet);
              continue;
            } else {
              // Give packet to service
              service->ioInput(packet);
            }
          }
        }
      }
    }
  }
}

bool Io::registerServiceIo(ServiceIo* service) {
  ServiceIo** last_service = &firstService_;
  while (*last_service != nullptr) {
    last_service = &(*last_service)->next_service_;
  }
  *last_service = service;
  return true;
}

bool Io::start() {
  return thread::initialize(&io_thread_, runIo, nullptr, &waIoThread,
                            sizeof(waIoThread));
}

}  // namespace xbot::service
