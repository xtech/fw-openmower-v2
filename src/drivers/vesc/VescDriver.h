//
// Created by clemens on 08.11.24.
//

#ifndef VESCDRIVER_H
#define VESCDRIVER_H

#include <etl/delegate.h>

#include <cstdint>
#include <debug/debuggable_driver.hpp>

#include "ch.h"
#include "hal.h"

namespace xbot::driver::esc {
class VescDriver : public DebuggableDriver {
 public:
  struct ESCState {
    enum class ESCStatus : uint8_t {
      ESC_STATUS_DISCONNECTED = 99u,
      ESC_STATUS_ERROR = 100u,
      ESC_STATUS_STALLED = 150u,
      ESC_STATUS_OK = 200u,
      ESC_STATUS_RUNNING = 201u,
    };

    uint8_t fw_major;
    uint8_t fw_minor;

    float voltage_input;
    float temperature_pcb;
    float temperature_motor;
    float current_input;
    float duty_cycle;
    uint32_t tacho;
    uint32_t tacho_absolute;
    float direction;
    float rpm;

    ESCStatus status;
  };

  typedef etl::delegate<void(const ESCState &new_state)> StateCallback;

  VescDriver();

  ~VescDriver() override = default;

  bool StartDriver(UARTDriver *uart, uint32_t baudrate);
  void SetStatusUpdateInterval(uint32_t interval_millis);
  void SetStateCallback(const StateCallback &function);
  void RequestStatus();
  void SetDuty(float duty);

  void RawDataInput(uint8_t *data, size_t size) override;

 private:
  StateCallback state_callback_{};

  ESCState latest_state{};
#pragma pack(push, 1)
  struct VescPayload {
    // prepend space will be used for packet size / CAN ID
    // its structure id dependend on packet length and if CAN or not
    uint8_t prepend[5]{};
    // the actual payload
    // 255 bytes for max payload + 3 bytes for CRC and a framing byte
    uint8_t payload[258]{};
    // Length of valid data in payload array
    uint8_t payload_length{};
  } __attribute__((packed));
#pragma pack(pop)

  // A buffer to prepare the next payload, so that we don't need to allocate on the stack
  // make sure to lock mutex_ before using.
  VescPayload payload_buffer_{};
  THD_WORKING_AREA(thd_wa_, 1024){};
  // Milliseconds for automatic status requests. 0 to disable
  uint32_t status_request_millis_ = 0;

  // Extend the config struct by a pointer to this instance, so that we can access it in callbacks.
  struct UARTConfigEx : UARTConfig {
    VescDriver *context;
  };

  static constexpr size_t RECV_BUFFER_SIZE = 260;
  uint32_t last_status_request_millis_ticks_ = 0;

  // Keep two buffers for streaming data while doing processing
  uint8_t recv_buffer1_[RECV_BUFFER_SIZE]{};
  uint8_t recv_buffer2_[RECV_BUFFER_SIZE]{};
  // We start by receiving into recv_buffer1, so processing_buffer is the 2 (but empty)
  uint8_t *volatile processing_buffer_ = recv_buffer2_;
  volatile size_t processing_buffer_len_ = 0;

  // working buffer for the receiving thread
  uint8_t working_buffer_fill_ = 0;
  // length of 260 = 1 header byte, 1 length byte, 255 max payload, 2 CRC, 1 trailer
  uint8_t working_buffer_[260];
  bool found_header_ = false;

  UARTDriver *uart_{};
  UARTConfigEx uart_config_{};
  thread_t *processing_thread_ = nullptr;
  // This is reset by the receiving ISR and set by the thread to signal if it's safe to process more data.
  volatile bool processing_done_ = true;
  bool stopped_ = true;

  void ProcessPayload();
  bool ProcessBytes(uint8_t *buffer, size_t len);
  void SendPacket();
  void threadFunc();
  static void threadHelper(void *instance);
};
}  // namespace xbot::driver::esc

#endif  // VESCDRIVER_H
