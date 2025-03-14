//
// Created by clemens on 01.10.24.
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include <etl/delegate.h>
#include <id_eeprom.h>

#include "ch.h"

// Event to notify threads whenever the emergency flags have changed
static constexpr uint32_t MOWER_EVT_EMERGENCY_CHANGED = 1;

struct MowerStatus {
  // Was there an emergency which has not been reset?
  bool emergency_latch : 1;
  // Is there currently an emergency (this is updated by the high-prio emergency task)?
  bool emergency_active : 1;
};

CC_SECTION(".ram4") extern struct board_info board_info;
CC_SECTION(".ram4") extern struct carrier_board_info carrier_board_info;

// event source for mower events (e.g. emergency)
extern event_source_t mower_events;

void InitGlobals();
MowerStatus GetMowerStatus();
MowerStatus UpdateMowerStatus(const etl::delegate<void(MowerStatus& mower_status)>& callback);
#endif  // GLOBALS_H
