//
// Created by clemens on 31.07.24.
//

#ifndef MOWER_SERVICE_HPP
#define MOWER_SERVICE_HPP

#include <ch.h>
#include <etl/atomic.h>

#include <MowerServiceBase.hpp>
#include <cmath>
#include <debug/debug_tcp_interface.hpp>
#include <drivers/motor/motor_driver.hpp>
#include <globals.hpp>
#include <xbot-service/portable/system.hpp>

using namespace xbot::driver::motor;
using namespace xbot::service;

class MowerService : public MowerServiceBase {
 public:
  explicit MowerService(const uint16_t service_id) : MowerServiceBase(service_id, wa, sizeof(wa)) {
  }

  MotorDriver::ESCState GetESCState() const {
    return esc_state_;
  }

  /**
   * @brief Whether a recent ESC status arrived (the last one may be a second old).
   *
   * The tick reports DISCONNECTED when this is false, and nothing that reads the last
   * known state may treat it as current.
   */
  [[nodiscard]] bool HasEscState() const {
    return esc_state_valid_ &&
           (xbot::service::system::getTimeMicros() - last_valid_esc_state_micros_ <= kEscStateTimeoutUs);
  }

  /**
   * @brief Whether the blade runs right now — the value `SendMowerRunning` reports.
   *
   * One definition of "the robot is mowing", used by the status message and by the sound
   * state machine: the ESC talks to us and the motor turns.
   */
  [[nodiscard]] bool IsMowerRunning() const {
    return HasEscState() && (std::fabs(esc_state_.rpm) > 0.0F);
  }

  void SetDriver(MotorDriver* motor_driver);

  void OnEmergencyChangedEvent();

  bool IsHealthy() override {
    return IsRunning() && esc_ever_connected_;
  }

 protected:
  void OnCreate() override;
  bool OnStart() override;
  void OnStop() override;

 private:
  void tick();
  ServiceSchedule tick_schedule_{*this, 500'000, XBOT_FUNCTION_FOR_METHOD(MowerService, &MowerService::tick, this)};

  void SetDuty();
  MUTEX_DECL(mtx);

  void ESCCallback(const MotorDriver::ESCState& state);

 protected:
  void OnMowerSpeedChanged(const float& new_value) override;

 private:
  THD_WORKING_AREA(wa, 1024){};
  MotorDriver::ESCState esc_state_{};
  bool esc_state_valid_ = false;

  // How old the last ESC status may be before it counts as "no ESC" (see HasEscState).
  static constexpr uint32_t kEscStateTimeoutUs = 1'000'000;
  uint32_t last_duty_received_micros_ = 0;
  uint32_t last_valid_esc_state_micros_ = 0;

  float mower_duty_ = 0;         // applied duty [-1, 1]; sign = direction, 0 = off
  float mower_duty_target_ = 0;  // commanded duty; mower_duty_ ramps to it on reversal
  bool ramping_ = false;         // reversal ramp in progress
  uint32_t last_ramp_micros_ = 0;
  etl::atomic<bool> esc_ever_connected_{false};
  MotorDriver* mower_driver_ = nullptr;

  // RPM safety limit from robot config. NAN = disabled.
  float mower_max_safe_rpm_ = std::numeric_limits<float>::quiet_NaN();
};

#endif  // MOWER_SERVICE_HPP
