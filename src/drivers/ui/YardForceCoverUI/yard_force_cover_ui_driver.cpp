//
// Created by clemens on 4/11/25.
//

#include "yard_force_cover_ui_driver.hpp"

#include <sys/unistd.h>
#include <ulog.h>

#include "COBS.h"
#include "ui_board.h"

static constexpr uint8_t EVT_PACKET_RECEIVED = 1;

void YardForceCoverUIDriver::Start(UARTDriver *uart) {
  if (uart == nullptr) {
    chDbgAssert(uart != nullptr, "UART cannot be null");
    return;
  }
  if (thread_ != nullptr) {
    ULOG_ERROR("Started YardForce CoverUI Driver twice!");
    return;
  }

  // Init the UART
  this->uart_ = uart;
  uart_config_.speed = 115200;
  uart_config_.context = this;
  uart_config_.rxchar_cb = YardForceCoverUIDriver::UartRxChar;
  if (uartStart(uart, &uart_config_) != MSG_OK) {
    ULOG_ERROR("Error starting UART");
    return;
  }

  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "YardForceUIDriver";
#endif
}

void YardForceCoverUIDriver::ThreadHelper(void *instance) {
  auto *i = static_cast<YardForceCoverUIDriver *>(instance);
  i->ThreadFunc();
}
void YardForceCoverUIDriver::UartRxChar(UARTDriver *driver, uint16_t data) {
  YardForceCoverUIDriver *instance = reinterpret_cast<const UARTConfigEx *>(driver->config)->context;
  chDbgAssert(instance != nullptr, "instance cannot be null!");
  chSysLockFromISR();
  if (instance->processing) {
    chSysUnlockFromISR();
    return;
  }
  instance->buffer[instance->buffer_fill++] = data;
  if (data == 0) {
    chEvtSignalI(instance->thread_, EVT_PACKET_RECEIVED);
  }
  chSysUnlockFromISR();
}
void YardForceCoverUIDriver::ThreadFunc() {
  while (true) {
    bool timeout = chEvtWaitAnyTimeout(EVT_PACKET_RECEIVED, TIME_S2I(1)) == 0;
    if (timeout) {
      RequestFWVersion();
      continue;
    }
    chSysLock();
    // Forbid packet reception
    processing = true;
    chSysUnlock();
    if (buffer_fill > 0) {
      ProcessPacket();
    }
    buffer_fill = 0;
    chSysLock();
    // Allow packet reception
    processing = false;
    chSysUnlock();
  }
}
void YardForceCoverUIDriver::ProcessPacket() {
  if (buffer_fill == 0) return;
  uint16_t size = cobs_.decode((uint8_t *)buffer, buffer_fill - 1, encode_decode_buf_);

  u_int16_t *crc_pointer = (uint16_t *)(encode_decode_buf_ + (size - 2));
  u_int16_t readcrc = *crc_pointer;

  // check structure size
  if (size < 4) {
    return;
  }

  // check the CRC
  CRC16.reset();
  etl::begin(encode_decode_buf_);
  CRC16.add(encode_decode_buf_, encode_decode_buf_ + size - 2);
  uint16_t crc = CRC16.value();

  if (crc != readcrc) return;

  if (buffer[0] == Get_Version && size == sizeof(struct msg_get_version)) {
    struct msg_get_version *msg = (struct msg_get_version *)buffer;
    board_found = true;
  } else if (buffer[0] == Get_Button && size == sizeof(struct msg_event_button)) {
    struct msg_event_button *msg = (struct msg_event_button *)buffer;

  } else if (buffer[0] == Get_Emergency && size == sizeof(struct msg_event_emergency)) {
    struct msg_event_emergency *msg = (struct msg_event_emergency *)buffer;
    stock_ui_emergency_state = msg->state;
  } else if (buffer[0] == Get_Rain && size == sizeof(struct msg_event_rain)) {
    struct msg_event_rain *msg = (struct msg_event_rain *)buffer;
    stock_ui_rain = (msg->value < llhl_config.rain_threshold);
  } else if (buffer[0] == Get_Subscribe && size == sizeof(struct msg_event_subscribe)) {
    struct msg_event_subscribe *msg = (struct msg_event_subscribe *)buffer;
    ui_topic_bitmask = msg->topic_bitmask;
    ui_interval = msg->interval;
  }
}
void YardForceCoverUIDriver::sendUIMessage(void *msg, size_t size) {
  // packages need to be at least 1 byte of type, 1 byte of data and 2 bytes of CRC
  if (size < 4) {
    return;
  }
  auto *data_pointer = static_cast<uint8_t *>(msg);

  // calculate the CRC
  CRC16.reset();
  CRC16.add(data_pointer, data_pointer + size - 2);
  uint16_t crc = CRC16.value();
  data_pointer[size - 1] = (crc >> 8) & 0xFF;
  data_pointer[size - 2] = crc & 0xFF;

  if (cobs_.getEncodedBufferSize(size) >= sizeof(encode_decode_buf_)) {
    ULOG_ERROR("out_but_ size too small!");
    return;
  }

  size_t encoded_size = cobs_.encode(data_pointer, size, encode_decode_buf_);
  encode_decode_buf_[encoded_size] = 0;
  encoded_size++;
  uartSendFullTimeout(uart_, &encoded_size, encode_decode_buf_, TIME_INFINITE);
}
void YardForceCoverUIDriver::RequestFWVersion() {
  msg_get_version msg{};
  msg.type = Get_Version;
  sendUIMessage(&msg, sizeof(msg));
}
