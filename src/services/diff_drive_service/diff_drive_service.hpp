//
// Created by clemens on 26.07.24.
//

#ifndef DIFF_DRIVE_SERVICE_HPP
#define DIFF_DRIVE_SERVICE_HPP

#include <drivers/vesc/VescDriver.h>

#include <DiffDriveServiceBase.hpp>
#include <debug/debug_tcp_interface.hpp>
#include <xbot-service/portable/socket.hpp>
using namespace xbot::driver::esc;

class DiffDriveService : public DiffDriveServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024);
  VescDriver left_esc_driver_{};
  VescDriver right_esc_driver_{};

  DebugTCPInterface left_esc_driver_interface_{65102, &left_esc_driver_};
  DebugTCPInterface right_esc_driver_interface_{65104, &right_esc_driver_};

  VescDriver::ESCState left_esc_state_{};
  VescDriver::ESCState right_esc_state_{};
  bool left_esc_state_valid_ = false;
  bool right_esc_state_valid_ = false;
  uint32_t last_valid_esc_state_micros_ = 0;
  uint32_t last_duty_received_micros_ = 0;

  uint32_t last_ticks_left = 0;
  uint32_t last_ticks_right = 0;
  bool last_ticks_valid = false;
  uint32_t last_ticks_micros_ = 0;

 // MUTEX_DECL(mtx);
  float speed_l_ = 0;
  float speed_r_ = 0;
  bool duty_sent_ = false;

 public:
  explicit DiffDriveService(uint16_t service_id) : DiffDriveServiceBase(service_id, 40000, wa, sizeof(wa)) {
  }

  void OnMowerStatusChanged(uint32_t new_status);

 protected:
  bool Configure() override;
  void OnStart() override;
  void OnCreate() override;
  void OnStop() override;

 private:
  void tick() override;

  void SetDuty();

  void LeftESCCallback(const VescDriver::ESCState &state);
  void RightESCCallback(const VescDriver::ESCState &state);
  void ProcessStatusUpdate();

 protected:
  void OnControlTwistChanged(const double *new_value, uint32_t length) override;
};

#endif  // DIFF_DRIVE_SERVICE_HPP
