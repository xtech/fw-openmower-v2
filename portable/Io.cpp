//
// Created by clemens on 7/14/24.
//
#include <lwip/sockets.h>
#include <ulog.h>

#include <xbot-service/Io.hpp>
#include <xbot-service/Lock.hpp>
#include <xbot-service/portable/thread.hpp>
#include <xbot/datatypes/XbotHeader.hpp>

namespace xbot::service {

// Keep track of the first registered service.
// All services have a pointer to the next one, so we can loop through all
// of them (for IO)
static ServiceIo* firstService_ = nullptr;

// This Socket is used for all UDP comms for all the services
static XBOT_SOCKET_TYPEDEF udp_socket_{};

/**
 * The IO thread is used to receive data for registered services.
 * On data reception, the handlePacket method will be called for the service.
 */
#ifdef XBOT_ENABLE_STATIC_STACK
static THD_WORKING_AREA(waIoThread, 128000);
#endif
XBOT_THREAD_TYPEDEF io_thread_{};

fd_set socket_set;

static const char* IO_THD_NAME = "xbot-io";

using namespace xbot::service;

void runIo(void* arg) {
  (void)arg;
  while (true) {
    packet::PacketPtr packet = nullptr;

    if (sock::receivePacket(&udp_socket_, &packet)) {
      // Got a packet, check if valid and put it into the processing queue.
      void* buffer = nullptr;
      size_t used_data = 0;
      if (!packet::packetGetData(packet, &buffer, &used_data)) {
        packet::freePacket(packet);
        continue;
      }
      if (used_data < sizeof(datatypes::XbotHeader)) {
        ULOG_ARG_ERROR(&service_id_, "Packet too short to contain header.");
        packet::freePacket(packet);
        continue;
      }

      const auto header = static_cast<datatypes::XbotHeader*>(buffer);
      // Check, if the header size is correct
      if (used_data - sizeof(datatypes::XbotHeader) != header->payload_size) {
        // TODO: In order to allow chaining of xBot packets in the future,
        // this needs to be adapted. (scan and split packets)
        ULOG_ARG_ERROR(&service_id_,
                       "Packet header size does not match actual packet size.");
        packet::freePacket(packet);
        continue;
      }
      bool packet_delivered = false;
      for (ServiceIo* service = firstService_; service != nullptr;
           service = service->next_service_) {
        if (service->service_id_ == header->service_id) {
          Lock lk(&service->state_mutex_);
          if (!service->stopped) {
            // Give packet to service
            service->ioInput(packet);
            packet_delivered = true;
            break;
          }
        }
      }
      if (!packet_delivered) {
        // service not running or not found
        packet::freePacket(packet);
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
bool Io::transmitPacket(packet::PacketPtr packet, uint32_t ip, uint16_t port) {
  return sock::transmitPacket(&udp_socket_, packet, ip, port);
}
bool Io::transmitPacket(packet::PacketPtr packet, const char* ip,
                        uint16_t port) {
  return sock::transmitPacket(&udp_socket_, packet, ip, port);
}
bool Io::getEndpoint(char* ip, size_t ip_len, uint16_t* port) {
  return sock::getEndpoint(&udp_socket_, ip, ip_len, port);
}

bool Io::start() {
  if (!sock::initialize(&udp_socket_, false)) {
    return false;
  }
  return thread::initialize(&io_thread_, runIo, nullptr, &waIoThread,
                            sizeof(waIoThread), IO_THD_NAME);
}

}  // namespace xbot::service
