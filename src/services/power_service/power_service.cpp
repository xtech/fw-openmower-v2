//
// Created by clemens on 09.09.24.
//

#include "power_service.hpp"

#include <drivers/bq_2576/bq_2576.hpp>
#include <robot.hpp>

#include "board.h"

PowerService::PowerService(uint16_t service_id) : PowerServiceBase(service_id, 1000000, wa, sizeof(wa)) {
}

void PowerService::OnStart() {
  charger_configured_ = false;
}

void PowerService::tick() {
  if (!charger_configured_) {
    // charger not configured, configure it
    if (charger.init(Robot::Power::GetPowerI2CD())) {
      // Set the currents low
      bool success = true;
      success &= charger.setPreChargeCurrent(0.250f);
      success &= charger.setTerminationCurrent(0.250f);
      success &= charger.setChargingCurrent(Robot::Power::GetChargeCurrent(), false);
      // Disable temperature sense, the battery doesnt have it
      success &= charger.setTsEnabled(false);
      charger_configured_ = success;
    }
  } else {
    // charger is configured, do monitoring
    bool success = true;
    success &= charger.resetWatchdog();
    success &= charger.readChargeCurrent(charge_current);
    success &= charger.readBatteryVoltage(battery_volts);
    success &= charger.readAdapterVoltage(adapter_volts);
    faults = charger.readFaults();
    success &= charger.getChargerFlags(flags1, flags2, flags3);
    success &= charger.getChargerStatus(status1, status2, status3);

    if (!success || status1 & 0b1000) {
      // Error during comms or watchdog timer expired, reconfigure charger
      charger_configured_ = false;
    } else {
      // TODO: force CM4 off here
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
    if (faults) {
      SendChargingStatus(CHARGE_STATUS_FAULT, strlen(CHARGE_STATUS_FAULT));
    } else {
      switch (status1 & 0b111) {
        case 0: SendChargingStatus(CHARGE_STATUS_NOT_CHARGING, strlen(CHARGE_STATUS_NOT_CHARGING)); break;
        case 1: SendChargingStatus(CHARGE_STATUS_TRICKLE, strlen(CHARGE_STATUS_TRICKLE)); break;
        case 2: SendChargingStatus(CHARGE_STATUS_PRE_CHARGE, strlen(CHARGE_STATUS_PRE_CHARGE)); break;
        case 3: SendChargingStatus(CHARGE_STATUS_CC, strlen(CHARGE_STATUS_CC)); break;
        case 4: SendChargingStatus(CHARGE_STATUS_CV, strlen(CHARGE_STATUS_CV)); break;
        case 6: SendChargingStatus(CHARGE_STATUS_TOP_OFF, strlen(CHARGE_STATUS_TOP_OFF)); break;
        case 7: SendChargingStatus(CHARGE_STATUS_DONE, strlen(CHARGE_STATUS_DONE)); break;
        default: SendChargingStatus(CHARGE_STATUS_ERROR, strlen(CHARGE_STATUS_ERROR)); break;
      }
    }
  } else {
    SendChargingStatus(CHARGE_STATUS_CHARGER_NOT_FOUND, strlen(CHARGE_STATUS_CHARGER_NOT_FOUND));
  }
  SendBatteryVoltage(battery_volts);
  SendChargeVoltage(adapter_volts);
  SendChargeCurrent(charge_current);
  SendChargerEnabled(true);
  CommitTransaction();
}

bool PowerService::OnChargingAllowedChanged(const uint8_t& new_value) {
  (void)new_value;
  return true;
}
