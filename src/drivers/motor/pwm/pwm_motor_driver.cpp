//
// Created by clemens on 14.04.25.
//

#include "pwm_motor_driver.hpp"

#include <ch.h>

namespace xbot::driver::motor {

void PwmMotorDriver::SetPWM(PWMDriver* pwm, pwmchannel_t channel1, pwmchannel_t channel2) {
  pwm_ = pwm;
  channel_1_ = channel1;
  channel_2_ = channel2;
}
void PwmMotorDriver::SetEncoder(uint32_t line_encoder_a, uint32_t line_encoder_b) {
  line_encoder_a_ = line_encoder_a;
  line_encoder_b_ = line_encoder_b;
}
void PwmMotorDriver::RequestStatus() {
  // Pause interrupts before copying
  chSysLock();
  latest_state_.tacho = tacho;
  latest_state_.tacho_absolute = tacho_abs;
  chSysUnlock();
  NotifyCallback();
}
void PwmMotorDriver::SetDuty(float duty) {
  if (duty > 0) {
    pwmEnableChannel(pwm_, channel_1_, (0xFFF * 4) * duty);
    pwmEnableChannel(pwm_, channel_2_, 0);
  } else {
    pwmEnableChannel(pwm_, channel_2_, (0xFFF * 4) * -duty);
    pwmEnableChannel(pwm_, channel_1_, 0);
  }
}
bool PwmMotorDriver::Start() {
  chDbgAssert(pwm_ != nullptr, "pwm cannot be null");
  chDbgAssert(line_encoder_a_ != 0 && line_encoder_b_ != 0, "encoder lines must be set");

  chDbgAssert(!IsStarted(), "don't start the driver twice");
  if (IsStarted()) {
    return false;
  }

  palSetLineMode(line_encoder_a_, PAL_MODE_INPUT);
  palSetLineCallback(line_encoder_a_, &PwmMotorDriver::HandleEncoderTick, this);
  palEnableLineEvent(line_encoder_a_, PAL_EVENT_MODE_RISING_EDGE);

  return MotorDriver::Start();
}
void PwmMotorDriver::HandleEncoderTick(void* i) {
  auto instance = static_cast<PwmMotorDriver*>(i);
  instance->tacho_abs++;
  if (palReadLine(instance->line_encoder_b_) == PAL_HIGH) {
    instance->tacho++;
  } else {
    instance->tacho--;
  }
}
}  // namespace xbot::driver::motor
