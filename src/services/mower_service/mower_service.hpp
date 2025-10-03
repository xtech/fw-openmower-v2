//
// Created by clemens on 31.07.24.
//

#ifndef MOWER_SERVICE_HPP
#define MOWER_SERVICE_HPP

#include <ch.h>

#include <MowerServiceBase.hpp>
#include <debug/debug_tcp_interface.hpp>
#include <drivers/motor/motor_driver.hpp>
#include <globals.hpp>

using namespace xbot::driver::motor;
using namespace xbot::service;

class MowerService : public MowerServiceBase {
 public:
  explicit MowerService(const uint16_t service_id) : MowerServiceBase(service_id, wa, sizeof(wa)) {
  }

  MotorDriver::ESCState GetESCState() const {
    return esc_state_;
  }

  void SetDriver(MotorDriver* motor_driver);

  void OnEmergencyChangedEvent();

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
  void OnMowerEnabledChanged(const uint8_t& new_value) override;

 private:
  THD_WORKING_AREA(wa, 1024){};
  MotorDriver::ESCState esc_state_{};
  bool esc_state_valid_ = false;
  uint32_t last_duty_received_micros_ = 0;
  uint32_t last_valid_esc_state_micros_ = 0;

  float mower_duty_ = 0;
  bool duty_sent_ = false;
  MotorDriver* mower_driver_ = nullptr;
};

#endif  // MOWER_SERVICE_HPP
