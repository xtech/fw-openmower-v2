//
// Created by clemens on 3/25/24.
//

#include <lwip/memp.h>
#include <stdio.h>
#include <ulog.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <xbot-service/portable/mutex.hpp>
#include <xbot-service/portable/system.hpp>
#include <xbot/packet_impl.hpp>

#include "ch.h"

namespace xbot::service::system {
XBOT_MUTEX_TYPEDEF ulog_mutex_;

uint8_t node_id_[16]{};

void initSystem(uint32_t node_id) {
  LWIP_MEMPOOL_INIT(xbot_packet_pool);
  xbot::service::mutex::initialize(&ulog_mutex_);

  // Init system with a hardcoded node_id or get a random one.
  if (node_id != 0) {
    node_id_[0] = node_id >> 8;
    node_id_[1] = node_id & 0xFF;
  } else {
    uint32_t w0 = 1;
    uint32_t w1 = 2;
    uint32_t w2 = 3;
    node_id_[0] = w0 & 0xFF;
    node_id_[1] = (w0 >> 8) & 0xFF;
    node_id_[2] = (w0 >> 16) & 0xFF;
    node_id_[3] = (w0 >> 24) & 0xFF;
    node_id_[4] = w1 & 0xFF;
    node_id_[5] = (w1 >> 8) & 0xFF;
    node_id_[6] = (w1 >> 16) & 0xFF;
    node_id_[7] = (w1 >> 24) & 0xFF;
    node_id_[8] = w2 & 0xFF;
    node_id_[9] = (w2 >> 8) & 0xFF;
    node_id_[10] = (w2 >> 16) & 0xFF;
    node_id_[11] = (w2 >> 24) & 0xFF;
    node_id_[12] = 0;
    node_id_[13] = 0;
    node_id_[14] = 0;
    node_id_[15] = 0;
  }

  bool zero_id = true;
  for (const uint8_t i : node_id_) {
    zero_id &= (i == 0);
  }
  if (zero_id) {
    // Error handler
    while (true)
      ;
  }

  // Init ULOG but don't register loggers.
  // This will be application dependend. E.g. a CLI app probably doesnt want us
  // to log to stdout by default. Also headless targets will want to log via
  // network.
  ULOG_INIT();
}

uint32_t getTimeMicros() {
  return TIME_I2US(chVTGetSystemTimeX());
}

bool getNodeId(uint8_t *id, size_t id_len) {
  if (id_len != sizeof(node_id_)) return false;
  memcpy(id, node_id_, id_len);
  return true;
}
}  // namespace xbot::service::system

void ulog_lock_mutex() {
  xbot::service::mutex::lockMutex(&xbot::service::system::ulog_mutex_);
}

void ulog_unlock_mutex() {
  xbot::service::mutex::unlockMutex(&xbot::service::system::ulog_mutex_);
}
