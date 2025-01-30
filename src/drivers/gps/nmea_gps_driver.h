#ifndef XBOT_DRIVER_GPS_NMEA_GPS_DRIVER_H
#define XBOT_DRIVER_GPS_NMEA_GPS_DRIVER_H

#include <debug/debuggable_driver.hpp>

#include "gps_driver.h"

namespace xbot::driver::gps {
class NmeaGpsDriver : public GpsDriver {
 protected:
  void ResetParserState() override;

 private:
  /**
   * Parses the rx buffer and looks for valid NMEA sentences
   */
  size_t ProcessBytes(const uint8_t* buffer, size_t len) override;

  bool ProcessLine(const char* line);
  void UpdateGpsStateValidity();

  char line[512]{};
  size_t line_len = 0;

  int fix_quality = 0;
};
}  // namespace xbot::driver::gps

#endif  // XBOT_DRIVER_GPS_NMEA_GPS_DRIVER_H
