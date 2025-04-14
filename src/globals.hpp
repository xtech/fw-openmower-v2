//
// Created by clemens on 01.10.24.
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include <etl/delegate.h>
#include <id_eeprom.h>

#include "ch.h"

namespace MowerEvents {
enum : eventflags_t {
  // Emergency flags have changed
  EMERGENCY_CHANGED = 1 << 0,
};
}

CC_SECTION(".ram4") extern struct board_info board_info;
CC_SECTION(".ram4") extern struct carrier_board_info carrier_board_info;

// event source for mower events (e.g. emergency)
extern event_source_t mower_events;

void InitGlobals();
#endif  // GLOBALS_H
