//
// Created by clemens on 31.07.24.
//

#ifndef MOWER_SERVICE_HPP
#define MOWER_SERVICE_HPP

#include <ch.h>
#include <drivers/vesc/VescUart.h>

#include <MowerServiceBase.hpp>

class MowerService : public MowerServiceBase {
 public:
  explicit MowerService(const uint16_t service_id);

  void OnMowerStatusChanged(uint32_t new_status);

 protected:
  bool Configure() override;
  void OnCreate() override;
  void OnStart() override;
  void OnStop() override;

 private:
  void tick() override;
  void SetDuty();
  mutex_t mtx{};

 protected:
  bool OnMowerEnabledChanged(const uint8_t& new_value) override;

 private:
  THD_WORKING_AREA(wa, 1024){};
  float mower_duty_ = 0;
  bool duty_sent_ = false;
  VescUart mower_uart_;
};

#endif  // MOWER_SERVICE_HPP
