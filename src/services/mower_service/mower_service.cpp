//
// Created by clemens on 31.07.24.
//

#include "mower_service.hpp"

#include <drivers/vesc/esc_status.h>

#include <globals.hpp>

bool MowerService::Configure() {
  // No configuration needed
  return true;
}
void MowerService::OnCreate() { mower_uart_.startDriver(); }
void MowerService::OnStart() { mower_duty_ = 0; }
void MowerService::OnStop() { mower_duty_ = 0; }

void MowerService::tick() {
  chMtxLock(&mtx);
  if (!duty_sent_) {
    // Send motor speed to VESC, if we havent in the meantime
    // (e.g. due to new value or emergency)
    SetDuty();
  }

  mower_uart_.requestVescValues();

  bool parse_success = mower_uart_.parseVescValues();

  StartTransaction();
  if (parse_success) {
    SendMowerESCTemperature(mower_uart_.data.tempMosfet);
    SendMowerMotorCurrent(mower_uart_.data.avgMotorCurrent);
    SendMowerStatus(mower_uart_.data.error == 0 ? ESC_STATUS_OK
                                                : ESC_STATUS_ERROR);
  } else {
    SendMowerStatus(ESC_STATUS_DISCONNECTED);
  }
  CommitTransaction();

  duty_sent_ = false;
  chMtxUnlock(&mtx);
}

void MowerService::SetDuty() {
  // Get the current emergency state
  chMtxLock(&mower_status_mutex);
  uint32_t status_copy = mower_status;
  chMtxUnlock(&mower_status_mutex);
  if (status_copy & MOWER_FLAG_EMERGENCY_LATCH) {
    mower_uart_.setDuty(0);
  } else {
    mower_uart_.setDuty(mower_duty_);
  }
  duty_sent_ = true;
}

bool MowerService::OnMowerEnabledChanged(const uint8_t& new_value) {
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
  return true;
}
void MowerService::OnMowerStatusChanged(uint32_t new_status) {
  if ((new_status &
       (MOWER_FLAG_EMERGENCY_LATCH | MOWER_FLAG_EMERGENCY_ACTIVE)) == 0) {
    // only set speed to 0 if the emergency happens, not if it's cleared
    return;
  }
  chMtxLock(&mtx);
  mower_duty_ = 0;
  // Instantly send the 0 duty cycle
  SetDuty();
  chMtxUnlock(&mtx);
}

MowerService::MowerService(const uint16_t service_id)
    : MowerServiceBase(service_id, 1000000, wa, sizeof(wa)), mower_uart_(&SD2) {
  chMtxObjectInit(&mtx);
}
