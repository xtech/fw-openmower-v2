//
// Created by clemens on 5/12/24.
//

#ifndef PACKET_IMPL_HPP
#define PACKET_IMPL_HPP

#include <lwip/memp.h>

#include <xbot/config.hpp>

LWIP_MEMPOOL_PROTOTYPE(xbot_packet_pool);

namespace xbot::service::packet {
struct Packet {
  size_t used_data;
  uint8_t buffer[xbot::config::max_packet_size];
};
}  // namespace xbot::service::packet

#define XBOT_PACKET_TYPEDEF Packet

#endif  // PACKET_IMPL_HPP
