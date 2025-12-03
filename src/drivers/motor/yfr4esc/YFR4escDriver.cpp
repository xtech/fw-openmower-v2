//
// Created by GitHub Copilot on 16.08.25.
//

#include "YFR4escDriver.h"

#include <cstdio>
#include <cstring>

#include "cobs.h"
#include "crc16_hw.hpp"
#define LOG_TAG_STR "YFR4esc"
#include "ulog_rate_limit.hpp"

namespace xbot::driver::motor {

using namespace yfr4esc;

// Event when the RX DMA buffer wrapped (full) and was re-armed.
static constexpr uint32_t EVT_RX_DMA_WRAP = 1;

bool YFR4escDriver::SetUART(UARTDriver *uart, uint32_t baudrate) {
  chDbgAssert(!IsStarted(), "Only set UART when the driver is stopped");
  chDbgAssert(uart != nullptr, "need to provide a driver");
  if (IsStarted()) return false;

  uart_ = uart;
  uart_config_.speed = baudrate;
  uart_config_.context = this;

  return true;
}

void YFR4escDriver::ProcessDecodedPacket(uint8_t *packet, size_t len) {
  // Packages have to be at least 1 byte of type + 1 byte of data + 2 bytes of CRC
  if (len < 4) {
    ULOGT_EVERY_MS(WARNING, 500, "Decoded packet too short (%zu bytes). Dropping!", len);
    return;
  }

  // Check CRC (on-wire is little-endian; STM32 is little-endian too)
  uint16_t crc_rx;
  memcpy(&crc_rx, &packet[len - 2], sizeof(crc_rx));  // memcpy avoids potential unaligned access
  uint16_t crc_calc = yfr4esc_crc::crc16_ccitt_false(packet, len - 2);
  if (crc_rx != crc_calc) {
    ULOGT_EVERY_MS(WARNING, 500, "CRC mismatch (rx=0x%04X calc=0x%04X len=%u). Dropping!", crc_rx, crc_calc,
                   (unsigned)len);
    return;
  }

  uint8_t msg_type = packet[0];
  switch (msg_type) {
    case MessageType::STATUS: {
      if (len != sizeof(StatusPacket)) {
        ULOGT_EVERY_MS(WARNING, 500, "Status packet length mismatch (%u != %zu). Dropping!", (unsigned)len,
                       sizeof(StatusPacket));
        break;
      }
      StatusPacket sp{};
      memcpy(&sp, packet, sizeof(StatusPacket));

      latest_state_.fw_major = sp.fw_version_major;
      latest_state_.fw_minor = sp.fw_version_minor;
      latest_state_.temperature_pcb = static_cast<float>(sp.temperature_pcb);
      latest_state_.current_input = static_cast<float>(sp.current_input);
      latest_state_.duty_cycle = static_cast<float>(sp.duty_cycle);
      latest_state_.direction = sp.direction ? 1.0f : 0.0f;
      latest_state_.tacho = sp.tacho;
      latest_state_.tacho_absolute = sp.tacho_absolute;
      latest_state_.rpm = static_cast<float>(sp.rpm);
      latest_state_.status =
          sp.fault_code == 0 ? ESCState::ESCStatus::ESC_STATUS_OK : ESCState::ESCStatus::ESC_STATUS_ERROR;

      if (sp.fault_code & (0b1 << FaultBit::UNINITIALIZED)) {
        SendSettings();
      }

      /* Debugging
      ULOGT_EVERY_MS(
          INFO, 1000, "Status: fw=%u.%u temp=%.2fC I_in=%.3fA duty=%.3f dir=%u tacho=%u tacho_abs=%u rpm=%u fault=%d",
          latest_state_.fw_major, latest_state_.fw_minor, latest_state_.temperature_pcb, latest_state_.current_input,
          latest_state_.duty_cycle, (unsigned)latest_state_.direction, latest_state_.tacho,
          latest_state_.tacho_absolute, (unsigned)latest_state_.rpm, (int)sp.fault_code); */

      NotifyCallback();
      break;
    }
    default: ULOGT_EVERY_MS(WARNING, 500, "Unknown msg type: 0x%02X len=%u. Dropping!", msg_type, (unsigned)len); break;
  }
}

void YFR4escDriver::ProcessRxBytes(const volatile uint8_t *data, size_t len) {
  if (len == 0) return;
  if (IsRawMode()) {
    RawDataOutput(const_cast<uint8_t *>(data), len);
    return;
  }
  for (size_t i = 0; i < len; ++i) {
    uint8_t b = data[i];
    if (b == 0) {  // COBS end marker
      if (cobs_rx_len_ > 0) {
        size_t dec = cobs_decode(cobs_rx_buffer_, cobs_rx_len_, cobs_decoded_);
        if (dec >= 3) {
          ProcessDecodedPacket(cobs_decoded_, dec);
        }
        cobs_rx_len_ = 0;
      }
    } else {
      if (cobs_rx_len_ < COBS_BUFFER_SIZE) {
        cobs_rx_buffer_[cobs_rx_len_++] = b;
      } else {
        cobs_rx_len_ = 0;  // overflow, reset
      }
    }
  }
}

void YFR4escDriver::SetDuty(float duty) {
  if (!IsStarted() || IsRawMode()) return;
  last_duty_ = duty;
  SendControl(duty);
}

void YFR4escDriver::SendControl(float duty) {
  ControlPacket cp{.message_type = MessageType::CONTROL, .duty_cycle = static_cast<double>(duty), .crc = 0};
  const uint8_t *payload = reinterpret_cast<const uint8_t *>(&cp);
  cp.crc = yfr4esc_crc::crc16_ccitt_false(payload, sizeof(ControlPacket) - sizeof(cp.crc));

  chMtxLock(&mutex_);  // protect shared tx_buffer_ and send
  size_t len = cobs_encode(reinterpret_cast<const uint8_t *>(&cp), sizeof(cp), tx_buffer_);
  if (len + 1 > TX_BUFFER_SIZE) {
    ULOGT_EVERY_MS(WARNING, 1000, "TX control frame too large: len=%u", (unsigned)len);
    chMtxUnlock(&mutex_);
    return;
  }
  tx_buffer_[len++] = 0;  // COBS end marker
  uartSendFullTimeout(uart_, &len, tx_buffer_, TIME_INFINITE);
  chMtxUnlock(&mutex_);
}

void YFR4escDriver::SendSettings() {
  // Defaults. How get those set?
  SettingsPacket sp{.message_type = MessageType::SETTINGS,
                    .motor_current_limit = 2.5f,
                    .min_pcb_temp = 0.0f,
                    .max_pcb_temp = 70.0f,
                    .crc = 0};

  // Compute CRC over message_type + data only (exclude crc), store as-is (LE on wire)
  const uint8_t *payload = reinterpret_cast<const uint8_t *>(&sp);
  sp.crc = yfr4esc_crc::crc16_ccitt_false(payload, sizeof(SettingsPacket) - sizeof(sp.crc));

  chMtxLock(&mutex_);  // protect shared tx_buffer_ and send
  size_t len = cobs_encode(reinterpret_cast<const uint8_t *>(&sp), sizeof(SettingsPacket), tx_buffer_);
  if (len + 1 > TX_BUFFER_SIZE) {
    ULOGT_EVERY_MS(WARNING, 1000, "TX settings frame too large: len=%u", (unsigned)len);
    chMtxUnlock(&mutex_);
    return;
  }
  tx_buffer_[len++] = 0;  // COBS end marker
  uartSendFullTimeout(uart_, &len, tx_buffer_, TIME_INFINITE);
  chMtxUnlock(&mutex_);
}

void YFR4escDriver::RawDataInput(uint8_t *data, size_t size) {
  if (!IsRawMode() || !IsStarted()) return;
  chMtxLock(&mutex_);
  size_t len = size > TX_BUFFER_SIZE ? TX_BUFFER_SIZE : size;
  memcpy(tx_buffer_, data, len);
  uartSendFullTimeout(uart_, &len, tx_buffer_, TIME_INFINITE);
  chMtxUnlock(&mutex_);
}

bool YFR4escDriver::Start() {
  chDbgAssert(!IsStarted(), "don't start the driver twice");
  if (IsStarted()) return false;

  // Configure RX callback
  uart_config_.rxend_cb = [](UARTDriver *uartp) {
    chSysLockFromISR();
    YFR4escDriver *instance = reinterpret_cast<const UARTConfigEx *>(uartp->config)->context;
    chDbgAssert(instance != nullptr, "instance cannot be null!");
    // Buffer reached full length. Re-arm DMA immediately to minimize RX gap, then signal the thread.
    uartStartReceiveI(uartp, DMA_RX_BUFFER_SIZE, const_cast<uint8_t *>(instance->dma_rx_buffer_));
    // Tell the thread a wrap occurred; it will handle the final tail and reset rx_seen_len_.
    if (instance->processing_thread_) {
      chEvtSignalI(instance->processing_thread_, EVT_RX_DMA_WRAP);
    }
    chSysUnlockFromISR();
  };

  if (!(uartStart(uart_, &uart_config_) == MSG_OK)) return false;

  // Set the started flag before launching the thread to avoid race where IsStarted() is false in threadFunc
  if (!MotorDriver::Start()) return false;

  // Now start the processing thread; IsStarted() should be true now and the thread will spin
  processing_thread_ = chThdCreateStatic(&thd_wa_, sizeof(thd_wa_), NORMALPRIO, threadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "YFR4escDriver";
#endif

  // Arm RX
  rx_seen_len_ = 0;
  uartStartReceive(uart_, DMA_RX_BUFFER_SIZE, const_cast<uint8_t *>(dma_rx_buffer_));

  // Send an initial settings packet
  SendSettings();

  return MotorDriver::IsStarted();
}

void YFR4escDriver::threadFunc() {
  systime_t last_heartbeat = 0;

  while (IsStarted()) {
    uint32_t events;

    // YFRev4-ESC always send a status packet every 20ms.
    // So, read with timeout, instead of waiting for a buffer full ISR call.
    events = chEvtWaitOneTimeout(EVT_RX_DMA_WRAP, TIME_MS2I(10));
    (void)events;

    // Process newly arrived bytes immediately using NDTR deltas; handle wrap if the buffer was refilled.
    uint32_t ndtr_now = uart_->dmarx->stream->NDTR;
    if (ndtr_now <= DMA_RX_BUFFER_SIZE) {
      size_t received_so_far = DMA_RX_BUFFER_SIZE - ndtr_now;
      if (received_so_far < rx_seen_len_) {
        // Buffer wrapped (rxend_cb re-armed). Process tail [rx_seen_len_ .. end), then reset.
        ProcessRxBytes(dma_rx_buffer_ + rx_seen_len_, DMA_RX_BUFFER_SIZE - rx_seen_len_);
        rx_seen_len_ = 0;
      }
      if (received_so_far > rx_seen_len_) {
        size_t delta = received_so_far - rx_seen_len_;
        ProcessRxBytes(dma_rx_buffer_ + rx_seen_len_, delta);
        rx_seen_len_ = received_so_far;
      }
    }

    // Heartbeat: send control periodically even if unchanged, to satisfy ESC watchdog
    systime_t now = chVTGetSystemTimeX();
    if ((systime_t)(now - last_heartbeat) >= HEARTBEAT_INTERVAL) {
      last_heartbeat = now;
      SendControl(last_duty_);
    }
  }
}

}  // namespace xbot::driver::motor
