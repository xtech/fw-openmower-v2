//
// Created by clemens on 31.07.24.
//

#include "mower_service.hpp"

#include <globals.hpp>

void MowerService::OnCreate() {
  mower_driver_.StartDriver(&UARTD2, 115200);
  mower_esc_driver_interface_.Start();
}

void MowerService::OnStart() {
  mower_duty_ = 0;
}

void MowerService::OnStop() {
  mower_duty_ = 0;
}

void MowerService::tick() {
  chMtxLock(&mtx);
  if (!duty_sent_) {
    // Send motor speed to VESC, if we havent in the meantime
    // (e.g. due to new value or emergency)
    SetDuty();
  }

  duty_sent_ = false;
  chMtxUnlock(&mtx);
}

void MowerService::SetDuty() {
  // Get the current emergency state
  chMtxLock(&mower_status_mutex);
  uint32_t status_copy = mower_status;
  chMtxUnlock(&mower_status_mutex);
  if (status_copy & MOWER_FLAG_EMERGENCY_LATCH) {
    mower_driver_.SetDuty(0);
  } else {
    mower_driver_.SetDuty(mower_duty_);
  }
  duty_sent_ = true;
}

void MowerService::OnMowerEnabledChanged(const uint8_t& new_value) {
  chMtxLock(&mtx);
  if (new_value) {
    mower_duty_ = 1.0;
  } else {
    mower_duty_ = 0;
  }
  if (!duty_sent_) {
    SetDuty();
  }
  chMtxUnlock(&mtx);
}

void MowerService::OnMowerStatusChanged(uint32_t new_status) {
  if ((new_status & (MOWER_FLAG_EMERGENCY_LATCH | MOWER_FLAG_EMERGENCY_ACTIVE)) == 0) {
    // only set speed to 0 if the emergency happens, not if it's cleared
    return;
  }
  chMtxLock(&mtx);
  mower_duty_ = 0;
  // Instantly send the 0 duty cycle
  SetDuty();
  chMtxUnlock(&mtx);
}

MowerService::MowerService(const uint16_t service_id) : MowerServiceBase(service_id, 1000000, wa, sizeof(wa)) {
}
