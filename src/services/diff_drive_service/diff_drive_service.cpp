//
// Created by clemens on 26.07.24.
//

#include "diff_drive_service.hpp"

#include <xbot-service/portable/system.hpp>

#include "../../globals.hpp"

void DiffDriveService::OnMowerStatusChanged(uint32_t new_status) {
  if ((new_status & (MOWER_FLAG_EMERGENCY_LATCH | MOWER_FLAG_EMERGENCY_ACTIVE)) == 0) {
    // only set speed to 0 if the emergency happens, not if it's cleared
    return;
  }
  chMtxLock(&mtx);
  speed_l_ = 0;
  speed_r_ = 0;
  // Instantly send the 0 duty cycle
  SetDuty();
  chMtxUnlock(&mtx);
}
bool DiffDriveService::Configure() {
  // Check, if configuration is valid, if not retry
  if (!WheelDistance.valid || !WheelTicksPerMeter.valid || WheelDistance.value == 0 ||
      WheelTicksPerMeter.value == 0.0) {
    return false;
  }
  // It's fine, we don't actually need to configure anything
  return true;
}
void DiffDriveService::OnStart() {
  speed_l_ = speed_r_ = 0;
  last_ticks_valid = false;
}
void DiffDriveService::OnCreate() {
  using namespace xbot::driver::esc;
  // Register callbacks
  left_esc_driver_.SetStateCallback(
      etl::delegate<void(const VescDriver::ESCState&)>::create<DiffDriveService, &DiffDriveService::LeftESCCallback>(
          *this));
  right_esc_driver_.SetStateCallback(
      etl::delegate<void(const VescDriver::ESCState&)>::create<DiffDriveService, &DiffDriveService::RightESCCallback>(
          *this));
  left_esc_driver_.StartDriver(&UARTD1, 115200);
  right_esc_driver_.StartDriver(&UARTD4, 115200);

  left_esc_driver_interface_.Start();
  right_esc_driver_interface_.Start();
}
void DiffDriveService::OnStop() {
  speed_l_ = speed_r_ = 0;
  last_ticks_valid = false;
}

void DiffDriveService::tick() {
  chMtxLock(&mtx);

  if (!duty_sent_) {
    SetDuty();
  }

  left_esc_driver_.RequestStatus();
  right_esc_driver_.RequestStatus();

  // Check, if we have received ESC status updates recently. If not, send a disconnected message
  if (xbot::service::system::getTimeMicros() - last_valid_esc_state_micros_ > 1000000) {
    StartTransaction();
    if (!left_esc_state_valid_) {
      SendLeftESCStatus(static_cast<uint8_t>(VescDriver::ESCState::ESCStatus::ESC_STATUS_DISCONNECTED));
    }
    if (!right_esc_state_valid_) {
      SendRightESCStatus(static_cast<uint8_t>(VescDriver::ESCState::ESCStatus::ESC_STATUS_DISCONNECTED));
    }
    CommitTransaction();
  }

  duty_sent_ = false;
  chMtxUnlock(&mtx);
}
void DiffDriveService::SetDuty() {
  // Get the current emergency state
  chMtxLock(&mower_status_mutex);
  uint32_t status_copy = mower_status;
  chMtxUnlock(&mower_status_mutex);
  if (status_copy & MOWER_FLAG_EMERGENCY_LATCH) {
    left_esc_driver_.SetDuty(0);
    right_esc_driver_.SetDuty(0);
  } else {
    left_esc_driver_.SetDuty(speed_l_);
    right_esc_driver_.SetDuty(speed_r_);
  }
  duty_sent_ = true;
}
void DiffDriveService::LeftESCCallback(const VescDriver::ESCState& state) {
  chMtxLock(&mtx);
  left_esc_state_ = state;
  left_esc_state_valid_ = true;
  if (right_esc_state_valid_) {
    ProcessStatusUpdate();
  }
  chMtxUnlock(&mtx);
}
void DiffDriveService::RightESCCallback(const VescDriver::ESCState& state) {
  chMtxLock(&mtx);
  right_esc_state_ = state;
  right_esc_state_valid_ = true;
  if (left_esc_state_valid_) {
    ProcessStatusUpdate();
  }
  chMtxUnlock(&mtx);
}

void DiffDriveService::ProcessStatusUpdate() {
  uint32_t micros = xbot::service::system::getTimeMicros();
  last_valid_esc_state_micros_ = micros;
  StartTransaction();
  SendLeftESCTemperature(left_esc_state_.temperature_pcb);
  SendLeftESCCurrent(left_esc_state_.current_input);
  SendLeftESCStatus(static_cast<uint8_t>(left_esc_state_.status));

  SendRightESCTemperature(right_esc_state_.temperature_pcb);
  SendRightESCCurrent(right_esc_state_.current_input);
  SendRightESCStatus(static_cast<uint8_t>(right_esc_state_.status));

  // Calculate the twist according to wheel ticks
  if (last_ticks_valid) {
    float dt = static_cast<float>(micros - last_ticks_micros_) / 1000000.0f;
    auto d_left = static_cast<float>(left_esc_state_.tacho - last_ticks_left);
    auto d_right = static_cast<float>(right_esc_state_.tacho - last_ticks_right);
    float vx = (d_right - d_left) / (2.0f * dt * static_cast<float>(WheelTicksPerMeter.value));
    float vr = (d_left + d_right) / (2.0f * dt * static_cast<float>(WheelTicksPerMeter.value));
    double data[6]{};
    data[0] = vx;
    data[5] = vr;
    SendActualTwist(data, 6);
    uint32_t ticks[2];
    ticks[0] = left_esc_state_.tacho;
    ticks[1] = right_esc_state_.tacho;
    SendWheelTicks(ticks, 2);
  }
  last_ticks_valid = true;
  last_ticks_left = left_esc_state_.tacho;
  last_ticks_right = right_esc_state_.tacho;
  last_ticks_micros_ = micros;

  right_esc_state_valid_ = left_esc_state_valid_ = false;

  CommitTransaction();
}

bool DiffDriveService::OnControlTwistChanged(const double* new_value, uint32_t length) {
  if (length != 6) return false;
  chMtxLock(&mtx);
  // we can only do forward and rotation around one axis
  const auto linear = static_cast<float>(new_value[0]);
  const auto angular = static_cast<float>(new_value[5]);

  // TODO: update this to rad/s values and implement xESC speed control
  speed_r_ = linear + 0.5f * static_cast<float>(WheelDistance.value) * angular;
  speed_l_ = -(linear - 0.5f * static_cast<float>(WheelDistance.value) * angular);

  if (speed_l_ >= 1.0) {
    speed_l_ = 1.0;
  } else if (speed_l_ <= -1.0) {
    speed_l_ = -1.0;
  }
  if (speed_r_ >= 1.0) {
    speed_r_ = 1.0;
  } else if (speed_r_ <= -1.0) {
    speed_r_ = -1.0;
  }

  // Limit comms frequency to once per tick()
  if (!duty_sent_) {
    SetDuty();
  }
  chMtxUnlock(&mtx);
  return true;
}
