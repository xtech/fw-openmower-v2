//
// Created by clemens on 27.01.25.
//

#include <ulog.h>

#include <drivers/charger/bq_2576/bq_2576.hpp>
#include <globals.hpp>

#include "robot.hpp"

namespace Robot {

static BQ2576 charger{};

namespace General {
void InitPlatform() {
  // Not used, we could star the GUI driver task here for example
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

Charger* GetCharger() {
  return &charger;
}

float GetMaxVoltage() {
  return 7.0f * 4.2f;
}

float GetChargeCurrent() {
  return 2.5;
}

float GetMinVoltage() {
  // 3.3V min, 7s pack
  return 7.0f * 3.3;
}

}  // namespace Power
}  // namespace Robot
