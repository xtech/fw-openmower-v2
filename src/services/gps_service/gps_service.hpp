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
  explicit GpsService(const uint16_t service_id) : GpsServiceBase(service_id, 1000000, wa, sizeof(wa)) {
  }

 protected:
  bool OnStart() override;
  void OnStop() override;

  void OnRTCMChanged(const uint8_t* new_value, uint32_t length) override;

 private:
  UARTDriver* uart_driver_;
  GpsDriver* gps_driver_;
  DebugTCPInterface debug_interface_{10000, nullptr};

  static UARTDriver* GetUARTDriverByIndex(uint8_t index);
  void GpsStateCallback(const GpsDriver::GpsState& state);
};

#endif  // GPS_SERVICE_HPP
