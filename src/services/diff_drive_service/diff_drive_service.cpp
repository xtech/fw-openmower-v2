//
// Created by clemens on 26.07.24.
//

#include "diff_drive_service.hpp"

#include <ulog.h>

#include <drivers/motor/motor_driver.hpp>
#include <xbot-service/portable/system.hpp>

using namespace xbot::driver::motor;

void DiffDriveService::OnEmergencyChangedEvent() {
  MowerStatus mower_status = GetMowerStatus();
  if (!mower_status.emergency_latch && !mower_status.emergency_active) {
    // only set speed to 0 if the emergency happens, not if it's cleared
    return;
  }
  chMtxLock(&state_mutex_);
  speed_l_ = 0;
  speed_r_ = 0;
  // Instantly send the 0 duty cycle
  SetDuty();
  chMtxUnlock(&state_mutex_);
}
void DiffDriveService::SetDrivers(MotorDriver* left_driver, MotorDriver* right_driver) {
  left_esc_driver_ = left_driver;
  right_esc_driver_ = right_driver;
}

bool DiffDriveService::OnStart() {
  // Check, if configuration is valid, if not retry
  if (WheelDistance.value == 0) {
    ULOG_ARG_ERROR(&service_id_, "WheelDistance was 0, cannot start service!");
    return false;
  }

  if (WheelTicksPerMeter.value == 0.0) {
    ULOG_ARG_ERROR(&service_id_, "WheelTicksPerMeter was 0, cannot start service!");
    return false;
  }

  speed_l_ = speed_r_ = 0;
  last_ticks_valid = false;
  return true;
}

void DiffDriveService::OnCreate() {
  chDbgAssert(left_esc_driver_ != nullptr, "Left Motor Driver cannot be null!");
  chDbgAssert(right_esc_driver_ != nullptr, "Right Motor Driver cannot be null!");

  // Register callbacks
  left_esc_driver_->SetStateCallback(
      etl::delegate<void(const MotorDriver::ESCState&)>::create<DiffDriveService, &DiffDriveService::LeftESCCallback>(
          *this));
  right_esc_driver_->SetStateCallback(
      etl::delegate<void(const MotorDriver::ESCState&)>::create<DiffDriveService, &DiffDriveService::RightESCCallback>(
          *this));

  left_esc_driver_->Start();
  right_esc_driver_->Start();
}

void DiffDriveService::OnStop() {
  speed_l_ = speed_r_ = 0;
  last_ticks_valid = false;
}

void DiffDriveService::tick() {
  chMtxLock(&state_mutex_);

  // Check, if we recently received duty. If not, set to zero for safety
  if (xbot::service::system::getTimeMicros() - last_duty_received_micros_ > 1'000'000) {
    // it's ok to set it here, because we know that duty_set_ is false (we're in a timeout after all)
    speed_l_ = speed_r_ = 0;
  }

  if (!duty_sent_) {
    SetDuty();
  }

  left_esc_driver_->RequestStatus();
  right_esc_driver_->RequestStatus();

  // Check, if we have received ESC status updates recently. If not, send a disconnected message
  if (xbot::service::system::getTimeMicros() - last_valid_esc_state_micros_ > 1'000'000) {
    StartTransaction();
    if (!left_esc_state_valid_) {
      SendLeftESCStatus(static_cast<uint8_t>(MotorDriver::ESCState::ESCStatus::ESC_STATUS_DISCONNECTED));
    }
    if (!right_esc_state_valid_) {
      SendRightESCStatus(static_cast<uint8_t>(MotorDriver::ESCState::ESCStatus::ESC_STATUS_DISCONNECTED));
    }
    CommitTransaction();
  }

  duty_sent_ = false;
  chMtxUnlock(&state_mutex_);
}

void DiffDriveService::SetDuty() {
  // Get the current emergency state
  MowerStatus mower_status = GetMowerStatus();
  if (mower_status.emergency_latch) {
    left_esc_driver_->SetDuty(0);
    right_esc_driver_->SetDuty(0);
  } else {
    left_esc_driver_->SetDuty(speed_l_);
    right_esc_driver_->SetDuty(speed_r_);
  }
  duty_sent_ = true;
}

void DiffDriveService::LeftESCCallback(const MotorDriver::ESCState& state) {
  chMtxLock(&state_mutex_);
  left_esc_state_ = state;
  left_esc_state_valid_ = true;
  if (right_esc_state_valid_) {
    ProcessStatusUpdate();
  }
  chMtxUnlock(&state_mutex_);
}

void DiffDriveService::RightESCCallback(const MotorDriver::ESCState& state) {
  chMtxLock(&state_mutex_);
  right_esc_state_ = state;
  right_esc_state_valid_ = true;
  if (left_esc_state_valid_) {
    ProcessStatusUpdate();
  }
  chMtxUnlock(&state_mutex_);
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
    float dt = static_cast<float>(micros - last_ticks_micros_) / 1'000'000.0f;
    int32_t d_left = static_cast<int32_t>(left_esc_state_.tacho - last_ticks_left);
    int32_t d_right = static_cast<int32_t>(right_esc_state_.tacho - last_ticks_right);
    float vx = static_cast<float>(d_left - d_right) / (2.0f * dt * static_cast<float>(WheelTicksPerMeter.value));
    float vr = -static_cast<float>(d_left + d_right) / (2.0f * dt * static_cast<float>(WheelTicksPerMeter.value));
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

void DiffDriveService::OnControlTwistChanged(const double* new_value, uint32_t length) {
  if (length != 6) return;
  chMtxLock(&state_mutex_);
  last_duty_received_micros_ = xbot::service::system::getTimeMicros();
  // we can only do forward and rotation around one axis
  const auto linear = static_cast<float>(new_value[0]);
  const auto angular = static_cast<float>(new_value[5]);

  // TODO: update this to rad/s values and implement xESC speed control
  speed_r_ = -(linear + 0.5f * static_cast<float>(WheelDistance.value) * angular);
  speed_l_ = linear - 0.5f * static_cast<float>(WheelDistance.value) * angular;

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
  chMtxUnlock(&state_mutex_);
}
