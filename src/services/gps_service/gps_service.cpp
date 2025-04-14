#include "gps_service.hpp"

#include <drivers/gps/nmea_gps_driver.h>
#include <drivers/gps/ublox_gps_driver.h>
#include <ulog.h>

#include <cstdio>

#include "debug/debug_udp_interface.hpp"
#include "robot.hpp"
#include <board_utils.h>



bool GpsService::OnStart() {


  using namespace xbot::driver::gps;

  if (gps_driver_ == nullptr) {
    // Get the requested UART port (if 0 is specified, ask the robot.cpp for the default port)
    UARTDriver* uart_driver = Uart.value == 0 ? Robot::GPS::GetUartPort() : GetUARTDriverByIndex(Uart.value);
    if (uart_driver == nullptr) {
      char msg[100]{};
      snprintf(msg, sizeof(msg), "Could not open UART. Check the provided uart_index: %i", Uart.value);
      ULOG_ARG_ERROR(&service_id_, msg);
      return false;
    }

    // We don't have a gps driver running yet, so create one.
    if (Protocol.value == ProtocolType::UBX) {
      gps_driver_ = new UbxGpsDriver();
    } else {
      gps_driver_ = new NmeaGpsDriver();
    }

    gps_driver_->SetStateCallback(
        etl::delegate<void(const GpsDriver::GpsState&)>::create<GpsService, &GpsService::GpsStateCallback>(*this));

    gps_driver_->StartDriver(uart_driver, Baudrate.value);
    debug_interface_.SetDriver(gps_driver_);
    debug_interface_.Start();
    // Keep track of the protocol type we used to initialize the driver
    used_protocol_type_ = Protocol.value;
    used_port_index_ = Uart.value;
  } else {
    // We already have the driver, if the protocol has changed, restart the board
    // (since we cannot stop the driver)
    if (used_protocol_type_ != Protocol.value || used_port_index_ != Uart.value) {
      ULOG_ARG_WARNING(&service_id_, "GPS protocol or port change detected - restarting");
      NVIC_SystemReset();
    }
  }
  return true;
}

void GpsService::OnRTCMChanged(const uint8_t* new_value, uint32_t length) {
  gps_driver_->SendRTCM(new_value, length);
}

void GpsService::GpsStateCallback(const GpsDriver::GpsState& state) {
  StartTransaction();
  double position[3] = {state.pos_lat, state.pos_lon, state.pos_height};
  SendPosition(position, 3);
  SendPositionHorizontalAccuracy(state.position_h_accuracy);
  SendPositionVerticalAccuracy(state.position_v_accuracy);
  if (state.rtk_type == xbot::driver::gps::GpsDriver::GpsState::RTK_FIX) {
    SendFixType("FIX", 3);
  } else if (state.rtk_type == xbot::driver::gps::GpsDriver::GpsState::RTK_FLOAT) {
    SendFixType("FLOAT", 5);
  }
  double vel[3] = {state.vel_e, state.vel_n, state.vel_u};
  SendMotionVectorENU(vel, 3);
  CommitTransaction();
}
