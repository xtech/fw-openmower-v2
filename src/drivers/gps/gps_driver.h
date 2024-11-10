//
// Created by clemens on 10.01.23.
//

#ifndef XBOT_DRIVER_GPS_GPS_DRIVER_H
#define XBOT_DRIVER_GPS_GPS_DRIVER_H

#include <etl/delegate.h>

#include "ch.h"
#include "hal.h"

namespace xbot::driver::gps {
class GpsDriver {
 public:
  explicit GpsDriver();

  virtual ~GpsDriver() = default;

  /*
   * The final GPS state we're interested in.
   */
  struct GpsState {
    enum FixType { NO_FIX = 0, DR_ONLY = 1, FIX_2D = 2, FIX_3D = 3, GNSS_DR_COMBINED = 4 };

    enum RTKType { RTK_NONE = 0, RTK_FLOAT = 1, RTK_FIX = 2 };

    uint32_t sensor_time;
    uint32_t received_time;

    // Position
    bool position_valid;
    // Position accuracy in m
    double position_accuracy;
    double pos_e, pos_n, pos_u;

    // Pos in lat/lon for VRS
    double pos_lat, pos_lon;

    // Motion
    bool motion_heading_valid;
    double vel_e, vel_n, vel_u;
    double motion_heading_accuracy;
    double motion_heading;

    // Heading
    bool vehicle_heading_valid;
    double vehicle_heading_accuracy;
    // Vehicle heading in rad.
    double vehicle_heading;

    FixType fix_type;
    RTKType rtk_type;
  };

  enum Level { VERBOSE, INFO, WARN, ERROR };

  typedef etl::delegate<void(const GpsState &new_state)> StateCallback;

 public:
  bool StartDriver(UARTDriver *uart, uint32_t baudrate);
  void SetStateCallback(const GpsDriver::StateCallback &function);

  void SetDatum(double datum_lat, double datum_long, double datum_height);

  void SendRTCM(const uint8_t *data, size_t size);

 protected:
  StateCallback state_callback_{};

  double datum_lat_ = 0, datum_long_ = 0, datum_u_ = 0;

  bool gps_state_valid_{};
  GpsState gps_state_{};

  /**
   * Send a message to the GPS. This will just output to the serial port
   * directly
   */
  bool send_raw(const void *data, size_t size);

  // Called on serial reconnect
  virtual void ResetParserState() = 0;

  virtual size_t ProcessBytes(const uint8_t *buffer, size_t len) = 0;

 private:
  // Extend the config struct by a pointer to this instance, so that we can access it in callbacks.
  struct UARTConfigEx : UARTConfig {
    GpsDriver *context;
  };

  static constexpr size_t RECV_BUFFER_SIZE = 512;
  // 10Hz timeout for reception
  static constexpr uint32_t RECV_TIMEOUT_MILLIS = 100;
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

  void threadFunc();

  static void threadHelper(void *instance);
};
}  // namespace xbot::driver::gps

#endif  // XBOT_DRIVER_GPS_GPS_INTERFACE_H
