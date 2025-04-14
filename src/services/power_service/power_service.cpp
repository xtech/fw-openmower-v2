//
// Created by clemens on 09.09.24.
//

#include "power_service.hpp"

#include <ulog.h>

#include <robot.hpp>

#include "board.h"

bool PowerService::OnStart() {
  charger_configured_ = false;
  return true;
}

void PowerService::tick() {
  if (charger == nullptr) {
    ULOG_ARG_ERROR(&service_id_, "Charger is null!");
    return;
  }

  if (!charger_configured_) {
    // charger not configured, configure it
    if (charger->init(Robot::Power::GetPowerI2CD())) {
      // Set the currents low
      bool success = true;
      success &= charger->setPreChargeCurrent(0.250f);
      success &= charger->setTerminationCurrent(0.250f);
      success &= charger->setChargingCurrent(Robot::Power::GetChargeCurrent(), false);
      // Disable temperature sense, the battery doesnt have it
      success &= charger->setTsEnabled(false);
      charger_configured_ = success;
    }

    if (charger_configured_) {
      ULOG_ARG_INFO(&service_id_, "Successfully Configured Charger");
    } else {
      ULOG_ARG_ERROR(&service_id_, "Unable to Configure Charger");
    }
  } else {
    // charger is configured, do monitoring
    bool success = true;
    {
      bool s = charger->resetWatchdog();
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Resetting Watchdog");
      }
      success &= s;
    }
    {
      bool s = charger->readChargeCurrent(charge_current);
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Reading Charge Current");
      }
      success &= s;
    }
    {
      bool s = charger->readBatteryVoltage(battery_volts);
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Reading Battery Voltage");
      }
      success &= s;
    }
    {
      bool s = charger->readAdapterVoltage(adapter_volts);
      if (!s) {
        ULOG_ARG_WARNING(&service_id_, "Error Reading Adapter Voltage");
      }
      success &= s;
    }
    charger_status = charger->getChargerStatus();

    if (!success || charger_status == CHARGER_STATUS::COMMS_ERROR) {
      // Error during comms or watchdog timer expired, reconfigure charger
      charger_configured_ = false;
      ULOG_ARG_ERROR(&service_id_, "Error during charging comms - reconfiguring");
    } else {
      if (battery_volts < Robot::Power::GetMinVoltage()) {
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
  // Send the sensor values
  StartTransaction();
  if (charger_configured_) {
    switch (charger_status) {
      case CHARGER_STATUS::NOT_CHARGING:
        SendChargingStatus(CHARGE_STATUS_NOT_CHARGING_STR, strlen(CHARGE_STATUS_NOT_CHARGING_STR));
        break;
      case CHARGER_STATUS::TRICKLE:
        SendChargingStatus(CHARGE_STATUS_TRICKLE_STR, strlen(CHARGE_STATUS_TRICKLE_STR));
        break;
      case CHARGER_STATUS::PRE_CHARGE:
        SendChargingStatus(CHARGE_STATUS_PRE_CHARGE_STR, strlen(CHARGE_STATUS_PRE_CHARGE_STR));
        break;
      case CHARGER_STATUS::CC: SendChargingStatus(CHARGE_STATUS_CC_STR, strlen(CHARGE_STATUS_CC_STR)); break;
      case CHARGER_STATUS::CV: SendChargingStatus(CHARGE_STATUS_CV_STR, strlen(CHARGE_STATUS_CV_STR)); break;
      case CHARGER_STATUS::TOP_OFF:
        SendChargingStatus(CHARGE_STATUS_TOP_OFF_STR, strlen(CHARGE_STATUS_TOP_OFF_STR));
        break;
      case CHARGER_STATUS::DONE: SendChargingStatus(CHARGE_STATUS_DONE_STR, strlen(CHARGE_STATUS_DONE_STR)); break;
      case CHARGER_STATUS::FAULT: SendChargingStatus(CHARGE_STATUS_FAULT_STR, strlen(CHARGE_STATUS_FAULT_STR)); break;
      case CHARGER_STATUS::COMMS_ERROR:
      case CHARGER_STATUS::UNKNOWN:
        SendChargingStatus(CHARGE_STATUS_UNKNOWN_STR, strlen(CHARGE_STATUS_UNKNOWN_STR));
        break;
      default: SendChargingStatus(CHARGE_STATUS_ERROR_STR, strlen(CHARGE_STATUS_ERROR_STR)); break;
    }
  } else {
    SendChargingStatus(CHARGE_STATUS_NOT_FOUND_STR, strlen(CHARGE_STATUS_NOT_FOUND_STR));
  }
  SendBatteryVoltage(battery_volts);
  SendChargeVoltage(adapter_volts);
  SendChargeCurrent(charge_current);
  SendChargerEnabled(true);
  CommitTransaction();
}

void PowerService::OnChargingAllowedChanged(const uint8_t& new_value) {
  (void)new_value;
}
