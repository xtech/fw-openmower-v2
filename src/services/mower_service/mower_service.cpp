//
// Created by clemens on 31.07.24.
//

#include "mower_service.hpp"

#include <cmath>
#include <xbot-service/portable/system.hpp>

#include "services.hpp"

void MowerService::OnCreate() {
  chDbgAssert(mower_driver_ != nullptr, "Mower Motor Driver cannot be null!");
  mower_driver_->SetStateCallback(
      etl::delegate<void(const MotorDriver::ESCState&)>::create<MowerService, &MowerService::ESCCallback>(*this));
  mower_driver_->Start();
}

bool MowerService::OnStart() {
  mower_duty_ = 0;
  mower_duty_target_ = 0;
  ramping_ = false;
  return true;
}

void MowerService::OnStop() {
  mower_duty_ = 0;
  mower_duty_target_ = 0;
  ramping_ = false;
  esc_ever_connected_ = false;
}

void MowerService::tick() {
  chMtxLock(&mtx);

  // Check, if we recently received duty. If not, set to zero for safety
  if (xbot::service::system::getTimeMicros() - last_duty_received_micros_ > 10'000'000) {
    // it's ok to set it here, because we know that duty_set_ is false (we're in a timeout after all)
    mower_duty_ = 0;
    mower_duty_target_ = 0;
    ramping_ = false;
  }

  // Ramp toward the target during a reversal (flagged in OnMowerSpeedChanged) so
  // a hard direction flip doesn't draw a large current spike. Timed off the real
  // clock, so it stays ~5 s regardless of the tick rate.
  if (ramping_) {
    constexpr float kRampRatePerSecond = 2.0f / 5.0f;  // full +-1 reversal (range 2.0) over 5 s
    const uint32_t now = xbot::service::system::getTimeMicros();
    const float max_step = kRampRatePerSecond * (now - last_ramp_micros_) / 1'000'000.0f;
    last_ramp_micros_ = now;
    const float delta = mower_duty_target_ - mower_duty_;
    if (std::fabs(delta) <= max_step) {
      mower_duty_ = mower_duty_target_;
      ramping_ = false;
    } else {
      mower_duty_ += (delta > 0.0f) ? max_step : -max_step;
    }
    duty_sent_ = false;  // force this step out even if a command already sent this tick
  }

  if (!duty_sent_) {
    // Send motor speed to VESC, if we havent in the meantime
    // (e.g. due to new value or emergency)
    SetDuty();
  }

  mower_driver_->RequestStatus();

  // TODO: actually detect some rain
  bool rain_detected = false;

  StartTransaction();
  SendRainDetected(rain_detected);

  // Check, if we have received ESC status updates recently. If not, send a disconnected message
  if (xbot::service::system::getTimeMicros() - last_valid_esc_state_micros_ > 1'000'000 || !esc_state_valid_) {
    // No recent update received (or none at all)
    mower_duty_ = 0;
    mower_duty_target_ = 0;
    ramping_ = false;
    SendMowerStatus(static_cast<uint8_t>(MotorDriver::ESCState::ESCStatus::ESC_STATUS_DISCONNECTED));
  } else {
    // We got recent data, send it
    StartTransaction();
    SendMowerESCTemperature(esc_state_.temperature_pcb);
    SendMowerMotorCurrent(esc_state_.current_input);
    SendMowerStatus(static_cast<uint8_t>(esc_state_.status));
    SendMowerMotorTemperature(esc_state_.temperature_motor);
    SendMowerRunning(std::fabs(esc_state_.rpm) > 0);
    // Sign by measured direction; not sign(rpm) (YFR4esc reports unsigned rpm).
    SendMowerMotorRPM(esc_state_.direction > 0.5f ? -std::fabs(esc_state_.rpm) : std::fabs(esc_state_.rpm));
  }
  CommitTransaction();

  duty_sent_ = false;
  chMtxUnlock(&mtx);
}

void MowerService::ESCCallback(const MotorDriver::ESCState& state) {
  chMtxLock(&state_mutex_);
  esc_state_ = state;
  esc_state_valid_ = true;
  esc_ever_connected_ = true;
  last_valid_esc_state_micros_ = xbot::service::system::getTimeMicros();
  chMtxUnlock(&state_mutex_);
}

void MowerService::SetDuty() {
  // Get the current emergency state
  bool emergency = emergency_service.GetEmergencyReasons() != 0;
  if (emergency) {
    mower_driver_->SetDuty(0);
  } else {
    mower_driver_->SetDuty(mower_duty_);
  }
  duty_sent_ = true;
}

void MowerService::OnMowerSpeedChanged(const float& new_value) {
  chMtxLock(&mtx);
  last_duty_received_micros_ = xbot::service::system::getTimeMicros();
  // Normalized speed in [-1, 1]: sign = direction, magnitude = duty, 0 = off.
  const float target = new_value < -1.0f ? -1.0f : (new_value > 1.0f ? 1.0f : new_value);
  mower_duty_target_ = target;

  // Ramp only when the blade is physically spinning the opposite way to the new
  // command, judged from ESC telemetry (not the last command, which misses a
  // stop-then-reverse). direction is meaningless at standstill, so require spin.
  chMtxLock(&state_mutex_);  // esc_state_ is written by ESCCallback under state_mutex_
  const float esc_rpm = esc_state_.rpm;
  const bool esc_reverse = esc_state_.direction > 0.5f;
  chMtxUnlock(&state_mutex_);
  constexpr float kSpinningRpm = 100.0f;  // raw ESC rpm; just rejects standstill noise
  const bool spinning = std::fabs(esc_rpm) > kSpinningRpm;
  const bool reversal_under_load = spinning && target != 0.0f && (esc_reverse != (target < 0.0f));

  if (target == 0.0f) {
    // Stop immediately (aborts any ramp).
    ramping_ = false;
    mower_duty_ = 0.0f;
  } else if (!ramping_ && reversal_under_load) {
    // Reversal while spinning: ramp it in tick() instead of snapping.
    ramping_ = true;
    last_ramp_micros_ = last_duty_received_micros_;  // ramp clock starts now
  } else if (!ramping_) {
    mower_duty_ = target;
  }

  if (!ramping_ && !duty_sent_) {
    SetDuty();
  }
  chMtxUnlock(&mtx);
}

void MowerService::SetDriver(MotorDriver* motor_driver) {
  mower_driver_ = motor_driver;
}
void MowerService::OnEmergencyChangedEvent() {
  bool emergency = emergency_service.GetEmergencyReasons() != 0;
  if (!emergency) {
    // only set speed to 0 if the emergency happens, not if it's cleared
    return;
  }
  chMtxLock(&mtx);
  mower_duty_ = 0;
  mower_duty_target_ = 0;
  ramping_ = false;
  // Instantly send the 0 duty cycle
  SetDuty();
  chMtxUnlock(&mtx);
}
