//
// Created by clemens on 09.09.24.
//

#ifndef POWER_SERVICE_HPP
#define POWER_SERVICE_HPP

#include <ch.h>

#include <PowerServiceBase.hpp>
#include <drivers/bq_2576/bq_2576.hpp>

class PowerService : public PowerServiceBase {
 public:
  explicit PowerService(uint16_t service_id) : PowerServiceBase(service_id, 1'000'000, wa, sizeof(wa)) {
  }

 protected:
  bool OnStart() override;

 private:
  static constexpr auto CHARGE_STATUS_ERROR = "Error";
  static constexpr auto CHARGE_STATUS_FAULT = "Error (Fault)";
  static constexpr auto CHARGE_STATUS_CHARGER_NOT_FOUND = "Charger Comms Error";
  static constexpr auto CHARGE_STATUS_NOT_CHARGING = "Not Charging";
  static constexpr auto CHARGE_STATUS_PRE_CHARGE = "Pre Charge";
  static constexpr auto CHARGE_STATUS_TRICKLE = "Trickle Charge";
  static constexpr auto CHARGE_STATUS_CC = "Fast Charge (CC)";
  static constexpr auto CHARGE_STATUS_CV = "Taper Charge (CV)";
  static constexpr auto CHARGE_STATUS_TOP_OFF = "Top Off";
  static constexpr auto CHARGE_STATUS_DONE = "Done";

  void tick() override;

  bool charger_configured_ = false;
  uint8_t faults = 0;
  float charge_current = 0;
  float adapter_volts = 0;
  float battery_volts = 0;
  int critical_count = 0;
  uint8_t status1 = 0;
  uint8_t status2 = 0;
  uint8_t status3 = 0;
  uint8_t flags1 = 0;
  uint8_t flags2 = 0;
  uint8_t flags3 = 0;
  BQ2576 charger{};
  THD_WORKING_AREA(wa, 1500){};

 protected:
  void OnChargingAllowedChanged(const uint8_t& new_value) override;
};

#endif  // POWER_SERVICE_HPP
