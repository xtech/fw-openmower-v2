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

  ID_EEPROM_GetBoardInfo(&board_info);
  ID_EEPROM_GetCarrierBoardInfo(&carrier_board_info);
}
