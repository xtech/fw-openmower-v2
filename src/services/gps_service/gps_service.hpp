#ifndef GPS_SERVICE_HPP
#define GPS_SERVICE_HPP

#include <drivers/gps/gps_driver.h>

#include "GpsServiceBase.hpp"

using namespace xbot::driver::gps;

class GpsService : public GpsServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024) {};

 public:
  explicit GpsService(const uint16_t service_id) : GpsServiceBase(service_id, 10000, wa, sizeof(wa)) {
  }

 protected:
  void OnCreate() override;
  bool Configure() override;
  void OnStart() override;
  void OnStop() override;

  bool OnRTCMChanged(const uint8_t* new_value, uint32_t length) override;

 private:
  void tick() override;

  enum ProtocolType { UBX, NMEA };
  ProtocolType protocol_;
  GpsDriver* gps_driver_;
  void GpsStateCallback(const GpsDriver::GpsState& state);
};

#endif  // GPS_SERVICE_HPP
