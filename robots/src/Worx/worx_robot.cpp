//
// Created by clemens on 27.01.25.
//

#include <ulog.h>

#include <globals.hpp>

#include "robot.hpp"
#include <drivers/charger/bq_2576/bq_2576.hpp>
#include <drivers/motor/vesc/VescDriver.h>
#include <debug/debug_tcp_interface.hpp>
#include <services.hpp>

namespace Robot {

static BQ2576 charger{};
static xbot::driver::motor::VescDriver left_motor_driver{};
static xbot::driver::motor::VescDriver right_motor_driver{};
static xbot::driver::motor::VescDriver mower_motor_driver{};

static DebugTCPInterface left_esc_driver_interface_{65102, &left_motor_driver};
static DebugTCPInterface mower_esc_driver_interface_{65103, &mower_motor_driver};
static DebugTCPInterface right_esc_driver_interface_{65104, &right_motor_driver};




namespace General {
void InitPlatform() {
  left_motor_driver.SetUART(&UARTD1, 115200);
  right_motor_driver.SetUART(&UARTD4, 115200);
  mower_motor_driver.SetUART(&UARTD2, 115200);
  left_esc_driver_interface_.Start();
  right_esc_driver_interface_.Start();
  mower_esc_driver_interface_.Start();

  diff_drive.SetDrivers(&left_motor_driver, &right_motor_driver);
  mower_service.SetDriver(&mower_motor_driver);
}

bool IsHardwareSupported() {
  // First batch of universal boards have a non-working EEPROM
  // so we assume that the firmware is compatible, if the xcore is the first batch and no carrier was found.
  if (carrier_board_info.board_info_version == 0 &&
      strncmp("N/A", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0 &&
      strncmp("xcore", board_info.board_id, sizeof(board_info.board_id)) == 0 &&
      board_info.version_major == 1 &&
      board_info.version_minor == 1 &&
      board_info.version_patch == 7) {
    return true;
  }


  // Else, we accept universal boards
  return strncmp("hw-openmower-universal", carrier_board_info.board_id, sizeof(carrier_board_info.board_id)) == 0;
}

}  // namespace General

namespace GPS {
UARTDriver* GetUartPort() {
  // on this platform we require a user setting
  return nullptr;
}
}  // namespace GPS

namespace Power {

I2CDriver* GetPowerI2CD() {
  return &I2CD1;
}

ChargerDriver* GetCharger() {
  return &charger;
}

float GetMaxVoltage() {
  return 5.0f * 4.2f;
}

float GetChargeCurrent() {
  return 1.0;
}

float GetMinVoltage() {
  // 3.3V min, 5s pack
  return 5.0f * 3.3;
}

}  // namespace Power
}  // namespace Robot
