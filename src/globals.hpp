//
// Created by clemens on 01.10.24.
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include <id_eeprom.h>

#include "ch.h"
// Event to notify threads whenever the emergency flags have changed
static constexpr uint32_t MOWER_EVT_EMERGENCY_CHANGED = 1;

// Flag to track if there was an emergency which has not been reset
static constexpr uint32_t MOWER_FLAG_EMERGENCY_LATCH = 1;
// Flag to track if there is currently an emergency (this is updated by the
// high-prio emergency task)
static constexpr uint32_t MOWER_FLAG_EMERGENCY_ACTIVE = 2;

CC_SECTION(".ram4") extern struct board_info board_info;
CC_SECTION(".ram4") extern struct carrier_board_info carrier_board_info;

// event source for mower events (e.g. emergency)
extern event_source_t mower_events;
extern mutex_t mower_status_mutex;
extern uint32_t mower_status;

void InitGlobals();
#endif  // GLOBALS_H
