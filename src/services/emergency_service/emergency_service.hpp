//
// Created by clemens on 26.07.24.
//

#ifndef EMERGENCY_SERVICE_HPP
#define EMERGENCY_SERVICE_HPP

#include <etl/string.h>

#include <EmergencyServiceBase.hpp>

using namespace xbot::service;

class EmergencyService : public EmergencyServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024){};

 public:
  explicit EmergencyService(uint16_t service_id) : EmergencyServiceBase(service_id, wa, sizeof(wa)) {
  }

 protected:
  bool OnStart() override;
  void OnStop() override;

 private:
  void tick();
  ManagedSchedule tick_schedule_{scheduler_, IsRunning(), 100'000,
                                 XBOT_FUNCTION_FOR_METHOD(EmergencyService, &EmergencyService::tick, this)};

  systime_t last_clear_emergency_message_ = 0;
  etl::string<100> emergency_reason{"Boot"};

 protected:
  void OnSetEmergencyChanged(const uint8_t& new_value) override;
};

#endif  // EMERGENCY_SERVICE_HPP
