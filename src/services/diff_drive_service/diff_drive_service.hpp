//
// Created by clemens on 26.07.24.
//

#ifndef DIFF_DRIVE_SERVICE_HPP
#define DIFF_DRIVE_SERVICE_HPP

#include <drivers/vesc/VescUart.h>

#include <DiffDriveServiceBase.hpp>
#include <xbot-service/portable/socket.hpp>

class DiffDriveService : public DiffDriveServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024);
  VescUart left_uart_{&SD1};
  VescUart right_uart_{&SD4};

  long last_ticks_left = 0;
  long last_ticks_right = 0;
  bool last_ticks_valid = false;
  uint32_t last_ticks_micros_ = 0;

  mutex_t mtx{};
  float speed_l_ = 0;
  float speed_r_ = 0;

  bool duty_sent_ = false;

 public:
  explicit DiffDriveService(uint16_t service_id)
      : DiffDriveServiceBase(service_id, 20000, wa, sizeof(wa)) {
    chMtxObjectInit(&mtx);
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

 protected:
  bool OnControlTwistChanged(const double* new_value, uint32_t length) override;
};

#endif  // DIFF_DRIVE_SERVICE_HPP
