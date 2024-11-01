//
// Created by clemens on 3/21/24.
//
#include <lwip/sockets.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <xbot-service/portable/socket.hpp>

#include "xbot/config.hpp"

using namespace xbot::service::sock;
using namespace xbot::service::packet;

bool xbot::service::sock::initialize(SocketPtr socket_ptr,
                                     bool bind_multicast) {
  *socket_ptr = -1;
  // Create a UDP socket

  int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd < 0) {
    return false;
  }

  // Set receive timeout
  {
    timeval opt{};
    opt.tv_sec = 1;
    opt.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &opt, sizeof(opt)) < 0) {
      close(fd);
      return false;
    }
  }

  // Create a pointer to the fd and return it.
  *socket_ptr = fd;
  return true;
}

void xbot::service::sock::deinitialize(SocketPtr socket) {
  if (socket != nullptr) {
    auto fd_ptr = socket;
    close(*fd_ptr);
    delete fd_ptr;
  }
}

bool xbot::service::sock::subscribeMulticast(SocketPtr socket, const char* ip) {
  return false;
}

bool xbot::service::sock::receivePacket(SocketPtr socket, PacketPtr* packet) {
  const PacketPtr pkt = allocatePacket();
  sockaddr_in fromAddr{};
  socklen_t fromLen = sizeof(fromAddr);
  const ssize_t recvLen =
      recvfrom(*socket, pkt->buffer, config::max_packet_size, 0,
               reinterpret_cast<struct sockaddr*>(&fromAddr), &fromLen);
  if (recvLen < 0) {
    freePacket(pkt);
    return false;
  }
  pkt->used_data = recvLen;
  *packet = pkt;
  return true;
}

bool xbot::service::sock::transmitPacket(SocketPtr socket, PacketPtr packet,
                                         uint32_t ip, uint16_t port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(ip);

  sendto(*static_cast<int*>(socket), packet->buffer, packet->used_data, 0,
         reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));

  freePacket(packet);

  return true;
}

bool xbot::service::sock::transmitPacket(SocketPtr socket, PacketPtr packet,
                                         const char* ip, uint16_t port) {
  return transmitPacket(socket, packet, ntohl(inet_addr(ip)), port);
}

bool xbot::service::sock::getEndpoint(SocketPtr socket, char* ip, size_t ip_len,
                                      uint16_t* port) {
  if (socket == nullptr || ip == nullptr || port == nullptr) return false;

  sockaddr_in addr{};
  socklen_t addrLen = sizeof(addr);

  if (getsockname(*static_cast<int*>(socket),
                  reinterpret_cast<sockaddr*>(&addr), &addrLen) < 0)
    return false;

  const char* addrStr = inet_ntoa(netif_default->ip_addr.addr);
  if (strlen(addrStr) >= ip_len) return false;

  strncpy(ip, addrStr, ip_len);

  *port = ntohs(addr.sin_port);

  return true;
}

bool xbot::service::sock::closeSocket(SocketPtr socket) {
  if (socket == nullptr) return true;
  if (close(*socket) < 0) {
    return false;
  }
  return true;
}
