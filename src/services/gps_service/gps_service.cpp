#include "gps_service.hpp"

#include <drivers/gps/nmea_gps_driver.h>
#include <drivers/gps/ublox_gps_driver.h>
#include <ulog.h>

#include <board_utils.hpp>
#include <cstdio>
#include <globals.hpp>

#include "debug/debug_udp_interface.hpp"

bool GpsService::LoadAndStartGpsDriver(ProtocolType protocol_type, uint8_t uart, uint32_t baudrate) {
  // Get the requested UART port (if 0 is specified, ask the robot.cpp for the default port)
  UARTDriver* uart_driver = uart == 0 ? robot->GPS_GetUartPort() : GetUARTDriverByIndex(uart);
  if (uart_driver == nullptr) {
    char msg[100]{};
    snprintf(msg, sizeof(msg), "Could not open UART. Check the provided uart_index: %i", uart);
    ULOG_ARG_ERROR(&service_id_, msg);
    return false;
  }

  if (protocol_type == ProtocolType::UBX) {
    gps_driver_ = new UbxGpsDriver();
  } else {
    gps_driver_ = new NmeaGpsDriver();
  }

  gps_driver_->SetStateCallback(
      etl::delegate<void(const GpsDriver::GpsState&)>::create<GpsService, &GpsService::GpsStateCallback>(*this));

  gps_driver_->StartDriver(uart_driver, baudrate);
  debug_interface_.SetDriver(gps_driver_);
  debug_interface_.Start();

  used_port_index_ = uart;

  return true;
}

bool GpsService::OnStart() {
  using namespace xbot::driver::gps;

  ULOG_ARG_INFO(&service_id_, "GpsService::OnStart()");

  // Check if we need to reconfigure the driver (protocol, port, or baudrate changed) due to a already existing driver
  if (gps_driver_ != nullptr) {
    if (gps_driver_->GetProtocolType() != Protocol.value || used_port_index_ != Uart.value ||
        gps_driver_->GetUartBaudrate() != Baudrate.value) {
      ULOG_ARG_INFO(&service_id_, "GPS configuration change detected - reconfiguring");
      debug_interface_.Stop();    // Stop the debug interface first (before stopping UART in the driver)
      gps_driver_->StopDriver();  // Stop and delete the old driver
      delete gps_driver_;
      gps_driver_ = nullptr;
      debug_interface_.SetDriver(nullptr);
    } else {
      // Driver still has correct configuration, no reconfiguration needed
      return true;
    }
  }

  if (gps_driver_ == nullptr) {
    // We don't have a gps driver running yet, so create one.
    return LoadAndStartGpsDriver(Protocol.value, Uart.value, Baudrate.value);
  }

  return true;
}

void GpsService::OnRTCMChanged(const uint8_t* new_value, uint32_t length) {
  // Update NTRIP timestamp when RTCM data is received
  last_ntrip_time_ = chVTGetSystemTimeX();

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
