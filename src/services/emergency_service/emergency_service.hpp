//
// Created by clemens on 26.07.24.
//

#ifndef EMERGENCY_SERVICE_HPP
#define EMERGENCY_SERVICE_HPP

#include <etl/string.h>

#include <EmergencyServiceBase.hpp>

#include "globals.hpp"

using namespace xbot::service;

class EmergencyService : public EmergencyServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024){};

 public:
  explicit EmergencyService(uint16_t service_id) : EmergencyServiceBase(service_id, wa, sizeof(wa)) {
  }

  bool GetEmergency();
  void CheckInputs();

 protected:
  void OnStop() override;
  void OnHighLevelEmergencyChanged(const uint16_t* new_value, uint32_t length) override;

 private:
  void Check();
  void CheckTimeouts();
  ServiceSchedule timeouts_schedule_{*this, 50'000,
                                     XBOT_FUNCTION_FOR_METHOD(EmergencyService, &EmergencyService::Check, this)};

  void SendStatus();
  ServiceSchedule status_schedule_{*this, 1'000'000,
                                   XBOT_FUNCTION_FOR_METHOD(EmergencyService, &EmergencyService::SendStatus, this)};

  MUTEX_DECL(mtx_);

  void UpdateEmergency(uint16_t add, uint16_t clear = 0);

  uint16_t reasons_ = EmergencyReason::TIMEOUT_INPUTS | EmergencyReason::TIMEOUT_HIGH_LEVEL;
  systime_t last_high_level_emergency_message_ = 0;
};

#endif  // EMERGENCY_SERVICE_HPP
