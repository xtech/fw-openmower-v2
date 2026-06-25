//
// Created by clemens on 09.09.24.
//

#ifndef POWER_SERVICE_HPP
#define POWER_SERVICE_HPP

#include <ch.h>
#include <etl/atomic.h>

#include <PowerServiceBase.hpp>
#include <drivers/charger/charger.hpp>
#include <limits>
#include <xbot-service/Lock.hpp>

using namespace xbot::service;

using CHARGER_STATUS = ChargerDriver::CHARGER_STATUS;

class PowerService : public PowerServiceBase {
 public:
  explicit PowerService(uint16_t service_id) : PowerServiceBase(service_id, wa, sizeof(wa)) {
  }

  void SetDriver(ChargerDriver* charger_driver);

  [[nodiscard]] float GetChargeCurrent() {
    xbot::service::Lock lk{&mtx_};
    return charge_current_;
  }

  [[nodiscard]] float GetAdapterVolts() {
    xbot::service::Lock lk{&mtx_};
    return adapter_volts_;
  }

  [[nodiscard]] float GetBatteryVolts() {
    xbot::service::Lock lk{&mtx_};
    return battery_volts_;
  }

  [[nodiscard]] float GetBatteryPercent() {
    xbot::service::Lock lk{&mtx_};
    return battery_percent_;
  }

  [[nodiscard]] CHARGER_STATUS GetChargerStatus() {
    xbot::service::Lock lk{&mtx_};
    return charger_status_;
  }

  [[nodiscard]] float GetAdapterCurrent() {
    xbot::service::Lock lk{&mtx_};
    return adapter_current_;
  }

  [[nodiscard]] float GetDCDCCurrent() {
    xbot::service::Lock lk{&mtx_};
    return dcdc_current_;
  }

  [[nodiscard]] float GetAdcAdapterVolts() {
    xbot::service::Lock lk{&mtx_};
    return adapter_volts_adc_;
  }

  [[nodiscard]] float GetAdcBatteryVolts() {
    xbot::service::Lock lk{&mtx_};
    return battery_volts_adc_;
  }

  float GetConfiguredChargeCurrent() const {
    return ChargeCurrent.valid ? ChargeCurrent.value : std::numeric_limits<float>::quiet_NaN();
  }

  float GetConfiguredSystemCurrent() const {
    return SystemCurrent.valid ? SystemCurrent.value : std::numeric_limits<float>::quiet_NaN();
  }

  using PowerManagementCallback = etl::delegate<void()>;
  void SetPowerManagementCallback(PowerManagementCallback callback) {
    power_management_callback_ = callback;
  }

  bool IsHealthy() override {
    return IsRunning() && charger_configured_.load();
  }

 protected:
  bool OnStart() override;

 private:
  MUTEX_DECL(mtx_);

  static constexpr auto CHARGE_STATUS_NOT_FOUND_STR = "Charger Not Found";

  void service_tick_();
  void driver_tick_();
  void update_charger_();
  void read_adc_();

  ServiceSchedule tick_schedule_{*this, 1'000'000,
                                 XBOT_FUNCTION_FOR_METHOD(PowerService, &PowerService::service_tick_, this)};
  Schedule driver_schedule_{scheduler_, true, 1'000'000,
                            XBOT_FUNCTION_FOR_METHOD(PowerService, &PowerService::driver_tick_, this)};

  etl::atomic<bool> charger_configured_{false};
  float charge_current_ = 0;
  float adapter_volts_ = 0;
  float battery_volts_ = 0;
  float battery_percent_ = 0;

  // Most designs don't have these
  float adapter_volts_adc_ = std::numeric_limits<float>::quiet_NaN();
  float battery_volts_adc_ = std::numeric_limits<float>::quiet_NaN();
  float adapter_current_ = std::numeric_limits<float>::quiet_NaN();
  float dcdc_current_ = std::numeric_limits<float>::quiet_NaN();

  int critical_count_ = 0;
  CHARGER_STATUS charger_status_ = CHARGER_STATUS::COMMS_ERROR;
  ChargerDriver* charger_ = nullptr;

  PowerManagementCallback power_management_callback_;

  THD_WORKING_AREA(wa, 1500){};

 protected:
  void OnChargingAllowedChanged(const uint8_t& new_value) override;
};

#endif  // POWER_SERVICE_HPP
