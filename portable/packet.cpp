//
// Created by clemens on 3/21/24.
//

#include <lwip/memp.h>

#include <cstring>
#include <xbot-service/portable/packet.hpp>
#include <xbot/config.hpp>

using namespace xbot::service::packet;

LWIP_MEMPOOL_DECLARE(xbot_packet_pool, 8, sizeof(Packet), "xbot packets")

PacketPtr xbot::service::packet::allocatePacket() {
  auto buffer = static_cast<Packet *>(LWIP_MEMPOOL_ALLOC(xbot_packet_pool));
#ifdef DEBUG_MEM
#warning DEBUG_MEM enabled, disable for performance
  memset(buffer, 0x42, sizeof(Packet));
#endif
  // Set used data to 0, because packet is empty (byte contents might be random
  // at this point though)
  buffer->used_data = 0;
  return buffer;
}

void xbot::service::packet::freePacket(PacketPtr packet_ptr) {
  LWIP_MEMPOOL_FREE(xbot_packet_pool, packet_ptr);
}

bool xbot::service::packet::packetAppendData(PacketPtr packet,
                                             const void *buffer, size_t size) {
  if (packet == nullptr) return false;
  // Data won't fit.
  if (size + packet->used_data > config::max_packet_size) return false;

  memcpy(packet->buffer + packet->used_data, buffer, size);
  packet->used_data += size;

  return true;
}

bool xbot::service::packet::packetGetData(PacketPtr packet, void **buffer,
                                          size_t *size) {
  if (packet == nullptr) return false;
  *buffer = packet->buffer;
  *size = packet->used_data;
  return true;
}
