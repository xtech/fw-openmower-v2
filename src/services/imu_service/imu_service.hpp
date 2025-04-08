//
// Created by clemens on 31.07.24.
//

#ifndef IMU_SERVICE_HPP
#define IMU_SERVICE_HPP

#include <etl/array.h>
#include <etl/string.h>

#include <ImuServiceBase.hpp>

class ImuService : public ImuServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024);

 public:
  explicit ImuService(const uint16_t service_id) : ImuServiceBase(service_id, 10'000, wa, sizeof(wa)) {
  }

 protected:
  void OnCreate() override;
  bool OnStart() override;

 private:
  bool imu_found = false;
  void tick() override;
  etl::string<255> error_message{};

  int16_t data_raw_acceleration[3];
  int16_t data_raw_angular_rate[3];
  int16_t data_raw_temperature;
  double axes[9]{};
  float temperature_degC;
  etl::array<uint8_t, 3> axis_remap_ = {0, 1, 2};  // Default (YardForce mainboard) mapping: X->0, Y->1, Z->2
  etl::array<int8_t, 3> axis_sign_ = {1, -1, -1};  // Default (YardForce mainboard) signs
};

#endif  // IMU_SERVICE_HPP
