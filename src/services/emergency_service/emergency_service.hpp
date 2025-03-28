//
// Created by clemens on 26.07.24.
//

#ifndef EMERGENCY_SERVICE_HPP
#define EMERGENCY_SERVICE_HPP

#include <etl/string.h>

#include <EmergencyServiceBase.hpp>

#include "ch.h"
#include "hal.h"

class EmergencyService : public EmergencyServiceBase {
 private:
  constexpr static uint BUTTON_EMERGENCY_MILLIS = 20;  // 20ms debounce time for an emergency button
  constexpr static size_t MAX_EMERGENCY_REASON_LEN = 100;
  THD_WORKING_AREA(wa, 1024) {};

 public:
  explicit EmergencyService(uint16_t service_id) : EmergencyServiceBase(service_id, 100'000, wa, sizeof(wa)) {
  }

 protected:
  bool OnStart() override;
  void OnStop() override;
  void OnCreate() override;

  /**
   * @brief Returns concatenated names of triggered sensors (or empty string if none).
   * @note If string is not large enough, it get truncated with ", ...".
   */
  etl::string<MAX_EMERGENCY_REASON_LEN> getTriggeredSensors();

 private:
  void tick() override;

  systime_t last_clear_emergency_message_ = 0;
  systime_t button_emergency_started_ = 0;
  systime_t lift_emergency_started_ = 0;
  systime_t tilt_emergency_started_ = 0;
  etl::string<MAX_EMERGENCY_REASON_LEN> emergency_reason{"Boot"};

 protected:
  void OnSetEmergencyChanged(const uint8_t& new_value) override;
};

#endif  // EMERGENCY_SERVICE_HPP
