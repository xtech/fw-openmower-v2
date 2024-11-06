//
// Created by clemens on 26.07.24.
//

#include "diff_drive_service.hpp"

#include <drivers/vesc/esc_status.h>

#include <xbot-service/portable/system.hpp>

#include "../../globals.hpp"

void DiffDriveService::OnMowerStatusChanged(uint32_t new_status) {
  if ((new_status &
       (MOWER_FLAG_EMERGENCY_LATCH | MOWER_FLAG_EMERGENCY_ACTIVE)) == 0) {
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
  if (!WheelDistance.valid || !WheelTicksPerMeter.valid ||
      WheelDistance.value == 0 || WheelTicksPerMeter.value == 0.0) {
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
  left_uart_.startDriver();
  right_uart_.startDriver();
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

  uint32_t micros = xbot::service::system::getTimeMicros();
  left_uart_.requestVescValues();
  right_uart_.requestVescValues();

  bool parse_left_success = left_uart_.parseVescValues();
  bool parse_right_success = right_uart_.parseVescValues();

  StartTransaction();

  if (parse_left_success) {
    SendLeftESCTemperature(left_uart_.data.tempMosfet);
    SendLeftESCCurrent(left_uart_.data.avgMotorCurrent);
    SendLeftESCStatus(left_uart_.data.error == 0 ? ESC_STATUS_OK
                                                 : ESC_STATUS_ERROR);
  } else {
    SendLeftESCStatus(ESC_STATUS_DISCONNECTED);
  }

  if (parse_right_success) {
    SendRightESCTemperature(right_uart_.data.tempMosfet);
    SendRightESCCurrent(right_uart_.data.avgMotorCurrent);
    SendRightESCStatus(left_uart_.data.error == 0 ? ESC_STATUS_OK
                                                  : ESC_STATUS_ERROR);
  } else {
    SendRightESCStatus(ESC_STATUS_DISCONNECTED);
  }

  if (parse_left_success && parse_right_success) {
    // Calculate the twist according to wheel ticks
    if (last_ticks_valid) {
      float dt = static_cast<float>(micros - last_ticks_micros_) / 1000000.0f;
      auto d_left =
          static_cast<float>(left_uart_.data.tachometer - last_ticks_left);
      auto d_right =
          static_cast<float>(right_uart_.data.tachometer - last_ticks_right);
      float vx = (d_right - d_left) /
                 (2.0f * dt * static_cast<float>(WheelTicksPerMeter.value));
      float vr = (d_left + d_right) /
                 (2.0f * dt * static_cast<float>(WheelTicksPerMeter.value));
      double data[6]{};
      data[0] = vx;
      data[5] = vr;
      SendActualTwist(data, 6);
      uint32_t ticks[2];
      ticks[0] = left_uart_.data.tachometer;
      ticks[1] = right_uart_.data.tachometer;
      SendWheelTicks(ticks, 2);
    }
    last_ticks_valid = true;
    last_ticks_left = left_uart_.data.tachometer;
    last_ticks_right = right_uart_.data.tachometer;
    last_ticks_micros_ = micros;
  }

  CommitTransaction();
  duty_sent_ = false;
  chMtxUnlock(&mtx);
}
void DiffDriveService::SetDuty() {
  // Get the current emergency state
  chMtxLock(&mower_status_mutex);
  uint32_t status_copy = mower_status;
  chMtxUnlock(&mower_status_mutex);
  if (status_copy & MOWER_FLAG_EMERGENCY_LATCH) {
    left_uart_.setDuty(0);
    right_uart_.setDuty(0);
  } else {
    left_uart_.setDuty(speed_l_);
    right_uart_.setDuty(speed_r_);
  }
  duty_sent_ = true;
}

bool DiffDriveService::OnControlTwistChanged(const double* new_value,
                                             uint32_t length) {
  if (length != 6) return false;
  chMtxLock(&mtx);
  // we can only do forward and rotation around one axis
  const auto linear = static_cast<float>(new_value[0]);
  const auto angular = static_cast<float>(new_value[5]);

  // TODO: update this to rad/s values and implement xESC speed control
  speed_r_ = linear + 0.5f * static_cast<float>(WheelDistance.value) * angular;
  speed_l_ =
      -(linear - 0.5f * static_cast<float>(WheelDistance.value) * angular);

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
