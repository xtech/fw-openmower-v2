//
// Created by clemens on 01.10.24.
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include <ch.h>
#include <etl/delegate.h>
#include <etl/initializer_list.h>
#include <id_eeprom.h>

#include <robot.hpp>

extern Robot* robot;

namespace Events {
enum eventid_t {
  GLOBAL,

  // InputService
  GPIO_TRIGGERED,
};

constexpr int ids_to_mask(std::initializer_list<eventid_t> ids) {
  int result = 0;
  for (eventid_t id : ids) {
    result |= EVENT_MASK(id);
  }
  return result;
}
}  // namespace Events

namespace MowerEvents {
enum : eventflags_t {
  EMERGENCY_CHANGED = 1 << 0,
  INPUTS_CHANGED = 1 << 1,
};
}

CC_SECTION(".ram4") extern struct board_info board_info;
CC_SECTION(".ram4") extern struct carrier_board_info carrier_board_info;

// event source for mower events (e.g. emergency)
extern event_source_t mower_events;

void InitGlobals();
#endif  // GLOBALS_H
