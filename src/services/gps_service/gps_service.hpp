#ifndef GPS_SERVICE_HPP
#define GPS_SERVICE_HPP

#include <drivers/gps/gps_driver.h>

#include "GpsServiceBase.hpp"

using namespace xbot::driver::gps;

class GpsService : public GpsServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024){};

 public:
  explicit GpsService(const uint16_t service_id) : GpsServiceBase(service_id, 10000, wa, sizeof(wa)) {
  }

 protected:
  bool Configure() override;
  void OnStart() override;
  void OnStop() override;

  void OnRTCMChanged(const uint8_t* new_value, uint32_t length) override;

 private:
  enum ProtocolType { UBX, NMEA };

  ProtocolType protocol_;
  UARTDriver* uart_driver_;
  GpsDriver* gps_driver_;

  UARTDriver* GetUARTDriverByIndex(uint8_t index);
  void GpsStateCallback(const GpsDriver::GpsState& state);
};

#endif  // GPS_SERVICE_HPP
