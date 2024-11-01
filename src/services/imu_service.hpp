//
// Created by clemens on 31.07.24.
//

#ifndef IMU_SERVICE_HPP
#define IMU_SERVICE_HPP

#include <ImuServiceBase.hpp>

class ImuService : public ImuServiceBase {
 private:
  THD_WORKING_AREA(stack, 25600){};

 public:
  explicit ImuService(const uint16_t service_id)
      : ImuServiceBase(service_id, 10000, stack, sizeof(stack)) {}

 protected:
  bool Configure() override;
  void OnStart() override;
  void OnStop() override;
  void OnCreate() override;

 private:
  bool imu_found = false;
  void tick() override;
};

#endif  // IMU_SERVICE_HPP
