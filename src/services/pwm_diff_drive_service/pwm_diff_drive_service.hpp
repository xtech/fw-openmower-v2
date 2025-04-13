//
// Created by clemens on 03.04.25.
//

#ifndef PWM_DIFF_DRIVE_SERVICE_HPP
#define PWM_DIFF_DRIVE_SERVICE_HPP


#include <drivers/vesc/VescDriver.h>

#include <DiffDriveServiceBase.hpp>
#include <debug/debug_tcp_interface.hpp>
#include <globals.hpp>
#include <xbot-service/portable/socket.hpp>

using namespace xbot::driver::esc;
using namespace xbot::service;

class PWMDiffDriveService : public DiffDriveServiceBase {
private:
  THD_WORKING_AREA(wa, 1024);
  float speed_l_ = 0;
  float speed_r_ = 0;
  bool duty_sent_ = false;

  volatile uint32_t tacho_left = 0;
  volatile uint32_t tacho_left_abs = 0;
  volatile uint32_t tacho_right = 0;
  volatile uint32_t tacho_right_abs = 0;

  uint32_t last_ticks_left = 0;
  uint32_t last_ticks_right = 0;
  bool last_ticks_valid = false;
  uint32_t last_ticks_micros_ = 0;

public:
 explicit PWMDiffDriveService(uint16_t service_id) : DiffDriveServiceBase(service_id, wa, sizeof(wa)) {
 }

  void OnMowerStatusChanged(MowerStatus new_status);

protected:
  bool OnStart() override;
  void OnCreate() override;
  void OnStop() override;

private:
 void tick();
 ManagedSchedule tick_schedule_{scheduler_, IsRunning(), 40'000,
                                XBOT_FUNCTION_FOR_METHOD(PWMDiffDriveService, &PWMDiffDriveService::tick, this)};

 static void HandleEncoderTickLeft(void* instance);
 static void HandleEncoderTickRight(void* instance);

 PWMConfig pwm_config_{};
 void SetDuty();

protected:
  void OnControlTwistChanged(const double *new_value, uint32_t length) override;
};


#endif //PWM_DIFF_DRIVE_SERVICE_HPP
