#include "input_service.hpp"

#include <ulog.h>

bool InputService::OnStart() {
  // Start drivers.
  for (auto& driver : drivers_) {
    if (!driver.second->OnStart()) {
      ULOG_ERROR("Failed to start input driver %d", driver.first);
      return false;
    }
  }
  return true;
}

void InputService::tick() {
  // TODO: Check if these can run at the same frequency.
  for (auto& driver : drivers_) {
    driver.second->tick();
  }
}
