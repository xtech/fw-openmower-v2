//
// Created by clemens on 03.04.25.
//

#include <robot.hpp>

#ifdef PWM_DIFF_DRIVE

#include <hal.h>

#include <xbot-service/portable/system.hpp>

#include "pwm_diff_drive_service.hpp"

void PWMDiffDriveService::OnMowerStatusChanged(MowerStatus new_status) {
  if (!new_status.emergency_latch && !new_status.emergency_active) {
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
bool PWMDiffDriveService::OnStart() {
  speed_l_ = speed_r_ = 0;
  SetDuty();
  return true;
}
void PWMDiffDriveService::OnCreate() {
  palSetLineMode(LINE_MOTOR1_ENCODER_A, PAL_MODE_INPUT);
  palSetLineCallback(LINE_MOTOR1_ENCODER_A, &PWMDiffDriveService::HandleEncoderTickLeft, this);
  palEnableLineEvent(LINE_MOTOR1_ENCODER_A, PAL_EVENT_MODE_RISING_EDGE);
  palSetLineMode(LINE_MOTOR2_ENCODER_A, PAL_MODE_INPUT);
  palSetLineCallback(LINE_MOTOR2_ENCODER_A, &PWMDiffDriveService::HandleEncoderTickRight, this);
  palEnableLineEvent(LINE_MOTOR2_ENCODER_A, PAL_EVENT_MODE_RISING_EDGE);

  speed_l_ = speed_r_ = 0;
  SetDuty();
}
void PWMDiffDriveService::OnStop() {
  speed_l_ = speed_r_ = 0;
  SetDuty();
}
void PWMDiffDriveService::tick() {

  if (!duty_sent_) {
    SetDuty();
  }

  uint32_t micros = xbot::service::system::getTimeMicros();


  StartTransaction();
  chSysLock();
  uint32_t tl = tacho_left;
  uint32_t tr = tacho_right;
  chSysUnlock();

  // Calculate the twist according to wheel ticks
  if (last_ticks_valid) {
    float dt = static_cast<float>(micros - last_ticks_micros_) / 1'000'000.0f;
    int32_t d_left = static_cast<int32_t>(tl - last_ticks_left);
    int32_t d_right = static_cast<int32_t>(tr - last_ticks_right);
    float vx = static_cast<float>(d_left - d_right) / (2.0f * dt * static_cast<float>(WheelTicksPerMeter.value));
    float vr = -static_cast<float>(d_left + d_right) / (2.0f * dt * static_cast<float>(WheelTicksPerMeter.value));
    double data[6]{};
    data[0] = vx;
    data[5] = vr;
    SendActualTwist(data, 6);
    uint32_t ticks[2];
    ticks[0] = tl;
    ticks[1] = tr;
    SendWheelTicks(ticks, 2);
  }
  last_ticks_valid = true;
  last_ticks_left = tl;
  last_ticks_right = tr;
  last_ticks_micros_ = micros;

  CommitTransaction();

  duty_sent_ = false;
}
void PWMDiffDriveService::HandleEncoderTickLeft(void* i) {
  auto instance = static_cast<PWMDiffDriveService*>(i);
    instance->tacho_left_abs++;
    if (palReadLine(LINE_MOTOR1_ENCODER_B) == PAL_HIGH) {
      instance->tacho_left++;
    } else {
      instance->tacho_left--;
    }

}
void PWMDiffDriveService::HandleEncoderTickRight(void* i) {
  auto instance = static_cast<PWMDiffDriveService*>(i);
    instance->tacho_right_abs++;
    if (palReadLine(LINE_MOTOR2_ENCODER_B) == PAL_HIGH) {
      instance->tacho_right++;
    } else {
      instance->tacho_right--;
    }

}
void PWMDiffDriveService::SetDuty() {
  if (speed_l_ > 0) {
    pwmEnableChannel(&MOTOR1_PWM, MOTOR1_PWM_CHANNEL_1, (0xFFF * 4) * speed_l_);
    pwmEnableChannel(&MOTOR1_PWM, MOTOR1_PWM_CHANNEL_2, 0);
  } else {
    pwmEnableChannel(&MOTOR1_PWM, MOTOR1_PWM_CHANNEL_2,  (0xFFF * 4) * -speed_l_);
    pwmEnableChannel(&MOTOR1_PWM, MOTOR1_PWM_CHANNEL_1,  0);
  }
  if (speed_l_ > 0) {
    pwmEnableChannel(&MOTOR2_PWM, MOTOR2_PWM_CHANNEL_1, (0xFFF * 4) * speed_r_);
    pwmEnableChannel(&MOTOR2_PWM, MOTOR2_PWM_CHANNEL_2, 0);
  } else {
    pwmEnableChannel(&MOTOR2_PWM, MOTOR2_PWM_CHANNEL_2,  (0xFFF * 4) * -speed_r_);
    pwmEnableChannel(&MOTOR2_PWM, MOTOR2_PWM_CHANNEL_1,  0);
  }
  duty_sent_ = true;
}
void PWMDiffDriveService::OnControlTwistChanged(const double* new_value, uint32_t length) {
  if (length != 6) return;
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
}

#endif
