//
// Created by clemens on 31.07.24.
//

#ifndef IMU_SERVICE_HPP
#define IMU_SERVICE_HPP

#include <ImuServiceBase.hpp>

class ImuService : public ImuServiceBase {
 private:
  THD_WORKING_AREA(wa, 1000);

 public:
  explicit ImuService(const uint16_t service_id) : ImuServiceBase(service_id, 10'000, wa, sizeof(wa)) {
  }

 protected:
  void OnCreate() override;

 private:
  bool imu_found = false;
  void tick() override;

  int16_t data_raw_acceleration[3];
  int16_t data_raw_angular_rate[3];
  int16_t data_raw_temperature;
  double axes[9]{};
  float temperature_degC;
};

#endif  // IMU_SERVICE_HPP
