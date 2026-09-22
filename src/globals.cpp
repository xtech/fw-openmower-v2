//
// Created by clemens on 01.10.24.
//

#include "globals.hpp"

#include <services.hpp>
#include <xbot-service/Lock.hpp>

using namespace xbot::service;

Robot* robot = nullptr;

CC_SECTION(".ram4") struct board_info board_info {};
CC_SECTION(".ram4") struct carrier_board_info carrier_board_info {};

EVENTSOURCE_DECL(mower_events);

void InitGlobals() {
  // Start with emergency engaged

  // A transient ID read failure must not select the wrong Ethernet PHY.
  // Keep retrying with bus recovery; never turn "N/A" into a hardware choice.
  while (!ID_EEPROM_GetBoardInfo(&board_info)) {
    palToggleLine(LINE_HEARTBEAT_LED_RED);
    chThdSleepMilliseconds(250);
  }
  palSetLine(LINE_HEARTBEAT_LED_RED);
  ID_EEPROM_GetCarrierBoardInfo(&carrier_board_info);
}
