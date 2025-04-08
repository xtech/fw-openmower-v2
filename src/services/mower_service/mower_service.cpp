//
// Created by clemens on 31.07.24.
//

#include "mower_service.hpp"

#include <cmath>
#include <xbot-service/portable/system.hpp>

void MowerService::OnCreate() {
  mower_driver_.StartDriver(&UARTD2, 115200);
  mower_driver_.SetStateCallback(
      etl::delegate<void(const VescDriver::ESCState&)>::create<MowerService, &MowerService::ESCCallback>(
          *this));

  mower_esc_driver_interface_.Start();
}

bool MowerService::OnStart() {
  mower_duty_ = 0;
  return true;
}

void MowerService::OnStop() {
  mower_duty_ = 0;
}

void MowerService::tick() {
  chMtxLock(&mtx);

  // Check, if we recently received duty. If not, set to zero for safety
  if (xbot::service::system::getTimeMicros() - last_duty_received_micros_ > 10'000'000) {
    // it's ok to set it here, because we know that duty_set_ is false (we're in a timeout after all)
    mower_duty_ = 0;
  }

  if (!duty_sent_) {
    // Send motor speed to VESC, if we havent in the meantime
    // (e.g. due to new value or emergency)
    SetDuty();
  }

  mower_driver_.RequestStatus();

  // TODO: actually detect some rain
  bool rain_detected = false;

  StartTransaction();
  SendRainDetected(rain_detected);

  // Check, if we have received ESC status updates recently. If not, send a disconnected message
  if (xbot::service::system::getTimeMicros() - last_valid_esc_state_micros_ > 1'000'000 || !esc_state_valid_) {
    // No recent update received (or none at all)
    mower_duty_ = 0;
    SendMowerStatus(static_cast<uint8_t>(VescDriver::ESCState::ESCStatus::ESC_STATUS_DISCONNECTED));
  } else {
    // We got recent data, send it
    StartTransaction();
    SendMowerESCTemperature(esc_state_.temperature_pcb);
    SendMowerMotorCurrent(esc_state_.current_input);
    SendMowerStatus(static_cast<uint8_t>(esc_state_.status));
    SendMowerMotorTemperature(esc_state_.temperature_motor);
    SendMowerRunning(std::fabs(esc_state_.rpm) > 0);
    SendMowerMotorRPM(esc_state_.rpm);
  }
  CommitTransaction();

  duty_sent_ = false;
  chMtxUnlock(&mtx);
}

void MowerService::ESCCallback(const VescDriver::ESCState& state) {
  chMtxLock(&state_mutex_);
  esc_state_ = state;
  esc_state_valid_ = true;
  last_valid_esc_state_micros_ = xbot::service::system::getTimeMicros();
  chMtxUnlock(&state_mutex_);
}

void MowerService::SetDuty() {
  // Get the current emergency state
  MowerStatus mower_status = GetMowerStatus();
  if (mower_status.emergency_latch) {
    mower_driver_.SetDuty(0);
  } else {
    mower_driver_.SetDuty(mower_duty_);
  }
  duty_sent_ = true;
}

void MowerService::OnMowerEnabledChanged(const uint8_t& new_value) {
  chMtxLock(&mtx);
  last_duty_received_micros_ = xbot::service::system::getTimeMicros();
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

void MowerService::OnMowerStatusChanged(MowerStatus new_status) {
  if (!new_status.emergency_latch && !new_status.emergency_active) {
    // only set speed to 0 if the emergency happens, not if it's cleared
    return;
  }
  chMtxLock(&mtx);
  mower_duty_ = 0;
  // Instantly send the 0 duty cycle
  SetDuty();
  chMtxUnlock(&mtx);
}
