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

void PowerService::SetDriver(ChargerDriver* charger_driver) {
  charger_ = charger_driver;
}

void PowerService::SetDriver(BmsDriver* bms_driver) {
  bms_ = bms_driver;
}

bool PowerService::OnStart() {
  charger_configured_ = false;
  return true;
}

void PowerService::tick() {
  xbot::service::Lock lk{&mtx_};
  // Send the sensor values
  StartTransaction();
  if (charger_configured_) {
    const char* status_text = ChargerDriver::statusToString(charger_status);
    SendChargingStatus(status_text, strlen(status_text));
  } else {
    SendChargingStatus(CHARGE_STATUS_NOT_FOUND_STR, strlen(CHARGE_STATUS_NOT_FOUND_STR));
  }
  SendBatteryVoltageCHG(battery_volts);
  SendChargeVoltageCHG(adapter_volts);
  SendChargeCurrent(charge_current);
  SendChargerEnabled(true);
  float battery_percent;
  if (BatteryFullVoltage.valid && BatteryEmptyVoltage.valid) {
    battery_percent =
        (battery_volts - BatteryEmptyVoltage.value) / (BatteryFullVoltage.value - BatteryEmptyVoltage.value);
  } else {
    battery_percent = (battery_volts - robot->Power_GetDefaultBatteryEmptyVoltage()) /
                      (robot->Power_GetDefaultBatteryFullVoltage() - robot->Power_GetDefaultBatteryEmptyVoltage());
  }
  SendBatteryPercentage(etl::max(0.0f, etl::min(1.0f, battery_percent)));
  SendChargerInputCurrent(adapter_current);

  // BMS values
  if (bms_data_ != nullptr) {
    SendBatteryCurrent(bms_data_->pack_current_a);
    SendBatteryVoltageBMS(bms_data_->pack_voltage_v);
    SendBatterySoC(bms_data_->battery_soc);
    SendBatteryTemperature(bms_data_->temperature_c);
    SendBatteryStatus(bms_data_->battery_status);
  }
  if (bms_extra_data_ != nullptr) {
    SendBMSExtraData(bms_extra_data_, (uint32_t)strlen(bms_extra_data_));
  }

  // ADC values
  SendBatteryVoltageADC(battery_volts_adc);
  SendChargeVoltageADC(adapter_volts_adc);
  SendDCDCInputCurrent(dcdc_current);

  CommitTransaction();
}

void PowerService::drivers_tick() {
  charger_tick();

  // Optional BMS tick and data retrieval
  if (bms_ != nullptr) {
    bms_->Tick();
    if (bms_->IsPresent()) {
      bms_data_ = bms_->GetData();
      bms_extra_data_ = bms_->GetExtraDataJson();
    }
  }

  // ADC readings (for debugging use e.g.: adc1::DumpBenchmarkMeasurement(Adc1ConversionId::V_BATTERY, "V-BAT");
  {
    xbot::service::Lock lk{&mtx_};
    adapter_volts_adc = adc1::GetValueOrNaN(adc1::Adc1ConversionId::V_CHARGER, 100);
    battery_volts_adc = adc1::GetValueOrNaN(adc1::Adc1ConversionId::V_BATTERY, 100);
    dcdc_current = adc1::GetValueOrNaN(adc1::Adc1ConversionId::I_IN_DCDC, 100);
  }

  if (charger_configured_ && power_management_callback_) {
    power_management_callback_();
  }
}

void PowerService::charger_tick() {
  if (charger_ == nullptr) {
    ULOG_ARG_ERROR(&service_id_, "Charger is null!");
    return;
  }

  if (!charger_configured_) {
    // charger not configured, configure it
    if (charger_->init()) {
      // Set the currents low
      bool success = true;
      success &= charger_->setPreChargeCurrent(0.250f);
      success &= charger_->setTerminationCurrent(0.250f);
      if (ChargeCurrent.valid && ChargeCurrent.value > 0) {
        success &= charger_->setChargingCurrent(ChargeCurrent.value, false);
      } else {
        success &= charger_->setChargingCurrent(robot->Power_GetDefaultChargeCurrent(), false);
      }
      // Disable temperature sense, the battery doesnt have it
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
      bool s = charger_->readChargeCurrent(charge_current);
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Reading Charge Current");
      }
      success &= s;
    }
    {
      bool s = charger_->readBatteryVoltage(battery_volts);
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Reading Battery Voltage");
      }
      success &= s;
    }
    {
      bool s = charger_->readAdapterVoltage(adapter_volts);
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Reading Adapter Voltage");
      }
      success &= s;
    }
    {
      bool s = charger_->readAdapterCurrent(adapter_current);
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Reading Adapter Current");
      }
      success &= s;
    }
    charger_status = charger_->getChargerStatus();

    if (!success || charger_status == CHARGER_STATUS::COMMS_ERROR) {
      // Error during comms or watchdog timer expired, reconfigure charger
      charger_configured_ = false;
      ULOG_ARG_ERROR(&service_id_, "Error during charging comms - reconfiguring");
    } else {
      if (battery_volts < robot->Power_GetAbsoluteMinVoltage()) {
        critical_count++;
        if (critical_count > 10) {
          palClearLine(LINE_HIGH_LEVEL_GLOBAL_EN);
        }
      } else {
        critical_count = 0;
        palSetLine(LINE_HIGH_LEVEL_GLOBAL_EN);
      }
    }
  }
}

void PowerService::OnChargingAllowedChanged(const uint8_t& new_value) {
  (void)new_value;
}
