//
// Created by clemens on 09.09.24.
//

#include "power_service.hpp"

#include "bq_2576/bq_2576.hpp"

PowerService::PowerService(uint16_t service_id)
    : PowerServiceBase(service_id, 1000000, wa, sizeof(wa)) {}
bool PowerService::Configure() { return true; }

void PowerService::OnStart() { charger_configured_ = false; }
void PowerService::OnCreate() {}
void PowerService::OnStop() {}

void PowerService::tick() {
  if (!charger_configured_) {
    // charger not configured, configure it
    if (BQ2576::init()) {
      // Set the currents low
      bool success = true;
      success &= BQ2576::setPreChargeCurrent(0.250f);
      success &= BQ2576::setTerminationCurrent(0.250f);
      success &= BQ2576::setChargingCurrent(0.4f, false);
      // Disable temperature sense, the battery doesnt have it
      success &= BQ2576::setTsEnabled(false);
      charger_configured_ = success;
    }
  } else {
    // charger is configured, do monitoring
    bool success = true;
    success &= BQ2576::resetWatchdog();
    success &= BQ2576::readChargeCurrent(charge_current);
    success &= BQ2576::readBatteryVoltage(battery_volts);
    success &= BQ2576::readAdapterVoltage(adapter_volts);
    faults = BQ2576::readFaults();
    success &= BQ2576::getChargerFlags(flags1, flags2, flags3);
    success &= BQ2576::getChargerStatus(status1, status2, status3);

    if (!success || status1 & 0b1000) {
      // Error during comms or watchdog timer expired, reconfigure charger
      charger_configured_ = false;
    } else {
      // TODO: force CM4 off here
      /*if (battery_volts < Robot::CRITICAL_VOLTAGE) {
        critical_count++;
        if (critical_count > 10) {
          xcore::CM::PowerPin::reset();
        }
      } else {
        critical_count = 0;
        xcore::CM::PowerPin::set();
      }*/
    }
  }
  // Send the sensor values
  StartTransaction();
  if (charger_configured_) {
    if (faults) {
      SendChargingStatus(CHARGE_STATUS_FAULT, strlen(CHARGE_STATUS_FAULT));
    } else {
      switch (status1 & 0b111) {
        case 0:
          SendChargingStatus(CHARGE_STATUS_NOT_CHARGING,
                             strlen(CHARGE_STATUS_NOT_CHARGING));
          break;
        case 1:
          SendChargingStatus(CHARGE_STATUS_TRICKLE,
                             strlen(CHARGE_STATUS_TRICKLE));
          break;
        case 2:
          SendChargingStatus(CHARGE_STATUS_PRE_CHARGE,
                             strlen(CHARGE_STATUS_PRE_CHARGE));
          break;
        case 3:
          SendChargingStatus(CHARGE_STATUS_CC, strlen(CHARGE_STATUS_CC));
          break;
        case 4:
          SendChargingStatus(CHARGE_STATUS_CV, strlen(CHARGE_STATUS_CV));
          break;
        case 6:
          SendChargingStatus(CHARGE_STATUS_TOP_OFF,
                             strlen(CHARGE_STATUS_TOP_OFF));
          break;
        case 7:
          SendChargingStatus(CHARGE_STATUS_DONE, strlen(CHARGE_STATUS_DONE));
          break;
        default:
          SendChargingStatus(CHARGE_STATUS_ERROR, strlen(CHARGE_STATUS_ERROR));
          break;
      }
    }
  } else {
    SendChargingStatus(CHARGE_STATUS_CHARGER_NOT_FOUND,
                       strlen(CHARGE_STATUS_CHARGER_NOT_FOUND));
  }
  SendBatteryVoltage(battery_volts);
  SendChargeVoltage(adapter_volts);
  SendChargeCurrent(charge_current);
  SendChargerEnabled(true);
  CommitTransaction();
}
bool PowerService::OnChargingAllowedChanged(const uint8_t& new_value) {
  return true;
}
