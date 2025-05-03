//
// Created by clemens on 10.01.23.
//

#include "rplidar_a1_driver.h"

#include <etl/algorithm.h>
#include <ulog.h>

namespace xbot::driver::lidar {

bool RpLidarA1Driver::StartDriver(UARTDriver *uart, uint32_t baudrate) {
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

  scanning_ = false;
  scan_mode_found_ = false;

  uart_config_.rxend_cb = [](UARTDriver *uartp) {
    chSysLockFromISR();
    RpLidarA1Driver *instance = reinterpret_cast<const UARTConfigEx *>(uartp->config)->context;
    chDbgAssert(instance != nullptr, "instance cannot be null!");
    if (!instance->processing_done_) {
      // This is bad, processing is too slow to keep up with updates!
      // We just read into the same buffer again
      uint8_t *next_recv_buffer =
          (instance->processing_buffer_ == instance->recv_buffer1_) ? instance->recv_buffer2_ : instance->recv_buffer1_;
      uartStartReceiveI(uartp, RECV_BUFFER_SIZE, next_recv_buffer);
    } else {
      // Swap buffers and read into the next one
      // Get the pointer to the receiving buffer (it's not the processing buffer)
      uint8_t *next_recv_buffer = instance->processing_buffer_;
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
  processing_thread_->name = "RpLidarDriver";
#endif

  uartStartReceive(uart, RECV_BUFFER_SIZE, recv_buffer1_);
  return true;
}

void RpLidarA1Driver::SetDataCallback(const RpLidarA1Driver::DataCallback &function) {
  data_callback_ = function;
}

void RpLidarA1Driver::SendCommand(const uint8_t *data, size_t size) {
  send_raw(data, size);
}

bool RpLidarA1Driver::send_raw(const void *data, size_t size) {
  // size_t checksum_size = 1;
  // uint8_t checksum = 0;
  // for (size_t i = 0; i < size; i++) {
  // checksum ^= ((uint8_t*)data)[i];
  // }
  uartSendFullTimeout(uart_, &size, data, TIME_INFINITE);
  // uartSendFullTimeout(uart_, &checksum_size, &checksum, TIME_INFINITE);
  return true;
}
void RpLidarA1Driver::ResetParserState() {
}
size_t RpLidarA1Driver::ProcessBytes(const uint8_t *buffer, size_t len) {
  static const uint8_t SYNC_BYTE1 = 0xA5;
  static const uint8_t SYNC_BYTE2 = 0x5A;
  size_t processed = 0;

  while (processed + 4 <= len) {  // Need at least 4 bytes for header
    // Look for sync bytes
    if (buffer[processed] == SYNC_BYTE1 && buffer[processed + 1] == SYNC_BYTE2) {
      // Found potential packet start
      [[maybe_unused]] uint8_t packet_type = buffer[processed + 2];
      [[maybe_unused]] uint8_t packet_size = buffer[processed + 3];
    }
    processed++;
  }

  return 1;
}

bool RpLidarA1Driver::StartScan() {
  static const uint8_t START_SCAN_CMD[] = {0xA5, 0x20};  // Start scan command
  // static const uint8_t START_MOTOR_CMD[] = {0xA5, 0xF0, 0x02, 0x94, 0x02, 0x0F};  // Motor PWM command

  // Start the motor first
  // if (!send_raw(START_MOTOR_CMD, sizeof(START_MOTOR_CMD))) {
  // return false;
  // }

  // Small delay to let motor start spinning
  chThdSleepMilliseconds(500);

  // Send scan command
  if (!send_raw(START_SCAN_CMD, sizeof(START_SCAN_CMD))) {
    return false;
  }

  return true;
}
bool RpLidarA1Driver::ResetLidar() {
  static const uint8_t CMD[] = {0xA5, 0x40};
  send_raw(CMD, sizeof(CMD));
  return true;
}
bool RpLidarA1Driver::QueryScanMode(uint16_t scan_mode_id) {
  static const uint8_t CMD[] = {0xA5,
                                0x84,
                                0x7F,
                                2,
                                (uint8_t)((scan_mode_id > 8) & 0xFF),
                                (uint8_t)(scan_mode_id & 0xFF)};  // Get scan mode count
  send_raw(CMD, sizeof(CMD));
  return true;
}

bool RpLidarA1Driver::QueryScanModeCount() {
  scan_mode_count_ = 0;
  scan_modes_queried_ = 0;
  ULOG_INFO("Querying lidar scan mode count");
  static const uint8_t CMD[] = {0xA5, 0x50};  // Get scan mode count
  send_raw(CMD, sizeof(CMD));
  return true;
}

void RpLidarA1Driver::threadFunc() {
  ResetLidar();
  uint32_t last_ndtr = 0;
  while (!stopped_) {
    // If not scanning,
    if (!scanning_) {
      if (!scan_mode_found_) {
        if (chVTTimeElapsedSinceX(last_scan_mode_request_) > TIME_MS2I(1000)) {
          last_scan_mode_request_ = chVTGetSystemTimeX();
          if (scan_mode_count_ == 0) {
            QueryScanModeCount();
          } else {
            QueryScanMode(scan_modes_queried_ + 1);
          }
        }
      }
    }

    // Wait for data to arrive
    bool timeout = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(RECV_TIMEOUT_MILLIS)) == 0;
    if (timeout) {
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
        size_t not_received_len = uartStopReceiveI(uart_);
        if (not_received_len != UART_ERR_NOT_ACTIVE) {
          // Uart was still receiving, so the buffer length is not complete
          processing_buffer_len_ = RECV_BUFFER_SIZE - not_received_len;
        } else {
          // Uart was not active.
          // This should not happen, but could during debug when pausing the chip
          // we ignore this buffer, but carry on as usual
          processing_buffer_len_ = 0;
        }
        uint8_t *next_recv_buffer = processing_buffer_;
        uartStartReceiveI(uart_, RECV_BUFFER_SIZE, next_recv_buffer);
        processing_buffer_ = (processing_buffer_ == recv_buffer1_) ? recv_buffer2_ : recv_buffer1_;
        processing_done_ = false;
      }
      // allow ISR again
      chSysUnlock();
    }
    if (processing_buffer_len_ > 0) {
      ProcessBytes(processing_buffer_, processing_buffer_len_);
    }
    last_ndtr = 0;
    processing_buffer_len_ = 0;
    processing_done_ = true;
  }
}

void RpLidarA1Driver::threadHelper(void *instance) {
  auto *lidar_interface = static_cast<RpLidarA1Driver *>(instance);
  lidar_interface->threadFunc();
}

void RpLidarA1Driver::TriggerDataCallback() {
  if (data_callback_) {
    data_callback_();
  }
}

}  // namespace xbot::driver::lidar
