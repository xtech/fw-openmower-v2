//
// Created by clemens on 26.07.24.
//

#ifndef DIFF_DRIVE_SERVICE_HPP
#define DIFF_DRIVE_SERVICE_HPP

#include <drivers/gps/nmea_gps_driver.h>

#include <DiffDriveServiceBase.hpp>
#include <drivers/motor/motor_driver.hpp>
#include <globals.hpp>
#include <xbot-service/portable/socket.hpp>

using namespace xbot::service;
using namespace xbot::driver::motor;

class DiffDriveService : public DiffDriveServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024);
  MotorDriver *left_esc_driver_ = nullptr;
  MotorDriver *right_esc_driver_ = nullptr;


  MotorDriver::ESCState left_esc_state_{};
  MotorDriver::ESCState right_esc_state_{};
  bool left_esc_state_valid_ = false;
  bool right_esc_state_valid_ = false;
  uint32_t last_valid_esc_state_micros_ = 0;
  uint32_t last_duty_received_micros_ = 0;

  uint32_t last_ticks_left = 0;
  uint32_t last_ticks_right = 0;
  bool last_ticks_valid = false;
  uint32_t last_ticks_micros_ = 0;

  float speed_l_ = 0;
  float speed_r_ = 0;
  bool duty_sent_ = false;

 public:
  explicit DiffDriveService(uint16_t service_id) : DiffDriveServiceBase(service_id, wa, sizeof(wa)) {
  }

  void OnEmergencyChangedEvent();

 void SetDrivers(MotorDriver* left_driver, MotorDriver* right_driver);

 protected:
  bool OnStart() override;
  void OnCreate() override;
  void OnStop() override;

 private:
  void tick();
  ManagedSchedule tick_schedule_{scheduler_, IsRunning(), 40'000,
                                 XBOT_FUNCTION_FOR_METHOD(DiffDriveService, &DiffDriveService::tick, this)};

  void SetDuty();

  void LeftESCCallback(const MotorDriver::ESCState &state);
  void RightESCCallback(const MotorDriver::ESCState &state);
  void ProcessStatusUpdate();

 protected:
  void OnControlTwistChanged(const double *new_value, uint32_t length) override;
};

#endif  // DIFF_DRIVE_SERVICE_HPP
