#ifndef GPS_SERVICE_HPP
#define GPS_SERVICE_HPP

#include <drivers/gps/gps_driver.h>

#include <cstring>
#include <globals.hpp>

#include "GpsServiceBase.hpp"
#include "debug/debug_tcp_interface.hpp"

using namespace xbot::driver::gps;

class GpsService : public GpsServiceBase {
 private:
  THD_WORKING_AREA(wa, 1536){};

 public:
  explicit GpsService(const uint16_t service_id) : GpsServiceBase(service_id, wa, sizeof(wa)) {
  }

  const GpsDriver::GpsState& GetGpsState() const {
    return gps_driver_ ? gps_driver_->GetGpsState() : empty_gps_state_;
  }

  bool IsGpsStateValid() const {
    return gps_driver_ ? gps_driver_->IsGpsStateValid() : false;
  }

  /**
   * @brief Get seconds since last RTCM packet was received
   * @return Seconds since last RTCM packet, 0 if no data received yet
   */
  uint32_t GetSecondsSinceLastRtcmPacket() const;

  /**
   * @brief Load and start GPS driver instance.
   * Allows initializing the GPS driver before service OnStart(), enabling
   * immediate GPS functionality without waiting for ROS configuration.
   */
  bool LoadAndStartGpsDriver(ProtocolType protocol_type, uint8_t uart, uint32_t baudrate);

 protected:
  bool OnStart() override;

  void OnRTCMChanged(const uint8_t* new_value, uint32_t length) override;

 private:
  GpsDriver* gps_driver_ = nullptr;
  DebugTCPInterface debug_interface_{10000, nullptr};

  // Keep track of the configuration used by the gps_driver_ loaded (initial values don't matter)
  // ATTN: Might become problematic as soon as GpsServiceBase.Uart defaults to != 0
  int used_port_index_ = 0;

  // NTRIP statistics - timestamp of last received RTCM packet
  uint32_t last_ntrip_time_ = 0;

  // Empty GPS state for fallback when no driver is available
  GpsDriver::GpsState empty_gps_state_{};

  void GpsStateCallback(const GpsDriver::GpsState& state);
};

#endif
