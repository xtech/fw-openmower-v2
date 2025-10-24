//
// Created by Apehaenger <joerg@ebeling.ws> on 16.08.25.
//

#ifndef YFR4ESCDRIVER_H
#define YFR4ESCDRIVER_H

#include <cstdint>
#include <debug/debuggable_driver.hpp>
#include <drivers/motor/motor_driver.hpp>

#include "ch.h"
#include "datatypes.h"
#include "hal.h"

namespace xbot::driver::motor {
class YFR4escDriver : public DebuggableDriver, public MotorDriver {
 public:
  YFR4escDriver() {
    latest_state_.status = ESCState::ESCStatus::ESC_STATUS_DISCONNECTED;
  };
  ~YFR4escDriver() override = default;

  bool SetUART(UARTDriver* uart, uint32_t baudrate);

  void RequestStatus() override{};  // No-op, ESC streams status periodically
  void SetDuty(float duty) override;
  bool Start() override;

  void RawDataInput(uint8_t* data, size_t size) override;

 private:
  // Heartbeat: resend control regularly to satisfy ESC watchdog
  static constexpr systime_t HEARTBEAT_INTERVAL = TIME_MS2I(100);  // 10 Hz

  float last_duty_ = 0.0f;

  struct UARTConfigEx : UARTConfig {
    YFR4escDriver* context;
  };

  // RX buffer size calc: We only receive Status packets
  static constexpr size_t RX_MAX_PKT_DECODED = sizeof(yfr4esc::StatusPacket);
  static constexpr size_t RX_MAX_PKT_CODED = RX_MAX_PKT_DECODED + (RX_MAX_PKT_DECODED / 254) + 1;  // COBS worst-case

  // RX DMA buffer holds two full encoded frames + delimiter for wrap/delta comfort
  static constexpr size_t DMA_RX_BUFFER_SIZE = 2 * (RX_MAX_PKT_CODED + 1);
  volatile uint8_t dma_rx_buffer_[DMA_RX_BUFFER_SIZE]{};

  // COBS RX buffer (enough for one full encoded Status frame (without trailing 0))
  static constexpr size_t COBS_BUFFER_SIZE = RX_MAX_PKT_CODED;
  uint8_t cobs_rx_buffer_[COBS_BUFFER_SIZE]{};
  // Decoded buffer holds one Status frame
  uint8_t cobs_decoded_[RX_MAX_PKT_DECODED]{};

  // DMA-safe TX buffer (avoid sending from stack/DTCM)
  static constexpr size_t TX_MAX_PKT_DECODED = (sizeof(yfr4esc::ControlPacket) > sizeof(yfr4esc::SettingsPacket))
                                                   ? sizeof(yfr4esc::ControlPacket)
                                                   : sizeof(yfr4esc::SettingsPacket);
  static constexpr size_t TX_MAX_PKT_CODED = TX_MAX_PKT_DECODED + (TX_MAX_PKT_DECODED / 254) + 1;  // COBS worst-case
  // One full encoded TX frame including trailing 0
  static constexpr size_t TX_BUFFER_SIZE = TX_MAX_PKT_CODED + 1;
  uint8_t tx_buffer_[TX_BUFFER_SIZE]{};

  size_t cobs_rx_len_ = 0;
  size_t rx_seen_len_ = 0;  // Track how many bytes we already processed in the receiving DMA buffer

  THD_WORKING_AREA(thd_wa_, 512){};  // AH20250922: Measured stack usage of 208 bytes (without ULOG errors)
  thread_t* processing_thread_ = nullptr;

  UARTDriver* uart_{};
  UARTConfigEx uart_config_{};

  void threadFunc();
  static void threadHelper(void* instance) {
    static_cast<YFR4escDriver*>(instance)->threadFunc();
  };

  void ProcessRxBytes(const volatile uint8_t* data, size_t len);
  void ProcessDecodedPacket(uint8_t* packet, size_t len);

  void SendControl(float duty);
  void SendSettings();
};
}  // namespace xbot::driver::motor

#endif  // YFR4ESCDRIVER_H
