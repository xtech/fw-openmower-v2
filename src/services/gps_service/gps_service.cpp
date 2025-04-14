#include "gps_service.hpp"

#include <drivers/gps/nmea_gps_driver.h>
#include <drivers/gps/ublox_gps_driver.h>
#include <ulog.h>

#include <cstdio>

#include "debug/debug_udp_interface.hpp"
#include "robot.hpp"

static UARTDriver* GetUARTDriverByIndex(uint8_t index) {
  switch (index) {
      // Get the default port for this platform (nullptr, if we need the user to specify it)
    case 0: return Robot::GPS::GetUartPort();
#if STM32_UART_USE_USART1
    case 1: return &UARTD1;
#endif
#if STM32_UART_USE_USART2
    case 2: return &UARTD2;
#endif
#if STM32_UART_USE_USART3
    case 3: return &UARTD3;
#endif
#if STM32_UART_USE_UART4
    case 4: return &UARTD4;
#endif
#if STM32_UART_USE_UART5
    case 5: return &UARTD5;
#endif
#if STM32_UART_USE_USART6
    case 6: return &UARTD6;
#endif
#if STM32_UART_USE_UART7
    case 7: return &UARTD7;
#endif
#if STM32_UART_USE_UART8
    case 8: return &UARTD8;
#endif
#if STM32_UART_USE_UART9
    case 9: return &UARTD9;
#endif
#if STM32_UART_USE_USART10
    case 10: return &UARTD10;
#endif
    default: return nullptr;
  }
}

bool GpsService::OnStart() {
  using namespace xbot::driver::gps;

  if (gps_driver_ == nullptr) {
    UARTDriver* uart_driver = GetUARTDriverByIndex(Uart.value);
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
