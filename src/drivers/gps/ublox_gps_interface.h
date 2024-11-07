//
// Created by Clemens Elflein on 14.10.22.
// Copyright (c) 2022 Clemens Elflein. All rights reserved.
//

#ifndef XBOT_WORKSPACE_UBLOX_GPS_INTERFACE_H
#define XBOT_WORKSPACE_UBLOX_GPS_INTERFACE_H

#include "gps_interface.h"
#include "ubx_datatypes.h"

namespace xbot {
namespace driver {
namespace gps {
class UbxGpsInterface : public GpsInterface {
 public:
  UbxGpsInterface();

 private:
 protected:
  void reset_parser_state() override;

 private:
  /**
   * Send a packet to the GPS. This will add a header and a checksum, but the
   * space is assumed to already be allocated
   */
  bool send_packet(uint8_t *data, size_t size);

  /**
   * Parses the rx buffer and looks for valid ubx messages
   */
  size_t process_bytes(const uint8_t *buffer, size_t len) override;

  /**
   * Gets called with a valid ubx frame and switches to the handle_xxx functions
   */
  void process_ubx_packet(const uint8_t *data, const size_t &size);

  /**
   * Validate a frame, return true on valid checksum
   */
  bool validate_checksum(const uint8_t *packet, size_t size);

  /**
   * calculates the checksum for a packet
   */
  void calculate_checksum(const uint8_t *packet, size_t size, uint8_t &ck_a,
                          uint8_t &ck_b);

  void handle_nav_pvt(const UbxNavPvt *msg);

  uint8_t gbuffer[4096]{};
  uint8_t debug_buffer[3][4096]{};
  uint8_t current_debug_buffer = 0;
  size_t gbuffer_fill = 0;

  // flag if we found the header for time tracking only
  bool found_header_ = false;
};
}  // namespace gps
}  // namespace driver
}  // namespace xbot

#endif  // XBOT_WORKSPACE_UBLOX_GPS_INTERFACE_H
