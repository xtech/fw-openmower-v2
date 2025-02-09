#include "gps_service.hpp"

#include <drivers/gps/nmea_gps_driver.h>
#include <drivers/gps/ublox_gps_driver.h>

bool GpsService::Configure() {
  if (strncmp(Protocol.value, "UBX", Protocol.length) == 0) {
    protocol_ = UBX;
  } else if (strncmp(Protocol.value, "NMEA", Protocol.length) == 0) {
    protocol_ = NMEA;
  } else {
    return false;
  }

  uart_driver_ = GetUARTDriverByIndex(Uart.value);
  if (uart_driver_ == nullptr) {
    return false;
  }

  return true;
}

UARTDriver* GpsService::GetUARTDriverByIndex(uint8_t index) {
  switch (index) {
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

void GpsService::OnStart() {
  using namespace xbot::driver::gps;
  if (protocol_ == UBX) {
    gps_driver_ = new UbxGpsDriver();
  } else {
    gps_driver_ = new NmeaGpsDriver();
  }

  gps_driver_->SetStateCallback(
      etl::delegate<void(const GpsDriver::GpsState&)>::create<GpsService, &GpsService::GpsStateCallback>(*this));

  gps_driver_->StartDriver(uart_driver_, Baudrate.value);
}

void GpsService::OnStop() {
  delete gps_driver_;
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
  CommitTransaction();
}
