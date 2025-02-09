//
// Created by clemens on 26.07.24.
//

#ifndef EMERGENCY_SERVICE_HPP
#define EMERGENCY_SERVICE_HPP

#include <etl/string.h>

#include <EmergencyServiceBase.hpp>

class EmergencyService : public EmergencyServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024){};

 public:
  explicit EmergencyService(uint16_t service_id) : EmergencyServiceBase(service_id, 100000, wa, sizeof(wa)) {
  }

 protected:
  bool Configure() override;
  void OnStart() override;
  void OnStop() override;
  void OnCreate() override;

 private:
  void tick() override;

  systime_t last_clear_emergency_message_ = 0;
  etl::string<100> emergency_reason{"Boot"};

 protected:
  bool OnSetEmergencyChanged(const uint8_t& new_value) override;
};

#endif  // EMERGENCY_SERVICE_HPP
