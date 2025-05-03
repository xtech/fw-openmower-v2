//
// Created by clemens on 10.01.23.
//
#ifndef XBOT_DRIVER_LIDAR_RPLIDARA1_DRIVER_H
#define XBOT_DRIVER_LIDAR_RPLIDARA1_DRIVER_H

#include <etl/delegate.h>

#include <debug/debuggable_driver.hpp>

#include "ch.h"
#include "hal.h"

namespace xbot::driver::lidar {
class RpLidarA1Driver {
 public:
  ~RpLidarA1Driver() = default;

  typedef etl::delegate<void()> DataCallback;

 public:
  bool StartDriver(UARTDriver *uart, uint32_t baudrate);
  void SetDataCallback(const RpLidarA1Driver::DataCallback &function);

  void SendCommand(const uint8_t *data, size_t size);

 protected:
  DataCallback data_callback_{};
  void TriggerDataCallback();

  /**
   * Send a message to the LIDAR. This will just output to the serial port
   * directly
   */
  bool send_raw(const void *data, size_t size);

  // Called on serial reconnect
  virtual void ResetParserState();

  size_t ProcessBytes(const uint8_t *buffer, size_t len);

 private:
  // Extend the config struct by a pointer to this instance, so that we can access it in callbacks.
  struct UARTConfigEx : UARTConfig {
    RpLidarA1Driver *context;
  };

  static constexpr size_t RECV_BUFFER_SIZE = 512;
  // 20Hz timeout for reception
  static constexpr uint32_t RECV_TIMEOUT_MILLIS = 25;
  // Keep two buffers for streaming data while doing processing
  uint8_t recv_buffer1_[RECV_BUFFER_SIZE]{};
  uint8_t recv_buffer2_[RECV_BUFFER_SIZE]{};
  // We start by receiving into recv_buffer1, so processing_buffer is the 2 (but empty)
  uint8_t *volatile processing_buffer_ = recv_buffer2_;
  volatile size_t processing_buffer_len_ = 0;

  UARTDriver *uart_{};
  UARTConfigEx uart_config_{};

  THD_WORKING_AREA(thd_wa_, 1024){};
  thread_t *processing_thread_ = nullptr;
  // This is reset by the receiving ISR and set by the thread to signal if it's safe to process more data.
  volatile bool processing_done_ = true;
  bool stopped_ = true;

  /**
   * @brief Starts the LIDAR scanning process
   * @return true if successful, false otherwise
   */
  bool StartScan();
  bool ResetLidar();

  /**
   * Requests count of available scan modes
   * @return true
   */
  bool QueryScanModeCount();

  /**
   * Request details for the given scan mode
   */
  bool QueryScanMode(uint16_t scan_mode_id);

  uint16_t scan_mode_count_ = 0;
  uint16_t scan_modes_queried_ = 0;
  uint16_t best_scan_mode_ = 0;
  bool scan_mode_found_ = 0;
  systime_t last_scan_mode_request_ = 0;
  bool scanning_ = false;

  void threadFunc();

  static void threadHelper(void *instance);
};
}  // namespace xbot::driver::lidar

#endif  // XBOT_DRIVER_LIDAR_RPLIDARA1_DRIVER_H
