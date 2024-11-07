//
// Created by clemens on 10.01.23.
//

#ifndef OPEN_MOWER_ROS_GPS_INTERFACE_H
#define OPEN_MOWER_ROS_GPS_INTERFACE_H

#include <etl/delegate.h>
#include "ch.h"
#include "hal.h"

namespace xbot {
namespace driver {
namespace gps {
class GpsInterface {
 public:
  explicit GpsInterface();

  virtual ~GpsInterface() = default;

  /*
   * The final GPS state we're interested in.
   */
  struct GpsState {
    enum FixType {
      NO_FIX = 0,
      DR_ONLY = 1,
      FIX_2D = 2,
      FIX_3D = 3,
      GNSS_DR_COMBINED = 4
    };

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
  bool start_driver(UARTDriver* uart, uint32_t baudrate);
  void set_state_callback(const GpsInterface::StateCallback &function);

  void set_datum(double datum_lat, double datum_long, double datum_height);

  void send_rtcm(const uint8_t *data, size_t size);

  void read_uart(uint32_t timeout);

 protected:
  StateCallback state_callback{};

  double datum_lat_ = 0, datum_long_ = 0, datum_u_ = 0;

  bool gps_state_valid_{};
  GpsState gps_state_{};

  /**
   * Send a message to the GPS. This will just output to the serial port
   * directly
   */
  bool send_raw(const void *data, size_t size);

  // Called on serial reconnect
  virtual void reset_parser_state() = 0;

  virtual size_t process_bytes(const uint8_t *buffer, size_t len) = 0;

 private:
  uint8_t gps_buffer[512]{};
  uint8_t gps_buffer2[512]{};
 uint8_t current_buffer = 0;
  size_t gps_buffer_count = 0;
  size_t requested_bytes = 0;
  UARTDriver *uart{};
 UARTConfig uart_config{};

 THD_WORKING_AREA(wa, 1024);

  bool stopped_ = true;

 void threadFunc();

 static void threadHelper(void* instance);
};
}  // namespace gps
}  // namespace driver
}  // namespace xbot
#endif  // OPEN_MOWER_ROS_GPS_INTERFACE_H
