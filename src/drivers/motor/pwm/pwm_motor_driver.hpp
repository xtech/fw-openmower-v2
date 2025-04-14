//
// Created by clemens on 14.04.25.
//

#ifndef PWM_DRIVER_HPP
#define PWM_DRIVER_HPP

#include <hal.h>

#include <drivers/motor/motor_driver.hpp>

namespace xbot::driver::motor {
class PwmMotorDriver : public MotorDriver {
 public:
  ~PwmMotorDriver() override = default;

  void SetPWM(PWMDriver* pwm, pwmchannel_t channel1, pwmchannel_t channel2);
  void SetEncoder(uint32_t line_encoder_a, uint32_t line_encoder_b);
  void RequestStatus() override;
  void SetDuty(float duty) override;
  bool Start() override;

 private:
  PWMDriver* pwm_ = nullptr;
  pwmchannel_t channel_1_ = 0, channel_2_ = 0;
  uint32_t line_encoder_a_ = 0, line_encoder_b_ = 0;

  float duty_now_ = 0.0f;
  bool duty_sent_ = false;
  volatile uint32_t tacho = 0;
  volatile uint32_t tacho_abs = 0;
  uint32_t last_ticks_left = 0;
  bool last_ticks_valid = false;
  uint32_t last_ticks_micros_ = 0;

  static void HandleEncoderTick(void* instance);
};
}  // namespace xbot::driver::motor

#endif  // PWM_DRIVER_HPP
