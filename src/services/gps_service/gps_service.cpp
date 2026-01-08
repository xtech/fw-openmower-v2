#include "gps_service.hpp"

#include <chprintf.h>
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
    chsnprintf(msg, sizeof(msg), "Could not open UART. Check the provided uart_index: %i", uart);
    ULOG_ARG_ERROR(&service_id_, msg);
    return false;
  }

  // Create the requested driver
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

  // Keep track of the UART port
  used_port_index_ = uart;

  return true;
}

bool GpsService::OnStart() {
  using namespace xbot::driver::gps;

  if (gps_driver_ == nullptr) {
    // We don't have a gps driver running yet, so create one.
    return LoadAndStartGpsDriver(Protocol.value, Uart.value, Baudrate.value);
  }

  // We already have the driver, if the protocol, uart or baudrate has changed, restart the board
  // (since we will not stop the driver, because of heap fragmentation issues)
  if (gps_driver_->GetProtocolType() != Protocol.value || used_port_index_ != Uart.value ||
      gps_driver_->GetUartBaudrate() != Baudrate.value) {
    ULOG_ARG_WARNING(&service_id_, "GPS protocol, uart or baudrate change detected - restarting");
    // Save new settings to persistent storage (if supported by robot)
    if (!robot->SaveGpsSettings(Protocol.value, Uart.value, Baudrate.value)) {
      ULOG_ERROR("Failed to save GPS settings! Abort GpsService::OnStart()");
      return false;
    }
    NVIC_SystemReset();
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

uint32_t GpsService::GetSecondsSinceLastRtcmPacket() const {
  if (last_ntrip_time_ == 0) {
    return 0;  // No RTCM data received yet
  }
  return TIME_I2S(chVTGetSystemTimeX() - last_ntrip_time_);
}
