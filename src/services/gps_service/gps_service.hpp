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
  bool Configure() override;
  void OnStart() override;
  void OnStop() override;
  void OnCreate() override;

 private:
  void tick() override;

  enum ProtocolType { UBX, NMEA };
  ProtocolType protocol_;
  GpsDriver* gps_driver_;
  void GpsStateCallback(const GpsDriver::GpsState& state);

 protected:
  bool OnRTCMChanged(const uint8_t* new_value, uint32_t length) override;
};

#endif  // GPS_SERVICE_HPP
