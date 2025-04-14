#ifndef GPS_SERVICE_HPP
#define GPS_SERVICE_HPP

#include <drivers/gps/gps_driver.h>

#include "GpsServiceBase.hpp"
#include "debug/debug_tcp_interface.hpp"

using namespace xbot::driver::gps;

class GpsService : public GpsServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024){};

 public:
  explicit GpsService(const uint16_t service_id) : GpsServiceBase(service_id, wa, sizeof(wa)) {
  }

 protected:
  bool OnStart() override;

  void OnRTCMChanged(const uint8_t* new_value, uint32_t length) override;

 private:
  GpsDriver* gps_driver_ = nullptr;
  DebugTCPInterface debug_interface_{10000, nullptr};

  // Keep track of the protocol type and uart index used by the gps_driver_ loaded (initial value doesn't matter)
  ProtocolType used_protocol_type_ = ProtocolType::UBX;
  int used_port_index_ = 0;

  void GpsStateCallback(const GpsDriver::GpsState& state);
};

#endif  // GPS_SERVICE_HPP
