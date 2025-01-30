//
// Created by clemens on 10.01.23.
//

#include "gps_driver.h"

#include <etl/algorithm.h>

#include <cmath>

namespace xbot::driver::gps {

void GpsDriver::RawDataInput(uint8_t *data, size_t size) {
  if(!IsRawMode())
    return;
  send_raw(data, size);
}

bool GpsDriver::StartDriver(UARTDriver *uart, uint32_t baudrate) {
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

  uart_config_.rxend_cb = [](UARTDriver *uartp) {
    chSysLockFromISR();
    GpsDriver *instance = reinterpret_cast<const UARTConfigEx *>(uartp->config)->context;
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
  processing_thread_->name = "GpsDriver";
#endif

  uartStartReceive(uart, RECV_BUFFER_SIZE, recv_buffer1_);
  return true;
}

void GpsDriver::SetStateCallback(const GpsDriver::StateCallback &function) {
  state_callback_ = function;
}

bool GpsDriver::send_raw(const void *data, size_t size) {
  chMtxLock(&mutex_);
  uartSendFullTimeout(uart_, &size, data, TIME_INFINITE);
  chMtxUnlock(&mutex_);
  return true;
}

void GpsDriver::threadFunc() {
  uint32_t last_ndtr = 0;
  while (!stopped_) {
    // Wait for data to arrive
    bool timeout = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(RECV_TIMEOUT_MILLIS)) == 0;
    if (timeout) {
      // If there is still reception going on, wait for the timeout again
      if(last_ndtr != uart_->dmarx->stream->NDTR) {
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
      if(IsRawMode()) {
        RawDataOutput(processing_buffer_, processing_buffer_len_);
      }
    }
    last_ndtr = 0;
    processing_buffer_len_ = 0;
    processing_done_ = true;
  }
}

void GpsDriver::threadHelper(void *instance) {
  auto *gps_interface = static_cast<GpsDriver *>(instance);
  gps_interface->threadFunc();
}

void GpsDriver::SendRTCM(const uint8_t *data, size_t size) {
  send_raw(data, size);
}

void GpsDriver::TriggerStateCallback() {
  if (state_callback_) {
    state_callback_(gps_state_);
  }
}
}  // namespace xbot::driver::gps
