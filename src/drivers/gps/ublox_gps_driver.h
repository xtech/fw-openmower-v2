//
// Created by Clemens Elflein on 14.10.22.
// Copyright (c) 2022 Clemens Elflein. All rights reserved.
//

#ifndef XBOT_DRIVER_GPS_UBLOX_GPS_DRIVER_H
#define XBOT_DRIVER_GPS_UBLOX_GPS_DRIVER_H

#include <debug/debuggable_driver.hpp>

#include "gps_driver.h"
#include "ubx_datatypes.h"

namespace xbot::driver::gps {
class UbxGpsDriver : public GpsDriver {
 public:
  UbxGpsDriver();

 private:
 protected:
  void ResetParserState() override;

 private:
  /**
   * Send a packet to the GPS. This will add a header and a checksum, but the
   * space is assumed to already be allocated
   */
  bool SendPacket(uint8_t *data, size_t size);

  /**
   * Parses the rx buffer and looks for valid ubx messages
   */
  size_t ProcessBytes(const uint8_t *buffer, size_t len) override;

  /**
   * Gets called with a valid ubx frame and switches to the handle_xxx functions
   */
  void ProcessUbxPacket(const uint8_t *data, const size_t &size);

  /**
   * Validate a frame, return true on valid checksum
   */
  bool ValidateChecksum(const uint8_t *packet, size_t size);

  /**
   * calculates the checksum for a packet
   */
  void CalculateChecksum(const uint8_t *packet, size_t size, uint8_t &ck_a,
                          uint8_t &ck_b);

  void HandleNavPvt(const UbxNavPvt *msg);

  uint8_t gbuffer_[512]{};
  size_t gbuffer_fill = 0;

  // flag if we found the header for time tracking only
  bool found_header_ = false;
};
} // namespace xbot::driver::gps



#endif  // XBOT_DRIVER_GPS_UBLOX_GPS_INTERFACE_H
