//
// Created by clemens on 08.11.24.
//

#include "VescDriver.h"

#include "buffer.h"
#include "crc.h"
#include "datatypes.h"

static constexpr uint32_t EVT_ID_RECEIVED = 1;
static constexpr uint32_t EVT_ID_EXPECT_PACKET = 2;

namespace xbot::driver::esc {
VescDriver::VescDriver() {
  latest_state.status = ESCState::ESCStatus::ESC_STATUS_DISCONNECTED;
};
bool VescDriver::StartDriver(UARTDriver* uart, uint32_t baudrate) {
  chDbgAssert(stopped_, "don't start the driver twice");
  chDbgAssert(uart != nullptr, "need to provide a driver");
  if (!stopped_) {
    return false;
  }
  this->uart_ = uart;
  uart_config_.speed = baudrate;
  uart_config_.context = this;
  bool uartStarted = uartStart(uart, &uart_config_) == MSG_OK;
  if (!uartStarted) {
    return false;
  }

  uart_config_.rxend_cb = [](UARTDriver* uartp) {
    chSysLockFromISR();
    VescDriver* instance = reinterpret_cast<const UARTConfigEx*>(uartp->config)->context;
    chDbgAssert(instance != nullptr, "instance cannot be null!");
    if (!instance->processing_done_) {
      // This is bad, processing is too slow to keep up with updates!
      // We just read into the same buffer again
      uint8_t* next_recv_buffer =
          (instance->processing_buffer_ == instance->recv_buffer1_) ? instance->recv_buffer2_ : instance->recv_buffer1_;
      uartStartReceiveI(uartp, RECV_BUFFER_SIZE, next_recv_buffer);
    } else {
      // Swap buffers and read into the next one
      // Get the pointer to the receiving buffer (it's not the processing buffer)
      uint8_t* next_recv_buffer = instance->processing_buffer_;
      uartStartReceiveI(uartp, RECV_BUFFER_SIZE, next_recv_buffer);
      instance->processing_buffer_ =
          (instance->processing_buffer_ == instance->recv_buffer1_) ? instance->recv_buffer2_ : instance->recv_buffer1_;
      instance->processing_buffer_len_ = RECV_BUFFER_SIZE;
      instance->processing_done_ = false;
      // Signal the processing thread
      if (instance->processing_thread_) {
        chEvtSignalI(instance->processing_thread_, 1);
      }
    }
    chSysUnlockFromISR();
  };

  stopped_ = false;
  processing_thread_ = chThdCreateStatic(&thd_wa_, sizeof(thd_wa_), NORMALPRIO, threadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "VESCDriver";
#endif

  uartStartReceive(uart, RECV_BUFFER_SIZE, recv_buffer1_);
  return true;
}
void VescDriver::SetStatusUpdateInterval(uint32_t interval_millis) {
  (void)interval_millis;
}
void VescDriver::SetStateCallback(const StateCallback& function) {
  state_callback_ = function;
}

void VescDriver::ProcessPayload() {
  // check size, reported payload should be the same as received bytes + 1 byte header + byte length + 2 byte CRC + 1
  // trailer
  uint8_t payload_length = working_buffer_[1];
  if (payload_length + 5 != working_buffer_fill_) {
    // reset
    working_buffer_fill_ = 0;
    return;
  }
  // first byte needs to be 0x02, last byte needs to be 0x03
  if (working_buffer_[0] != 0x02 || working_buffer_[working_buffer_fill_ - 1] != 0x03) {
    working_buffer_fill_ = 0;
    return;
  }
  // Check the CRC
  uint16_t expectedCRC = crc16(working_buffer_ + 2, payload_length);
  uint16_t actualCRC =
      static_cast<uint16_t>(working_buffer_[working_buffer_fill_ - 3]) << 8 | working_buffer_[working_buffer_fill_ - 2];

  if (expectedCRC != actualCRC) {
    working_buffer_fill_ = 0;
    return;
  }

  uint8_t packet_id = working_buffer_[2];
  int32_t index = 0;
  // 3 = start of actual payload
  uint8_t* message = working_buffer_ + 3;
  switch (packet_id) {
    case COMM_FW_VERSION:  // Structure defined here:
                           // https://github.com/vedderb/bldc/blob/43c3bbaf91f5052a35b75c2ff17b5fe99fad94d1/commands.c#L164

      latest_state.fw_major = message[index++];
      latest_state.fw_minor = message[index++];
      break;
    case COMM_GET_VALUES:  // Structure defined here:
                           // https://github.com/vedderb/bldc/blob/43c3bbaf91f5052a35b75c2ff17b5fe99fad94d1/commands.c#L164

      latest_state.temperature_pcb =
          buffer_get_float16(message, 10.0, &index);  // 2 bytes - mc_interface_temp_fet_filtered()
      latest_state.temperature_motor = buffer_get_float16(message, 10.0,
                                                          &index);  // 2 bytes - mc_interface_temp_motor_filtered()
      index += 4;  // 4 bytes - mc_interface_read_reset_avg_motor_current()
      latest_state.current_input = buffer_get_float32(message, 100.0,
                                                      &index);  // 4 bytes - mc_interface_read_reset_avg_input_current()
      index += 4;                                               // Skip 4 bytes - mc_interface_read_reset_avg_id()
      index += 4;                                               // Skip 4 bytes - mc_interface_read_reset_avg_iq()
      latest_state.duty_cycle = buffer_get_float16(message, 1000.0,
                                                   &index);         // 2 bytes - mc_interface_get_duty_cycle_now()
      latest_state.rpm = buffer_get_float32(message, 1.0, &index);  // 4 bytes - mc_interface_get_rpm()
      latest_state.voltage_input = buffer_get_float16(message, 10.0, &index);  // 2 bytes - GET_INPUT_VOLTAGE()
      index += 4;  // 4 bytes - mc_interface_get_amp_hours(false)
      index += 4;  // 4 bytes - mc_interface_get_amp_hours_charged(false)
      index += 4;  // 4 bytes - mc_interface_get_watt_hours(false)
      index += 4;  // 4 bytes - mc_interface_get_watt_hours_charged(false)
      latest_state.tacho = buffer_get_int32(message,
                                            &index);  // 4 bytes - mc_interface_get_tachometer_value(false)
      latest_state.tacho_absolute = buffer_get_int32(message,
                                                     &index);  // 4 bytes - mc_interface_get_tachometer_abs_value(false)
      latest_state.status = static_cast<mc_fault_code>(message[index++]) != FAULT_CODE_NONE
                                ? ESCState::ESCStatus::ESC_STATUS_ERROR
                                : ESCState::ESCStatus::ESC_STATUS_OK;
      index += 4;  // 4 bytes - mc_interface_get_pid_pos_now()
      latest_state.direction = latest_state.rpm < 0;
      break;
    default:
      // ignore
      break;
  }

  if (state_callback_) state_callback_(latest_state);

  working_buffer_fill_ = 0;
}
bool VescDriver::ProcessBytes(uint8_t* buffer, size_t len) {
  if (len == 0) {
    // expect more data
    return true;
  }

  while (len > 0) {
    switch (working_buffer_fill_) {
      case 0: {
        if (len == 0) {
          continue;
        }
        // buffer empty, looking for 0x02
        const auto header_start = static_cast<uint8_t*>(memchr(buffer, 0x02, len));
        if (header_start == nullptr) {
          // reject the whole input, we don't have 0x02
          len = 0;
          continue;
        }
        // Throw away all bytes before the header start
        len -= (header_start - buffer);
        buffer = header_start;
        working_buffer_[working_buffer_fill_++] = *buffer;
        buffer++;
        len--;
      } break;
      case 1: {
        // we have started the packet, read the length (it's a '2' packet, so length is one byte and cannot exceed our
        // buffer size)
        working_buffer_[working_buffer_fill_++] = *buffer;
        buffer++;
        len--;
      } break;
      default: {
        // take until the payload is complete
        size_t payload_length = working_buffer_[1];
        // remove the two header bytes
        size_t payload_in_buffer = (working_buffer_fill_ - 2);

        // Take the remaining payload + CRC + trailer
        size_t bytes_to_take = etl::min(len, payload_length - payload_in_buffer + 3);
        memcpy(&working_buffer_[working_buffer_fill_], buffer, bytes_to_take);
        working_buffer_fill_ += bytes_to_take;
        buffer += bytes_to_take;
        len -= bytes_to_take;

        if (payload_length + 5 == working_buffer_fill_) {
          // finished reading payload, process it
          ProcessPayload();
          // done with this packet, reset parser
          working_buffer_fill_ = 0;
        }
      }
    }
  }

  // we expect more data if the working buffer is not clean
  return working_buffer_fill_ != 0;
}
void VescDriver::SendPacket() {
  // header starts with a 2
  payload_buffer_.prepend[3] = 2;
  chDbgAssert(payload_buffer_.payload_length < 256, "Max payload size of 255");
  payload_buffer_.prepend[4] = payload_buffer_.payload_length;
  uint16_t crcPayload = crc16(payload_buffer_.payload, payload_buffer_.payload_length);
  payload_buffer_.payload[payload_buffer_.payload_length] = static_cast<uint8_t>(crcPayload >> 8);
  payload_buffer_.payload[payload_buffer_.payload_length + 1] = static_cast<uint8_t>(crcPayload & 0xFF);
  payload_buffer_.payload[payload_buffer_.payload_length + 2] = 3;
  size_t total_size = payload_buffer_.payload_length + 5;
  uartSendFullTimeout(uart_, &total_size, &payload_buffer_.prepend[3], TIME_INFINITE);
}

void VescDriver::RequestStatus() {
  if (IsRawMode()) {
    // ignore when a raw data stream is connected
    return;
  }
  chMtxLock(&mutex_);
  payload_buffer_.payload_length = 1;
  payload_buffer_.payload[0] = COMM_GET_VALUES;
  SendPacket();
  // Signal that we are waiting for a response, so the receiving thread becomes active
  chEvtSignal(processing_thread_, EVT_ID_EXPECT_PACKET);
  chMtxUnlock(&mutex_);
}
void VescDriver::SetDuty(float duty) {
  if (IsRawMode()) {
    // ignore when a raw data stream is connected
    return;
  }

  chMtxLock(&mutex_);
  payload_buffer_.payload_length = 5;
  payload_buffer_.payload[0] = COMM_SET_DUTY;
  int32_t index = 1;
  buffer_append_int32(payload_buffer_.payload, static_cast<int32_t>(duty * 100000), &index);
  SendPacket();
  chMtxUnlock(&mutex_);
}
void VescDriver::RawDataInput(uint8_t* data, size_t size) {
  if (!IsRawMode()) {
    return;
  }
  // Lock mutex so that during transmission we don't start a second one
  chMtxLock(&mutex_);
  uartSendFullTimeout(uart_, &size, data, TIME_INFINITE);
  // Signal that we are waiting for a response, so the receiving thread becomes active
  chEvtSignal(processing_thread_, EVT_ID_EXPECT_PACKET);
  chMtxUnlock(&mutex_);
}

void VescDriver::threadFunc() {
  bool expects_packet = false;
  uint32_t last_ndtr = 0;
  while (!stopped_) {
    uint32_t events;
    if (expects_packet) {
      // read with timeout, so that if the packet is too short to fill the whole buffer (and thereby interrupting this),
      // we will still process it
      events = chEvtWaitOneTimeout(ALL_EVENTS, TIME_MS2I(10));
    } else {
      // we don't expect a packet, so in order to not loop all the time for empty buffers,
      // wait an infinite time (or status_request_millis_ if we need to request status periodically)
      if (status_request_millis_ > 0) {
        events = chEvtWaitOneTimeout(ALL_EVENTS, TIME_MS2I(status_request_millis_));
      } else {
        events = chEvtWaitOneTimeout(ALL_EVENTS, TIME_INFINITE);
      }
    }

    bool expects_packet_received = (events & EVT_ID_EXPECT_PACKET) != 0;
    // handle the "expect a packet" event by setting the flag and continuing
    // this will wait for the reception with a timeout
    if (expects_packet_received) {
      expects_packet = true;
      last_ndtr = 0;
      continue;
    }

    bool packet_received = (events & EVT_ID_RECEIVED) != 0;
    if (!packet_received) {
      // If there is still reception going on, wait for the timeout again
      if (last_ndtr != uart_->dmarx->stream->NDTR) {
        last_ndtr = uart_->dmarx->stream->NDTR;
        continue;
      }

      // If timeout, we take the buffer and restart the reception
      // Else, the ISR has already prepared the buffer for us
      // Lock the core to prevent the RX interrupt from firing
      chSysLock();
      // Check, that processing_done_ is still true
      // (in case ISR happened between the timeout and now, this will be set to false by ISR)
      if (processing_done_) {
        // Stop reception and get the partial received length
        uint8_t* next_recv_buffer = processing_buffer_;
        chDbgAssert(uart_->dmarx->stream->M0AR != (uint32_t)(next_recv_buffer), "invalid buffer");
        size_t not_received_len = uartStopReceiveI(uart_);
        uartStartReceiveI(uart_, RECV_BUFFER_SIZE, next_recv_buffer);
        if (not_received_len != UART_ERR_NOT_ACTIVE) {
          // Uart was still receiving, so the buffer length is not complete
          processing_buffer_len_ = RECV_BUFFER_SIZE - not_received_len;
        } else {
          // Uart was not active.
          // This should not happen, but could during debug when pausing the chip
          // we ignore this buffer, but carry on as usual
          processing_buffer_len_ = 0;
        }
        processing_buffer_ = (processing_buffer_ == recv_buffer1_) ? recv_buffer2_ : recv_buffer1_;
        processing_done_ = false;
      }
      // allow ISR again
      chSysUnlock();
    }
    if (processing_buffer_len_ > 0) {
      // Process and call callbacks
      if (!IsRawMode()) {
        if (!ProcessBytes(processing_buffer_, processing_buffer_len_) && !IsRawMode()) {
          // we don't expect more data, so we can wait for the next request
          expects_packet = false;
        }
      } else {
        // Send to raw data receiver
        RawDataOutput(processing_buffer_, processing_buffer_len_);
      }
      // In any case (partial payload received), the processing buffer needs to be reset
      processing_buffer_len_ = 0;
    }
    processing_done_ = true;
  }
}

void VescDriver::threadHelper(void* instance) {
  auto* i = static_cast<VescDriver*>(instance);
  i->threadFunc();
}
}  // namespace xbot::driver::esc
