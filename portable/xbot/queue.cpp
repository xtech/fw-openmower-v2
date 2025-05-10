//
// Created by clemens on 3/21/24.
//
#include <xbot-service/portable/queue.hpp>

bool xbot::service::queue::initialize(QueuePtr queue, size_t queue_length, void* buffer, size_t buffer_size) {
  (void)queue_length;
  chMBObjectInit(queue, static_cast<msg_t*>(buffer), buffer_size / sizeof(msg_t));
  return true;
}

bool xbot::service::queue::queuePopItem(QueuePtr queue, void** result, uint32_t timeout_micros) {
  sysinterval_t timeout = TIME_IMMEDIATE;
  if (timeout_micros > 0) {
    timeout = TIME_US2I(timeout_micros);
  }
  static_assert(sizeof(msg_t) == sizeof(void*));
  return chMBFetchTimeout(queue, reinterpret_cast<msg_t*>(result), timeout) == MSG_OK && *result;
}

bool xbot::service::queue::queuePushItem(QueuePtr queue, void* item) {
  return chMBPostTimeout(queue, reinterpret_cast<msg_t>(item), TIME_MS2I(10)) == MSG_OK;
}

void xbot::service::queue::deinitialize(QueuePtr queue) {
  (void)queue;
}
