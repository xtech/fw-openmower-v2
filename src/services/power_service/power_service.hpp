//
// Created by clemens on 09.09.24.
//

#ifndef POWER_SERVICE_HPP
#define POWER_SERVICE_HPP

#include <ch.h>

#include <PowerServiceBase.hpp>
#include <drivers/bms/bms_driver.hpp>
#include <drivers/charger/charger.hpp>
#include <limits>
#include <xbot-service/Lock.hpp>

using namespace xbot::service;
using namespace xbot::driver::bms;

using CHARGER_STATUS = ChargerDriver::CHARGER_STATUS;

class PowerService : public PowerServiceBase {
 public:
  explicit PowerService(uint16_t service_id) : PowerServiceBase(service_id, wa, sizeof(wa)) {
  }

  void SetDriver(ChargerDriver* charger_driver);
  void SetDriver(BmsDriver* bms_driver);

  [[nodiscard]] float GetChargeCurrent() {
    xbot::service::Lock lk{&mtx_};
    return charge_current;
  }

  [[nodiscard]] float GetAdapterVolts() {
    xbot::service::Lock lk{&mtx_};
    return adapter_volts;
  }

  [[nodiscard]] float GetBatteryVolts() {
    xbot::service::Lock lk{&mtx_};
    return battery_volts;
  }

  [[nodiscard]] float GetBatteryPercent() {
    xbot::service::Lock lk{&mtx_};
    return battery_percent;
  }

  [[nodiscard]] CHARGER_STATUS GetChargerStatus() {
    xbot::service::Lock lk{&mtx_};
    return charger_status;
  }

  [[nodiscard]] float GetAdapterCurrent() {
    xbot::service::Lock lk{&mtx_};
    return adapter_current;
  }

  [[nodiscard]] float GetDCDCCurrent() {
    xbot::service::Lock lk{&mtx_};
    return dcdc_current;
  }

  [[nodiscard]] float GetConfiguredChargeCurrent() const {
    return ChargeCurrent.valid ? ChargeCurrent.value : std::numeric_limits<float>::quiet_NaN();
  }

  float GetSystemCurrentLimit() const {
    return SystemCurrentLimit.valid ? SystemCurrentLimit.value : std::numeric_limits<float>::quiet_NaN();
  }

  using PowerManagementCallback = etl::delegate<void()>;
  void SetPowerManagementCallback(PowerManagementCallback callback) {
    power_management_callback_ = callback;
  }

 protected:
  bool OnStart() override;

 private:
  MUTEX_DECL(mtx_);

  static constexpr auto CHARGE_STATUS_NOT_FOUND_STR = "Charger Not Found";

  void tick();          // Service tick
  void drivers_tick();  // Tick for all drivers (i.e charger, BMS, ...)
  void charger_tick();  // Charger specific tick

  ServiceSchedule tick_schedule_{*this, 1'000'000, XBOT_FUNCTION_FOR_METHOD(PowerService, &PowerService::tick, this)};
  Schedule driver_schedule_{scheduler_, true, 1'000'000,
                            XBOT_FUNCTION_FOR_METHOD(PowerService, &PowerService::drivers_tick, this)};

  bool charger_configured_ = false;
  float charge_current = 0;
  float adapter_volts = 0;
  float battery_volts = 0;
  float battery_percent = 0;

  // Most designs don't have these
  float adapter_volts_adc = std::numeric_limits<float>::quiet_NaN();
  float battery_volts_adc = std::numeric_limits<float>::quiet_NaN();
  float adapter_current = std::numeric_limits<float>::quiet_NaN();
  float dcdc_current = std::numeric_limits<float>::quiet_NaN();

  int critical_count = 0;
  CHARGER_STATUS charger_status = CHARGER_STATUS::COMMS_ERROR;
  ChargerDriver* charger_ = nullptr;
  BmsDriver* bms_ = nullptr;

  PowerManagementCallback power_management_callback_;

  THD_WORKING_AREA(wa, 1500){};

 protected:
  void OnChargingAllowedChanged(const uint8_t& new_value) override;
};

#endif  // POWER_SERVICE_HPP
