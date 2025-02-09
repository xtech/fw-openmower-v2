//
// Created by clemens on 31.07.24.
//

#ifndef MOWER_SERVICE_HPP
#define MOWER_SERVICE_HPP

#include <ch.h>
#include <drivers/vesc/VescDriver.h>

#include <MowerServiceBase.hpp>
#include <debug/debug_tcp_interface.hpp>

class MowerService : public MowerServiceBase {
 public:
  explicit MowerService(const uint16_t service_id);

  void OnMowerStatusChanged(uint32_t new_status);

 protected:
  void OnCreate() override;
  void OnStart() override;
  void OnStop() override;

 private:
  void tick() override;
  void SetDuty();
  MUTEX_DECL(mtx);

 protected:
  void OnMowerEnabledChanged(const uint8_t& new_value) override;

 private:
  THD_WORKING_AREA(wa, 1024){};
  float mower_duty_ = 0;
  bool duty_sent_ = false;
  xbot::driver::esc::VescDriver mower_driver_{};
  DebugTCPInterface mower_esc_driver_interface_{65103, &mower_driver_};
};

#endif  // MOWER_SERVICE_HPP
