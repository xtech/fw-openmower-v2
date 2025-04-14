//
// Created by clemens on 31.07.24.
//

#ifndef IMU_SERVICE_HPP
#define IMU_SERVICE_HPP

#include <etl/array.h>
#include <etl/string.h>

#include <ImuServiceBase.hpp>

using namespace xbot::service;

class ImuService : public ImuServiceBase {
 private:
  THD_WORKING_AREA(wa, 1000){};

 public:
  explicit ImuService(const uint16_t service_id) : ImuServiceBase(service_id, wa, sizeof(wa)) {
  }

 protected:
  void OnCreate() override;
  bool OnStart() override;

 private:
  bool imu_found = false;
  etl::string<255> error_message{};

  int16_t data_raw_acceleration[3];
  int16_t data_raw_angular_rate[3];
  int16_t data_raw_temperature;
  double axes[9]{};
  float temperature_degC;

  // Default (YardForce mainboard) mapping: +X-Y-Z
  etl::array<uint8_t, 3> axis_remap_idx_{1, 2, 3};
  etl::array<int8_t, 3> axis_remap_sign_{1, -1, -1};

  void tick();
  ManagedSchedule tick_schedule_{scheduler_, IsRunning(), 10'000,
                                 XBOT_FUNCTION_FOR_METHOD(ImuService, &ImuService::tick, this)};
};

#endif  // IMU_SERVICE_HPP
