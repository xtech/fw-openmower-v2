//
// Created by clemens on 26.07.24.
//

#ifndef EMERGENCY_SERVICE_HPP
#define EMERGENCY_SERVICE_HPP

#include <etl/string.h>
#include <etl/vector.h>

#include <EmergencyServiceBase.hpp>

#include "globals.hpp"

using namespace xbot::service;

class EmergencyService : public EmergencyServiceBase {
 private:
  THD_WORKING_AREA(wa, 1024){};

 public:
  explicit EmergencyService(uint16_t service_id) : EmergencyServiceBase(service_id, wa, sizeof(wa)) {
  }

  uint16_t GetEmergencyReasons();
  uint32_t CheckInputs(uint32_t now);

  void RequireService(ServiceExt* svc);

  /**
   * @brief Apply @p add / @p clear to the emergency reasons.
   *
   * @return Bitmask of the reasons that actually changed (0 when nothing changed), so a
   *         caller can react to a specific transition.
   */
  uint16_t UpdateEmergency(uint16_t add, uint16_t clear = 0);

 protected:
  void OnStop() override;
  uint32_t OnLoop(uint32_t now_micros, uint32_t last_tick_micros) override;
  void OnHighLevelEmergencyChanged(const uint16_t* new_value, uint32_t length) override;

 private:
  uint32_t CheckTimeouts(uint32_t now);
  uint32_t CheckRequiredServices();
  void SendStatus();

  /**
   * Heartbeat timeout of the high level (ROS) link — the high level heartbeats every 0.5 s
   * (mower_comms.cpp), so two missed beats mean the link is gone.  Drives
   * EmergencyReason::TIMEOUT_HIGH_LEVEL, which the UI shows as a status and which is
   * reported to the high level.
   */
  static constexpr uint32_t kHighLevelTimeoutUs = 1'000'000;

  ServiceSchedule status_schedule_{*this, 1'000'000,
                                   XBOT_FUNCTION_FOR_METHOD(EmergencyService, &EmergencyService::SendStatus, this)};

  MUTEX_DECL(mtx_);

  uint16_t reasons_ = EmergencyReason::TIMEOUT_INPUTS | EmergencyReason::TIMEOUT_HIGH_LEVEL;
  uint32_t last_high_level_emergency_message_ = 0;

  etl::vector<ServiceExt*, 16> required_services_{};
};

#endif  // EMERGENCY_SERVICE_HPP
