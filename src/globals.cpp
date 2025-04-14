//
// Created by clemens on 01.10.24.
//

#include "globals.hpp"

#include <xbot-service/Lock.hpp>

using namespace xbot::service;

CC_SECTION(".ram4") struct board_info board_info {};
CC_SECTION(".ram4") struct carrier_board_info carrier_board_info {};

EVENTSOURCE_DECL(mower_events);
MUTEX_DECL(mower_status_mutex);
MowerStatus mower_status;

void InitGlobals() {
  // Start with emergency engaged
  mower_status.emergency_latch = true;
  ID_EEPROM_GetBoardInfo(&board_info);
  ID_EEPROM_GetCarrierBoardInfo(&carrier_board_info);
}

MowerStatus GetMowerStatus() {
  Lock lock(&mower_status_mutex);
  return mower_status;
}

MowerStatus UpdateMowerStatus(const etl::delegate<void(MowerStatus& mower_status)>& callback) {
  Lock lock(&mower_status_mutex);
  callback(mower_status);
  return mower_status;
}
