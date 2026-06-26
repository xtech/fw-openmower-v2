//
// Created by clemens on 09.09.24.
//

#include "power_service.hpp"

#include <ulog.h>

#include <cstdio>
#include <cstring>
#include <globals.hpp>

#include "board.h"
#include "drivers/adc/adc1.hpp"

using namespace xbot::driver;

// Static assertions to ensure ChargerDriver::ReChargeVoltage enum matches PowerService ReChargeVoltages enum
static_assert(static_cast<uint8_t>(ChargerDriver::ReChargeVoltage::PERCENT_93_0) == 0, "ReChargeVoltage enum mismatch");
static_assert(static_cast<uint8_t>(ChargerDriver::ReChargeVoltage::PERCENT_94_3) == 1, "ReChargeVoltage enum mismatch");
static_assert(static_cast<uint8_t>(ChargerDriver::ReChargeVoltage::PERCENT_95_2) == 2, "ReChargeVoltage enum mismatch");
static_assert(static_cast<uint8_t>(ChargerDriver::ReChargeVoltage::PERCENT_97_6) == 3, "ReChargeVoltage enum mismatch");

void PowerService::SetDriver(ChargerDriver* charger_driver) {
  charger_ = charger_driver;
}

bool PowerService::OnStart() {
  charger_configured_ = false;
  if (DangerouslyOverrideHardwareChargeCurrentLimit.valid && DangerouslyOverrideHardwareChargeCurrentLimit.value) {
    ULOG_ARG_WARNING(
        &service_id_,
        "DangerouslyOverrideHardwareChargeCurrentLimit is set - hardware current limits will be bypassed!");
  }
  return true;
}

void PowerService::service_tick_() {
  xbot::service::Lock lk{&mtx_};
  // Send the sensor values
  StartTransaction();
  if (charger_configured_) {
    const char* status_text = ChargerDriver::statusToString(charger_status_);
    SendChargingStatus(status_text, strlen(status_text));
  } else {
    SendChargingStatus(CHARGE_STATUS_NOT_FOUND_STR, strlen(CHARGE_STATUS_NOT_FOUND_STR));
  }
  SendBatteryVoltage(battery_volts_);
  SendChargeVoltage(adapter_volts_);
  SendChargeCurrent(charge_current_);
  SendChargerEnabled(true);
  if (BatteryFullVoltage.valid && BatteryEmptyVoltage.valid) {
    battery_percent_ =
        (battery_volts_ - BatteryEmptyVoltage.value) / (BatteryFullVoltage.value - BatteryEmptyVoltage.value);
  } else {
    battery_percent_ = (battery_volts_ - robot->Power_GetDefaultBatteryEmptyVoltage()) /
                       (robot->Power_GetDefaultBatteryFullVoltage() - robot->Power_GetDefaultBatteryEmptyVoltage());
  }
  SendBatteryPercentage(etl::max(0.0f, etl::min(1.0f, battery_percent_)));
  SendChargerInputCurrent(adapter_current_);

  // ADC values
  SendBatteryVoltageADC(battery_volts_adc_);
  SendChargeVoltageADC(adapter_volts_adc_);
  SendDCDCInputCurrent(dcdc_current_);

  CommitTransaction();
}

void PowerService::driver_tick_() {
  update_charger_();
  read_adc_();

  if (charger_configured_ && power_management_callback_) {
    power_management_callback_();
  }
}

void PowerService::read_adc_() {
  // Debugging: adc1::DumpBenchmarkMeasurement(Adc1ConversionId::V_BATTERY, "V-BAT");
  xbot::service::Lock lk{&mtx_};
  adapter_volts_adc_ = adc1::GetValueOrNaN(adc1::Adc1ConversionId::V_CHARGER, 100);
  battery_volts_adc_ = adc1::GetValueOrNaN(adc1::Adc1ConversionId::V_BATTERY, 100);
  dcdc_current_ = adc1::GetValueOrNaN(adc1::Adc1ConversionId::I_IN_DCDC, 100);
}

void PowerService::update_charger_() {
  if (charger_ == nullptr) {
    ULOG_ARG_ERROR(&service_id_, "Charger is null!");
    return;
  }

  if (!charger_configured_) {
    // charger not configured, configure it
    if (charger_->init()) {
      // Set the currents low
      bool success = true;
      if (PreChargeCurrent.valid && PreChargeCurrent.value > 0) {
        success &= charger_->setPreChargeCurrent(PreChargeCurrent.value);
      } else {
        success &= charger_->setPreChargeCurrent(robot->Power_GetDefaultPreChargeCurrent());
      }

      float software_charge_current = robot->Power_GetDefaultChargeCurrent();

      // Check, if the user has provided custom current. If so, use it
      if (ChargeCurrent.valid && ChargeCurrent.value > 0) {
        software_charge_current = ChargeCurrent.value;
      }

      // Check, if the user feels dangerous and allows higher charging currents
      bool override_limit =
          DangerouslyOverrideHardwareChargeCurrentLimit.valid && DangerouslyOverrideHardwareChargeCurrentLimit.value;

      if (!override_limit) {
        // Limit the current to the max value provided by the robot
        software_charge_current = std::min(robot->Power_GetMaxChargeCurrent(), software_charge_current);
      }

      // Hardware resistor is the default setting when not using software-control.
      // It's a very conservative choice so that charging without firmware is safe.
      // Therefore, we set "overwrite_hardware_limit" to true here, so that we can go for a higher current.
      // On watchdog timeout, the charger will automatically switch to hardware resistor.
      success &= charger_->setChargingCurrent(software_charge_current, true);

      if (ChargeVoltage.valid && ChargeVoltage.value > 0) {
        success &= charger_->setChargingVoltage(ChargeVoltage.value);
      } else {
        // Only set a custom value, if the robot implementation provides one.
        if (robot->Power_GetDefaultChargeVoltage() > 0.0) {
          success &= charger_->setChargingVoltage(robot->Power_GetDefaultChargeVoltage());
        }
      }
      if (TerminationCurrent.valid && TerminationCurrent.value > 0) {
        success &= charger_->setTerminationCurrent(TerminationCurrent.value);
      } else {
        success &= charger_->setTerminationCurrent(robot->Power_GetDefaultTerminationCurrent());
      }
      if (ReChargeVoltage.valid) {
        success &= charger_->setReChargeVoltage(static_cast<ChargerDriver::ReChargeVoltage>(ReChargeVoltage.value));
      } else {
        success &= charger_->setReChargeVoltage(robot->Power_GetDefaultReChargeVoltage());
      }

      // Disable temperature sense, the battery doesn't have it
      success &= charger_->setTsEnabled(false);
      charger_configured_ = success;
    }

    if (charger_configured_) {
      ULOG_ARG_INFO(&service_id_, "Successfully Configured Charger");
    } else {
      ULOG_ARG_ERROR(&service_id_, "Unable to Configure Charger");
    }
  } else {
    xbot::service::Lock lk{&mtx_};
    // charger is configured, do monitoring
    bool success = true;
    {
      bool s = charger_->resetWatchdog();
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Resetting Watchdog");
      }
      success &= s;
    }
    {
      bool s = charger_->readChargeCurrent(charge_current_);
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Reading Charge Current");
      }
      success &= s;
    }
    {
      bool s = charger_->readBatteryVoltage(battery_volts_);
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Reading Battery Voltage");
      }
      success &= s;
    }
    {
      bool s = charger_->readAdapterVoltage(adapter_volts_);
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Reading Adapter Voltage");
      }
      success &= s;
    }
    {
      bool s = charger_->readAdapterCurrent(adapter_current_);
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Reading Adapter Current");
      }
      success &= s;
    }
    charger_status_ = charger_->getChargerStatus();

    if (!success || charger_status_ == CHARGER_STATUS::COMMS_ERROR) {
      // Error during comms or watchdog timer expired, reconfigure charger
      charger_configured_ = false;
      ULOG_ARG_ERROR(&service_id_, "Error during charging comms - reconfiguring");
    } else {
      if (battery_volts_ < robot->Power_GetAbsoluteMinVoltage()) {
        critical_count_++;
        if (critical_count_ > 10) {
          palClearLine(LINE_HIGH_LEVEL_GLOBAL_EN);
        }
      } else {
        critical_count_ = 0;
        palSetLine(LINE_HIGH_LEVEL_GLOBAL_EN);
      }
    }
  }
}

void PowerService::OnChargingAllowedChanged(const uint8_t& new_value) {
  (void)new_value;
}
